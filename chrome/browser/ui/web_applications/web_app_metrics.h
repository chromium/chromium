// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/power_monitor/power_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/web_applications/diagnostics/web_app_icon_health_checks.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/site_engagement/content/site_engagement_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Profile;
class Browser;
class TabStripModel;

namespace base {
class Time;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace site_engagement {
enum class EngagementType;
}  // namespace site_engagement

namespace web_app {

// A per-profile keyed service, responsible for all Web Applications-related
// metrics recording (UMA histograms and UKM keyed by web-apps).
class WebAppMetrics : public KeyedService,
                      public site_engagement::SiteEngagementObserver,
                      public BrowserListObserver,
                      public TabStripModelObserver,
                      public base::PowerSuspendObserver {
 public:
  static WebAppMetrics* Get(Profile* profile);

  static void DisableAutomaticIconHealthChecksForTesting();

  explicit WebAppMetrics(Profile* profile);
  WebAppMetrics(const WebAppMetrics&) = delete;
  WebAppMetrics& operator=(const WebAppMetrics&) = delete;
  ~WebAppMetrics() override;

  // SiteEngagementObserver:
  void OnEngagementEvent(
      content::WebContents* web_contents,
      const GURL& url,
      double score,
      site_engagement::EngagementType engagement_type) override;

  // BrowserListObserver:
  void OnBrowserNoLongerActive(Browser* browser) override;
  void OnBrowserSetLastActive(Browser* browser) override;

  // TabStripModelObserver for all Browsers:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // base::PowerSuspendObserver:
  void OnSuspend() override;

  // Called when a web contents changes associated AppId (may be empty).
  void NotifyOnAssociatedAppChanged(
      content::WebContents* web_contents,
      const absl::optional<AppId>& previous_app_id,
      const absl::optional<AppId>& new_app_id);

  // Notify WebAppMetrics that an installability check has been completed for
  // a WebContents (see AppBannerManager::OnInstallableWebAppStatusUpdated).
  void NotifyInstallableWebAppStatusUpdated(content::WebContents* web_contents);
  // Notify WebAppMetrics that a WebContents is being destroyed.
  void NotifyWebContentsDestroyed(content::WebContents* web_contents);

  // Browser activation causes flaky tests. Call observer methods directly.
  void RemoveBrowserListObserverForTesting();
  void CountUserInstalledAppsForTesting();

  WebAppIconHealthChecks& icon_health_checks_for_testing() {
    return icon_health_checks_;
  }

 private:
  void CountUserInstalledApps();
  enum class TabSwitching {
    kFrom,
    kTo,
    kBackgroundClosing,
    kForegroundClosing
  };
  void UpdateUkmData(content::WebContents* web_contents, TabSwitching mode);

  // Calculate number of user installed apps once on start to avoid cpu costs
  // in OnEngagementEvent: sacrifice histograms accuracy for speed.
  static constexpr int kNumUserInstalledAppsNotCounted = -1;
  int num_user_installed_apps_ = kNumUserInstalledAppsNotCounted;

  base::flat_map<web_app::AppId, base::Time> app_last_interacted_time_{};
  raw_ptr<content::WebContents> foreground_web_contents_ = nullptr;
  GURL last_recorded_web_app_start_url_;

  const raw_ptr<Profile> profile_;

  WebAppIconHealthChecks icon_health_checks_;

  BrowserTabStripTracker browser_tab_strip_tracker_;

  base::WeakPtrFactory<WebAppMetrics> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_H_
