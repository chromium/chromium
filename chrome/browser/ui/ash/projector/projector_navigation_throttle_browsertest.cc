// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kFilePath[] = "xyz";

}  // namespace

// Summary of expected behavior on ChromeOS:
// ____________________________|_SWA_launches_|_URL_redirection
// projector.apps.chrome       | Yes          | Yes
// chrome://projector/app/     | Yes          | No
// chrome-untrusted://projector| No           | No

class ProjectorNavigationThrottleTest : public InProcessBrowserTest {
 public:
  ProjectorNavigationThrottleTest()
      : scoped_feature_list_(features::kProjector) {}

  void SetUpOnMainThread() override {
    web_app::WebAppProvider::GetForTest(profile())
        ->system_web_app_manager()
        .InstallSystemAppsForTesting();
  }

  ~ProjectorNavigationThrottleTest() override = default;

 protected:
  Profile* profile() { return browser()->profile(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ProjectorNavigationThrottleTestParameterized
    : public ProjectorNavigationThrottleTest,
      public testing::WithParamInterface<bool> {
 protected:
  bool navigate_from_link() const { return GetParam(); }
};

// Verifies that navigating to https://projector.apps.chrome/xyz redirects to
// chrome://projector/app/xyz and launches the SWA.
IN_PROC_BROWSER_TEST_P(ProjectorNavigationThrottleTestParameterized,
                       PwaNavigationRedirects) {
  base::HistogramTester histogram_tester;

  std::string url = kChromeUIUntrustedProjectorPwaUrl;
  url += "/";
  url += kFilePath;
  GURL gurl(url);

  // Prior to navigation, there is only one browser available.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser* old_browser = browser();

  if (navigate_from_link()) {
    // Simulate the user clicking a link.
    NavigateParams params(browser(), gurl,
                          ui::PageTransition::PAGE_TRANSITION_LINK);
    ui_test_utils::NavigateToURL(&params);
  } else {
    // Simulate the user typing the url into the omnibox.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), gurl, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BrowserTestWaitFlags::BROWSER_TEST_WAIT_FOR_BROWSER);
  }
  web_app::FlushSystemWebAppLaunchesForTesting(profile());

  // During the navigation, we closed the previous browser to prevent dangling
  // about:blank pages and opened a new app browser for the Projector SWA.
  // There is still only one browser available.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  // Select the first available browser, which should be the SWA.
  SelectFirstBrowser();
  Browser* new_browser = browser();
  // However, the new browser is not the same as the previous browser because
  // the previous one closed.
  EXPECT_NE(old_browser, new_browser);

  Browser* app_browser =
      FindSystemWebAppBrowser(profile(), web_app::SystemAppType::PROJECTOR);
  // Projector SWA is now open.
  ASSERT_TRUE(app_browser);
  EXPECT_EQ(app_browser, new_browser);
  content::WebContents* tab =
      app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_EQ(tab->GetController().GetVisibleEntry()->GetPageType(),
            content::PAGE_TYPE_NORMAL);

  // Construct the new redirected URL.
  std::string expected_url = kChromeUITrustedProjectorAppUrl;
  expected_url += kFilePath;
  EXPECT_EQ(tab->GetVisibleURL().spec(), expected_url);

  std::string histogram_name = navigate_from_link()
                                   ? "Apps.DefaultAppLaunch.FromLink"
                                   : "Apps.DefaultAppLaunch.FromOmnibox";
  histogram_tester.ExpectUniqueSample(
      histogram_name,
      /*sample=*/apps::DefaultAppName::kProjector, /*count=*/1);
}

INSTANTIATE_TEST_SUITE_P(,
                         ProjectorNavigationThrottleTestParameterized,
                         /*navigate_from_link=*/testing::Bool());

// Verifies that navigating to chrome-untrusted://projector does not redirect.
IN_PROC_BROWSER_TEST_F(ProjectorNavigationThrottleTest,
                       UntrustedNavigationNoRedirect) {
  std::string url = kChromeUIUntrustedProjectorAppUrl;
  GURL gurl(url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  web_app::FlushSystemWebAppLaunchesForTesting(profile());

  Browser* app_browser =
      FindSystemWebAppBrowser(profile(), web_app::SystemAppType::PROJECTOR);
  // Projector SWA is not open. We don't capture navigations to
  // chrome-untrusted://projector.
  EXPECT_FALSE(app_browser);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_EQ(tab->GetController().GetLastCommittedEntry()->GetPageType(),
            content::PAGE_TYPE_NORMAL);

  // URL remains unchanged.
  EXPECT_EQ(tab->GetVisibleURL().spec(), url);
}

// Verifies that navigating to chrome://projector/app/ does not redirect.
IN_PROC_BROWSER_TEST_F(ProjectorNavigationThrottleTest,
                       TrustedNavigationNoRedirect) {
  std::string url = kChromeUITrustedProjectorAppUrl;
  GURL gurl(url);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), gurl, WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BrowserTestWaitFlags::BROWSER_TEST_WAIT_FOR_BROWSER);
  web_app::FlushSystemWebAppLaunchesForTesting(profile());

  Browser* app_browser =
      FindSystemWebAppBrowser(profile(), web_app::SystemAppType::PROJECTOR);
  // Projector SWA is now open.
  ASSERT_TRUE(app_browser);
  content::WebContents* tab =
      app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_EQ(tab->GetController().GetVisibleEntry()->GetPageType(),
            content::PAGE_TYPE_NORMAL);

  // URL remains unchanged.
  EXPECT_EQ(tab->GetVisibleURL().spec(), url);
}

}  // namespace ash
