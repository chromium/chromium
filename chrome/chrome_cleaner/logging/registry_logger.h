// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_REGISTRY_LOGGER_H_
#define CHROME_CHROME_CLEANER_LOGGING_REGISTRY_LOGGER_H_

#include <string>
#include <vector>

#include "base/threading/thread_checker.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/logging/safe_browsing_reporter.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"

namespace base {
class FilePath;
}

namespace chrome_cleaner {

enum class RegistryError : char;

// This class contains all logic related to logging data to the registry. It
// is largely a collection of convenience methods to mutate specific registry
// state. It also contains the serialization logic for the more complicated
// reporting.
class RegistryLogger {
 public:
  enum class Mode {
    REMOVER,   // Writes to the registry location for the chrome cleanup tool.
    REPORTER,  // Writes to the appropriate location for the reporter tool.
    NOOP_FOR_TESTING,  // Drops all writes, used for testing.
  };

  // Construct a RegistryLogger that writes to the location specified by |mode|.
  explicit RegistryLogger(Mode mode);
  RegistryLogger(Mode mode, const std::string& suffix);

  RegistryLogger(const RegistryLogger&) = delete;
  RegistryLogger& operator=(const RegistryLogger&) = delete;

  ~RegistryLogger();

  // Write the currently running version of the tool to the key specified by
  // |mode| at construction.
  void WriteVersion();

  // Persist |exit_code| to the key specified by |mode| at construction.
  void WriteExitCode(int exit_code);

  // Clear the persisted exit code from the key specified by |mode|.
  void ClearExitCode();

  // Persist the current time as the start time to the key specified by |mode|.
  void WriteStartTime();

  // Persist the current time as the end time to the key specified by |mode|.
  void WriteEndTime();

  // Clear the currently persisted end time from the key specified by |mode|.
  void ClearEndTime();

  // Clear the previously written scan times from the key specified by
  // |mode|. Must be called on the same thread that created the registry logger
  // object.
  void ClearScanTimes();

  // Write |memory_usage_kb| to the key specified by |mode|. |memory_used_kb|
  // must be given in units of KBs.
  void WriteMemoryUsage(size_t memory_used_kb);

  // Append the last log upload result to a series of results stored in the
  // key specified by |mode|. This sequence will be bounded in length by
  // kMaxUploadResultLength and older entries will be truncated if this length
  // is exceeded.
  void AppendLogUploadResult(bool success);

  // Write the result of the attempt to write logs upload from the Software
  // Reporter.
  void WriteReporterLogsUploadResult(
      SafeBrowsingReporter::Result upload_result);

  // Append the given full path to a persisted log to the appropriate registry
  // value, or create the registry value if it doesn't exist yet. Return false
  // on failures.
  bool AppendLogFilePath(const base::FilePath& log_file);

  // Retrieve the head of the list of registered log files to upload. Return an
  // empty path when there are none.
  void GetNextLogFilePath(base::FilePath* log_file);

  // Remove |log_file| from the list of registered log files to upload. Return
  // false if there are no more registered log files after removing |log_file|
  // (whether there was a |log_file| to remove or not).
  bool RemoveLogFilePath(const base::FilePath& log_file);

  // Record the given list of found pup ids to the key specified by |mode|.
  bool RecordFoundPUPs(const std::vector<UwSId>& pups_to_store);

  // Writes the result of the experimental engine.
  void WriteExperimentalEngineResultCode(int exit_code);

  // Writes that a cleanup has successfully completed.
  void RecordCompletedCleanup();

  // Erases information written by previous cleaner runs that a cleanup has
  // completed.
  void ResetCompletedCleanup();

 protected:
  // Return the full path of the key where the cleaner / reporter info is
  // stored. Exposed for tests.
  std::wstring GetLoggingKeyPath(Mode mode) const;

  // Return the full path of the key where the PUP scan times are
  // stored. Exposed for tests.
  std::wstring GetScanTimesKeyPath(Mode mode) const;

  // Return the registry key suffix. Exposed for tests.
  std::wstring GetKeySuffix() const;

  // Read values from the given registry key. Exposed for tests.
  static bool ReadValues(const base::win::RegKey& logging_key,
                         const wchar_t* name,
                         std::vector<std::wstring>* values,
                         RegistryError* registry_error);

  // Exposed for testing.
  static const wchar_t kPendingLogFilesValue[];
  static const size_t kMaxUploadResultLength;

 private:
  bool ReadPendingLogFiles(std::vector<std::wstring>* log_files,
                           RegistryError* registry_error);

  THREAD_CHECKER(thread_checker_);
  base::win::RegKey logging_key_;
  base::win::RegKey scan_times_key_;
  Mode mode_;
  std::wstring suffix_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_REGISTRY_LOGGER_H_
