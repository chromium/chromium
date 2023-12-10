// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/app/android/cast_crash_reporter_client_android.h"

#include "base/android/build_info.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chromecast/base/cast_sys_info_android.h"
#include "chromecast/base/version.h"
#include "chromecast/common/global_descriptors.h"
#include "content/public/common/content_switches.h"

namespace chromecast {

CastCrashReporterClientAndroid::CastCrashReporterClientAndroid(
    const std::string& process_type)
    : process_type_(process_type) {
}

CastCrashReporterClientAndroid::~CastCrashReporterClientAndroid() {
}

void CastCrashReporterClientAndroid::GetProductNameAndVersion(
    std::string* product_name,
    std::string* version,
    std::string* channel) {
  *product_name = "media_shell";
  *version = CAST_BUILD_RELEASE ".";
  *version += base::android::BuildInfo::GetInstance()->package_version_code();
#if CAST_IS_DEBUG_BUILD()
  *version += ".debug";
#endif
  CastSysInfoAndroid sys_info;
  *channel = sys_info.GetSystemReleaseChannel();
}

base::FilePath CastCrashReporterClientAndroid::GetReporterLogFilename() {
  return base::FilePath(FILE_PATH_LITERAL("uploads.log"));
}

// static
bool CastCrashReporterClientAndroid::GetCrashReportsLocation(
    const std::string& process_type,
    base::FilePath* crash_dir) {
  base::FilePath crash_dir_local;
  if (!base::PathService::Get(base::DIR_ANDROID_APP_DATA, &crash_dir_local)) {
    return false;
  }
  crash_dir_local = crash_dir_local.Append("crashes");

  // Only try to create the directory in the browser process (empty value).
  if (process_type.empty()) {
    if (!base::DirectoryExists(crash_dir_local)) {
      if (!base::CreateDirectory(crash_dir_local)) {
        return false;
      }
    }
  }

  // Provide value to crash_dir once directory is known to be a valid path.
  *crash_dir = crash_dir_local;
  return true;
}

bool CastCrashReporterClientAndroid::GetCrashDumpLocation(
    base::FilePath* crash_dir) {
  base::FilePath app_data;
  if (!base::PathService::Get(base::DIR_ANDROID_APP_DATA, &app_data)) {
    return false;
  }

  *crash_dir = app_data.Append("Crashpad");
  return true;
}

int CastCrashReporterClientAndroid::GetAndroidMinidumpDescriptor() {
  return kAndroidMinidumpDescriptor;
}

bool CastCrashReporterClientAndroid::EnableBreakpadForProcess(
    const std::string& process_type) {
  return process_type == switches::kRendererProcess ||
         process_type == switches::kGpuProcess;
}

}  // namespace chromecast
