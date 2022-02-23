// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_APP_ANDROID_CRASH_HANDLER_H_
#define CHROMECAST_APP_ANDROID_CRASH_HANDLER_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"

namespace chromecast {
class CastCrashReporterClientAndroid;

class CrashHandler {
 public:
  CrashHandler(const CrashHandler&) = delete;
  CrashHandler& operator=(const CrashHandler&) = delete;

  // Initializes the crash handler for attempting to upload crash dumps with
  // the current process's log file.
  // Must not be called more than once.
  static void Initialize(const std::string& process_type,
                         const base::FilePath& log_file_path);

  // Returns the directory location to use for a crashpad::CrashReportDatabase
  // of raw minidump files. Prior to upload, these files are re-written as MIME
  // multipart messages allowing upload metadata and attachments to be included
  // as parts of the message.
  static bool GetCrashDumpLocation(base::FilePath* crash_dir);

  // Returns the directory of crash reports re-written as MIME messages.
  static bool GetCrashReportsLocation(base::FilePath* reports_dir);

  static void UploadDumps(const base::FilePath& crash_dump_path,
                          const base::FilePath& reports_path,
                          const std::string& uuid,
                          const std::string& application_feedback);

 private:
  CrashHandler(const std::string& process_type,
               const base::FilePath& log_file_path);
  ~CrashHandler();

  void Initialize();

  // Path to the current process's log file.
  base::FilePath log_file_path_;

  // Location to which crash dumps should be written.
  base::FilePath crash_dump_path_;

  std::string process_type_;

  std::unique_ptr<CastCrashReporterClientAndroid> crash_reporter_client_;
};

}  // namespace chromecast

#endif  // CHROMECAST_APP_ANDROID_CRASH_HANDLER_H_
