// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_sys_info_android.h"

#include <sys/system_properties.h>
#include <memory>
#include <string>

#include "base/android/apk_assets.h"
#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "chromecast/base/cast_sys_info_util.h"
#include "chromecast/base/version.h"
#include "chromecast/browser/jni_headers/CastSysInfoAndroid_jni.h"
#include "chromecast/chromecast_buildflags.h"

namespace chromecast {

namespace {
const char kCastConfigAssetPath[] = "assets/cast_config";

std::string GetAndroidProperty(const std::string& key,
                               const std::string& default_value) {
  char value[PROP_VALUE_MAX];
  int ret = __system_property_get(key.c_str(), value);
  if (ret <= 0) {
    DVLOG(1) << "No value set for property: " << key;
    return default_value;
  }

  return std::string(value);
}

bool DoesCastConfigFileExist() {
  base::MemoryMappedFile::Region config_region;
  int config_fd =
      base::android::OpenApkAsset(kCastConfigAssetPath, &config_region);
  if (config_fd > 0)
    close(config_fd);
  return config_fd > 0;
}
}  // namespace

CastSysInfoAndroid::CastSysInfoAndroid()
    : build_info_(base::android::BuildInfo::GetInstance()) {}

CastSysInfoAndroid::~CastSysInfoAndroid() {}

CastSysInfo::BuildType CastSysInfoAndroid::GetBuildType() {
  // TODO(b/110375912): Update this to parse the file contents and allow
  // selection based on the config values.  For now this is just checking for
  // file existence.
  if (!DoesCastConfigFileExist())
    return BUILD_PRODUCTION;

  if (CAST_IS_DEBUG_BUILD())
    return BUILD_ENG;

  // Default to BETA build.
  return BUILD_BETA;
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

}  // namespace chromecast
