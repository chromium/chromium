// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_metrics.h"

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/one_shot_event.h"
#include "base/power_monitor/power_monitor.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_metrics_factory.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/site_engagement/content/engagement_type.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

namespace {

bool g_disable_automatic_icon_health_checks_for_testing = false;

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
      icon_health_checks_(profile) {
  if (!g_disable_automatic_icon_health_checks_for_testing) {
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

WebAppMetrics::~WebAppMetrics() = default;

void WebAppMetrics::OnEngagementEvent(
    content::WebContents* web_contents,
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

}  // namespace web_app
