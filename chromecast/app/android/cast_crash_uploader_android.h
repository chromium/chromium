// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_APP_ANDROID_CAST_CRASH_UPLOADER_ANDROID_H_
#define CHROMECAST_APP_ANDROID_CAST_CRASH_UPLOADER_ANDROID_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chromecast/app/cast_crash_uploader.h"
#include "components/prefs/pref_service.h"

namespace chromecast {
class CastCrashReporterClientAndroid;

class CastCrashUploaderAndroid : public CastCrashUploader {
 public:
  void UploadDumps(const std::string& uuid,
                   const std::string& application_feedback,
                   const bool can_send_usage_stats) override;

  CastCrashUploaderAndroid(const std::string process_type,
                           const base::FilePath log_file_path,
                           PrefService* pref_service);
  ~CastCrashUploaderAndroid() override;

 private:
  void StartPeriodicCrashReportUpload();
  void OnStartPeriodicCrashReportUpload();
  void UploadCrashReport(bool opt_in_stats);

  // Path to the current process's log file.
  base::FilePath log_file_path_;

  PrefService* pref_service_;

  std::string process_type_;
  scoped_refptr<base::SequencedTaskRunner> crash_reporter_runner_;
  std::unique_ptr<base::RepeatingTimer> crash_reporter_timer_;

  std::unique_ptr<CastCrashReporterClientAndroid> crash_reporter_client_;
  base::WeakPtrFactory<CastCrashUploaderAndroid> weak_factory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_APP_ANDROID_CAST_CRASH_UPLOADER_ANDROID_H_
