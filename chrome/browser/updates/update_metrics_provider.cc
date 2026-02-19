// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/update_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/webui_url_constants.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace {

using PendingUpdateState = UpdateMetricsProvider::PendingUpdateState;
using PageBlockingUpdate = UpdateMetricsProvider::PageBlockingUpdate;

PendingUpdateState GetPendingUpdateState() {
  // Is Chrome backgrounded?
  if (KeepAliveRegistry::GetInstance()->WouldRestartWithout({
          KeepAliveOrigin::SESSION_RESTORE,
          KeepAliveOrigin::BACKGROUND_MODE_MANAGER_STARTUP,
          KeepAliveOrigin::BACKGROUND_SYNC,
          KeepAliveOrigin::NOTIFICATION,
          KeepAliveOrigin::PENDING_NOTIFICATION_CLICK_EVENT,
          KeepAliveOrigin::PENDING_NOTIFICATION_CLOSE_EVENT,
          KeepAliveOrigin::IN_FLIGHT_PUSH_MESSAGE,
      })) {
    return PendingUpdateState::kBackgrounded;
  }

  // Is there only one window and one tab?
  size_t window_count = chrome::GetTotalBrowserCount();
  if (window_count == 0) {
    return PendingUpdateState::kUnknown;
  }
  // Multiple windows.
  if (window_count > 1) {
    return PendingUpdateState::kMultipleTabsOrWindows;
  }

  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    // This might happen during startup.
    return PendingUpdateState::kUnknown;
  }

  // Multiple tabs.
  if (browser->tab_strip_model()->count() > 1) {
    return PendingUpdateState::kMultipleTabsOrWindows;
  }
  return PendingUpdateState::kOneWindowOneTab;
}

// When there is one tab in one window, what is it?
PageBlockingUpdate GetPageBlockingUpdate() {
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return PageBlockingUpdate::kErrorNoBrowser;
  }

  content::WebContents* active_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!active_contents) {
    return PageBlockingUpdate::kErrorNoTabs;
  }

  const GURL& url = active_contents->GetVisibleURL();
  if (url == GURL(chrome::kChromeUINewTabURL)) {
    return PageBlockingUpdate::kNtp;
  }
  if (url.IsAboutBlank()) {
    return PageBlockingUpdate::kAboutBlank;
  }
  if (url == GURL(chrome::kChromeUIWhatsNewURL)) {
    return PageBlockingUpdate::kWhatsNew;
  }
  if (url.SchemeIs(content::kChromeUIScheme)) {
    return PageBlockingUpdate::kChromeScheme;
  }
  return PageBlockingUpdate::kUnspecified;
}

}  // namespace

void UpdateMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  UpgradeDetector* upgrade_detector = UpgradeDetector::GetInstance();
  if (!upgrade_detector->is_upgrade_available()) {
    base::UmaHistogramEnumeration("Chrome.BuildState.PendingUpdateState",
                                  PendingUpdateState::kNoUpdate);
    return;
  }

  base::TimeDelta time_since_update =
      base::Time::Now() - upgrade_detector->upgrade_detected_time();
  // This is similar to UpgradeDetector.DaysBeforeUpgrade but logged every
  // time UMA is emitted to get population metrics that match pending state.
  base::UmaHistogramCustomCounts("Chrome.BuildState.TimeSinceUpdateAvailable",
                                 time_since_update.InMinutes(), 1,
                                 base::Days(30).InMinutes(), 50);
  auto pending_update_state = GetPendingUpdateState();
  base::UmaHistogramEnumeration("Chrome.BuildState.PendingUpdateState",
                                pending_update_state);
  if (pending_update_state == PendingUpdateState::kOneWindowOneTab) {
    base::UmaHistogramEnumeration("Chrome.BuildState.PageBlockingUpdate",
                                  GetPageBlockingUpdate());
  }
}
