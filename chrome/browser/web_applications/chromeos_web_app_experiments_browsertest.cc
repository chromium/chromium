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
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
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
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/input/input_event.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

static_assert(BUILDFLAG(IS_CHROMEOS), "For Chrome OS only");

namespace {
constexpr char kMicrosoft365ManifestUrlsFinchParam[] = "m365-manifest-urls";
}

namespace web_app {

class ChromeOsWebAppExperimentsBrowserTest
    : public WebAppNavigationBrowserTest,
      public testing::WithParamInterface<
          apps::test::LinkCapturingFeatureVersion> {
 public:
  ChromeOsWebAppExperimentsBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        apps::test::GetFeaturesToEnableLinkCapturingUX(GetParam());
    enabled_features.emplace_back(chromeos::features::kUploadOfficeToCloud,
                                  base::FieldTrialParams());
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }
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
  }
  void TearDownOnMainThread() override {
    WebAppNavigationBrowserTest::TearDownOnMainThread();
    ChromeOsWebAppExperiments::ClearOverridesForTesting();
  }

 protected:
  webapps::AppId app_id_;
  GURL extended_scope_;
  GURL extended_scope_page_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ChromeOsWebAppExperimentsBrowserTest,
                       OutOfScopeBarRemoval) {
  // Check that the out of scope banner doesn't show after navigating to the
  // different scope in the web app window.
  Browser* app_browser = LaunchWebAppBrowser(app_id_);
  NavigateViaLinkClickToURLAndWait(app_browser, extended_scope_page_);
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

IN_PROC_BROWSER_TEST_P(ChromeOsWebAppExperimentsBrowserTest,
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

IN_PROC_BROWSER_TEST_P(ChromeOsWebAppExperimentsBrowserTest,
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

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeOsWebAppExperimentsBrowserTest,
    testing::Values(apps::test::LinkCapturingFeatureVersion::kV1DefaultOff,
                    apps::test::LinkCapturingFeatureVersion::kV2DefaultOff,
                    apps::test::LinkCapturingFeatureVersion::
                        kV2DefaultOffCaptureExistingFrames),
    apps::test::LinkCapturingVersionToString);

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
IN_PROC_BROWSER_TEST_P(ChromeOsWebAppExperimentsNavigationBrowserTest,
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
IN_PROC_BROWSER_TEST_P(ChromeOsWebAppExperimentsNavigationBrowserTest,
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
IN_PROC_BROWSER_TEST_P(ChromeOsWebAppExperimentsNavigationBrowserTest,
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
IN_PROC_BROWSER_TEST_P(ChromeOsWebAppExperimentsNavigationBrowserTest,
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
IN_PROC_BROWSER_TEST_P(ChromeOsWebAppExperimentsNavigationBrowserTest,
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
IN_PROC_BROWSER_TEST_P(ChromeOsWebAppExperimentsNavigationBrowserTest,
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

// Test that clicking a noreferrer noopener target=_blank link to an
// out-of-scope URL results in opening a browser tab.
IN_PROC_BROWSER_TEST_P(ChromeOsWebAppExperimentsNavigationBrowserTest,
                       NoopenerNoreferrerBlankLinkToOutOfScope) {
  ASSERT_TRUE(https_server().Start());
  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id_);
  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  const GURL target_url = https_server().GetURL("/empty.html");
  auto observer = GetTestNavigationObserver(target_url);
  ClickLink(app_web_contents, target_url, LinkTarget::BLANK,
            /*rel=*/"noreferrer noopener");
  observer->Wait();

  // A browser tab is opened for the target URL.
  Browser* active_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_FALSE(AppBrowserController::IsForWebApp(active_browser, app_id_));
  EXPECT_EQ(active_browser->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL(),
            target_url);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeOsWebAppExperimentsNavigationBrowserTest,
    testing::Values(apps::test::LinkCapturingFeatureVersion::kV1DefaultOff,
                    apps::test::LinkCapturingFeatureVersion::kV2DefaultOff,
                    apps::test::LinkCapturingFeatureVersion::
                        kV2DefaultOffCaptureExistingFrames),
    apps::test::LinkCapturingVersionToString);

class ChromeOsWebAppExperimentsManifestOverrideBrowserTest
    : public InProcessBrowserTest {
 public:
  ChromeOsWebAppExperimentsManifestOverrideBrowserTest() = default;

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    scoped_feature_list_.Reset();
  }

  Profile* profile() { return browser()->profile(); }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* RenderFrameHost() const {
    return web_contents()->GetPrimaryMainFrame();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

class ChromeOsWebAppExperimentsManifestOverrideDisabledBrowserTest
    : public ChromeOsWebAppExperimentsManifestOverrideBrowserTest {
 public:
  ChromeOsWebAppExperimentsManifestOverrideDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        chromeos::features::kMicrosoft365ManifestOverride);
  }
};

IN_PROC_BROWSER_TEST_F(
    ChromeOsWebAppExperimentsManifestOverrideDisabledBrowserTest,
    DontOverrideManifestWithFlagDisabled) {
  const GURL m365PWAUrl = GURL("https://www.microsoft365.com/");

  blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
  manifest->id = m365PWAUrl;
  manifest->start_url = m365PWAUrl;

  ChromeOsWebAppExperiments::MaybeOverrideManifest(RenderFrameHost(), manifest);

  EXPECT_EQ(m365PWAUrl, manifest->id);
}

class ChromeOsWebAppExperimentsManifestOverrideEnabledBrowserTest
    : public ChromeOsWebAppExperimentsManifestOverrideBrowserTest {
 public:
  ChromeOsWebAppExperimentsManifestOverrideEnabledBrowserTest() {
    EnableM365ManifestUrls(
        "https://www.microsoft365.com/,https://www.example.com/");
  }

  void EnableM365ManifestUrls(const std::string& urls) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        chromeos::features::kMicrosoft365ManifestOverride,
        {{kMicrosoft365ManifestUrlsFinchParam, urls}});
  }
};

// The manifest id should not be overridden if the start URL is not contained in
// the Url list of the corresponding finch parameter.
IN_PROC_BROWSER_TEST_F(
    ChromeOsWebAppExperimentsManifestOverrideEnabledBrowserTest,
    DontOverrideManifestForNonMatchingUrl) {
  const GURL m365PWAUrl = GURL("https://www.example2.com/");

  blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
  manifest->id = m365PWAUrl;
  manifest->start_url = m365PWAUrl;

  ChromeOsWebAppExperiments::MaybeOverrideManifest(RenderFrameHost(), manifest);

  EXPECT_EQ(m365PWAUrl, manifest->id);
}

// The manifest id should not be overridden if the start URL origin matches but
// the path does not.
IN_PROC_BROWSER_TEST_F(
    ChromeOsWebAppExperimentsManifestOverrideEnabledBrowserTest,
    DontOverrideManifestForUrlWithPath) {
  const GURL m365PWAUrlWithPath =
      GURL("https://www.microsoft365.com/launch/word/");

  blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
  manifest->id = m365PWAUrlWithPath;
  manifest->start_url = m365PWAUrlWithPath;

  ChromeOsWebAppExperiments::MaybeOverrideManifest(RenderFrameHost(), manifest);

  EXPECT_EQ(m365PWAUrlWithPath, manifest->id);
}

// The manifest id should be overridden if the start URL matches a URL in the
// corresponding finch flag.
IN_PROC_BROWSER_TEST_F(
    ChromeOsWebAppExperimentsManifestOverrideEnabledBrowserTest,
    OverrideManifestIdForMatchingUrl) {
  // Override manifest for plain Url.
  const GURL m365PWAUrl = GURL("https://www.microsoft365.com/");

  blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
  manifest->id = m365PWAUrl;
  manifest->start_url = m365PWAUrl;

  ChromeOsWebAppExperiments::MaybeOverrideManifest(RenderFrameHost(), manifest);

  EXPECT_EQ(GURL("https://www.microsoft365.com/?from=Homescreen"),
            manifest->id);
}

// The manifest id should be overridden if the start URL matches a URL in the
// corresponding finch flag except for query parameters.
IN_PROC_BROWSER_TEST_F(
    ChromeOsWebAppExperimentsManifestOverrideEnabledBrowserTest,
    OverrideManifestIdForMatchingUrlWithQueryParams) {
  const GURL m365PWAUrl = GURL("https://www.microsoft365.com/?auth=1");

  blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
  manifest->id = m365PWAUrl;
  manifest->start_url = m365PWAUrl;

  ChromeOsWebAppExperiments::MaybeOverrideManifest(RenderFrameHost(), manifest);

  EXPECT_EQ(GURL("https://www.microsoft365.com/?from=Homescreen"),
            manifest->id);
}

// The manifest id should be overridden if the start URL matches a URL in the
// corresponding finch flag except for a file name.
IN_PROC_BROWSER_TEST_F(
    ChromeOsWebAppExperimentsManifestOverrideEnabledBrowserTest,
    OverrideManifestIdForMatchingUrlWithFileName) {
  const GURL m365PWAUrl = GURL("https://www.microsoft365.com/index.html");

  blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
  manifest->id = m365PWAUrl;
  manifest->start_url = m365PWAUrl;

  ChromeOsWebAppExperiments::MaybeOverrideManifest(RenderFrameHost(), manifest);

  EXPECT_EQ(GURL("https://www.microsoft365.com/?from=Homescreen"),
            manifest->id);
}

// The manifest id should be overridden if the start URL matches a URL in the
// corresponding finch flag except for a fragment.
IN_PROC_BROWSER_TEST_F(
    ChromeOsWebAppExperimentsManifestOverrideEnabledBrowserTest,
    OverrideManifestIdForMatchingUrlWithFragment) {
  const GURL m365PWAUrl = GURL("https://www.microsoft365.com/#test");

  blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
  manifest->id = m365PWAUrl;
  manifest->start_url = m365PWAUrl;

  ChromeOsWebAppExperiments::MaybeOverrideManifest(RenderFrameHost(), manifest);

  EXPECT_EQ(GURL("https://www.microsoft365.com/?from=Homescreen"),
            manifest->id);
}

// The manifest id should be overridden if the start URL matches one of multiple
// URLs in the corresponding finch flag.
IN_PROC_BROWSER_TEST_F(
    ChromeOsWebAppExperimentsManifestOverrideEnabledBrowserTest,
    OverrideManifestIdForMultipleUrls) {
  const GURL m365PWAUrl1 = GURL("https://www.microsoft365.com/");
  const GURL m365PWAUrl2 = GURL("https://www.example.com/?test=a");

  {
    blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
    manifest->id = m365PWAUrl1;
    manifest->start_url = m365PWAUrl1;

    ChromeOsWebAppExperiments::MaybeOverrideManifest(RenderFrameHost(),
                                                     manifest);

    EXPECT_EQ(GURL("https://www.microsoft365.com/?from=Homescreen"),
              manifest->id);
  }

  {
    blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
    manifest->id = m365PWAUrl2;
    manifest->start_url = m365PWAUrl2;

    ChromeOsWebAppExperiments::MaybeOverrideManifest(RenderFrameHost(),
                                                     manifest);

    EXPECT_EQ(GURL("https://www.example.com/?from=Homescreen"), manifest->id);
  }
}

}  // namespace web_app
