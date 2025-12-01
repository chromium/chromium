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
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
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
class ListValue;
class Value;
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
  static base::FilePath GetLogPath(Profile* profile,
                                   std::string_view log_filename);
  static int GetMaxInMemoryLogEntries();
  static PersistableLogMode GetMode();

  static base::AutoReset<int> SetMaxLogFileSizeBytesForTesting(int size);

  PersistableLog(const base::FilePath& log_file,
                 PersistableLogMode mode,
                 int max_log_entries_in_memory,
                 scoped_refptr<FileUtilsWrapper> file_utils);
  ~PersistableLog();

  // Appends a value to the log.
  void Append(base::Value object);

  // Returns the log entries in descending order of time (newest entry is at the
  // front).
  const base::circular_deque<base::Value>& GetEntries() const;

  // Runs the callback when the logs are finished loading, deleting, or writing
  // to disk.
  void WaitForLoadAndFlushForTesting(base::OnceClosure done) const;

 private:
  void OnLatestLogLoaded(std::optional<base::ListValue> loaded_log);
  void MaybeWriteCurrentLog();

  const base::FilePath log_file_;
  const PersistableLogMode mode_;
  const scoped_refptr<FileUtilsWrapper> file_utils_;
  const int max_log_entries_in_memory_;

  base::OneShotEvent on_load_complete_;
  base::DelayTimer log_write_timer_;

  // This is the log that is currently in memory, and is in descending order of
  // time (newest entry is at the front).
  base::circular_deque<base::Value> log_;

  // This is the log that is currently being written to disk. The log is in
  // descending order of time (newest entry is at the end).
  base::ListValue current_log_for_disk_;
  int current_disk_log_entry_count_ = 0;

  base::WeakPtrFactory<PersistableLog> weak_ptr_factory_{this};
};

// Helper class to accumulate structured error information during a web app
// installation process. This is only enabled if the
// |kRecordWebAppDebugInfo| feature flag is enabled. The logs are sent to the
// WebAppInstallManager and are exposed in chrome://web-app-internals.
//
// An InstallErrorLogEntry is typically created at the beginning of an install
// command and is passed around. Various stages of the installation process can
// log errors to it. At the end of the command, if any errors were logged, the
// entire log entry is sent to the install manager. This is done by the command
// calling `WebAppCommandManager::LogToInstallManager`, which then calls
// `WebAppInstallManager::TakeCommandErrorLog`.
class InstallErrorLogEntry {
 public:
  explicit InstallErrorLogEntry(bool background_installation,
                                webapps::WebappInstallSource install_surface);
  ~InstallErrorLogEntry();

  // The InstallWebAppTask determines this after construction, so a setter is
  // required.
  void set_background_installation(bool background_installation) {
    background_installation_ = background_installation;
  }

  bool HasErrorDict() const;

  // Collects install errors (unbounded) if the |kRecordWebAppDebugInfo|
  // flag is enabled to be used by: chrome://web-app-internals
  base::Value TakeErrorDict();

  void LogUrlLoaderError(const char* stage,
                         const std::string& url,
                         webapps::WebAppUrlLoaderResult result);
  void LogExpectedAppIdError(const char* stage,
                             const std::string& url,
                             const webapps::AppId& app_id,
                             const webapps::AppId& expected_app_id);
  void LogDownloadedIconsErrors(
      const WebAppInstallInfo& web_app_info,
      IconsDownloadedResult icons_downloaded_result,
      const IconsMap& icons_map,
      const DownloadedIconsHttpResults& icons_http_results);

 private:
  void LogHeaderIfLogEmpty(const std::string& url);

  void LogErrorObject(const char* stage,
                      const std::string& url,
                      base::DictValue object);

  std::unique_ptr<base::DictValue> error_dict_;
  bool background_installation_;
  webapps::WebappInstallSource install_surface_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LOGGING_H_
