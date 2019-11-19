// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALL_PROGRESS_OBSERVER_H_
#define CHROME_UPDATER_WIN_INSTALL_PROGRESS_OBSERVER_H_

#include <vector>

#include "base/logging.h"
#include "base/strings/string16.h"

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
  base::string16 display_name;
  base::string16 app_id;
  base::string16 completion_message;
  CompletionCodes completion_code;
  int error_code = 0;
  int extra_code1 = 0;
  uint32_t installer_result_code = 0;
  bool is_canceled = false;
  bool is_noupdate = false;  // |noupdate| response from server.
  base::string16 post_install_launch_command_line;
  base::string16 post_install_url;

  AppCompletionInfo();
  AppCompletionInfo(const AppCompletionInfo&);
  ~AppCompletionInfo();
};

struct ObserverCompletionInfo {
  CompletionCodes completion_code = CompletionCodes::COMPLETION_CODE_SUCCESS;
  base::string16 completion_text;
  base::string16 help_url;
  std::vector<AppCompletionInfo> apps_info;

  ObserverCompletionInfo();
  ~ObserverCompletionInfo();
};

// Defines an interface for observing install progress. This interface is
// typically implemented by a progress window.
class InstallProgressObserver {
 public:
  virtual ~InstallProgressObserver() {}
  virtual void OnCheckingForUpdate() = 0;
  virtual void OnUpdateAvailable(const base::string16& app_id,
                                 const base::string16& app_name,
                                 const base::string16& version_string) = 0;
  virtual void OnWaitingToDownload(const base::string16& app_id,
                                   const base::string16& app_name) = 0;
  virtual void OnDownloading(const base::string16& app_id,
                             const base::string16& app_name,
                             int time_remaining_ms,
                             int pos) = 0;
  virtual void OnWaitingRetryDownload(const base::string16& app_id,
                                      const base::string16& app_name,
                                      const base::Time& next_retry_time) = 0;
  virtual void OnWaitingToInstall(const base::string16& app_id,
                                  const base::string16& app_name,
                                  bool* can_start_install) = 0;
  virtual void OnInstalling(const base::string16& app_id,
                            const base::string16& app_name,
                            int time_remaining_ms,
                            int pos) = 0;
  virtual void OnPause() = 0;
  virtual void OnComplete(const ObserverCompletionInfo& observer_info) = 0;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_INSTALL_PROGRESS_OBSERVER_H_
