// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/theme_change_waiter.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"
#endif

static_assert(BUILDFLAG(IS_CHROMEOS), "For Chrome OS only");

namespace web_app {

class ChromeOsWebAppExperimentsBrowserTest
    : public WebAppNavigationBrowserTest {
 public:
  ChromeOsWebAppExperimentsBrowserTest() = default;
  ~ChromeOsWebAppExperimentsBrowserTest() override = default;

  // WebAppNavigationBrowserTest:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    WebAppNavigationBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    WebAppNavigationBrowserTest::SetUpOnMainThread();

    // Override the experiment parameters before installing the app so that it
    // gets published with the test extended scope.
    extended_scope_ = embedded_test_server()->GetURL("/pwa/");
    extended_scope_page_ = extended_scope_.Resolve("app2.html");
    ChromeOsWebAppExperiments::SetAlwaysEnabledForTesting();
    ChromeOsWebAppExperiments::SetScopeExtensionsForTesting(
        {extended_scope_.spec().c_str()});

    app_id_ = InstallWebAppFromPageAndCloseAppBrowser(
        browser(), embedded_test_server()->GetURL(
                       "/web_apps/get_manifest.html?theme_color.json"));
    apps::AppReadinessWaiter(profile(), app_id_).Await();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    auto init_params = chromeos::BrowserInitParams::GetForTests()->Clone();
    init_params->is_upload_office_to_cloud_enabled = true;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif
  }
  void TearDownOnMainThread() override {
    WebAppNavigationBrowserTest::TearDownOnMainThread();
    ChromeOsWebAppExperiments::ClearOverridesForTesting();
  }

 protected:
  webapps::AppId app_id_;
  GURL extended_scope_;
  GURL extended_scope_page_;
  // This has no effect in Lacros, the feature is enabled via
  // `chromeos::BrowserInitParams` instead.
  base::test::ScopedFeatureList scoped_feature_list_{
      chromeos::features::kUploadOfficeToCloud};
};

IN_PROC_BROWSER_TEST_F(ChromeOsWebAppExperimentsBrowserTest,
                       OutOfScopeBarRemoval) {
  // Check that the out of scope banner doesn't show after navigating to the
  // different scope in the web app window.
  Browser* app_browser = LaunchWebAppBrowser(app_id_);
  NavigateViaLinkClickToURLAndWait(app_browser, extended_scope_page_);
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

// TODO(https://issuetracker.google.com/248979304): Deflake these tests on
// Lacros + Ash.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)

IN_PROC_BROWSER_TEST_F(ChromeOsWebAppExperimentsBrowserTest,
                       LinkCaptureScopeExtension) {
  // Turn on link capturing for the web app.
  apps_util::SetSupportedLinksPreferenceAndWait(profile(), app_id_);

  // Navigate the main browser to the different scope.
  ClickLinkAndWait(browser()->tab_strip_model()->GetActiveWebContents(),
                   extended_scope_page_, LinkTarget::SELF, "");

  // The navigation should get link captured into the web app.
  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  AppBrowserController::IsForWebApp(app_browser, app_id_);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      extended_scope_page_);
}

IN_PROC_BROWSER_TEST_F(ChromeOsWebAppExperimentsBrowserTest,
                       IgnoreManifestColor) {
  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id_);
  EXPECT_FALSE(app_browser->app_controller()->GetThemeColor().has_value());

  // If the page starts setting its own theme-color it should not be ignored.
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  content::ThemeChangeWaiter waiter(web_contents);
  const char* script = R"(
    const meta = document.createElement('meta');
    meta.name = 'theme-color';
    meta.content = 'lime';
    document.head.append(meta);
  )";
  ASSERT_TRUE(EvalJs(web_contents, script).error.empty());
  waiter.Wait();

  EXPECT_EQ(app_browser->app_controller()->GetThemeColor(),
            SkColorSetARGB(0xFF, 0x0, 0xFF, 0x0));
}

#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace web_app
