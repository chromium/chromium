// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/auto_reset.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/apps/link_capturing/chromeos_link_capturing_delegate.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kFilePath[] = "xyz";

constexpr char kStartTime[] = "21 Jan 2022 10:00:00 GMT";

}  // namespace

// Summary of expected behavior on ChromeOS:
// _____________________________|_SWA_launches_|_URL_redirection
// screencast.apps.chrome       | Yes          | Yes
// projector.apps.chrome        | Yes          | Yes (online only)
// chrome://projector/app/      | Yes          | No
// chrome-untrusted://projector | No           | No

class ProjectorNavigationThrottleTest : public InProcessBrowserTest {
 public:
  ProjectorNavigationThrottleTest() = default;

  ~ProjectorNavigationThrottleTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SystemWebAppManager::GetForTest(profile())->InstallSystemAppsForTesting();

    base::Time start_time;
    ASSERT_TRUE(base::Time::FromUTCString(kStartTime, &start_time));
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    base::TimeDelta forward_by = start_time - task_runner_->Now();
    EXPECT_LT(base::TimeDelta(), forward_by);
    task_runner_->AdvanceMockTickClock(forward_by);
    clock_reset_ = std::make_unique<base::AutoReset<const base::TickClock*>>(
        apps::ChromeOsLinkCapturingDelegate::SetClockForTesting(
            task_runner_->GetMockTickClock()));
  }

 protected:
  Profile* profile() { return browser()->profile(); }
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

 private:
  std::unique_ptr<base::AutoReset<const base::TickClock*>> clock_reset_;
  base::OnceClosure on_browser_removed_callback_;
};

class ProjectorNavigationThrottleTestParameterized
    : public ProjectorNavigationThrottleTest,
      public testing::WithParamInterface<
          ::testing::tuple<bool, ::std::string>> {
 protected:
  bool navigate_from_link() const { return std::get<0>(GetParam()); }
  std::string url_params() const { return std::get<1>(GetParam()); }
};

// Verifies that navigating to
// https://screencast.apps.chrome/xyz?resourceKey=abc redirects to
// chrome://projector/app/xyz?timestamp=[timestamp]&resourceKey=abc and launches
// the SWA.
// TODO(crbug.com/327056386): Re-enable on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_PwaNavigationRedirects DISABLED_PwaNavigationRedirects
#else
#define MAYBE_PwaNavigationRedirects PwaNavigationRedirects
#endif
IN_PROC_BROWSER_TEST_P(ProjectorNavigationThrottleTestParameterized,
                       MAYBE_PwaNavigationRedirects) {
  base::HistogramTester histogram_tester;

  std::string url = kChromeUIUntrustedProjectorPwaUrl;
  url += "/";
  url += kFilePath;
  if (!url_params().empty())
    url += "?" + url_params();
  GURL gurl(url);

  // Prior to navigation, there is only one browser available.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);

  // We have to listen for both the browser being removed AND the new browser
  // being added.
  ui_test_utils::BrowserChangeObserver removed_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kRemoved);
  ui_test_utils::BrowserChangeObserver added_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  if (navigate_from_link()) {
    // Simulate the user clicking a link.
    NavigateParams params(browser(), gurl,
                          ui::PageTransition::PAGE_TRANSITION_LINK);
    Navigate(&params);
  } else {
    // Simulate the user typing the url into the omnibox.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), gurl, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BrowserTestWaitFlags::BROWSER_TEST_WAIT_FOR_BROWSER);
  }
  removed_observer.Wait();
  added_observer.Wait();

  // During the navigation, we closed the previous browser to prevent dangling
  // about:blank pages and opened a new app browser for the Projector SWA.
  // There is still only one browser available.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  // Select the first available browser, which should be the SWA.
  SelectFirstBrowser();
  Browser* app_browser =
      FindSystemWebAppBrowser(profile(), SystemWebAppType::PROJECTOR);
  // Projector SWA is now open.
  ASSERT_TRUE(app_browser);
  content::WebContents* tab =
      app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_EQ(tab->GetController().GetVisibleEntry()->GetPageType(),
            content::PAGE_TYPE_NORMAL);

  // Construct the new redirected URL.
  std::string expected_url = kChromeUIUntrustedProjectorUrl;
  expected_url += kFilePath;
  // The timestamp corresponds to 21 Jan 2022 10:00:00 GMT in microseconds since
  // Unix epoch (Jan 1 1970).
  expected_url += "?timestamp=1642759200000000%20bogo-microseconds";
  if (!url_params().empty())
    expected_url += "&" + url_params();
  EXPECT_EQ(tab->GetVisibleURL().spec(), expected_url);

  std::string histogram_name = navigate_from_link()
                                   ? "Apps.DefaultAppLaunch.FromLink"
                                   : "Apps.DefaultAppLaunch.FromOmnibox";
  histogram_tester.ExpectUniqueSample(
      histogram_name,
      /*sample=*/apps::DefaultAppName::kProjector, /*count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ProjectorNavigationThrottleTestParameterized,
    ::testing::Combine(
        /*navigate_from_link=*/testing::Bool(),
        /*url_params=*/::testing::Values("resourceKey=abc",
                                         "resourceKey=abc&xyz=123",
                                         "")));

// Verifies that opening a redirect link from an app such as gchat does not
// leave a blank tab behind. Prevents a regression to b/211788287.
IN_PROC_BROWSER_TEST_F(ProjectorNavigationThrottleTest,
                       AppNavigationRedirectNoBlankTab) {
  // Prior to navigation, there is only one browser available.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);

  // Suppose the user clicks a link like https://screencast.apps.chrome in
  // gchat. The redirect URL actually looks like the below.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL(
          "https://www.google.com/url?q=https://"
          "screencast.apps.chrome&sa=D&source=hangouts&ust=1642759200000000")));

  // We wait for both the old browser to close and the new app browser to open.
  ui_test_utils::BrowserChangeObserver removed_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kRemoved);
  ui_test_utils::BrowserChangeObserver added_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  // The Google servers would redirect to the URL in the ?q= query parameter.
  // Simulate this behavior in this test without actually pinging the Google
  // servers to prevent flakiness.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(kChromeUIUntrustedProjectorPwaUrl),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BrowserTestWaitFlags::BROWSER_TEST_WAIT_FOR_BROWSER);
  removed_observer.Wait();
  added_observer.Wait();

  // During the navigation, we closed the previous browser to prevent dangling
  // blank redirect pages and opened a new app browser for the Projector SWA.
  // There is still only one browser available.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  // Select the first available browser, which should be the SWA.
  SelectFirstBrowser();
  Browser* app_browser =
      FindSystemWebAppBrowser(profile(), SystemWebAppType::PROJECTOR);

  // Projector SWA is now open.
  ASSERT_TRUE(app_browser);
  content::WebContents* tab =
      app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_EQ(tab->GetController().GetVisibleEntry()->GetPageType(),
            content::PAGE_TYPE_NORMAL);

  std::string expected_url = kChromeUIUntrustedProjectorUrl;
  expected_url += "?timestamp=1642759200000000%20bogo-microseconds";
  EXPECT_EQ(tab->GetVisibleURL().spec(), expected_url);
}

// Verifies that navigating to chrome-untrusted://projector/app/ does not
// redirect but launches the SWA.
IN_PROC_BROWSER_TEST_F(ProjectorNavigationThrottleTest,
                       TrustedNavigationNoRedirect) {
  GURL untrusted_url(kChromeUIUntrustedProjectorUrl);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), untrusted_url, WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BrowserTestWaitFlags::BROWSER_TEST_WAIT_FOR_BROWSER);

  Browser* app_browser =
      FindSystemWebAppBrowser(profile(), SystemWebAppType::PROJECTOR);
  // Projector SWA is now open.
  ASSERT_TRUE(app_browser);
  content::WebContents* tab =
      app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_EQ(tab->GetController().GetVisibleEntry()->GetPageType(),
            content::PAGE_TYPE_NORMAL);

  // URL remains unchanged.
  EXPECT_EQ(tab->GetVisibleURL(), untrusted_url);
}

// Verifies that navigating to chrome-untrusted://projector-annotator does not
// lead to a crash. Prevents a regression to b/229124074.
IN_PROC_BROWSER_TEST_F(ProjectorNavigationThrottleTest,
                       UntrustedAnnotatorNavigationDoesNotCrash) {
  GURL untrusted_annotator_url(kChromeUIUntrustedAnnotatorUrl);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), untrusted_annotator_url));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_EQ(tab->GetVisibleURL(), untrusted_annotator_url);
}

class ProjectorNavigationThrottleLocaleTest
    : public ProjectorNavigationThrottleTest,
      public testing::WithParamInterface<std::string> {
 protected:
  std::string locale() const { return GetParam(); }
};

// Verifies that the Projector app can detect locale changes.
IN_PROC_BROWSER_TEST_P(ProjectorNavigationThrottleLocaleTest,
                       UntrustedNavigationLocaleDetection) {
  g_browser_process->SetApplicationLocale(locale());

  GURL untrusted_url(kChromeUIUntrustedProjectorUrl);

  content::TestNavigationObserver navigation_observer(untrusted_url);
  navigation_observer.StartWatchingNewWebContents();

  LaunchSystemWebAppAsync(profile(), SystemWebAppType::PROJECTOR);

  navigation_observer.Wait();

  Browser* app_browser =
      FindSystemWebAppBrowser(profile(), SystemWebAppType::PROJECTOR);

  content::WebContents* tab =
      app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(WaitForLoadStop(tab));

  EXPECT_EQ(tab->GetController().GetVisibleEntry()->GetPageType(),
            content::PAGE_TYPE_NORMAL);

  // Verify the document language.
  EXPECT_EQ(content::EvalJs(tab, "document.documentElement.lang"), locale());
}

INSTANTIATE_TEST_SUITE_P(,
                         ProjectorNavigationThrottleLocaleTest,
                         /*locale=*/testing::Values("en-US", "zh-CN"));

}  // namespace ash
