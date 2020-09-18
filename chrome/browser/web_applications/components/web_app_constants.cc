// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_constants.h"

#include "base/compiler_specific.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace web_app {

namespace {

// Note: This can never return kBrowser. This is because the user has
// specified that the web app should be displayed in a window, and thus
// the lowest fallback that we can go to is kMinimalUi.
DisplayMode ResolveAppDisplayModeForStandaloneLaunchContainer(
    DisplayMode app_display_mode) {
  switch (app_display_mode) {
    case DisplayMode::kBrowser:
    case DisplayMode::kMinimalUi:
      return DisplayMode::kMinimalUi;
    case DisplayMode::kUndefined:
      NOTREACHED();
      FALLTHROUGH;
    case DisplayMode::kStandalone:
    case DisplayMode::kFullscreen:
      return DisplayMode::kStandalone;
  }
}
}  // namespace

static_assert(Source::kMinValue == 0, "Source enum should be zero based");

static_assert(OsHookType::kShortcuts == 0,
              "OsHookType enum should be zero based");

bool IsSuccess(InstallResultCode code) {
  switch (code) {
    case InstallResultCode::kSuccessNewInstall:
    case InstallResultCode::kSuccessAlreadyInstalled:
    case InstallResultCode::kSuccessOfflineOnlyInstall:
    case InstallResultCode::kSuccessOfflineFallbackInstall:
      return true;
    default:
      return false;
  }
}

bool IsNewInstall(InstallResultCode code) {
  return IsSuccess(code) && code != InstallResultCode::kSuccessAlreadyInstalled;
}

DisplayMode ResolveEffectiveDisplayMode(
    DisplayMode app_display_mode,
    const std::vector<DisplayMode>& app_display_mode_overrides,
    DisplayMode user_display_mode) {
  switch (user_display_mode) {
    case DisplayMode::kBrowser:
      return user_display_mode;
    case DisplayMode::kUndefined:
    case DisplayMode::kMinimalUi:
    case DisplayMode::kFullscreen:
      NOTREACHED();
      FALLTHROUGH;
    case DisplayMode::kStandalone:
      break;
  }

  for (const DisplayMode& app_display_mode_override :
       app_display_mode_overrides) {
    DisplayMode resolved_display_mode =
        ResolveAppDisplayModeForStandaloneLaunchContainer(
            app_display_mode_override);
    if (resolved_display_mode == app_display_mode_override)
      return resolved_display_mode;
  }

  return ResolveAppDisplayModeForStandaloneLaunchContainer(app_display_mode);
}

apps::mojom::LaunchContainer ConvertDisplayModeToAppLaunchContainer(
    DisplayMode display_mode) {
  switch (display_mode) {
    case DisplayMode::kBrowser:
      return apps::mojom::LaunchContainer::kLaunchContainerTab;
    case DisplayMode::kMinimalUi:
      return apps::mojom::LaunchContainer::kLaunchContainerWindow;
    case DisplayMode::kStandalone:
      return apps::mojom::LaunchContainer::kLaunchContainerWindow;
    case DisplayMode::kFullscreen:
      return apps::mojom::LaunchContainer::kLaunchContainerWindow;
    case DisplayMode::kUndefined:
      return apps::mojom::LaunchContainer::kLaunchContainerNone;
  }
}

std::string RunOnOsLoginModeToString(RunOnOsLoginMode mode) {
  switch (mode) {
    case RunOnOsLoginMode::kWindowed:
      return "windowed";
    case RunOnOsLoginMode::kMinimized:
      return "minimized";
    case RunOnOsLoginMode::kUndefined:
      return "undefined";
  }
}

}  // namespace web_app
