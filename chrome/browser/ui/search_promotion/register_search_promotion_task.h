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

// LINT.IfChange(SearchPromotionExitCode)
enum class SearchPromotionExitCode {
  // Pre-execution registration errors:
  kInvalidExtensionId = 100,
  kInvalidPostInstallUrl = 101,
  kRegistryWriteFailed = 102,

  // Open action:
  kUrlLaunchFailed = 103,
  kUrlLaunchSuccess = 104,

  // Install action:
  kTimeout = 105,
  kSuccessBackground = 106,
  kSuccessWithForegroundFallback = 107,
  kForegroundFallbackLaunchFailed = 108,
  kEscapeHatchSuccess = 109,
  kEscapeHatchLaunchFailed = 110,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/search/enums.xml:SearchPromotionExitCode)

// Returns the string/variant name for the given exit code.
std::string_view SearchPromotionExitCodeToString(SearchPromotionExitCode code);

namespace base {
class CommandLine;
}

class RegisterSearchPromotionTask : public platform_experience::DelegatedTask {
 public:
  // `escape_hatch_url` and `escape_hatch_time` are either both set or both
  // empty.
  RegisterSearchPromotionTask(
      const GURL& post_install_url,
      std::string_view extension_id,
      const GURL& escape_hatch_url = GURL(),
      std::optional<base::TimeDelta> escape_hatch_time = std::nullopt);
  ~RegisterSearchPromotionTask() override;

  RegisterSearchPromotionTask(const RegisterSearchPromotionTask&) = delete;
  RegisterSearchPromotionTask& operator=(const RegisterSearchPromotionTask&) =
      delete;

  // platform_experience::DelegatedTask
  platform_experience::DelegatedTaskType GetTaskType() const override;
  std::string_view GetTaskName() const override;
  base::TimeDelta GetTimeout() const override;
  void AppendCommandLineSwitches(base::CommandLine& cmd_line) const override;

  // Deserializes task switches passed from Chrome to the out-of-process
  // platform_experience_helper.exe binary (which does not have Chrome's
  // FeatureList).
  static std::unique_ptr<RegisterSearchPromotionTask> FromCommandLine(
      const base::CommandLine& command_line);

  const GURL& post_install_url() const { return post_install_url_; }
  std::string_view extension_id() const { return extension_id_; }
  const GURL& escape_hatch_url() const { return escape_hatch_url_; }
  std::optional<base::TimeDelta> escape_hatch_time() const {
    return escape_hatch_time_;
  }

 private:
  const GURL post_install_url_;
  const std::string extension_id_;
  const GURL escape_hatch_url_;
  const std::optional<base::TimeDelta> escape_hatch_time_;
};

#endif  // CHROME_BROWSER_UI_SEARCH_PROMOTION_REGISTER_SEARCH_PROMOTION_TASK_H_
