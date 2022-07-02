// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_constants.h"

#include <ostream>

#include "base/compiler_specific.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/common/content_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

static_assert(WebAppManagement::kMinValue == 0,
              "Source enum should be zero based");

std::ostream& operator<<(std::ostream& os, WebAppManagement::Type type) {
  switch (type) {
    case WebAppManagement::Type::kSystem:
      return os << "System";
    case WebAppManagement::Type::kPolicy:
      return os << "Policy";
    case WebAppManagement::Type::kSubApp:
      return os << "SubApp";
    case WebAppManagement::Type::kWebAppStore:
      return os << "WebAppStore";
    case WebAppManagement::Type::kSync:
      return os << "Sync";
    case WebAppManagement::Type::kDefault:
      return os << "Default";
  }
}

static_assert(OsHookType::kShortcuts == 0,
              "OsHookType enum should be zero based");

namespace {

absl::optional<DisplayMode> TryResolveUserDisplayMode(
    UserDisplayMode user_display_mode) {
  switch (user_display_mode) {
    case UserDisplayMode::kBrowser:
      return DisplayMode::kBrowser;
    case UserDisplayMode::kTabbed:
      if (base::FeatureList::IsEnabled(features::kDesktopPWAsTabStripSettings))
        return DisplayMode::kTabbed;
      // Treat as standalone.
      [[fallthrough]];
    case UserDisplayMode::kStandalone:
      break;
  }

  return absl::nullopt;
}

absl::optional<DisplayMode> TryResolveOverridesDisplayMode(
    const std::vector<DisplayMode>& display_mode_overrides) {
  for (DisplayMode override_display_mode : display_mode_overrides) {
    DisplayMode resolved_display_mode =
        ResolveAppDisplayModeForStandaloneLaunchContainer(
            override_display_mode);
    if (override_display_mode == resolved_display_mode) {
      return resolved_display_mode;
    }
  }

  return absl::nullopt;
}

DisplayMode ResolveNonIsolatedEffectiveDisplayMode(
    DisplayMode app_display_mode,
    const std::vector<DisplayMode>& display_mode_overrides,
    UserDisplayMode user_display_mode) {
  const absl::optional<DisplayMode> resolved_display_mode =
      TryResolveUserDisplayMode(user_display_mode);
  if (resolved_display_mode.has_value()) {
    return *resolved_display_mode;
  }

  const absl::optional<DisplayMode> resolved_override_display_mode =
      TryResolveOverridesDisplayMode(display_mode_overrides);
  if (resolved_override_display_mode.has_value()) {
    return *resolved_override_display_mode;
  }

  return ResolveAppDisplayModeForStandaloneLaunchContainer(app_display_mode);
}

}  // namespace

DisplayMode ResolveEffectiveDisplayMode(
    DisplayMode app_display_mode,
    const std::vector<DisplayMode>& app_display_mode_overrides,
    UserDisplayMode user_display_mode,
    bool is_isolated) {
  const DisplayMode resolved_display_mode =
      ResolveNonIsolatedEffectiveDisplayMode(
          app_display_mode, app_display_mode_overrides, user_display_mode);
  if (is_isolated && resolved_display_mode == DisplayMode::kBrowser) {
    return DisplayMode::kStandalone;
  }

  return resolved_display_mode;
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

apps::RunOnOsLoginMode ConvertOsLoginMode(
    web_app::RunOnOsLoginMode login_mode) {
  switch (login_mode) {
    case web_app::RunOnOsLoginMode::kWindowed:
      return apps::RunOnOsLoginMode::kWindowed;
    case web_app::RunOnOsLoginMode::kNotRun:
      return apps::RunOnOsLoginMode::kNotRun;
    case web_app::RunOnOsLoginMode::kMinimized:
      return apps::RunOnOsLoginMode::kUnknown;
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
