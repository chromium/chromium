// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <bitset>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_metrics.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/site_engagement/content/engagement_type.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

GURL GetUrlForSuffix(const std::string& prefix, int suffix) {
  return GURL(prefix + base::NumberToString(suffix + 1) + ".html");
}

// Must be zero-based as this will be stored in a bitset.
enum HistogramIndex {
  kHistogramInTab = 0,
  kHistogramInWindow,
  kHistogramDefaultInstalled_InTab,
  kHistogramDefaultInstalled_InWindow,
  kHistogramUserInstalled_InTab,
  kHistogramUserInstalled_InWindow,
  kHistogramUserInstalledDiy_InTab,
  kHistogramUserInstalledDiy_InWindow,
  kHistogramUserInstalledCrafted_InTab,
  kHistogramUserInstalledCrafted_InWindow,
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
    "WebApp.Engagement.UserInstalled.Diy.InTab",
    "WebApp.Engagement.UserInstalled.Diy.InWindow",
    "WebApp.Engagement.UserInstalled.Crafted.InTab",
    "WebApp.Engagement.UserInstalled.Crafted.InWindow",
    "WebApp.Engagement.MoreThanThreeUserInstalledApps",
    "WebApp.Engagement.UpToThreeUserInstalledApps",
    "WebApp.Engagement.NoUserInstalledApps"};

const char* HistogramEnumIndexToStr(int histogram_index) {
  DCHECK_GE(histogram_index, 0);
  DCHECK_LT(histogram_index, kHistogramMaxValue);
  return kHistogramNames[histogram_index];
}

using Histograms = std::bitset<kHistogramMaxValue>;

void ExpectBucketCounts(const base::HistogramTester& tester,
                        const Histograms& histograms_mask,
                        site_engagement::EngagementType type,
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
  tester.ExpectBucketCount("WebApp.LaunchContainer",
                           apps::LaunchContainer::kLaunchContainerWindow,
                           windowLaunches);
  tester.ExpectBucketCount("WebApp.LaunchContainer",
                           apps::LaunchContainer::kLaunchContainerTab,
                           tabLaunches);
  tester.ExpectTotalCount("WebApp.LaunchContainer",
                          windowLaunches + tabLaunches);

  tester.ExpectUniqueSample("WebApp.LaunchSource",
                            apps::LaunchSource::kFromTest,
                            windowLaunches + tabLaunches);
}

}  // namespace

namespace web_app {

class WebAppEngagementBrowserTest : public WebAppBrowserTestBase {
 public:
  WebAppEngagementBrowserTest() = default;
  WebAppEngagementBrowserTest(const WebAppEngagementBrowserTest&) = delete;
  WebAppEngagementBrowserTest& operator=(const WebAppEngagementBrowserTest&) =
      delete;
  ~WebAppEngagementBrowserTest() override = default;

  // Test some other engagement events by directly calling into
  // SiteEngagementService.
  void TestEngagementEventsAfterLaunch(const Histograms& histograms,
                                       Browser* browser) {
    base::HistogramTester tester;

    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    auto* site_engagement_service =
        site_engagement::SiteEngagementService::Get(browser->profile());

    // Simulate 4 events of various types.
    site_engagement_service->HandleMediaPlaying(web_contents, false);
    site_engagement_service->HandleMediaPlaying(web_contents, true);
    site_engagement_service->HandleNavigation(web_contents,
                                              ui::PAGE_TRANSITION_TYPED);
    site_engagement_service->HandleUserInput(
        web_contents, site_engagement::EngagementType::kMouse);

    ExpectTotalCounts(tester, histograms, 4);
    ExpectTotalCounts(tester, ~histograms, 0);

    ExpectBucketCounts(tester, histograms,
                       site_engagement::EngagementType::kMediaVisible, 1);
    ExpectBucketCounts(tester, histograms,
                       site_engagement::EngagementType::kMediaHidden, 1);
    ExpectBucketCounts(tester, histograms,
                       site_engagement::EngagementType::kNavigation, 1);
    ExpectBucketCounts(tester, histograms,
                       site_engagement::EngagementType::kMouse, 1);
  }

 protected:
  void CountUserInstalledApps() {
    WebAppMetrics* web_app_metrics = WebAppMetrics::Get(profile());
    web_app_metrics->CountUserInstalledAppsForTesting();
  }

  webapps::AppId InstallWebAppAndCountApps(
      std::unique_ptr<WebAppInstallInfo> web_app_info) {
    webapps::AppId app_id = InstallWebApp(std::move(web_app_info));
    CountUserInstalledApps();
    return app_id;
  }

  void InstallDefaultAppAndCountApps(ExternalInstallOptions install_options) {
    ExternallyManagedAppManager::InstallResult result =
        ExternallyManagedAppManagerInstall(browser()->profile(),
                                           std::move(install_options));
    result_code_ = result.code;
    CountUserInstalledApps();
  }

  std::optional<webapps::InstallResultCode> result_code_;
};

IN_PROC_BROWSER_TEST_F(WebAppEngagementBrowserTest, AppInWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester tester;

  GURL example_url(
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
  auto web_app_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(example_url);
  web_app_info->scope = example_url;
  web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  webapps::AppId app_id = InstallWebAppAndCountApps(std::move(web_app_info));

  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id);
  NavigateViaLinkClickToURLAndWait(app_browser, example_url);

  EXPECT_EQ(GetAppIdFromApplicationName(app_browser->app_name()), app_id);

  Histograms histograms;
  histograms[kHistogramInWindow] = true;
  histograms[kHistogramUserInstalled_InWindow] = true;
  histograms[kHistogramUserInstalledCrafted_InWindow] = true;
  histograms[kHistogramUpToThreeUserInstalledApps] = true;

  ExpectBucketCounts(tester, histograms,
                     site_engagement::EngagementType::kWebappShortcutLaunch, 1);
  ExpectBucketCounts(tester, histograms,
                     site_engagement::EngagementType::kNavigation, 1);
  ExpectTotalCounts(tester, ~histograms, 0);

  TestEngagementEventsAfterLaunch(histograms, app_browser);
  ExpectLaunchCounts(tester, /*windowLaunches=*/1, /*tabLaunches=*/0);
}

IN_PROC_BROWSER_TEST_F(WebAppEngagementBrowserTest, DiyAppInWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester tester;

  GURL example_url(
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
  auto web_app_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(example_url);
  web_app_info->scope = example_url;
  web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  web_app_info->is_diy_app = true;
  webapps::AppId app_id = InstallWebAppAndCountApps(std::move(web_app_info));

  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id);
  NavigateViaLinkClickToURLAndWait(app_browser, example_url);

  EXPECT_EQ(GetAppIdFromApplicationName(app_browser->app_name()), app_id);

  Histograms histograms;
  histograms[kHistogramInWindow] = true;
  histograms[kHistogramUserInstalled_InWindow] = true;
  histograms[kHistogramUserInstalledDiy_InWindow] = true;
  histograms[kHistogramUpToThreeUserInstalledApps] = true;

  ExpectBucketCounts(tester, histograms,
                     site_engagement::EngagementType::kWebappShortcutLaunch, 1);
  ExpectBucketCounts(tester, histograms,
                     site_engagement::EngagementType::kNavigation, 1);
  ExpectTotalCounts(tester, ~histograms, 0);

  TestEngagementEventsAfterLaunch(histograms, app_browser);
  ExpectLaunchCounts(tester, /*windowLaunches=*/1, /*tabLaunches=*/0);
}

IN_PROC_BROWSER_TEST_F(WebAppEngagementBrowserTest, AppInTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester tester;

  GURL example_url(
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));

  auto web_app_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(example_url);
  web_app_info->scope = example_url;
  web_app_info->user_display_mode = mojom::UserDisplayMode::kBrowser;
  webapps::AppId app_id = InstallWebAppAndCountApps(std::move(web_app_info));

  Browser* browser = LaunchBrowserForWebAppInTab(app_id);
  EXPECT_FALSE(browser->app_controller());
  NavigateViaLinkClickToURLAndWait(browser, example_url);

  Histograms histograms;
  histograms[kHistogramInTab] = true;
  histograms[kHistogramUserInstalled_InTab] = true;
  histograms[kHistogramUserInstalledCrafted_InTab] = true;
  histograms[kHistogramUpToThreeUserInstalledApps] = true;

  // Note: We explicitly do NOT expect engagement to be recorded in kNavigation.
  // This is because the open-in-tab behavior only records the launch, and
  // treats the navigation as a 'link' navigation, so it is not considered by
  // the engagement service as a navigation. See the `IsEngagementNavigation`
  // method.
  ExpectBucketCounts(tester, histograms,
                     site_engagement::EngagementType::kWebappShortcutLaunch, 1);
  ExpectTotalCounts(tester, ~histograms, 0);
  TestEngagementEventsAfterLaunch(histograms, browser);
  ExpectLaunchCounts(tester, /*windowLaunches=*/0, /*tabLaunches=*/1);
}

IN_PROC_BROWSER_TEST_F(WebAppEngagementBrowserTest, DiyAppInTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester tester;

  GURL example_url(
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));

  auto web_app_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(example_url);
  web_app_info->scope = example_url;
  web_app_info->user_display_mode = mojom::UserDisplayMode::kBrowser;
  web_app_info->is_diy_app = true;
  webapps::AppId app_id = InstallWebAppAndCountApps(std::move(web_app_info));

  Browser* browser = LaunchBrowserForWebAppInTab(app_id);
  EXPECT_FALSE(browser->app_controller());
  NavigateViaLinkClickToURLAndWait(browser, example_url);

  Histograms histograms;
  histograms[kHistogramInTab] = true;
  histograms[kHistogramUserInstalled_InTab] = true;
  histograms[kHistogramUserInstalledDiy_InTab] = true;
  histograms[kHistogramUpToThreeUserInstalledApps] = true;

  // Note: We explicitly do NOT expect engagement to be recorded in kNavigation.
  // This is because the open-in-tab behavior only records the launch, and
  // treats the navigation as a 'link' navigation, so it is not considered by
  // the engagement service as a navigation. See the `IsEngagementNavigation`
  // method.
  ExpectBucketCounts(tester, histograms,
                     site_engagement::EngagementType::kWebappShortcutLaunch, 1);
  ExpectTotalCounts(tester, ~histograms, 0);
  TestEngagementEventsAfterLaunch(histograms, browser);
  ExpectLaunchCounts(tester, /*windowLaunches=*/0, /*tabLaunches=*/1);
}

IN_PROC_BROWSER_TEST_F(WebAppEngagementBrowserTest, AppWithoutScope) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester tester;

  GURL example_url(
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));

  auto web_app_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(example_url);
  // If app has no scope then UrlHandlers::GetUrlHandlers are empty. Therefore,
  // the app is counted as installed via the Create Shortcut button.
  web_app_info->scope = GURL();
  web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  webapps::AppId app_id = InstallWebAppAndCountApps(std::move(web_app_info));

  Browser* browser = LaunchWebAppBrowserAndWait(app_id);

  EXPECT_EQ(GetAppIdFromApplicationName(browser->app_name()), app_id);
  EXPECT_TRUE(browser->app_controller());
  NavigateViaLinkClickToURLAndWait(browser, example_url);

  Histograms histograms;
  histograms[kHistogramInWindow] = true;
  histograms[kHistogramUserInstalled_InWindow] = true;
  histograms[kHistogramUserInstalledCrafted_InWindow] = true;
  histograms[kHistogramUpToThreeUserInstalledApps] = true;

  ExpectBucketCounts(tester, histograms,
                     site_engagement::EngagementType::kWebappShortcutLaunch, 1);
  ExpectBucketCounts(tester, histograms,
                     site_engagement::EngagementType::kNavigation, 1);
  ExpectTotalCounts(tester, ~histograms, 0);
  TestEngagementEventsAfterLaunch(histograms, browser);
  ExpectLaunchCounts(tester, /*windowLaunches=*/1, /*tabLaunches=*/0);
}

IN_PROC_BROWSER_TEST_F(WebAppEngagementBrowserTest, TwoApps) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester tester;

  const GURL example_url1 =
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html");
  const GURL example_url2 =
      embedded_test_server()->GetURL("/banners/scope_a/page_1.html");

  webapps::AppId app_id1, app_id2;

  // Install two apps.
  {
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(example_url1);
    web_app_info->scope = example_url1;
    app_id1 = InstallWebAppAndCountApps(std::move(web_app_info));
  }
  {
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(example_url2);
    web_app_info->scope = example_url2;
    app_id2 = InstallWebAppAndCountApps(std::move(web_app_info));
  }

  // Launch them three times. This ensures that each launch only logs once.
  // (Since all apps receive the notification on launch, there is a danger that
  // we might log too many times.)
  Browser* app_browser1 = LaunchWebAppBrowserAndWait(app_id1);
  Browser* app_browser2 = LaunchWebAppBrowserAndWait(app_id1);
  Browser* app_browser3 = LaunchWebAppBrowserAndWait(app_id2);

  EXPECT_EQ(GetAppIdFromApplicationName(app_browser1->app_name()), app_id1);
  EXPECT_EQ(GetAppIdFromApplicationName(app_browser2->app_name()), app_id1);
  EXPECT_EQ(GetAppIdFromApplicationName(app_browser3->app_name()), app_id2);

  Histograms histograms;
  histograms[kHistogramInWindow] = true;
  histograms[kHistogramUserInstalled_InWindow] = true;
  histograms[kHistogramUserInstalledCrafted_InWindow] = true;
  histograms[kHistogramUpToThreeUserInstalledApps] = true;

  ExpectBucketCounts(tester, histograms,
                     site_engagement::EngagementType::kWebappShortcutLaunch, 3);
  ExpectBucketCounts(tester, histograms,
                     site_engagement::EngagementType::kNavigation, 3);
  ExpectTotalCounts(tester, ~histograms, 0);
  ExpectLaunchCounts(tester, /*windowLaunches=*/3, /*tabLaunches=*/0);
}

IN_PROC_BROWSER_TEST_F(WebAppEngagementBrowserTest, ManyUserApps) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester tester;

  // More than 3 user-installed apps:
  const int num_user_apps = 4;

  // A small number of launches, to avoid timeouts.
  const int num_launches = 2;

  std::vector<webapps::AppId> app_ids;

  // Install apps.
  const std::string base_url =
      embedded_test_server()->GetURL("/banners/many_apps/app").spec();
  for (int i = 0; i < num_user_apps; ++i) {
    const GURL url = GetUrlForSuffix(base_url, i);

    auto web_app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(url);
    web_app_info->scope = url;
    webapps::AppId app_id = InstallWebAppAndCountApps(std::move(web_app_info));
    app_ids.push_back(app_id);
  }

  // Launch an app in a window.
  DCHECK_LE(num_launches, num_user_apps);
  for (int i = 0; i < num_launches; ++i) {
    Browser* browser = LaunchWebAppBrowserAndWait(app_ids[i]);

    const GURL url = GetUrlForSuffix(base_url, i);
    NavigateViaLinkClickToURLAndWait(browser, url);
  }

  Histograms histograms;
  histograms[kHistogramInWindow] = true;
  histograms[kHistogramUserInstalled_InWindow] = true;
  histograms[kHistogramUserInstalledCrafted_InWindow] = true;
  histograms[kHistogramMoreThanThreeUserInstalledApps] = true;

  ExpectBucketCounts(tester, histograms,
                     site_engagement::EngagementType::kWebappShortcutLaunch,
                     num_launches);
  ExpectBucketCounts(tester, histograms,
                     site_engagement::EngagementType::kNavigation,
                     num_launches);
  ExpectTotalCounts(tester, ~histograms, 0);
  ExpectLaunchCounts(tester, /*windowLaunches=*/num_launches,
                     /*tabLaunches=*/0);
}

// TODO(crbug.com/40884336): Flaky on Mac.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DefaultApp DISABLED_DefaultApp
#else
#define MAYBE_DefaultApp DefaultApp
#endif
IN_PROC_BROWSER_TEST_F(WebAppEngagementBrowserTest, MAYBE_DefaultApp) {
  base::HistogramTester tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL example_url(
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
  InstallDefaultAppAndCountApps(CreateInstallOptions(example_url));
  ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());

  std::optional<webapps::AppId> app_id = FindAppWithUrlInScope(example_url);
  ASSERT_TRUE(app_id);
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalledByDefaultManagement(
      app_id.value()));

  Browser* browser = LaunchWebAppBrowserAndWait(*app_id);
  NavigateViaLinkClickToURLAndWait(browser, example_url);

  Histograms histograms;
  histograms[kHistogramInWindow] = true;
  histograms[kHistogramDefaultInstalled_InWindow] = true;
  histograms[kHistogramNoUserInstalledApps] = true;

  ExpectBucketCounts(tester, histograms,
                     site_engagement::EngagementType::kWebappShortcutLaunch, 1);
  ExpectBucketCounts(tester, histograms,
                     site_engagement::EngagementType::kNavigation, 1);
  ExpectTotalCounts(tester, ~histograms, 0);
  NavigateViaLinkClickToURLAndWait(browser, example_url);
  TestEngagementEventsAfterLaunch(histograms, browser);
  ExpectLaunchCounts(tester, /*windowLaunches=*/1, /*tabLaunches=*/0);
}

IN_PROC_BROWSER_TEST_F(WebAppEngagementBrowserTest, NavigateAwayFromAppTab) {
  base::HistogramTester tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL start_url =
      embedded_test_server()->GetURL("/banners/scope_a/page_1.html");
  const GURL outer_url =
      embedded_test_server()->GetURL("/banners/manifest_test_page.html");

  auto web_app_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  web_app_info->scope = start_url;
  web_app_info->user_display_mode = mojom::UserDisplayMode::kBrowser;
  webapps::AppId app_id = InstallWebAppAndCountApps(std::move(web_app_info));

  Browser* browser = LaunchBrowserForWebAppInTab(app_id);
  EXPECT_FALSE(browser->app_controller());

  NavigateViaLinkClickToURLAndWait(browser, start_url);
  {
    Histograms histograms;
    histograms[kHistogramInTab] = true;
    histograms[kHistogramUserInstalled_InTab] = true;
    histograms[kHistogramUserInstalledCrafted_InTab] = true;
    histograms[kHistogramUpToThreeUserInstalledApps] = true;
    TestEngagementEventsAfterLaunch(histograms, browser);
  }

  // Navigate away from the web app to an outer simple web site:
  NavigateViaLinkClickToURLAndWait(browser, outer_url);
  {
    Histograms histograms;
    histograms[kHistogramUpToThreeUserInstalledApps] = true;
    TestEngagementEventsAfterLaunch(histograms, browser);
  }
  ExpectLaunchCounts(tester, /*windowLaunches=*/0, /*tabLaunches=*/1);
}

IN_PROC_BROWSER_TEST_F(WebAppEngagementBrowserTest, RecordedForNonApps) {
  base::HistogramTester tester;
  ASSERT_TRUE(embedded_test_server()->Start());
  CountUserInstalledApps();

  // Launch a non-app tab in default browser.
  const GURL example_url =
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html");
  NavigateViaLinkClickToURLAndWait(browser(), example_url);

  // Check that no histograms recorded, e.g. no
  // SiteEngagementService::ENGAGEMENT_WEBAPP_SHORTCUT_LAUNCH.
  Histograms histograms;
  ExpectTotalCounts(tester, ~histograms, 0);

  // The engagement broken down by the number of apps installed must be recorded
  // for all engagement events.
  histograms[kHistogramNoUserInstalledApps] = true;
  TestEngagementEventsAfterLaunch(histograms, browser());
}

// On Chrome OS, PWAs are launched via the app service rather than via command
// line flags.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(WebAppEngagementBrowserTest, CommandLineWindowByUrl) {
  base::HistogramTester tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  // There should be one browser to start with.
  unsigned int expected_browsers = 1;
  const int expected_tabs = 1;
  EXPECT_EQ(expected_browsers, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(expected_tabs, browser()->tab_strip_model()->count());

  const GURL example_url(
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));

  auto result = ExternallyManagedAppManagerInstall(
      browser()->profile(), CreateInstallOptions(example_url));
  ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  std::optional<webapps::AppId> app_id = FindAppWithUrlInScope(example_url);
  ASSERT_TRUE(app_id);
  content::CreateAndLoadWebContentsObserver app_loaded_observer;

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kApp, example_url.spec());

  // The app should open as a window.
  EXPECT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));
  app_loaded_observer.Wait();

  Browser* const app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(app_browser->is_type_app());

#if BUILDFLAG(IS_WIN)
  {
    // From c/b/ui/startup/launch_mode_recorder.h:
    constexpr char kLaunchModesHistogram[] = "Launch.Mode2";
    const base::HistogramBase::Sample kWebAppOther = 22;

    tester.ExpectUniqueSample(kLaunchModesHistogram, kWebAppOther, 1);
  }
#endif  // BUILDFLAG(IS_WIN

  // Check that the number of browsers and tabs is correct.
  expected_browsers++;

  EXPECT_EQ(expected_browsers, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(expected_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(expected_tabs, app_browser->tab_strip_model()->count());
}

// TODO(crbug.com/40877225): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CommandLineWindowByAppId DISABLED_CommandLineWindowByAppId
#else
#define MAYBE_CommandLineWindowByAppId CommandLineWindowByAppId
#endif
IN_PROC_BROWSER_TEST_F(WebAppEngagementBrowserTest,
                       MAYBE_CommandLineWindowByAppId) {
  base::HistogramTester tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  // There should be one browser to start with.
  unsigned int expected_browsers = 1;
  const int expected_tabs = 1;
  EXPECT_EQ(expected_browsers, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(expected_tabs, browser()->tab_strip_model()->count());

  const GURL example_url(
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));

  auto result = ExternallyManagedAppManagerInstall(
      browser()->profile(), CreateInstallOptions(example_url));
  ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  std::optional<webapps::AppId> app_id = FindAppWithUrlInScope(example_url);
  ASSERT_TRUE(app_id);
  content::CreateAndLoadWebContentsObserver app_loaded_observer;

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, *app_id);

  // The app should open as a window.
  EXPECT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));
  app_loaded_observer.Wait();

  Browser* const app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_EQ(app_browser->type(), Browser::TYPE_APP);
  EXPECT_TRUE(app_browser->app_controller());
  EXPECT_TRUE(AppBrowserController::IsWebApp(app_browser));

#if BUILDFLAG(IS_WIN)
  {
    // From c/b/ui/startup/launch_mode_recorder.h:
    constexpr char kLaunchModesHistogram[] = "Launch.Mode2";
    const base::HistogramBase::Sample kWebAppOther = 22;

    tester.ExpectUniqueSample(kLaunchModesHistogram, kWebAppOther, 1);
  }
#endif  // BUILDFLAG(IS_WIN)

  // Check that the number of browsers and tabs is correct.
  expected_browsers++;

  EXPECT_EQ(expected_browsers, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(expected_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(expected_tabs, app_browser->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(WebAppEngagementBrowserTest, CommandLineTab) {
  base::HistogramTester tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  // There should be one browser to start with.
  const unsigned int expected_browsers = 1;
  int expected_tabs = 1;
  EXPECT_EQ(expected_browsers, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(expected_tabs, browser()->tab_strip_model()->count());

  const GURL example_url(
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));

  ExternalInstallOptions install_options = CreateInstallOptions(example_url);
  install_options.user_display_mode = mojom::UserDisplayMode::kBrowser;
  auto result =
      ExternallyManagedAppManagerInstall(browser()->profile(), install_options);
  ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  std::optional<webapps::AppId> app_id = FindAppWithUrlInScope(example_url);
  ASSERT_TRUE(app_id);
  content::CreateAndLoadWebContentsObserver app_loaded_observer;

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, *app_id);

  // The app should open as a tab.
  EXPECT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));
  app_loaded_observer.Wait();

#if BUILDFLAG(IS_WIN)
  {
    // From startup_browser_creator_impl.cc:
    constexpr char kLaunchModesHistogram[] = "Launch.Mode2";
    const base::HistogramBase::Sample kWebAppOther = 22;

    tester.ExpectUniqueSample(kLaunchModesHistogram, kWebAppOther, 1);
  }
#endif  // BUILDFLAG(IS_WIN)

  // Check that the number of browsers and tabs is correct.
  expected_tabs++;

  EXPECT_EQ(expected_browsers, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(expected_tabs, browser()->tab_strip_model()->count());
}
#endif

}  // namespace web_app
