// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_sys_info_android.h"

#include <sys/system_properties.h>
#include <memory>
#include <string>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/system/sys_info.h"
#include "chromecast/base/cast_sys_info_util.h"
#include "chromecast/base/version.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chromecast/base/jni_headers/CastSysInfoAndroid_jni.h"

namespace chromecast {

CastSysInfoAndroid::CastSysInfoAndroid()
    : build_info_(base::android::BuildInfo::GetInstance()) {}

CastSysInfoAndroid::~CastSysInfoAndroid() {}

CastSysInfo::BuildType CastSysInfoAndroid::GetBuildType() {
  return CAST_IS_DEBUG_BUILD() ? BUILD_ENG : BUILD_PRODUCTION;
}

std::string CastSysInfoAndroid::GetSerialNumber() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      Java_CastSysInfoAndroid_getSerialNumber(env));
}

std::string CastSysInfoAndroid::GetProductName() {
  return build_info_->device();
}

std::string CastSysInfoAndroid::GetDeviceModel() {
  return build_info_->model();
}

std::string CastSysInfoAndroid::GetManufacturer() {
  return build_info_->manufacturer();
}

std::string CastSysInfoAndroid::GetSystemBuildNumber() {
  return base::SysInfo::GetAndroidBuildID();
}

std::string CastSysInfoAndroid::GetSystemReleaseChannel() {
  return "";
}

std::string CastSysInfoAndroid::GetBoardName() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      Java_CastSysInfoAndroid_getBoard(env));
}

std::string CastSysInfoAndroid::GetBoardRevision() {
  return "";
}

std::string CastSysInfoAndroid::GetFactoryCountry() {
  return GetAndroidProperty("ro.boot.wificountrycode", "");
}

std::vector<std::string> CastSysInfoAndroid::GetFactoryLocaleList() {
  const std::string factory_locale_list =
      GetAndroidProperty("ro.product.factory_locale_list", "");
  if (!factory_locale_list.empty()) {
    std::vector<std::string> results;
    std::stringstream stream(factory_locale_list);
    while (stream.good()) {
      std::string locale;
      getline(stream, locale, ',');
      results.push_back(locale);
    }
    if (!results.empty()) {
      return results;
    }
  }
  // This duplicates the read-only property portion of
  // frameworks/base/core/jni/AndroidRuntime.cpp in the Android tree, which is
  // effectively the "factory locale", i.e. the locale chosen by Android
  // assuming the other persist.sys.* properties are not set.
  const std::string locale = GetAndroidProperty("ro.product.locale", "");
  if (!locale.empty()) {
    return {locale};
  }

  const std::string language =
      GetAndroidProperty("ro.product.locale.language", "en");
  const std::string region =
      GetAndroidProperty("ro.product.locale.region", "US");
  return {language + "-" + region};
}

std::string CastSysInfoAndroid::GetWifiInterface() {
  return "";
}

std::string CastSysInfoAndroid::GetApInterface() {
  return "";
}

std::string CastSysInfoAndroid::GetProductSsidSuffix() {
  return GetAndroidProperty("ro.odm.cast.ssid_suffix", "");
}

std::string CastSysInfoAndroid::GetAndroidProperty(
    const std::string& key,
    const std::string& default_value) {
  char value[PROP_VALUE_MAX];
  int ret = __system_property_get(key.c_str(), value);
  if (ret <= 0) {
    DVLOG(1) << "No value set for property: " << key;
    return default_value;
  }

  return std::string(value);
}

}  // namespace chromecast
