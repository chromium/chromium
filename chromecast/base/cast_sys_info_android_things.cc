// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_sys_info_android_things.h"

#include "base/android/jni_string.h"
#include "chromecast/chromecast_buildflags.h"
#if BUILDFLAG(IS_ANDROID_THINGS_NON_PUBLIC)
#include "base/android/jni_array.h"
#include "chromecast/browser/jni_headers/CastSysInfoAndroidThings_jni.h"
#endif

namespace chromecast {

CastSysInfoAndroidThings::CastSysInfoAndroidThings() = default;
CastSysInfoAndroidThings::~CastSysInfoAndroidThings() = default;

std::string CastSysInfoAndroidThings::GetProductName() {
#if BUILDFLAG(IS_ANDROID_THINGS_NON_PUBLIC)
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      Java_CastSysInfoAndroidThings_getProductName(env));
#else
  return "";
#endif
}

std::string CastSysInfoAndroidThings::GetDeviceModel() {
#if BUILDFLAG(IS_ANDROID_THINGS_NON_PUBLIC)
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      Java_CastSysInfoAndroidThings_getDeviceModel(env));
#else
  return "";
#endif
}

std::string CastSysInfoAndroidThings::GetManufacturer() {
#if BUILDFLAG(IS_ANDROID_THINGS_NON_PUBLIC)
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      Java_CastSysInfoAndroidThings_getManufacturer(env));
#else
  return "";
#endif
}

std::string CastSysInfoAndroidThings::GetSystemReleaseChannel() {
#if BUILDFLAG(IS_ANDROID_THINGS_NON_PUBLIC)
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      Java_CastSysInfoAndroidThings_getReleaseChannel(env));
#else
  return "";
#endif
}

std::vector<std::string> CastSysInfoAndroidThings::GetFactoryLocaleList() {
  std::vector<std::string> locale_list;
#if BUILDFLAG(IS_ANDROID_THINGS_NON_PUBLIC)
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::AppendJavaStringArrayToStringVector(
      env, Java_CastSysInfoAndroidThings_getFactoryLocaleList(env),
      &locale_list);
#endif
  if (locale_list.empty()) {
      locale_list = CastSysInfoAndroid::GetFactoryLocaleList();
  }
  return locale_list;
}

}  // namespace chromecast
