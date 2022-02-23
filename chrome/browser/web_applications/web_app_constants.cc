// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_constants.h"

#include <ostream>

#include "base/compiler_specific.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/common/content_features.h"

namespace web_app {

const char kRunOnOsLoginModeWindowed[] = "windowed";

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
      [[fallthrough]];
    case DisplayMode::kStandalone:
    case DisplayMode::kFullscreen:
      return DisplayMode::kStandalone;
    case DisplayMode::kWindowControlsOverlay:
      return DisplayMode::kWindowControlsOverlay;
    case DisplayMode::kTabbed:
      if (base::FeatureList::IsEnabled(features::kDesktopPWAsTabStrip))
        return DisplayMode::kTabbed;
      else
        return DisplayMode::kStandalone;
  }
}
}  // namespace

static_assert(Source::kMinValue == 0, "Source enum should be zero based");

std::ostream& operator<<(std::ostream& os, Source::Type type) {
  switch (type) {
    case Source::Type::kSystem:
      return os << "System";
    case Source::Type::kPolicy:
      return os << "Policy";
    case Source::Type::kSubApp:
      return os << "SubApp";
    case Source::Type::kWebAppStore:
      return os << "WebAppStore";
    case Source::Type::kSync:
      return os << "Sync";
    case Source::Type::kDefault:
      return os << "Default";
  }
}

static_assert(OsHookType::kShortcuts == 0,
              "OsHookType enum should be zero based");

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
    case DisplayMode::kWindowControlsOverlay:
      NOTREACHED();
      [[fallthrough]];
    case DisplayMode::kTabbed:
      if (base::FeatureList::IsEnabled(features::kDesktopPWAsTabStripSettings))
        return user_display_mode;
      // Treat as standalone.
      [[fallthrough]];
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
    case DisplayMode::kStandalone:
    case DisplayMode::kFullscreen:
    case DisplayMode::kWindowControlsOverlay:
    case DisplayMode::kTabbed:
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
    case RunOnOsLoginMode::kNotRun:
      return "not run";
  }
}

const char* IconsDownloadedResultToString(IconsDownloadedResult result) {
  switch (result) {
    case IconsDownloadedResult::kCompleted:
      return "Completed";
    case IconsDownloadedResult::kPrimaryPageChanged:
      return "PrimaryPageChanged";
    case IconsDownloadedResult::kAbortedDueToFailure:
      return "AbortedDueToFailure";
  }
}

}  // namespace web_app
