// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_PENDING_LOGS_SERVICE_H_
#define CHROME_CHROME_CLEANER_LOGGING_PENDING_LOGS_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/threading/thread_checker.h"

namespace chrome_cleaner {

class ChromeCleanerReport;
class RegistryLogger;

// A combination of static helper functions as well as a living service to
// retry sending logs when others fail to do so successfully. All these methods
// must be called from the main UI message loop.
class PendingLogsService {
 public:
  // Returns the name of a task to run hourly, until logs upload for the
  // product named |product_shortname| succeeded.
  static std::wstring LogsUploadRetryTaskName(
      const std::wstring& product_shortname);

  // TODO(csharp): Many of these methods receive a RegistryLogger. Maybe we
  // should turn it into a singleton.

  // Persist |chrome_cleaner_report| and schedule a task to try uploading it
  // again. Return the path to the temporary file where the logs were saved in
  // |log_file|. Caller specifies |registry_logger| so it can be mocked.
  static void ScheduleLogsUploadTask(
      const std::wstring& product_shortname,
      const ChromeCleanerReport& chrome_cleaner_report,
      base::FilePath* log_file,
      RegistryLogger* registry_logger);

  // Clear the specified log file from pending logs upload, and also remove
  // the scheduled task if there are no pending logs files in there. Caller
  // specifies |registry_logger| so it can be mocked.
  static void ClearPendingLogFile(const std::wstring& product_shortname,
                                  const base::FilePath& log_file,
                                  RegistryLogger* registry_logger);

  PendingLogsService();

  PendingLogsService(const PendingLogsService&) = delete;
  PendingLogsService& operator=(const PendingLogsService&) = delete;

  ~PendingLogsService();

  // Retry to upload the next pending log if any. |done_callback| is called with
  // success/failure result of the logs upload, when it's done, which can be
  // synchronously on some failures, and asynchronously upon success.  Caller
  // specifies |registry_logger| so it can be mocked.
  void RetryNextPendingLogsUpload(const std::wstring& product_shortname,
                                  base::OnceCallback<void(bool)> done_callback,
                                  RegistryLogger* registry_logger);

 private:
  // Callback to be registered in the logging service.
  void UploadResultCallback(const std::wstring& product_shortname,
                            RegistryLogger* registry_logger,
                            bool success);

  // Thread checker, to make sure |done_callback_| gets run on the right thread.
  THREAD_CHECKER(thread_checker_);

  // Our caller's callback to Run when we're done.
  base::OnceCallback<void(bool)> done_callback_;

  // The file we are currently attempting to upload.
  base::FilePath log_file_;

  // Remember whether we are currently retrying to upload logs so that we don't
  // re-re-re-re-retry... ;-)
  static bool retrying_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_PENDING_LOGS_SERVICE_H_
