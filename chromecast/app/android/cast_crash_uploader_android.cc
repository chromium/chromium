// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/app/android/cast_crash_uploader_android.h"

#include <jni.h>
#include <stdlib.h>
#include <memory>
#include <string>

#include "base/android/java_exception_reporter.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/path_service.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromecast/app/android/cast_crash_reporter_client_android.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/base/pref_names.h"
#include "components/crash/core/app/crash_reporter_client.h"
#include "components/crash/core/app/crashpad.h"
#include "components/crash/core/common/crash_key.h"
#include "content/public/common/content_switches.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chromecast/browser/android/crash_handler_jni_headers/CastCrashHandler_jni.h"

namespace chromecast {
// static
std::unique_ptr<CastCrashUploader> CastCrashUploader::Create(
    PrefService* pref_service) {
  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  base::FilePath log_file;
  base::PathService::Get(FILE_CAST_ANDROID_LOG, &log_file);

  return std::make_unique<CastCrashUploaderAndroid>(
      std::move(process_type), std::move(log_file), pref_service);
}

CastCrashUploaderAndroid::CastCrashUploaderAndroid(
    const std::string process_type,
    const base::FilePath log_file_path,
    PrefService* pref_service)
    : log_file_path_(log_file_path),
      pref_service_(pref_service),
      process_type_(process_type),
      crash_reporter_client_(new CastCrashReporterClientAndroid(process_type)),
      weak_factory_(this) {
  crash_reporter::SetCrashReporterClient(crash_reporter_client_.get());
  crash_reporter::InitializeCrashpad(process_type_.empty(), process_type_);
  crash_reporter::InitializeCrashKeys();
  base::android::InitJavaExceptionReporter();
  crash_reporter_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  StartPeriodicCrashReportUpload();
}

CastCrashUploaderAndroid::~CastCrashUploaderAndroid() = default;

void CastCrashUploaderAndroid::UploadDumps(
    const std::string& uuid,
    const std::string& application_feedback,
    const bool can_send_usage_stats) {
  base::FilePath crash_dump_path;
  if (!crash_reporter_client_->GetCrashDumpLocation(&crash_dump_path)) {
    LOG(ERROR) << "Could not get crash dump location.";
    return;
  }
  base::FilePath crash_reports_path;
  if (!crash_reporter_client_->GetCrashReportsLocation(process_type_,
                                                       &crash_reports_path)) {
    LOG(ERROR) << "Could not get crash report location.";
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> crash_dump_path_java =
      base::android::ConvertUTF8ToJavaString(env, crash_dump_path.value());
  base::android::ScopedJavaLocalRef<jstring> reports_path_java =
      base::android::ConvertUTF8ToJavaString(env, crash_reports_path.value());
  base::android::ScopedJavaLocalRef<jstring> uuid_java =
      base::android::ConvertUTF8ToJavaString(env, uuid);
  base::android::ScopedJavaLocalRef<jstring> application_feedback_java =
      base::android::ConvertUTF8ToJavaString(env, application_feedback);
  // TODO(servolk): Remove the UploadToStaging param and clean up Java code, if
  // dev crash uploading to prod server works fine (b/113130776)

  if (can_send_usage_stats) {
    Java_CastCrashHandler_uploadOnce(env, crash_dump_path_java,
                                     reports_path_java, uuid_java,
                                     application_feedback_java,
                                     /* uploadCrashToStaging = */ false);
  } else {
    Java_CastCrashHandler_removeCrashDumps(env, crash_dump_path_java,
                                           reports_path_java, uuid_java,
                                           application_feedback_java,
                                           /* uploadCrashToStaging = */ false);
  }
}

void CastCrashUploaderAndroid::StartPeriodicCrashReportUpload() {
  OnStartPeriodicCrashReportUpload();
  crash_reporter_timer_ = std::make_unique<base::RepeatingTimer>();
  crash_reporter_timer_->Start(
      FROM_HERE, base::Minutes(20), this,
      &CastCrashUploaderAndroid::OnStartPeriodicCrashReportUpload);
}

void CastCrashUploaderAndroid::OnStartPeriodicCrashReportUpload() {
  crash_reporter_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CastCrashUploaderAndroid::UploadCrashReport,
                                weak_factory_.GetWeakPtr(),
                                pref_service_->GetBoolean(prefs::kOptInStats)));
}

void CastCrashUploaderAndroid::UploadCrashReport(bool opt_in_stats) {
  DCHECK(crash_reporter_runner_->RunsTasksInCurrentSequence());
  CastCrashUploaderAndroid::UploadDumps("", "", opt_in_stats);
}
}  // namespace chromecast
