// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"

namespace web_app {

url::Origin GetOrigin(const char* url) {
  GURL gurl = GURL(url);
  DCHECK(gurl.is_valid());

  url::Origin origin = url::Origin::Create(gurl);
  DCHECK(!origin.opaque());

  return origin;
}

SystemWebAppDelegate::SystemWebAppDelegate(
    const SystemAppType type,
    const std::string& internal_name,
    const GURL& install_url,
    Profile* profile,
    const OriginTrialsMap& origin_trials_map)
    : type_(type),
      internal_name_(internal_name),
      install_url_(install_url),
      profile_(profile),
      origin_trials_map_(origin_trials_map) {
  DCHECK(!(ShouldShowNewWindowMenuOption() && ShouldBeSingleWindow()))
      << "App can't show 'new window' menu option and be a single window at "
         "the same time.";
}

SystemWebAppDelegate::~SystemWebAppDelegate() = default;

std::vector<AppId> SystemWebAppDelegate::GetAppIdsToUninstallAndReplace()
    const {
  return {};
}

gfx::Size SystemWebAppDelegate::GetMinimumWindowSize() const {
  return gfx::Size();
}

bool SystemWebAppDelegate::ShouldBeSingleWindow() const {
  return true;
}

bool SystemWebAppDelegate::ShouldShowNewWindowMenuOption() const {
  return false;
}

bool SystemWebAppDelegate::ShouldIncludeLaunchDirectory() const {
  return false;
}

const OriginTrialsMap& SystemWebAppDelegate::GetEnabledOriginTrials() const {
  return origin_trials_map_;
}

std::vector<int> SystemWebAppDelegate::GetAdditionalSearchTerms() const {
  return {};
}

bool SystemWebAppDelegate::ShouldShowInLauncher() const {
  return true;
}

bool SystemWebAppDelegate::ShouldShowInSearch() const {
  return true;
}

bool SystemWebAppDelegate::ShouldCaptureNavigations() const {
  return false;
}

bool SystemWebAppDelegate::ShouldAllowResize() const {
  return true;
}

bool SystemWebAppDelegate::ShouldAllowMaximize() const {
  return true;
}

bool SystemWebAppDelegate::ShouldHaveTabStrip() const {
  return false;
}

bool SystemWebAppDelegate::ShouldHaveReloadButtonInMinimalUi() const {
  return true;
}

bool SystemWebAppDelegate::ShouldAllowScriptsToCloseWindows() const {
  return false;
}

absl::optional<SystemAppBackgroundTaskInfo> SystemWebAppDelegate::GetTimerInfo()
    const {
  return absl::nullopt;
}

bool SystemWebAppDelegate::IsAppEnabled() const {
  return true;
}

gfx::Rect SystemWebAppDelegate::GetDefaultBounds(Browser* browser) const {
  return {};
}

bool SystemWebAppDelegate::HasCustomTabMenuModel() const {
  return false;
}

std::unique_ptr<ui::SimpleMenuModel> SystemWebAppDelegate::GetTabMenuModel(
    ui::SimpleMenuModel::Delegate* delegate) const {
  return nullptr;
}

bool SystemWebAppDelegate::ShouldShowTabContextMenuShortcut(
    Profile* profile,
    int command_id) const {
  return true;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool SystemWebAppDelegate::HasTitlebarTerminalSelectNewTabButton() const {
  return false;
}
#endif

}  // namespace web_app
