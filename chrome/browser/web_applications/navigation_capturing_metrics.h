// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_METRICS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_METRICS_H_

#include <ostream>

#include "components/webapps/common/web_app_id.h"

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

// Used to record histograms for the result of navigation capturing before a
// redirection happens. For redirections, both this enum and the
// `RedirectionResult` enum below will be measured. Please keep in sync with
// tools/metrics/histograms/metadata/webapps/enums.xml and do not reuse
// existing entries.
enum class NavigationCapturingInitialResult {
  // The navigation was captured into a new browser tab, where it will
  // continue.
  kNewAppBrowserTab = 0,
  // The navigation was captured into a new app window, where it will
  // continue.
  kNewAppWindow = 1,
  // The navigation was cancelled, and an existing browser tab belonging to a
  // web app was focused.
  kFocusExistingAppBrowserTab = 2,
  // The navigation was cancelled, and an existing app window belonging to a
  // web
  // app was focused.
  kFocusExistingAppWindow = 3,
  // The navigation was captured and it resulted in the creation of a new
  // browser tab for the navigation. This can only occur when the app opens in
  // a browser tab.
  kNavigateExistingAppBrowserTab = 4,
  // The navigation was captured and it resulted in a existing web contents
  // (either in an app window or browser tab) to be navigated.
  kNavigateExistingAppWindow = 5,
  // The capturing logic forced this to launch the app in a new app tab
  // context, with the same behavior of `navigate-new`. This is used when it
  // was a user-modified navigation, triggered by a shift or middle click.
  // Launch parameters are enqueued.
  kForcedContextAppBrowserTab = 6,
  // Same as above but for an app that opens in a new app window.
  kForcedContextAppWindow = 7,
  // The navigation opens an auxiliary context, and opens in a new browser
  // 'window container`
  kAuxiliaryContextAppBrowserTab = 8,
  // Same as above but opens in a new app window.
  kAuxiliaryContextAppWindow = 9,
  // The navigation was cancelled by the process.
  kNavigationCanceled = 10,
  // The navigation was overridden by a browser that was either created by the
  // process or passed in via the `NavigateParams`.
  kOverrideBrowser = 11,
  // A browser tab was opened that wasn't the result of web app navigation
  // capturing, but due to redirection the final behavior could change.
  // Note: New context & capturable behavior for open-in-browser-tab apps
  // apply to the cases below, and are not part of this category.
  kNewTabRedirectionEligible = 12,
  // This navigation should be excluded from redirection handling. The
  // NavigationCapturingProcess instance will not be attached to the
  // NavigationHandle.
  kNotHandled = 13,
  kMaxValue = kNotHandled
};

std::ostream& operator<<(std::ostream& out,
                         NavigationCapturingInitialResult nav_capturing_result);

// Used to record histograms for the result of navigation capturing after a
// redirection happens. The intermediary stages of the navigation, if captured,
// will be measured using the `NavigationCapturingResult` enum. Please keep in
// sync with tools/metrics/histograms/metadata/webapps/enums.xml and do not
// reuse existing entries.
enum class NavigationCapturingRedirectionResult {
  kReparentBrowserTabToBrowserTab = 0,
  kReparentBrowserTabToApp = 1,
  kReparentAppToBrowserTab = 2,
  kReparentAppToApp = 3,
  kAppWindowOpened = 4,
  kAppBrowserTabOpened = 5,
  kNavigateExistingAppBrowserTab = 6,
  kNavigateExistingAppWindow = 7,
  kFocusExistingAppBrowserTab = 8,
  kFocusExistingAppWindow = 9,
  kSameContext = 10,
  kNotCapturable = 11,
  kNotHandled = 12,
  kMaxValue = kNotHandled
};

std::ostream& operator<<(
    std::ostream& out,
    NavigationCapturingRedirectionResult redirection_result);

// Records the final container of navigation capturing with respect to the
// effective display mode of the PWA it opened in. Measured once per navigation
// at the end of navigation capturing. Please keep in sync with
// tools/metrics/histograms/metadata/webapps/enums.xml and do not reuse existing
// entries.
enum class NavigationCapturingDisplayModeResult {
  kAppStandaloneFinalStandalone = 0,
  kAppBrowserTabFinalBrowserTab = 1,
  kAppBrowserTabFinalStandalone = 2,
  kAppStandaloneFinalBrowserTab = 3,
  kMaxValue = kAppStandaloneFinalBrowserTab
};

std::ostream& operator<<(
    std::ostream& out,
    NavigationCapturingDisplayModeResult display_mode_result);

// Records the NavigationCapturingDisplayModeResult into histograms once per
// navigation (including redirects) based on a comparison between the final
// "container" of the web app the navigation ended up with and the effective
// display mode.
void RecordNavigationCapturingDisplayModeMetrics(
    const webapps::AppId& app_id,
    content::WebContents* web_contents,
    bool is_launch_container_tab);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_METRICS_H_
