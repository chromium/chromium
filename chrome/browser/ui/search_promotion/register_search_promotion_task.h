// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_PROMOTION_REGISTER_SEARCH_PROMOTION_TASK_H_
#define CHROME_BROWSER_UI_SEARCH_PROMOTION_REGISTER_SEARCH_PROMOTION_TASK_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "chrome/browser/platform_experience/delegated_tasks/delegated_task.h"
#include "url/gurl.h"

enum class SearchPromotionExitCode {
  // Pre-execution registration errors:
  kInvalidExtensionId = 100,
  kInvalidPostInstallUrl = 101,
  kRegistryWriteFailed = 102,

  // Arm A:
  kUrlLaunchFailed = 103,
  kUrlLaunchSuccess = 104,

  // Arm B:
  kTimeout = 105,
  kSuccessBackground = 106,
  kSuccessWithForegroundFallback = 107,
  kForegroundFallbackLaunchFailed = 108,

  kMaxValue = kForegroundFallbackLaunchFailed,
};

namespace base {
class CommandLine;
}

class RegisterSearchPromotionTask : public platform_experience::DelegatedTask {
 public:
  RegisterSearchPromotionTask(const GURL& post_install_url,
                              std::string_view extension_id);
  ~RegisterSearchPromotionTask() override;

  RegisterSearchPromotionTask(const RegisterSearchPromotionTask&) = delete;
  RegisterSearchPromotionTask& operator=(const RegisterSearchPromotionTask&) =
      delete;

  // platform_experience::DelegatedTask
  platform_experience::DelegatedTaskType GetTaskType() const override;
  std::string_view GetTaskName() const override;
  base::TimeDelta GetTimeout() const override;
  void AppendCommandLineSwitches(base::CommandLine& cmd_line) const override;

  static std::unique_ptr<RegisterSearchPromotionTask> FromCommandLine(
      const base::CommandLine& command_line);

  const GURL& post_install_url() const { return post_install_url_; }
  std::string_view extension_id() const { return extension_id_; }

 private:
  const GURL post_install_url_;
  const std::string extension_id_;
};

#endif  // CHROME_BROWSER_UI_SEARCH_PROMOTION_REGISTER_SEARCH_PROMOTION_TASK_H_
