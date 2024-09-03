// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_metrics.h"

#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/clamped_math.h"
#include "base/observer_list.h"
#include "base/one_shot_event.h"
#include "base/power_monitor/power_monitor.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_metrics_factory.h"
#include "chrome/browser/web_applications/daily_metrics_helper.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"
#include "components/site_engagement/content/engagement_type.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/banners/installable_web_app_check_result.h"
#include "components/webapps/browser/banners/web_app_banner_data.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-forward.h"

using DisplayMode = blink::mojom::DisplayMode;
using content::WebContents;

namespace web_app {

namespace {

bool g_disable_automatic_icon_health_checks_for_testing = false;

// Max amount of time to record as a session. If a session exceeds this length,
// treat it as invalid (0 time).
// TODO (crbug.com/1081187): Use an idle timeout instead.
constexpr base::TimeDelta max_valid_session_delta_ = base::Hours(12);

void RecordEngagementHistogram(
    const std::string& histogram_name,
    site_engagement::EngagementType engagement_type) {
  base::UmaHistogramEnumeration(histogram_name, engagement_type,
                                site_engagement::EngagementType::kLast);
}

void RecordTabOrWindowHistogram(
    const std::string& histogram_prefix,
    bool in_window,
    site_engagement::EngagementType engagement_type) {
  RecordEngagementHistogram(
      histogram_prefix + (in_window ? ".InWindow" : ".InTab"), engagement_type);
}

bool IsPreferredAppForSupportedLinks(const webapps::AppId& app_id,
                                     Profile* profile) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return false;
  }
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  return proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_id);
}

}  // namespace

// static
WebAppMetrics* WebAppMetrics::Get(Profile* profile) {
  return WebAppMetricsFactory::GetForProfile(profile);
}

// static
void WebAppMetrics::DisableAutomaticIconHealthChecksForTesting() {
  g_disable_automatic_icon_health_checks_for_testing = true;
}

WebAppMetrics::WebAppMetrics(Profile* profile)
    : SiteEngagementObserver(
          site_engagement::SiteEngagementService::Get(profile)),
      profile_(profile),
      icon_health_checks_(profile),
      browser_tab_strip_tracker_(this, nullptr) {
  browser_tab_strip_tracker_.Init();
  base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);
  BrowserList::AddObserver(this);
  // This isn't around on ChromeOS or tests.
  if (metrics::DesktopSessionDurationTracker::IsInitialized()) {
    metrics::DesktopSessionDurationTracker::Get()->AddObserver(this);
  }

  if (base::FeatureList::IsEnabled(features::kDesktopPWAsIconHealthChecks) &&
      !g_disable_automatic_icon_health_checks_for_testing) {
    AfterStartupTaskUtils::PostTask(
        FROM_HERE, base::SequencedTaskRunner::GetCurrentDefault(),
        base::BindOnce(&WebAppIconHealthChecks::Start,
                       icon_health_checks_.GetWeakPtr(), base::DoNothing()));
  }

  WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(profile_);
  DCHECK(provider);
  provider->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebAppMetrics::CountUserInstalledApps,
                                weak_ptr_factory_.GetWeakPtr()));
}

WebAppMetrics::~WebAppMetrics() {
  BrowserList::RemoveObserver(this);
  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
  if (metrics::DesktopSessionDurationTracker::IsInitialized()) {
    metrics::DesktopSessionDurationTracker::Get()->RemoveObserver(this);
  }
}

void WebAppMetrics::OnEngagementEvent(
    WebContents* web_contents,
    const GURL& url,
    double score,
    double old_score,
    site_engagement::EngagementType engagement_type,
    const std::optional<webapps::AppId>& app_id) {
  if (!web_contents)
    return;

  Browser* browser = chrome::FindBrowserWithTab(web_contents);
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

  if (!app_id) {
    return;
  }
  CHECK(!app_id->empty());

  // No HostedAppBrowserController if app is running as a tab in common browser.
  const bool in_window = !!browser->app_controller();
  WebAppRegistrar& registrar =
      WebAppProvider::GetForLocalAppsUnchecked(profile_)->registrar_unsafe();
  const bool user_installed = registrar.WasInstalledByUser(*app_id);
  const bool is_diy_app = registrar.IsDiyApp(*app_id);
  const bool is_default_installed =
      registrar.IsInstalledByDefaultManagement(*app_id);

  // Record all web apps:
  RecordTabOrWindowHistogram("WebApp.Engagement", in_window, engagement_type);

  if (user_installed) {
    RecordTabOrWindowHistogram("WebApp.Engagement.UserInstalled", in_window,
                               engagement_type);
    if (is_diy_app) {
      RecordTabOrWindowHistogram("WebApp.Engagement.UserInstalled.Diy",
                                 in_window, engagement_type);
    } else {
      RecordTabOrWindowHistogram("WebApp.Engagement.UserInstalled.Crafted",
                                 in_window, engagement_type);
    }
  }
  if (is_default_installed) {
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
  foreground_web_contents_ = nullptr;
}

void WebAppMetrics::OnBrowserSetLastActive(Browser* browser) {
  // OnBrowserNoLongerActive is called before OnBrowserSetLastActive for any
  // focus switch.
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  foreground_web_contents_ = web_contents;
  UpdateUkmData(web_contents, TabSwitching::kTo);
}

void WebAppMetrics::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // Process deselection, removal, then selection in-order so we have a
  // consistent view of selected and last-interacted tabs.
  TabSwitching initial_mode = TabSwitching::kFrom;
  // Foreground usage duration should be counted when the web app is being
  // closed, despite IsInAppWindow returning false at this point.
  if (change.type() == TabStripModelChange::kRemoved &&
      tab_strip_model->empty()) {
    auto iter = base::ranges::find(*BrowserList::GetInstance(), tab_strip_model,
                                   &Browser::tab_strip_model);
    if (iter != BrowserList::GetInstance()->end() &&
        AppBrowserController::IsWebApp(*iter)) {
      initial_mode = TabSwitching::kForegroundClosing;
    }
  }
  UpdateUkmData(selection.old_contents, initial_mode);

  foreground_web_contents_ = selection.new_contents;
  // Newly-selected foreground contents should not be going away.
  if (foreground_web_contents_ &&
      foreground_web_contents_->IsBeingDestroyed()) {
    base::debug::DumpWithoutCrashing();
    foreground_web_contents_ = nullptr;
  }

  if (change.type() == TabStripModelChange::kRemoved) {
    for (const TabStripModelChange::RemovedTab& contents :
         change.GetRemove()->contents) {
      if (contents.remove_reason ==
          TabStripModelChange::RemoveReason::kDeleted) {
        const webapps::AppId* app_id =
            WebAppTabHelper::GetAppId(contents.contents);
        if (app_id)
          app_last_interacted_time_.erase(*app_id);
        // Newly-selected foreground contents should not be going away.
        if (contents.contents == foreground_web_contents_) {
          base::debug::DumpWithoutCrashing();
          foreground_web_contents_ = nullptr;
        }
      }
    }
  }

  UpdateUkmData(foreground_web_contents_, TabSwitching::kTo);
}

void WebAppMetrics::OnSuspend() {
  // Update current tab as foreground time.
  if (foreground_web_contents_) {
    const webapps::AppId* app_id =
        WebAppTabHelper::GetAppId(foreground_web_contents_);
    if (app_id && app_last_interacted_time_.contains(*app_id)) {
      UpdateUkmData(foreground_web_contents_, TabSwitching::kFrom);
    }
  }
  // Update all other tabs as background time.
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    int tab_count = browser->tab_strip_model()->GetTabCount();
    for (int i = 0; i < tab_count; i++) {
      WebContents* contents = browser->tab_strip_model()->GetWebContentsAt(i);
      DCHECK(contents);
      const webapps::AppId* app_id = WebAppTabHelper::GetAppId(contents);
      if (app_id && app_last_interacted_time_.contains(*app_id)) {
        UpdateUkmData(contents, TabSwitching::kBackgroundClosing);
      }
    }
  }
  // Clear all times for all apps, which are reset in either OnResume(), or
  // GetOrSetLastInteractedTimeForApp(), depending on ordering of OnResume().
  for (std::pair<webapps::AppId, std::optional<base::Time>>& app_time :
       app_last_interacted_time_) {
    app_time.second = std::nullopt;
  }
}

void WebAppMetrics::OnResume() {
  for (std::pair<webapps::AppId, std::optional<base::Time>>& app_time :
       app_last_interacted_time_) {
    app_time.second = base::Time::Now();
  }
}

void WebAppMetrics::OnSessionStarted(base::TimeTicks session_start) {}
void WebAppMetrics::OnSessionEnded(base::TimeDelta session_length,
                                   base::TimeTicks session_end) {
  // Ensure that we do not over-count foreground usage if the session is
  // considered 'ended' by the browser. This allows the cumulative sum of
  // foreground time to be comparable with Session.TotalDuration.
  if (foreground_web_contents_) {
    const webapps::AppId* app_id =
        WebAppTabHelper::GetAppId(foreground_web_contents_);
    if (app_id && app_last_interacted_time_.contains(*app_id)) {
      UpdateUkmData(foreground_web_contents_, TabSwitching::kFrom);
    }
  }
}

void WebAppMetrics::NotifyOnAssociatedAppChanged(
    content::WebContents* web_contents,
    const std::optional<webapps::AppId>& previous_app_id,
    const std::optional<webapps::AppId>& new_app_id) {
  // Ensure we aren't counting closed app as still open.
  // TODO (crbug.com/1081187): If there were multiple app instances open, this
  // will prevent background time being counted until the app is next active.
  if (previous_app_id.has_value())
    app_last_interacted_time_.erase(previous_app_id.value());
  // Don't record any UKM data here. It will be recorded in
  // |NotifyInstallableWebAppStatusUpdated| once fully fetched.
}

void WebAppMetrics::NotifyInstallableWebAppStatusUpdated(
    WebContents* web_contents,
    webapps::InstallableWebAppCheckResult result,
    const std::optional<webapps::WebAppBannerData>& data) {
  DCHECK(web_contents);
  // Skip recording if app isn't in the foreground.
  if (web_contents != foreground_web_contents_)
    return;
  // Skip recording if we just recorded this App ID (when switching tabs/windows
  // or on a previous navigation that triggered this event). Otherwise we would
  // count navigations within a web app as sessions.
  auto* app_banner_manager =
      webapps::AppBannerManager::FromWebContents(foreground_web_contents_);
  DCHECK(app_banner_manager);
  if (!data) {
    return;
  }
  if (!data->manifest().start_url.is_valid()) {
    return;
  }
  if (data->manifest().start_url == last_recorded_web_app_start_url_) {
    return;
  }

  UpdateUkmData(foreground_web_contents_, TabSwitching::kTo);
}

void WebAppMetrics::NotifyWebContentsDestroyed(WebContents* web_contents) {
  // Won't save us if we set a destroyed WebContents as foreground after this
  // point. Maybe we should keep a set of destroyed WebContents pointers?
  if (foreground_web_contents_ == web_contents)
    foreground_web_contents_ = nullptr;
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

  WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(profile_);

  num_user_installed_apps_ =
      provider->registrar_unsafe().CountUserInstalledApps();
  DCHECK_NE(kNumUserInstalledAppsNotCounted, num_user_installed_apps_);
  DCHECK_GE(num_user_installed_apps_, 0);
}

void WebAppMetrics::UpdateUkmData(WebContents* web_contents,
                                  TabSwitching mode) {
  // TODO(crbug.com/362130525): The discarded check can be removed once
  // TabStripModelChange::kReplaced has been removed.
  if (!web_contents || web_contents->WasDiscarded()) {
    return;
  }
  auto* app_banner_manager =
      webapps::AppBannerManager::FromWebContents(web_contents);
  // May be null in unit tests.
  if (!app_banner_manager)
    return;
  WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(profile_);
  // WebAppProvider may be removed after WebAppMetrics construction in tests.
  if (!provider)
    return;
  DailyInteraction features;

  webapps::InstallableWebAppCheckResult installable =
      app_banner_manager->GetInstallableWebAppCheckResult();
  std::optional<webapps::WebAppBannerData> banner_data =
      app_banner_manager->GetCurrentWebAppBannerData();

  WebAppTabHelper* tab_helper = WebAppTabHelper::FromWebContents(web_contents);
  std::optional<webapps::AppId> maybe_app_id = tab_helper->app_id();
  if (maybe_app_id &&
      provider->registrar_unsafe().IsInstallState(
          *maybe_app_id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                          proto::INSTALLED_WITH_OS_INTEGRATION})) {
    webapps::AppId app_id = maybe_app_id.value();
    // App is installed
    features.start_url = provider->registrar_unsafe().GetAppStartUrl(app_id);
    features.installed = true;
    auto install_source =
        provider->registrar_unsafe().GetLatestAppInstallSource(app_id);
    if (install_source)
      features.install_source = static_cast<int>(*install_source);
    DisplayMode display_mode =
        provider->registrar_unsafe().GetAppEffectiveDisplayMode(app_id);
    features.effective_display_mode = static_cast<int>(display_mode);
    features.captures_links = IsPreferredAppForSupportedLinks(app_id, profile_);
    features.promotable = !provider->registrar_unsafe().IsDiyApp(app_id);
    bool preinstalled_app =
        provider->registrar_unsafe().IsInstalledByDefaultManagement(app_id);
    // Record usage duration and session counts only for installed web apps that
    // are currently open in a window, and all preinstalled apps.
    if (tab_helper->is_in_app_window() || preinstalled_app ||
        mode == TabSwitching::kForegroundClosing) {
      base::Time now = base::Time::Now();
      if (app_last_interacted_time_.contains(app_id)) {
        base::TimeDelta delta = now - GetOrSetLastInteractedTimeForApp(app_id);
        if (delta < max_valid_session_delta_) {
          switch (mode) {
            case TabSwitching::kFrom:
            case TabSwitching::kForegroundClosing:
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

      if (mode == TabSwitching::kTo) {
        features.num_sessions = 1;
      }
    }
  } else if (banner_data &&
             installable ==
                 webapps::InstallableWebAppCheckResult::kYes_Promotable) {
    // App is not installed, but is promotable. Record a subset of features.
    features.start_url = banner_data->manifest().start_url;
    DCHECK(features.start_url.is_valid());
    features.installed = false;
    // TODO(dmurph): Consider display override here too.
    DisplayMode display_mode = banner_data->manifest().display;
    features.effective_display_mode = static_cast<int>(display_mode);
    features.promotable = true;
  } else {
    last_recorded_web_app_start_url_ = GURL();
    return;
  }
  last_recorded_web_app_start_url_ = features.start_url;

  FlushOldRecordsAndUpdate(features, profile_);
}

base::Time WebAppMetrics::GetOrSetLastInteractedTimeForApp(
    const webapps::AppId& app_id) {
  std::optional<base::Time>& maybe_time = app_last_interacted_time_[app_id];
  base::Time now = base::Time::Now();
  if (!maybe_time.has_value()) {
    maybe_time = now;
  }
  return maybe_time.value();
}
}  // namespace web_app
