// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_APP_INSTALL_PROGRESS_H_
#define CHROME_UPDATER_APP_APP_INSTALL_PROGRESS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/notreached.h"
#include "url/gurl.h"

namespace base {
class Time;
class TimeDelta;
class Version;
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
  return completion_code != CompletionCodes::COMPLETION_CODE_ERROR;
}

struct AppCompletionInfo {
  std::string app_id;
  std::u16string display_name;
  std::u16string completion_message;
  CompletionCodes completion_code = CompletionCodes::COMPLETION_CODE_SUCCESS;
  int error_code = 0;
  int extra_code1 = 0;
  uint32_t installer_result_code = 0;
  bool is_canceled = false;
  bool is_noupdate = false;  // |noupdate| response from server.
  std::string post_install_launch_command_line;
  GURL post_install_url;

  AppCompletionInfo();
  AppCompletionInfo(const AppCompletionInfo&);
  AppCompletionInfo& operator=(const AppCompletionInfo&);
  ~AppCompletionInfo();
};

struct ObserverCompletionInfo {
  CompletionCodes completion_code = CompletionCodes::COMPLETION_CODE_SUCCESS;
  std::u16string completion_text;
  GURL help_url;
  std::vector<AppCompletionInfo> apps_info;

  ObserverCompletionInfo();
  ObserverCompletionInfo(const ObserverCompletionInfo&);
  ObserverCompletionInfo& operator=(const ObserverCompletionInfo&);
  ~ObserverCompletionInfo();
};

// Defines an interface for observing install progress. This interface is
// typically implemented by a progress window.
class AppInstallProgress {
 public:
  virtual ~AppInstallProgress() = default;
  virtual void OnCheckingForUpdate() = 0;
  virtual void OnUpdateAvailable(const std::string& app_id,
                                 const std::u16string& app_name,
                                 const base::Version& version) = 0;
  virtual void OnWaitingToDownload(const std::string& app_id,
                                   const std::u16string& app_name) = 0;
  virtual void OnDownloading(
      const std::string& app_id,
      const std::u16string& app_name,
      const std::optional<base::TimeDelta> time_remaining,
      int pos) = 0;
  virtual void OnWaitingRetryDownload(const std::string& app_id,
                                      const std::u16string& app_name,
                                      const base::Time& next_retry_time) = 0;
  virtual void OnWaitingToInstall(const std::string& app_id,
                                  const std::u16string& app_name) = 0;
  virtual void OnInstalling(const std::string& app_id,
                            const std::u16string& app_name,
                            const std::optional<base::TimeDelta> time_remaining,
                            int pos) = 0;
  virtual void OnPause() = 0;
  virtual void OnComplete(const ObserverCompletionInfo& observer_info) = 0;
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_APP_INSTALL_PROGRESS_H_
