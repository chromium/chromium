// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALL_PROGRESS_OBSERVER_H_
#define CHROME_UPDATER_WIN_INSTALL_PROGRESS_OBSERVER_H_

#include <string>
#include <vector>

#include "base/notreached.h"

namespace base {
class Time;
}  // namespace base

// The data structures defined in this file are similar to those defined
// defined in Omaha. Keep them as they are for now for documentation purposes
// until https://crbug.com/1014630 has been fixed.
namespace updater {

enum class CompletionCodes {
  COMPLETION_CODE_SUCCESS = 1,
  COMPLETION_CODE_EXIT_SILENTLY,
  COMPLETION_CODE_ERROR = COMPLETION_CODE_SUCCESS + 2,
  COMPLETION_CODE_RESTART_ALL_BROWSERS,
  COMPLETION_CODE_REBOOT,
  COMPLETION_CODE_RESTART_BROWSER,
  COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY,
  COMPLETION_CODE_REBOOT_NOTICE_ONLY,
  COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY,
  COMPLETION_CODE_LAUNCH_COMMAND,
  COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND,
  COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL,
};

inline bool IsCompletionCodeSuccess(CompletionCodes completion_code) {
  switch (completion_code) {
    case CompletionCodes::COMPLETION_CODE_SUCCESS:
    case CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY:
    case CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS:
    case CompletionCodes::COMPLETION_CODE_REBOOT:
    case CompletionCodes::COMPLETION_CODE_RESTART_BROWSER:
    case CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY:
    case CompletionCodes::COMPLETION_CODE_REBOOT_NOTICE_ONLY:
    case CompletionCodes::COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY:
    case CompletionCodes::COMPLETION_CODE_LAUNCH_COMMAND:
    case CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND:
    case CompletionCodes::COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL:
      return true;

    case CompletionCodes::COMPLETION_CODE_ERROR:
      return false;

    default:
      NOTREACHED();
      return false;
  }
}

struct AppCompletionInfo {
  std::u16string display_name;
  std::u16string app_id;
  std::u16string completion_message;
  CompletionCodes completion_code;
  int error_code = 0;
  int extra_code1 = 0;
  uint32_t installer_result_code = 0;
  bool is_canceled = false;
  bool is_noupdate = false;  // |noupdate| response from server.
  std::wstring post_install_launch_command_line;
  std::u16string post_install_url;

  AppCompletionInfo();
  AppCompletionInfo(const AppCompletionInfo&);
  AppCompletionInfo& operator=(const AppCompletionInfo&);
  ~AppCompletionInfo();
};

struct ObserverCompletionInfo {
  CompletionCodes completion_code = CompletionCodes::COMPLETION_CODE_SUCCESS;
  std::wstring completion_text;
  std::string help_url;
  std::vector<AppCompletionInfo> apps_info;

  ObserverCompletionInfo();
  ObserverCompletionInfo(const ObserverCompletionInfo&);
  ObserverCompletionInfo& operator=(const ObserverCompletionInfo&);
  ~ObserverCompletionInfo();
};

// Defines an interface for observing install progress. This interface is
// typically implemented by a progress window.
class InstallProgressObserver {
 public:
  virtual ~InstallProgressObserver() = default;
  virtual void OnCheckingForUpdate() = 0;
  virtual void OnUpdateAvailable(const std::u16string& app_id,
                                 const std::u16string& app_name,
                                 const std::u16string& version_string) = 0;
  virtual void OnWaitingToDownload(const std::u16string& app_id,
                                   const std::u16string& app_name) = 0;
  virtual void OnDownloading(const std::u16string& app_id,
                             const std::u16string& app_name,
                             int time_remaining_ms,
                             int pos) = 0;
  virtual void OnWaitingRetryDownload(const std::u16string& app_id,
                                      const std::u16string& app_name,
                                      const base::Time& next_retry_time) = 0;
  virtual void OnWaitingToInstall(const std::u16string& app_id,
                                  const std::u16string& app_name,
                                  bool* can_start_install) = 0;
  virtual void OnInstalling(const std::u16string& app_id,
                            const std::u16string& app_name,
                            int time_remaining_ms,
                            int pos) = 0;
  virtual void OnPause() = 0;
  virtual void OnComplete(const ObserverCompletionInfo& observer_info) = 0;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_INSTALL_PROGRESS_OBSERVER_H_
