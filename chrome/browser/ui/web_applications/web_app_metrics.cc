// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_metrics.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/power_monitor/power_monitor.h"
#include "base/time/time.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/web_app_metrics_factory.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/browser/web_applications/daily_metrics_helper.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

using DisplayMode = blink::mojom::DisplayMode;
using base::Optional;
using content::WebContents;

namespace web_app {

namespace {

// Max amount of time to record as a session. If a session exceeds this length,
// treat it as invalid (0 time).
// TODO (crbug.com/1081187): Use an idle timeout instead.
constexpr base::TimeDelta max_valid_session_delta_ =
    base::TimeDelta::FromHours(12);

void RecordEngagementHistogram(
    const std::string& histogram_name,
    SiteEngagementService::EngagementType engagement_type) {
  base::UmaHistogramEnumeration(histogram_name, engagement_type,
                                SiteEngagementService::ENGAGEMENT_LAST);
}

void RecordTabOrWindowHistogram(
    const std::string& histogram_prefix,
    bool in_window,
    SiteEngagementService::EngagementType engagement_type) {
  RecordEngagementHistogram(
      histogram_prefix + (in_window ? ".InWindow" : ".InTab"), engagement_type);
}

void RecordUserInstalledHistogram(
    bool in_window,
    SiteEngagementService::EngagementType engagement_type) {
  const std::string histogram_prefix = "WebApp.Engagement.UserInstalled";
  RecordTabOrWindowHistogram(histogram_prefix, in_window, engagement_type);
}

Optional<int> GetLatestWebAppInstallSource(const AppId& app_id,
                                           PrefService* prefs) {
  Optional<int> value =
      GetIntWebAppPref(prefs, app_id, kLatestWebAppInstallSource);
  DCHECK_GE(value.value_or(0), 0);
  DCHECK_LT(value.value_or(0), static_cast<int>(WebappInstallSource::COUNT));
  return value;
}

}  // namespace

// static
WebAppMetrics* WebAppMetrics::Get(Profile* profile) {
  return WebAppMetricsFactory::GetForProfile(profile);
}

WebAppMetrics::WebAppMetrics(Profile* profile)
    : SiteEngagementObserver(SiteEngagementService::Get(profile)),
      profile_(profile),
      browser_tab_strip_tracker_(this, nullptr) {
  browser_tab_strip_tracker_.Init();
  base::PowerMonitor::AddObserver(this);
  BrowserList::AddObserver(this);

  WebAppProvider* provider = WebAppProvider::Get(profile_);
  DCHECK(provider);
  provider->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebAppMetrics::CountUserInstalledApps,
                                weak_ptr_factory_.GetWeakPtr()));
}

WebAppMetrics::~WebAppMetrics() {
  UpdateForegroundWebContents(nullptr);
  BrowserList::RemoveObserver(this);
  base::PowerMonitor::RemoveObserver(this);
}

void WebAppMetrics::OnEngagementEvent(
    WebContents* web_contents,
    const GURL& url,
    double score,
    SiteEngagementService::EngagementType engagement_type) {
  if (!web_contents)
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  // Number of apps is not yet counted.
  if (num_user_installed_apps_ == kNumUserInstalledAppsNotCounted)
    return;

  // The engagement broken down by the number of apps installed must be recorded
  // for all engagement events, not just web apps.
  if (num_user_installed_apps_ > 3) {
    RecordEngagementHistogram(
        "WebApp.Engagement.MoreThanThreeUserInstalledApps", engagement_type);
  } else if (num_user_installed_apps_ > 0) {
    RecordEngagementHistogram("WebApp.Engagement.UpToThreeUserInstalledApps",
                              engagement_type);
  } else {
    RecordEngagementHistogram("WebApp.Engagement.NoUserInstalledApps",
                              engagement_type);
  }

  // A presence of WebAppTabHelperBase with valid app_id indicates an installed
  // web app.
  WebAppTabHelperBase* tab_helper =
      WebAppTabHelperBase::FromWebContents(web_contents);
  if (!tab_helper)
    return;
  AppId app_id = tab_helper->GetAppId();
  if (app_id.empty())
    return;

  // No HostedAppBrowserController if app is running as a tab in common browser.
  const bool in_window = !!browser->app_controller();
  const bool user_installed =
      WebAppProvider::Get(profile_)->registrar().WasInstalledByUser(app_id);

  // Record all web apps:
  RecordTabOrWindowHistogram("WebApp.Engagement", in_window, engagement_type);

  if (user_installed) {
    RecordUserInstalledHistogram(in_window, engagement_type);
  } else {
    // Record this app into more specific bucket if was installed by default:
    RecordTabOrWindowHistogram("WebApp.Engagement.DefaultInstalled", in_window,
                               engagement_type);
  }
}

void WebAppMetrics::OnBrowserNoLongerActive(Browser* browser) {
  // OnBrowserNoLongerActive is called before OnBrowserSetLastActive for any
  // focus switch.
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  // It is possible for the browser's active web contents to be different
  // from foreground_web_contents_ eg. when reparenting tabs. Just make both
  // inactive.
  if (web_contents != foreground_web_contents_)
    UpdateUkmData(web_contents, TabSwitching::kFrom);
  UpdateUkmData(foreground_web_contents_, TabSwitching::kFrom);
  UpdateForegroundWebContents(nullptr);
}

void WebAppMetrics::OnBrowserSetLastActive(Browser* browser) {
  // OnBrowserNoLongerActive is called before OnBrowserSetLastActive for any
  // focus switch.
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  UpdateForegroundWebContents(web_contents);
  UpdateUkmData(web_contents, TabSwitching::kTo);
}

void WebAppMetrics::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // Process deselection, removal, then selection in-order so we have a
  // consistent view of selected and last-interacted tabs.
  UpdateUkmData(selection.old_contents, TabSwitching::kFrom);

  UpdateForegroundWebContents(selection.new_contents);

  if (change.type() == TabStripModelChange::kRemoved &&
      change.GetRemove()->will_be_deleted) {
    for (const TabStripModelChange::ContentsWithIndex& contents :
         change.GetRemove()->contents) {
      auto* tab_helper =
          WebAppTabHelperBase::FromWebContents(contents.contents);
      if (tab_helper && !tab_helper->GetAppId().empty())
        app_last_interacted_time_.erase(tab_helper->GetAppId());
      // Foreground contents should not be going away.
      DCHECK_NE(contents.contents, foreground_web_contents_);
    }
  }

  UpdateUkmData(selection.new_contents, TabSwitching::kTo);
}

void WebAppMetrics::OnSuspend() {
  // Update current tab as foreground time.
  if (foreground_web_contents_) {
    auto* tab_helper =
        WebAppTabHelperBase::FromWebContents(foreground_web_contents_);
    if (tab_helper && !tab_helper->GetAppId().empty() &&
        app_last_interacted_time_.contains(tab_helper->GetAppId())) {
      UpdateUkmData(foreground_web_contents_, TabSwitching::kFrom);
      app_last_interacted_time_.erase(tab_helper->GetAppId());
    }
  }
  // Update all other tabs as background time.
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    int tab_count = browser->tab_strip_model()->GetTabCount();
    for (int i = 0; i < tab_count; i++) {
      WebContents* contents = browser->tab_strip_model()->GetWebContentsAt(i);
      DCHECK(contents);
      auto* tab_helper = WebAppTabHelperBase::FromWebContents(contents);
      if (tab_helper && !tab_helper->GetAppId().empty() &&
          app_last_interacted_time_.contains(tab_helper->GetAppId())) {
        UpdateUkmData(contents, TabSwitching::kBackgroundClosing);
      }
    }
  }
  app_last_interacted_time_.clear();
}

void WebAppMetrics::NotifyOnAssociatedAppChanged(
    content::WebContents* web_contents,
    const AppId& previous_app_id,
    const AppId& new_app_id) {
  // Ensure we aren't counting closed app as still open.
  // TODO (crbug.com/1081187): If there were multiple app instances open, this
  // will prevent background time being counted until the app is next active.
  app_last_interacted_time_.erase(previous_app_id);
  // Don't record any UKM data here. It will be recorded in
  // |OnInstallableWebAppStatusUpdated| once fully fetched.
}

void WebAppMetrics::OnInstallableWebAppStatusUpdated() {
  DCHECK(foreground_web_contents_);
  // Skip recording if we just recorded this App ID (when switching tabs/windows
  // or on a previous navigation that triggered this event). Otherwise we would
  // count navigations within a web app as sessions.
  auto* app_banner_manager =
      banners::AppBannerManager::FromWebContents(foreground_web_contents_);
  DCHECK(app_banner_manager);
  if (!app_banner_manager->GetManifestStartUrl().is_valid())
    return;
  if (app_banner_manager->GetManifestStartUrl() ==
      last_recorded_web_app_start_url_) {
    return;
  }

  UpdateUkmData(foreground_web_contents_, TabSwitching::kTo);
}

void WebAppMetrics::RemoveBrowserListObserverForTesting() {
  BrowserList::RemoveObserver(this);
}

void WebAppMetrics::CountUserInstalledAppsForTesting() {
  // Reset and re-count.
  num_user_installed_apps_ = kNumUserInstalledAppsNotCounted;
  CountUserInstalledApps();
}

void WebAppMetrics::CountUserInstalledApps() {
  DCHECK_EQ(kNumUserInstalledAppsNotCounted, num_user_installed_apps_);

  WebAppProvider* provider = WebAppProvider::Get(profile_);

  num_user_installed_apps_ = provider->registrar().CountUserInstalledApps();
  DCHECK_NE(kNumUserInstalledAppsNotCounted, num_user_installed_apps_);
  DCHECK_GE(num_user_installed_apps_, 0);
}

void WebAppMetrics::UpdateUkmData(WebContents* web_contents,
                                  TabSwitching mode) {
  if (!web_contents)
    return;
  auto* app_banner_manager =
      banners::AppBannerManager::FromWebContents(web_contents);
  DCHECK(app_banner_manager);
  WebAppProvider* provider = WebAppProvider::Get(profile_);
  // WebAppProvider not available in kiosk mode.
  if (!provider)
    return;
  DailyInteraction features;

  auto* tab_helper = WebAppTabHelperBase::FromWebContents(web_contents);
  if (tab_helper &&
      provider->registrar().IsLocallyInstalled(tab_helper->GetAppId())) {
    // App is installed
    const AppId& app_id = tab_helper->GetAppId();
    features.start_url = provider->registrar().GetAppStartUrl(app_id);
    features.installed = true;
    features.install_source =
        GetLatestWebAppInstallSource(app_id, profile_->GetPrefs());
    DisplayMode display_mode =
        provider->registrar().GetAppUserDisplayMode(app_id);
    features.effective_display_mode = static_cast<int>(display_mode);
    // AppBannerManager treats already-installed web-apps as non-promotable, so
    // include already-installed findings as promotable.
    features.promotable = app_banner_manager->IsProbablyPromotableWebApp(
        /*ignore_existing_installations=*/true);
    // Record usage duration and session counts only for installed web apps that
    // are currently open in a window.
    if (provider->ui_manager().IsInAppWindow(web_contents)) {
      base::Time now = base::Time::Now();
      if (app_last_interacted_time_.contains(app_id)) {
        base::TimeDelta delta = now - app_last_interacted_time_[app_id];
        if (delta < max_valid_session_delta_) {
          switch (mode) {
            case TabSwitching::kFrom:
              features.foreground_duration = delta;
              break;
            case TabSwitching::kTo:
            case TabSwitching::kBackgroundClosing:
              features.background_duration = delta;
              break;
          }
        }
      }
      app_last_interacted_time_[app_id] = now;

      // Note: real web app launch counts 2 sessions immediately, as app window
      // is actually activated twice in the launch process.
      if (mode == TabSwitching::kTo)
        features.num_sessions = 1;
    }
  } else if (app_banner_manager->IsPromotableWebApp()) {
    // App is not installed, but is promotable. Record a subset of features.
    features.start_url = app_banner_manager->GetManifestStartUrl();
    DCHECK(features.start_url.is_valid());
    features.installed = false;
    DisplayMode display_mode = app_banner_manager->GetManifestDisplayMode();
    features.effective_display_mode = static_cast<int>(display_mode);
    features.promotable = true;
  } else {
    last_recorded_web_app_start_url_ = GURL();
    return;
  }
  last_recorded_web_app_start_url_ = features.start_url;
  FlushOldRecordsAndUpdate(features, profile_);
}

// Store web_contents for use in ABM observer callback.
void WebAppMetrics::UpdateForegroundWebContents(WebContents* web_contents) {
  if (web_contents == foreground_web_contents_)
    return;
  app_banner_manager_observer_.RemoveAll();
  foreground_web_contents_ = web_contents;
  if (web_contents) {
    auto* app_banner_manager =
        banners::AppBannerManager::FromWebContents(web_contents);
    DCHECK(app_banner_manager);
    app_banner_manager_observer_.Add(app_banner_manager);
  }
}

}  // namespace web_app
