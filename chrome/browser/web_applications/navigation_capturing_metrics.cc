// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/navigation_capturing_metrics.h"

#include <ostream>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

std::ostream& operator<<(
    std::ostream& out,
    NavigationCapturingInitialResult nav_capturing_result) {
  switch (nav_capturing_result) {
    case NavigationCapturingInitialResult::kNewAppBrowserTab:
      return out << "NewAppBrowserTab";
    case NavigationCapturingInitialResult::kNewAppWindow:
      return out << "NewAppWindow";
    case NavigationCapturingInitialResult::kFocusExistingAppBrowserTab:
      return out << "FocusExistingAppBrowserTab";
    case NavigationCapturingInitialResult::kFocusExistingAppWindow:
      return out << "FocusExistingAppWindow";
    case NavigationCapturingInitialResult::kNavigateExistingAppBrowserTab:
      return out << "NavigateExistingAppBrowserTab";
    case NavigationCapturingInitialResult::kNavigateExistingAppWindow:
      return out << "NavigateExistingAppWindow";
    case NavigationCapturingInitialResult::kForcedContextAppBrowserTab:
      return out << "ForcedContextAppBrowserTab";
    case NavigationCapturingInitialResult::kForcedContextAppWindow:
      return out << "ForcedContextAppWindow";
    case NavigationCapturingInitialResult::kAuxiliaryContextAppBrowserTab:
      return out << "AuxiliaryContextAppBrowserTab";
    case NavigationCapturingInitialResult::kAuxiliaryContextAppWindow:
      return out << "AuxiliaryContextAppWindow";
    case NavigationCapturingInitialResult::kNavigationCanceled:
      return out << "NavigationCanceled";
    case NavigationCapturingInitialResult::kOverrideBrowser:
      return out << "OverrideBrowser";
    case NavigationCapturingInitialResult::kNewTabRedirectionEligible:
      return out << "NewTabRedirectionEligible";
    case NavigationCapturingInitialResult::kNotHandled:
      return out << "NotHandled";
  }
}

std::ostream& operator<<(
    std::ostream& out,
    NavigationCapturingRedirectionResult redirection_result) {
  switch (redirection_result) {
    case NavigationCapturingRedirectionResult::kReparentBrowserTabToBrowserTab:
      return out << "ReparentBrowserTabToBrowserTab";
    case NavigationCapturingRedirectionResult::kReparentBrowserTabToApp:
      return out << "ReparentBrowserTabToApp";
    case NavigationCapturingRedirectionResult::kReparentAppToBrowserTab:
      return out << "ReparentAppToBrowserTab";
    case NavigationCapturingRedirectionResult::kReparentAppToApp:
      return out << "ReparentAppToApp";
    case NavigationCapturingRedirectionResult::kAppWindowOpened:
      return out << "AppWindowOpened";
    case NavigationCapturingRedirectionResult::kAppBrowserTabOpened:
      return out << "AppBrowserTabOpened";
    case NavigationCapturingRedirectionResult::kNavigateExistingAppBrowserTab:
      return out << "NavigateExistingAppBrowserTab";
    case NavigationCapturingRedirectionResult::kNavigateExistingAppWindow:
      return out << "NavigateExistingAppWindow";
    case NavigationCapturingRedirectionResult::kFocusExistingAppBrowserTab:
      return out << "FocusExistingAppBrowserTab";
    case NavigationCapturingRedirectionResult::kFocusExistingAppWindow:
      return out << "FocusExistingAppWindow";
    case NavigationCapturingRedirectionResult::kSameContext:
      return out << "SameContext";
    case NavigationCapturingRedirectionResult::kNotCapturable:
      return out << "NotCapturable";
    case NavigationCapturingRedirectionResult::kNotHandled:
      return out << "NotHandled";
  }
}

std::ostream& operator<<(
    std::ostream& out,
    NavigationCapturingDisplayModeResult display_mode_result) {
  switch (display_mode_result) {
    case NavigationCapturingDisplayModeResult::kAppStandaloneFinalStandalone:
      return out << "AppStandaloneFinalStandalone";
    case NavigationCapturingDisplayModeResult::kAppBrowserTabFinalBrowserTab:
      return out << "AppBrowserTabFinalBrowserTab";
    case NavigationCapturingDisplayModeResult::kAppBrowserTabFinalStandalone:
      return out << "AppBrowserTabFinalStandalone";
    case NavigationCapturingDisplayModeResult::kAppStandaloneFinalBrowserTab:
      return out << "AppStandaloneFinalBrowserTab";
  }
}

void RecordNavigationCapturingDisplayModeMetrics(
    const webapps::AppId& app_id,
    content::WebContents* web_contents,
    bool is_launch_container_tab) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  WebAppProvider* web_app_provider = WebAppProvider::GetForWebApps(profile);
  DisplayMode display_mode =
      web_app_provider->registrar_unsafe().GetAppEffectiveDisplayMode(app_id);
  CHECK_NE(display_mode, DisplayMode::kUndefined);
  bool is_display_mode_browser = display_mode == DisplayMode::kBrowser;

  NavigationCapturingDisplayModeResult display_mode_result;
  if (is_launch_container_tab && is_display_mode_browser) {
    display_mode_result =
        NavigationCapturingDisplayModeResult::kAppBrowserTabFinalBrowserTab;
  } else if (is_launch_container_tab && !is_display_mode_browser) {
    display_mode_result =
        NavigationCapturingDisplayModeResult::kAppStandaloneFinalBrowserTab;
  } else if (!is_launch_container_tab && is_display_mode_browser) {
    display_mode_result =
        NavigationCapturingDisplayModeResult::kAppBrowserTabFinalStandalone;
  } else {
    CHECK(!is_launch_container_tab && !is_display_mode_browser);
    display_mode_result =
        NavigationCapturingDisplayModeResult::kAppStandaloneFinalStandalone;
  }

  base::UmaHistogramEnumeration(
      "Webapp.NavigationCapturing.FinalDisplay.Result", display_mode_result);
}

}  // namespace web_app
