// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_promotion/register_search_promotion_task.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace {

constexpr char kExtensionId[] = "extension-id";
constexpr char kPostInstallUrlSwitch[] = "post-install-url";
constexpr char kEscapeHatchUrlSwitch[] = "escape-hatch-url";
constexpr char kEscapeHatchTimeMsSwitch[] = "escape-hatch-time-ms";

}  // namespace

std::string_view SearchPromotionExitCodeToString(SearchPromotionExitCode code) {
  switch (code) {
    case SearchPromotionExitCode::kInvalidExtensionId:
      return "InvalidExtensionId";
    case SearchPromotionExitCode::kInvalidPostInstallUrl:
      return "InvalidPostInstallUrl";
    case SearchPromotionExitCode::kRegistryWriteFailed:
      return "RegistryWriteFailed";
    case SearchPromotionExitCode::kUrlLaunchFailed:
      return "UrlLaunchFailed";
    case SearchPromotionExitCode::kUrlLaunchSuccess:
      return "UrlLaunchSuccess";
    case SearchPromotionExitCode::kTimeout:
      return "Timeout";
    case SearchPromotionExitCode::kSuccessBackground:
      return "SuccessBackground";
    case SearchPromotionExitCode::kSuccessWithForegroundFallback:
      return "SuccessWithForegroundFallback";
    case SearchPromotionExitCode::kForegroundFallbackLaunchFailed:
      return "ForegroundFallbackLaunchFailed";
    case SearchPromotionExitCode::kEscapeHatchSuccess:
      return "EscapeHatchSuccess";
    case SearchPromotionExitCode::kEscapeHatchLaunchFailed:
      return "EscapeHatchLaunchFailed";
  }
  return std::string_view();
}

RegisterSearchPromotionTask::RegisterSearchPromotionTask(
    const GURL& post_install_url,
    std::string_view extension_id,
    const GURL& escape_hatch_url,
    std::optional<base::TimeDelta> escape_hatch_time)
    : post_install_url_(post_install_url),
      extension_id_(extension_id),
      escape_hatch_url_(escape_hatch_url),
      escape_hatch_time_(escape_hatch_time) {
  CHECK(post_install_url_.is_valid());
  CHECK(escape_hatch_url_.is_empty()
            ? !escape_hatch_time_.has_value()
            : escape_hatch_url_.is_valid() && escape_hatch_time_.has_value() &&
                  escape_hatch_time_->is_positive());
}

RegisterSearchPromotionTask::~RegisterSearchPromotionTask() = default;

platform_experience::DelegatedTaskType
RegisterSearchPromotionTask::GetTaskType() const {
  return platform_experience::DelegatedTaskType::kRegisterSearchPromotion;
}

base::TimeDelta RegisterSearchPromotionTask::GetTimeout() const {
  return base::Seconds(15);
}

void RegisterSearchPromotionTask::AppendCommandLineSwitches(
    base::CommandLine& command_line) const {
  command_line.AppendSwitchASCII(kPostInstallUrlSwitch,
                                 post_install_url_.spec());
  if (!extension_id_.empty()) {
    command_line.AppendSwitchASCII(kExtensionId, extension_id_);
  }
  if (escape_hatch_url_.is_valid() && escape_hatch_time_.has_value()) {
    command_line.AppendSwitchASCII(kEscapeHatchUrlSwitch,
                                   escape_hatch_url_.spec());
    // Round up so sub-millisecond values stay positive.
    command_line.AppendSwitchASCII(
        kEscapeHatchTimeMsSwitch,
        base::NumberToString(escape_hatch_time_->InMillisecondsRoundedUp()));
  }
}

std::string_view RegisterSearchPromotionTask::GetTaskName() const {
  return "RegisterSearchPromotion";
}

// static.
std::unique_ptr<RegisterSearchPromotionTask>
RegisterSearchPromotionTask::FromCommandLine(
    const base::CommandLine& command_line) {
  std::string post_install_url_str =
      command_line.GetSwitchValueASCII(kPostInstallUrlSwitch);

  GURL post_install_url = GURL(post_install_url_str);
  if (!post_install_url.is_valid()) {
    return nullptr;
  }

  GURL escape_hatch_url;
  std::optional<base::TimeDelta> escape_hatch_time;
  if (command_line.HasSwitch(kEscapeHatchUrlSwitch) ||
      command_line.HasSwitch(kEscapeHatchTimeMsSwitch)) {
    escape_hatch_url =
        GURL(command_line.GetSwitchValueASCII(kEscapeHatchUrlSwitch));
    int64_t escape_hatch_time_ms = 0;
    if (!escape_hatch_url.is_valid() ||
        !base::StringToInt64(
            command_line.GetSwitchValueASCII(kEscapeHatchTimeMsSwitch),
            &escape_hatch_time_ms) ||
        escape_hatch_time_ms <= 0) {
      return nullptr;
    }
    escape_hatch_time = base::Milliseconds(escape_hatch_time_ms);
  }

  std::string extension_id = command_line.GetSwitchValueASCII(kExtensionId);

  return std::make_unique<RegisterSearchPromotionTask>(
      post_install_url, extension_id, escape_hatch_url, escape_hatch_time);
}
