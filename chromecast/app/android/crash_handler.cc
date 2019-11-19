// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/app/android/crash_handler.h"

#include <jni.h>
#include <stdlib.h>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chromecast/app/android/cast_crash_reporter_client_android.h"
#include "chromecast/base/chromecast_config_android.h"
#include "chromecast/base/version.h"
#include "chromecast/browser/jni_headers/CastCrashHandler_jni.h"
#include "components/crash/content/app/crash_reporter_client.h"
#include "components/crash/content/app/crashpad.h"
#include "content/public/common/content_switches.h"

namespace {

chromecast::CrashHandler* g_crash_handler = NULL;

}  // namespace

namespace chromecast {

// static
void CrashHandler::Initialize(const std::string& process_type,
                              const base::FilePath& log_file_path) {
  DCHECK(!g_crash_handler);
  g_crash_handler = new CrashHandler(process_type, log_file_path);
  g_crash_handler->Initialize();
}

// static
bool CrashHandler::GetCrashDumpLocation(base::FilePath* crash_dir) {
  DCHECK(g_crash_handler);
  return g_crash_handler->crash_reporter_client_->GetCrashDumpLocation(
      crash_dir);
}

// static
bool CrashHandler::GetCrashReportsLocation(base::FilePath* reports_dir) {
  DCHECK(g_crash_handler);
  return g_crash_handler->crash_reporter_client_->GetCrashReportsLocation(
      g_crash_handler->process_type_, reports_dir);
}

CrashHandler::CrashHandler(const std::string& process_type,
                           const base::FilePath& log_file_path)
    : log_file_path_(log_file_path),
      process_type_(process_type),
      crash_reporter_client_(new CastCrashReporterClientAndroid(process_type)) {
  if (!crash_reporter_client_->GetCrashDumpLocation(&crash_dump_path_)) {
    LOG(ERROR) << "Could not get crash dump location";
  }
  SetCrashReporterClient(crash_reporter_client_.get());
}

CrashHandler::~CrashHandler() {
  DCHECK(g_crash_handler);
  g_crash_handler = NULL;
}

void CrashHandler::Initialize() {
  crash_reporter::InitializeCrashpad(process_type_.empty(), process_type_);
}

// static
void CrashHandler::UploadDumps(const base::FilePath& crash_dump_path,
                               const base::FilePath& reports_path,
                               const std::string& uuid,
                               const std::string& application_feedback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> crash_dump_path_java =
      base::android::ConvertUTF8ToJavaString(env, crash_dump_path.value());
  base::android::ScopedJavaLocalRef<jstring> reports_path_java =
      base::android::ConvertUTF8ToJavaString(env, reports_path.value());
  base::android::ScopedJavaLocalRef<jstring> uuid_java =
      base::android::ConvertUTF8ToJavaString(env, uuid);
  base::android::ScopedJavaLocalRef<jstring> application_feedback_java =
      base::android::ConvertUTF8ToJavaString(env, application_feedback);
  // TODO(servolk): Remove the UploadToStaging param and clean up Java code, if
  // dev crash uploading to prod server works fine (b/113130776)
  bool can_send_usage_stats =
      android::ChromecastConfigAndroid::GetInstance()->CanSendUsageStats();

  if (can_send_usage_stats) {
    Java_CastCrashHandler_uploadOnce(env, crash_dump_path_java,
                                     reports_path_java, uuid_java,
                                     application_feedback_java,
                                     /* UploadToStaging = */ false);
  } else {
    Java_CastCrashHandler_removeCrashDumps(env, crash_dump_path_java,
                                           reports_path_java, uuid_java,
                                           application_feedback_java,
                                           /* UploadToStaging = */ false);
  }
}

}  // namespace chromecast
