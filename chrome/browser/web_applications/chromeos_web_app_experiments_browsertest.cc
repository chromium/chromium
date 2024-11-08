// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"

#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/theme_change_waiter.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/input/input_event.mojom-shared.h"
#include "url/gurl.h"
#include "url/url_constants.h"

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
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id_));
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

class ChromeOsWebAppExperimentsNavigationBrowserTest
    : public ChromeOsWebAppExperimentsBrowserTest {
 protected:
  ChromeOsWebAppExperimentsNavigationBrowserTest() = default;

  ~ChromeOsWebAppExperimentsNavigationBrowserTest() override = default;

  void AddAndClickLinkWithCode(content::WebContents* web_contents,
                               const std::string& on_click_code) {
    const std::string script = base::StringPrintf(
        R"(
          (() => {
            const link = document.createElement('a');
            link.href = '#';
            link.onclick = function(e) {
              e.preventDefault();
              %s
            };
            // Make a click target that covers the whole viewport.
            const clickTarget = document.createElement('textarea');
            clickTarget.style.position = 'absolute';
            clickTarget.style.top = 0;
            clickTarget.style.left = 0;
            clickTarget.style.height = '100vh';
            clickTarget.style.width = '100vw';
            link.appendChild(clickTarget);
            document.body.appendChild(link);
          })();
        )",
        on_click_code.c_str());
    ASSERT_TRUE(content::ExecJs(web_contents, script));
    content::SimulateMouseClick(web_contents,
                                blink::WebInputEvent::Modifiers::kNoModifiers,
                                blink::WebMouseEvent::Button::kLeft);
  }

  std::string GetFormBasedRedirectorCode(const GURL& target_url) const {
    const GURL redirector_url = https_server().GetURL(
        "redirector-host", CreateServerRedirect(target_url));
    return base::StringPrintf(
        R"(
          const f = document.createElement('form');
          f.setAttribute('method', 'post');
          f.setAttribute('action', '%s');
          document.body.appendChild(f);
          f.submit();
        )",
        redirector_url.spec().c_str());
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      chromeos::features::kOfficeNavigationCapturingReimpl};
};

// Test that submitting a POST form in the app's window doesn't result in
// leaving that window.
IN_PROC_BROWSER_TEST_F(ChromeOsWebAppExperimentsNavigationBrowserTest,
                       PostForm) {
  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id_);
  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  const std::string on_click_code = base::StringPrintf(
      R"(
        const f = document.createElement('form');
        f.setAttribute('method', 'post');
        f.setAttribute('action', '%s');
        f.setAttribute('target', '_top');
        const p = document.createElement('input');
        p.setAttribute('name', 'foo');
        p.setAttribute('value', 'bar');
        f.appendChild(p);
        document.body.appendChild(f);
        f.submit();
      )",
      extended_scope_page_.spec().c_str());

  auto observer = GetTestNavigationObserver(extended_scope_page_);
  AddAndClickLinkWithCode(app_web_contents, on_click_code);
  observer->Wait();

  // The web app handles the navigation.
  Browser* active_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(AppBrowserController::IsForWebApp(active_browser, app_id_));
  EXPECT_EQ(active_browser->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL(),
            extended_scope_page_);
}

// Test that submitting a POST form to an app-controlled URL, happening in a
// window opened via target=_blank, ends up in a new app window.
IN_PROC_BROWSER_TEST_F(ChromeOsWebAppExperimentsNavigationBrowserTest,
                       PostFormInBlankWindow) {
  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id_);
  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  const std::string on_click_code = base::StringPrintf(
      R"(
        const w = window.open('', '_blank');
        const f = w.document.createElement('form');
        f.setAttribute('method', 'post');
        f.setAttribute('action', '%s');
        f.setAttribute('target', '_top');
        const p = w.document.createElement('input');
        p.setAttribute('name', 'foo');
        p.setAttribute('value', 'bar');
        f.appendChild(p);
        w.document.body.appendChild(f);
        f.submit();
      )",
      extended_scope_page_.spec().c_str());

  auto observer = GetTestNavigationObserver(extended_scope_page_);
  AddAndClickLinkWithCode(app_web_contents, on_click_code);
  observer->Wait();

  // The web app handles the navigation by opening a new app window.
  Browser* active_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(AppBrowserController::IsForWebApp(active_browser, app_id_));
  EXPECT_EQ(active_browser->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL(),
            extended_scope_page_);
}

// Test that opening a target=_blank window with an app-controlled URL ends up
// in a new app window.
IN_PROC_BROWSER_TEST_F(ChromeOsWebAppExperimentsNavigationBrowserTest,
                       OpenAsBlankWindow) {
  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id_);
  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  const std::string on_click_code = base::StringPrintf(
      R"(
        window.open('%s', '_blank');
      )",
      extended_scope_page_.spec().c_str());

  auto observer = GetTestNavigationObserver(extended_scope_page_);
  AddAndClickLinkWithCode(app_web_contents, on_click_code);
  observer->Wait();

  // The web app handles the navigation by opening a new app window.
  Browser* active_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(AppBrowserController::IsForWebApp(active_browser, app_id_));
  EXPECT_EQ(active_browser->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL(),
            extended_scope_page_);
}

// Test that opening an empty target=_blank window and then navigating it as
// target=_top to an app-controlled URL ends up in a new app window.
IN_PROC_BROWSER_TEST_F(ChromeOsWebAppExperimentsNavigationBrowserTest,
                       OpenTopWindowInBlankWindow) {
  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id_);
  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  const std::string on_click_code = base::StringPrintf(
      R"(
        const w = window.open('', '_blank');
        w.open('%s', '_top');
      )",
      extended_scope_page_.spec().c_str());

  auto observer = GetTestNavigationObserver(extended_scope_page_);
  AddAndClickLinkWithCode(app_web_contents, on_click_code);
  observer->Wait();

  // The web app handles the navigation by opening a new app window.
  Browser* active_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(AppBrowserController::IsForWebApp(active_browser, app_id_));
  EXPECT_EQ(active_browser->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL(),
            extended_scope_page_);
}

// Test that submitting a form that redirects to the app-controlled URL results
// in launching that app - if it's marked as "open supported links".
IN_PROC_BROWSER_TEST_F(ChromeOsWebAppExperimentsNavigationBrowserTest,
                       OutOfScopeFormAndRedirectToPreferred) {
  ASSERT_TRUE(https_server().Start());
  // Start from a blank page - the form below will be added to it.
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  content::WebContents* page_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto observer = GetTestNavigationObserver(extended_scope_page_);
  AddAndClickLinkWithCode(page_web_contents,
                          GetFormBasedRedirectorCode(extended_scope_page_));
  observer->Wait();

  // The web app handles the navigation by opening a new app window.
  Browser* active_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(AppBrowserController::IsForWebApp(active_browser, app_id_));
  EXPECT_EQ(active_browser->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL(),
            extended_scope_page_);
}

// Opposite to the previous test, verifies that the app is NOT launched if it's
// not marked as "open supported links".
IN_PROC_BROWSER_TEST_F(ChromeOsWebAppExperimentsNavigationBrowserTest,
                       OutOfScopeFormAndRedirectToNotPreferred) {
  ASSERT_TRUE(https_server().Start());
  // The link capturing is turned on by default; simulate the user opt-out here.
  apps_util::RemoveSupportedLinksPreferenceAndWait(profile(), app_id_);
  // Start from a blank page - the form below will be added to it.
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  content::WebContents* page_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto observer = GetTestNavigationObserver(extended_scope_page_);
  AddAndClickLinkWithCode(page_web_contents,
                          GetFormBasedRedirectorCode(extended_scope_page_));
  observer->Wait();

  // The app window was not launched for the navigation.
  Browser* active_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_FALSE(AppBrowserController::IsForWebApp(active_browser, app_id_));
  EXPECT_EQ(active_browser->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL(),
            extended_scope_page_);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace web_app
