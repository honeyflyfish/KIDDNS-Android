//
// Created by vipkid on 2018/12/11.
//

#include <jni.h>
#include <stdint.h>

#include <unistd.h>
#include <dlfcn.h>
#include <android/log.h>

#include <netdb.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "v_hook.h"

#define TAG_NAME        "httpdns"
#define LOGE(fmt, args...) __android_log_print(ANDROID_LOG_ERROR, TAG_NAME, (const char *) fmt, ##args)

static int
(*sys_getaddrinfo)(const char *__node, const char *__service, const struct addrinfo *__hints,
                   struct addrinfo **__result);

static JavaVM *sjavaVM;

static jclass utils;
static jmethodID getIp;

static const char *getIpByHttpDns(const char *hostname) {
  JNIEnv *senv;
  sjavaVM->AttachCurrentThread(&senv, NULL);

  jstring ip = static_cast<jstring>(senv->CallStaticObjectMethod(utils,
                                                                 getIp,
                                                                 senv->NewStringUTF(hostname)));

  if (ip) {
    return senv->GetStringUTFChars(ip, 0);
  }
  return NULL;
}

static int my_getaddrinfo(const char *__node, const char *__service, const struct addrinfo *__hints,
                          struct addrinfo **__result) {
  if (__hints->ai_flags == AI_NUMERICHOST) {
    return sys_getaddrinfo(__node, __service, __hints, __result);
  }

  const char *ip = getIpByHttpDns(__node);
  if (ip != NULL) {
    struct addrinfo *cloneInfo = new addrinfo;
    cloneInfo->ai_flags = AI_NUMERICHOST;
    cloneInfo->ai_addr = __hints->ai_addr;
    cloneInfo->ai_addrlen = __hints->ai_addrlen;
    cloneInfo->ai_canonname = __hints->ai_canonname;
    cloneInfo->ai_family = __hints->ai_family;
    cloneInfo->ai_next = __hints->ai_next;
    cloneInfo->ai_protocol = __hints->ai_protocol;
    cloneInfo->ai_socktype = __hints->ai_socktype;
    LOGE("这里是webview的哇,走了httpdns %s", ip);
    return sys_getaddrinfo(ip, __service, cloneInfo, __result);
  }
  return sys_getaddrinfo(__node, __service, __hints, __result);;
}

int hook_libc_getaddrinfo() {
  void *libc = v_dlopen("/system/lib/libc.so", RTLD_NOW);
#ifdef __aarch64__
  libc = v_dlopen("/system/lib64/libc.so", RTLD_NOW);
#endif
  void *p = v_dlsym(libc, "getaddrinfo");
  if (p != NULL) {
    v_hook_function(p,
                    reinterpret_cast<void *>(my_getaddrinfo),
                    reinterpret_cast<void **>(&sys_getaddrinfo)
    );
    v_dlclose(libc);
  }
  return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_cn_com_vipkid_vkdns_HttpDnsServiceProvider_hook(JNIEnv *env, jobject instance) {

  env->GetJavaVM(&sjavaVM);
  utils = static_cast<jclass>(env->NewGlobalRef(env->FindClass("cn/com/vipkid/vkdns/Utils")));
  if (utils != NULL) {
    getIp = env->GetStaticMethodID(utils, "getIp", "(Ljava/lang/String;)Ljava/lang/String;");
  }
  hook_libc_getaddrinfo();
}
