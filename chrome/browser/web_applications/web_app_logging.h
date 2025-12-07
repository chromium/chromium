// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LOGGING_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LOGGING_H_

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/containers/circular_deque.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace base {
class Clock;
class ListValue;
class Value;
class DictValue;
class DelayTimer;
}  // namespace base

namespace webapps {
enum class WebAppUrlLoaderResult;
}

namespace web_app {
class FileUtilsWrapper;

enum class PersistableLogMode {
  // Logs are stored in memory only. Any logs on disk will be deleted.
  kInMemory,
  // Logs are periodically persisted to disk with file rotation. The latest log
  // on disk is loaded into memory on startup.
  kPersistToDisk,
};

// This class is used to hold log entries and optionally save logs to disk, with
// file rotation to prevent files that are too large.
//
// The log is used to display debug information on the
// chrome://web-app-internals page.
class PersistableLog {
 public:
  static std::unique_ptr<PersistableLog> Create(
      const base::FilePath& log_file,
      PersistableLogMode mode,
      int max_log_entries_in_memory,
      scoped_refptr<FileUtilsWrapper> file_utils);

  // `clock` must be non-null and outlive the log.
  static std::unique_ptr<PersistableLog> CreateForTesting(
      const base::FilePath& log_file,
      PersistableLogMode mode,
      int max_log_entries_in_memory,
      scoped_refptr<FileUtilsWrapper> file_utils,
      scoped_refptr<base::SequencedTaskRunner> log_writing_task_runner,
      scoped_refptr<base::SequencedTaskRunner> log_deletion_task_runner,
      const base::Clock* clock);

  static base::FilePath GetLogPath(Profile* profile,
                                   std::string_view log_filename);
  static int GetMaxInMemoryLogEntries();
  static PersistableLogMode GetMode();

  static base::AutoReset<int> SetMaxLogFileSizeBytesForTesting(int size);

  ~PersistableLog();

  // Appends a value to the log, always populating a timestamp under the key
  // "timestamp_ms" if it is not already present.
  void Append(base::DictValue object);
  // Shortcut method for calling Append with "value" set to `value`, and
  // "timestamp_ms" set to the current time.
  void AppendValue(base::Value value);

  // Returns the log entries in descending order of time (newest entry is at the
  // front).
  const base::circular_deque<base::DictValue>& GetEntries() const;

  // Helper method to convert the log entries to a ListValue.
  base::ListValue CloneToList() const;

 private:
  PersistableLog(
      const base::FilePath& log_file,
      PersistableLogMode mode,
      int max_log_entries_in_memory,
      scoped_refptr<FileUtilsWrapper> file_utils,
      scoped_refptr<base::SequencedTaskRunner> log_writing_task_runner,
      scoped_refptr<base::SequencedTaskRunner> log_deletion_task_runner,
      const base::Clock* clock);

  void OnLatestLogLoaded(std::optional<base::ListValue> loaded_log);
  void MaybeWriteCurrentLog();

  const base::FilePath log_file_;
  const PersistableLogMode mode_;
  const scoped_refptr<FileUtilsWrapper> file_utils_;
  const int max_log_entries_in_memory_;

  // Task runners for file operations. Injected for testing.
  const scoped_refptr<base::SequencedTaskRunner> log_writing_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> log_deletion_task_runner_;

  // Clock for timestamping. Injected for testing.
  const raw_ref<const base::Clock> clock_;

  base::OneShotEvent on_load_complete_;
  base::DelayTimer log_write_timer_;

  // This is the log that is currently in memory, and is in descending order of
  // time (newest entry is at the front).
  base::circular_deque<base::DictValue> log_;

  // This is the log that is currently being written to disk. The log is in
  // descending order of time (newest entry is at the end).
  base::ListValue current_log_for_disk_;
  int current_disk_log_entry_count_ = 0;

  base::WeakPtrFactory<PersistableLog> weak_ptr_factory_{this};
};

// Returns a dictionary value containing errors from the downloaded icons. If
// there are no errors, returns a an empty dict.
base::DictValue LogDownloadedIconsErrors(
    IconsDownloadedResult icons_downloaded_result,
    const IconsMap& icons_map,
    const DownloadedIconsHttpResults& icons_http_results);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LOGGING_H_
