// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/navigation_capturing_metrics.h"

#include <ostream>

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

}  // namespace web_app
