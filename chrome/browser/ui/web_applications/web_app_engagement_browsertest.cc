// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_metrics.h"

#include <bitset>

#include "base/metrics/statistics_recorder.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_metrics.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "url/gurl.h"

namespace {

void NavigateToURLAndWait(Browser* browser, const GURL& url) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver observer(
      web_contents, content::MessageLoopRunner::QuitMode::DEFERRED);
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);
  observer.WaitForNavigationFinished();
}

GURL GetUrlForSuffix(const std::string& prefix, int suffix) {
  return GURL(prefix + base::NumberToString(suffix) + ".com/");
}

// Must be zero-based as this will be stored in a bitset.
enum HistogramIndex {
  kHistogramInTab = 0,
  kHistogramInWindow,
  kHistogramDefaultInstalled_InTab,
  kHistogramDefaultInstalled_InWindow,
  kHistogramUserInstalled_InTab,
  kHistogramUserInstalled_InWindow,
  kHistogramUserInstalled_FromInstallButton_InTab,
  kHistogramUserInstalled_FromInstallButton_InWindow,
  kHistogramUserInstalled_FromCreateShortcutButton_InTab,
  kHistogramUserInstalled_FromCreateShortcutButton_InWindow,
  kHistogramMoreThanThreeUserInstalledApps,
  kHistogramUpToThreeUserInstalledApps,
  kHistogramNoUserInstalledApps,
  kHistogramMaxValue
};

// The order (indices) must match HistogramIndex enum above:
const char* kHistogramNames[] = {
    "WebApp.Engagement.InTab",
    "WebApp.Engagement.InWindow",
    "WebApp.Engagement.DefaultInstalled.InTab",
    "WebApp.Engagement.DefaultInstalled.InWindow",
    "WebApp.Engagement.UserInstalled.InTab",
    "WebApp.Engagement.UserInstalled.InWindow",
    "WebApp.Engagement.UserInstalled.FromInstallButton.InTab",
    "WebApp.Engagement.UserInstalled.FromInstallButton.InWindow",
    "WebApp.Engagement.UserInstalled.FromCreateShortcutButton.InTab",
    "WebApp.Engagement.UserInstalled.FromCreateShortcutButton.InWindow",
    "WebApp.Engagement.MoreThanThreeUserInstalledApps",
    "WebApp.Engagement.UpToThreeUserInstalledApps",
    "WebApp.Engagement.NoUserInstalledApps"};

const char* HistogramEnumIndexToStr(int histogram_index) {
  DCHECK_GE(histogram_index, 0);
  DCHECK_LT(histogram_index, kHistogramMaxValue);
  return kHistogramNames[histogram_index];
}

using Histograms = std::bitset<kHistogramMaxValue>;

void ExpectUniqueSamples(const base::HistogramTester& tester,
                         const Histograms& histograms_mask,
                         SiteEngagementService::EngagementType type,
                         base::HistogramBase::Count count) {
  for (int h = 0; h < kHistogramMaxValue; ++h) {
    if (histograms_mask[h]) {
      const char* histogram_name = HistogramEnumIndexToStr(h);
      tester.ExpectUniqueSample(histogram_name, type, count);
    }
  }
}

void ExpectBucketCounts(const base::HistogramTester& tester,
                        const Histograms& histograms_mask,
                        SiteEngagementService::EngagementType type,
                        base::HistogramBase::Count count) {
  for (int h = 0; h < kHistogramMaxValue; ++h) {
    if (histograms_mask[h]) {
      const char* histogram_name = HistogramEnumIndexToStr(h);
      tester.ExpectBucketCount(histogram_name, type, count);
    }
  }
}

void ExpectTotalCounts(const base::HistogramTester& tester,
                       const Histograms& histograms_mask,
                       base::HistogramBase::Count count) {
  for (int h = 0; h < kHistogramMaxValue; ++h) {
    if (histograms_mask[h]) {
      const char* histogram_name = HistogramEnumIndexToStr(h);
      tester.ExpectTotalCount(histogram_name, count);
    }
  }
}

void ExpectLaunchCounts(const base::HistogramTester& tester,
                        base::HistogramBase::Count windowLaunches,
                        base::HistogramBase::Count tabLaunches) {
  tester.ExpectBucketCount("Extensions.HostedAppLaunchContainer",
                           apps::mojom::LaunchContainer::kLaunchContainerWindow,
                           windowLaunches);
  tester.ExpectBucketCount("Extensions.HostedAppLaunchContainer",
                           apps::mojom::LaunchContainer::kLaunchContainerTab,
                           tabLaunches);
  tester.ExpectTotalCount("Extensions.HostedAppLaunchContainer",
                          windowLaunches + tabLaunches);

  if (tabLaunches > 0) {
    tester.ExpectUniqueSample("Extensions.AppTabLaunchType",
                              extensions::LAUNCH_TYPE_REGULAR, tabLaunches);
  } else {
    EXPECT_EQ(nullptr, base::StatisticsRecorder::FindHistogram(
                           "Extensions.AppTabLaunchType"));
  }

  tester.ExpectUniqueSample("Extensions.BookmarkAppLaunchSource",
                            apps::mojom::AppLaunchSource::kSourceTest,
                            windowLaunches + tabLaunches);

  tester.ExpectBucketCount("Extensions.BookmarkAppLaunchContainer",
                           apps::mojom::LaunchContainer::kLaunchContainerWindow,
                           windowLaunches);
  tester.ExpectBucketCount("Extensions.BookmarkAppLaunchContainer",
                           apps::mojom::LaunchContainer::kLaunchContainerTab,
                           tabLaunches);
  tester.ExpectTotalCount("Extensions.BookmarkAppLaunchContainer",
                          windowLaunches + tabLaunches);
}

}  // namespace

namespace web_app {

class WebAppEngagementBrowserTest : public WebAppControllerBrowserTestBase {
 public:
  WebAppEngagementBrowserTest() = default;
  ~WebAppEngagementBrowserTest() override = default;

  void TestEngagementEventWebAppLaunch(const base::HistogramTester& tester,
                                       const Histograms& histograms) {
    ExpectUniqueSamples(
        tester, histograms,
        SiteEngagementService::ENGAGEMENT_WEBAPP_SHORTCUT_LAUNCH, 1);
    ExpectTotalCounts(tester, ~histograms, 0);
  }

  // Test some other engagement events by directly calling into
  // SiteEngagementService.
  void TestEngagementEventsAfterLaunch(const Histograms& histograms,
                                       Browser* browser) {
    base::HistogramTester tester;

    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    SiteEngagementService* site_engagement_service =
        SiteEngagementService::Get(browser->profile());

    // Simulate 4 events of various types.
    site_engagement_service->HandleMediaPlaying(web_contents, false);
    site_engagement_service->HandleMediaPlaying(web_contents, true);
    site_engagement_service->HandleNavigation(web_contents,
                                              ui::PAGE_TRANSITION_TYPED);
    site_engagement_service->HandleUserInput(
        web_contents, SiteEngagementService::ENGAGEMENT_MOUSE);

    ExpectTotalCounts(tester, histograms, 4);
    ExpectTotalCounts(tester, ~histograms, 0);

    ExpectBucketCounts(tester, histograms,
                       SiteEngagementService::ENGAGEMENT_MEDIA_VISIBLE, 1);
    ExpectBucketCounts(tester, histograms,
                       SiteEngagementService::ENGAGEMENT_MEDIA_HIDDEN, 1);
    ExpectBucketCounts(tester, histograms,
                       SiteEngagementService::ENGAGEMENT_NAVIGATION, 1);
    ExpectBucketCounts(tester, histograms,
                       SiteEngagementService::ENGAGEMENT_MOUSE, 1);
  }

 protected:
  void CountUserInstalledApps() {
    WebAppMetrics* web_app_metrics = WebAppMetrics::Get(profile());
    web_app_metrics->CountUserInstalledAppsForTesting();
  }

  AppId InstallWebAppAndCountApps(
      std::unique_ptr<WebApplicationInfo> web_app_info) {
    AppId app_id = InstallWebApp(std::move(web_app_info));
    CountUserInstalledApps();
    return app_id;
  }

  void InstallDefaultAppAndCountApps(ExternalInstallOptions install_options) {
    result_code_ = InstallApp(browser()->profile(), std::move(install_options));
    CountUserInstalledApps();
  }

  base::Optional<InstallResultCode> result_code_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAppEngagementBrowserTest);
};

// TODO(crbug.com/1012171): Migrate all to WebAppEngagementBrowserTest.
class HostedAppEngagementBrowserTest : public WebAppEngagementBrowserTest {
 public:
  HostedAppEngagementBrowserTest() = default;
  ~HostedAppEngagementBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_P(WebAppEngagementBrowserTest, AppInWindow) {
  base::HistogramTester tester;

  const GURL example_url = GURL("http://example.org/");

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = example_url;
  web_app_info->scope = example_url;
  web_app_info->open_as_window = true;
  AppId app_id = InstallWebAppAndCountApps(std::move(web_app_info));

  Browser* app_browser = LaunchWebAppBrowser(app_id);
  NavigateToURLAndWait(app_browser, example_url);

  EXPECT_EQ(GetAppIdFromApplicationName(app_browser->app_name()), app_id);

  Histograms histograms;
  histograms[kHistogramInWindow] = true;
  histograms[kHistogramUserInstalled_InWindow] = true;
  histograms[kHistogramUserInstalled_FromInstallButton_InWindow] = true;
  histograms[kHistogramUpToThreeUserInstalledApps] = true;

  TestEngagementEventWebAppLaunch(tester, histograms);
  TestEngagementEventsAfterLaunch(histograms, app_browser);
  ExpectLaunchCounts(tester, /*windowLaunches=*/1, /*tabLaunches=*/0);
}

IN_PROC_BROWSER_TEST_P(HostedAppEngagementBrowserTest, AppInTab) {
  base::HistogramTester tester;

  const GURL example_url = GURL("http://example.org/");

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = example_url;
  web_app_info->scope = example_url;
  web_app_info->open_as_window = false;
  AppId app_id = InstallWebAppAndCountApps(std::move(web_app_info));

  Browser* browser = LaunchBrowserForWebAppInTab(app_id);
  EXPECT_FALSE(browser->app_controller());
  NavigateToURLAndWait(browser, example_url);

  Histograms histograms;
  histograms[kHistogramInTab] = true;
  histograms[kHistogramUserInstalled_InTab] = true;
  histograms[kHistogramUserInstalled_FromInstallButton_InTab] = true;
  histograms[kHistogramUpToThreeUserInstalledApps] = true;

  TestEngagementEventWebAppLaunch(tester, histograms);
  TestEngagementEventsAfterLaunch(histograms, browser);
  ExpectLaunchCounts(tester, /*windowLaunches=*/0, /*tabLaunches=*/1);
}

IN_PROC_BROWSER_TEST_P(HostedAppEngagementBrowserTest, AppWithoutScope) {
  base::HistogramTester tester;

  const GURL example_url = GURL("http://example.org/");

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = example_url;
  // If app has no scope then UrlHandlers::GetUrlHandlers are empty. Therefore,
  // the app is counted as installed via the Create Shortcut button.
  web_app_info->scope = GURL();
  web_app_info->open_as_window = true;
  AppId app_id = InstallWebAppAndCountApps(std::move(web_app_info));

  Browser* browser = LaunchWebAppBrowser(app_id);

  EXPECT_EQ(GetAppIdFromApplicationName(browser->app_name()), app_id);
  EXPECT_TRUE(browser->app_controller());
  NavigateToURLAndWait(browser, example_url);

  Histograms histograms;
  histograms[kHistogramInWindow] = true;
  histograms[kHistogramUserInstalled_InWindow] = true;
  histograms[kHistogramUserInstalled_FromCreateShortcutButton_InWindow] = true;
  histograms[kHistogramUpToThreeUserInstalledApps] = true;

  TestEngagementEventWebAppLaunch(tester, histograms);
  TestEngagementEventsAfterLaunch(histograms, browser);
  ExpectLaunchCounts(tester, /*windowLaunches=*/1, /*tabLaunches=*/0);
}

IN_PROC_BROWSER_TEST_P(WebAppEngagementBrowserTest, TwoApps) {
  base::HistogramTester tester;

  const GURL example_url1 = GURL("http://example.org/");
  const GURL example_url2 = GURL("http://example.com/");

  AppId app_id1, app_id2;

  // Install two apps.
  {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = example_url1;
    web_app_info->scope = example_url1;
    app_id1 = InstallWebAppAndCountApps(std::move(web_app_info));
  }
  {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = example_url2;
    web_app_info->scope = example_url2;
    app_id2 = InstallWebAppAndCountApps(std::move(web_app_info));
  }

  // Launch them three times. This ensures that each launch only logs once.
  // (Since all apps receive the notification on launch, there is a danger that
  // we might log too many times.)
  Browser* app_browser1 = LaunchWebAppBrowser(app_id1);
  Browser* app_browser2 = LaunchWebAppBrowser(app_id1);
  Browser* app_browser3 = LaunchWebAppBrowser(app_id2);

  EXPECT_EQ(GetAppIdFromApplicationName(app_browser1->app_name()), app_id1);
  EXPECT_EQ(GetAppIdFromApplicationName(app_browser2->app_name()), app_id1);
  EXPECT_EQ(GetAppIdFromApplicationName(app_browser3->app_name()), app_id2);

  Histograms histograms;
  histograms[kHistogramInWindow] = true;
  histograms[kHistogramUserInstalled_InWindow] = true;
  histograms[kHistogramUserInstalled_FromInstallButton_InWindow] = true;
  histograms[kHistogramUpToThreeUserInstalledApps] = true;

  ExpectUniqueSamples(tester, histograms,
                      SiteEngagementService::ENGAGEMENT_WEBAPP_SHORTCUT_LAUNCH,
                      3);
  ExpectTotalCounts(tester, ~histograms, 0);
  ExpectLaunchCounts(tester, /*windowLaunches=*/3, /*tabLaunches=*/0);
}

IN_PROC_BROWSER_TEST_P(WebAppEngagementBrowserTest, ManyUserApps) {
  base::HistogramTester tester;

  // More than 3 user-installed apps:
  const int num_user_apps = 4;

  std::vector<AppId> app_ids;

  // Install apps.
  const std::string base_url = "http://example";
  for (int i = 0; i < num_user_apps; ++i) {
    const GURL url = GetUrlForSuffix(base_url, i);

    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = url;
    web_app_info->scope = url;
    AppId app_id = InstallWebAppAndCountApps(std::move(web_app_info));
    app_ids.push_back(app_id);
  }

  // Launch apps in windows.
  for (int i = 0; i < num_user_apps; ++i) {
    Browser* browser = LaunchWebAppBrowser(app_ids[i]);

    const GURL url = GetUrlForSuffix(base_url, i);
    NavigateToURLAndWait(browser, url);
  }

  Histograms histograms;
  histograms[kHistogramInWindow] = true;
  histograms[kHistogramUserInstalled_InWindow] = true;
  histograms[kHistogramUserInstalled_FromInstallButton_InWindow] = true;
  histograms[kHistogramMoreThanThreeUserInstalledApps] = true;

  ExpectUniqueSamples(tester, histograms,
                      SiteEngagementService::ENGAGEMENT_WEBAPP_SHORTCUT_LAUNCH,
                      4);
  ExpectTotalCounts(tester, ~histograms, 0);
  ExpectLaunchCounts(tester, /*windowLaunches=*/4, /*tabLaunches=*/0);
}

IN_PROC_BROWSER_TEST_P(HostedAppEngagementBrowserTest, DefaultApp) {
  base::HistogramTester tester;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL example_url(
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
  InstallDefaultAppAndCountApps(CreateInstallOptions(example_url));
  ASSERT_EQ(InstallResultCode::kSuccessNewInstall, result_code_.value());

  base::Optional<AppId> app_id = FindAppWithUrlInScope(example_url);
  ASSERT_TRUE(app_id);
  // TODO(ericwilligers): Assert app_id was installed by default.

  Browser* browser = LaunchWebAppBrowser(*app_id);
  NavigateToURLAndWait(browser, example_url);

  Histograms histograms;
  histograms[kHistogramInWindow] = true;
  histograms[kHistogramDefaultInstalled_InWindow] = true;
  histograms[kHistogramNoUserInstalledApps] = true;

  TestEngagementEventWebAppLaunch(tester, histograms);
  TestEngagementEventsAfterLaunch(histograms, browser);
  ExpectLaunchCounts(tester, /*windowLaunches=*/1, /*tabLaunches=*/0);
}

IN_PROC_BROWSER_TEST_P(HostedAppEngagementBrowserTest, NavigateAwayFromAppTab) {
  base::HistogramTester tester;
  const GURL app_url = GURL("http://example.org/app/");
  const GURL outer_url = GURL("http://example.org/");

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = app_url;
  web_app_info->scope = app_url;
  web_app_info->open_as_window = false;
  AppId app_id = InstallWebAppAndCountApps(std::move(web_app_info));

  Browser* browser = LaunchBrowserForWebAppInTab(app_id);
  EXPECT_FALSE(browser->app_controller());

  NavigateToURLAndWait(browser, app_url);
  {
    Histograms histograms;
    histograms[kHistogramInTab] = true;
    histograms[kHistogramUserInstalled_InTab] = true;
    histograms[kHistogramUserInstalled_FromInstallButton_InTab] = true;
    histograms[kHistogramUpToThreeUserInstalledApps] = true;
    TestEngagementEventsAfterLaunch(histograms, browser);
  }

  // Navigate away from the web app to an outer simple web site:
  NavigateToURLAndWait(browser, outer_url);
  {
    Histograms histograms;
    histograms[kHistogramUpToThreeUserInstalledApps] = true;
    TestEngagementEventsAfterLaunch(histograms, browser);
  }
  ExpectLaunchCounts(tester, /*windowLaunches=*/0, /*tabLaunches=*/1);
}

IN_PROC_BROWSER_TEST_P(WebAppEngagementBrowserTest, RecordedForNonApps) {
  base::HistogramTester tester;
  CountUserInstalledApps();

  // Launch a non-app tab in default browser.
  const GURL example_url = GURL("http://example.org/");
  NavigateToURLAndWait(browser(), example_url);

  // Check that no histograms recorded, e.g. no
  // SiteEngagementService::ENGAGEMENT_WEBAPP_SHORTCUT_LAUNCH.
  Histograms histograms;
  ExpectTotalCounts(tester, ~histograms, 0);

  // The engagement broken down by the number of apps installed must be recorded
  // for all engagement events.
  histograms[kHistogramNoUserInstalledApps] = true;
  TestEngagementEventsAfterLaunch(histograms, browser());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WebAppEngagementBrowserTest,
    ::testing::Values(ControllerType::kHostedAppController,
                      ControllerType::kUnifiedControllerWithBookmarkApp,
                      ControllerType::kUnifiedControllerWithWebApp),
    ControllerTypeParamToString);

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    HostedAppEngagementBrowserTest,
    ::testing::Values(ControllerType::kHostedAppController,
                      ControllerType::kUnifiedControllerWithBookmarkApp),
    ControllerTypeParamToString);

}  // namespace web_app
