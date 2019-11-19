// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_metrics.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/web_app_metrics_factory.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

namespace {

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
    bool from_install_button,
    bool in_window,
    SiteEngagementService::EngagementType engagement_type) {
  const std::string histogram_prefix = "WebApp.Engagement.UserInstalled";
  RecordTabOrWindowHistogram(histogram_prefix, in_window, engagement_type);

  // Record it into more specific buckets:
  RecordTabOrWindowHistogram(
      histogram_prefix + (from_install_button ? ".FromInstallButton"
                                              : ".FromCreateShortcutButton"),
      in_window, engagement_type);
}

}  // namespace

// static
WebAppMetrics* WebAppMetrics::Get(Profile* profile) {
  return WebAppMetricsFactory::GetForProfile(profile);
}

WebAppMetrics::WebAppMetrics(Profile* profile)
    : SiteEngagementObserver(SiteEngagementService::Get(profile)),
      profile_(profile) {
  WebAppProvider* provider = WebAppProvider::Get(profile_);
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

  // A presence of WebAppTabHelper with valid app_id indicates a web app.
  WebAppTabHelper* tab_helper = WebAppTabHelper::FromWebContents(web_contents);
  if (!tab_helper || tab_helper->app_id().empty())
    return;

  // No HostedAppBrowserController if app is running as a tab in common browser.
  const bool in_window = !!browser->app_controller();
  const bool from_install_button = tab_helper->IsFromInstallButton();
  const bool user_installed = tab_helper->IsUserInstalled();

  // Record all web apps:
  RecordTabOrWindowHistogram("WebApp.Engagement", in_window, engagement_type);

  if (user_installed) {
    RecordUserInstalledHistogram(from_install_button, in_window,
                                 engagement_type);
  } else {
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

  WebAppProvider* provider = WebAppProvider::Get(profile_);

  num_user_installed_apps_ = provider->registrar().CountUserInstalledApps();
  DCHECK_NE(kNumUserInstalledAppsNotCounted, num_user_installed_apps_);
  DCHECK_GE(num_user_installed_apps_, 0);
}

}  // namespace web_app
