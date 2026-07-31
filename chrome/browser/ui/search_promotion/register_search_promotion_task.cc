// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_promotion/register_search_promotion_task.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/command_line.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace {

constexpr char kExtensionId[] = "extension-id";
constexpr char kPostInstallUrlSwitch[] = "post-install-url";

}  // namespace

RegisterSearchPromotionTask::RegisterSearchPromotionTask(
    const GURL& post_install_url,
    std::string_view extension_id)
    : post_install_url_(post_install_url), extension_id_(extension_id) {
  CHECK(post_install_url_.is_valid());
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

  std::string extension_id = command_line.GetSwitchValueASCII(kExtensionId);

  return std::make_unique<RegisterSearchPromotionTask>(post_install_url,
                                                       extension_id);
}
