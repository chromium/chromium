// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"

using ui_test_utils::BrowserChangeObserver;

namespace {

void AwaitTabCount(Browser* browser, int tab_count) {
  if (browser->tab_strip_model()->count() == tab_count)
    return;

  class Observer : public TabStripModelObserver {
   public:
    explicit Observer(int tab_count) : tab_count_(tab_count) {}

    void OnTabStripModelChanged(
        TabStripModel* tab_strip_model,
        const TabStripModelChange& change,
        const TabStripSelectionChange& selection) override {
      if (tab_strip_model->count() == tab_count_)
        run_loop_.Quit();
    }

    void Wait() { run_loop_.Run(); }

   private:
    int tab_count_;
    base::RunLoop run_loop_;
  } observer(tab_count);

  browser->tab_strip_model()->AddObserver(&observer);
  observer.Wait();
}

}  // namespace

namespace web_app {

class WebAppLinkCapturingBrowserTest : public WebAppNavigationBrowserTest {
 public:
  WebAppLinkCapturingBrowserTest() {
    os_hooks_supress_ = OsIntegrationManager::ScopedSuppressOsHooksForTesting();
  }

  ~WebAppLinkCapturingBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebAppNavigationBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(https_server().Start());
    ASSERT_TRUE(embedded_test_server()->Start());
    out_of_scope_ = https_server().GetURL("/");
  }

  void InstallTestApp(const char* path, bool await_metric) {
    start_url_ = embedded_test_server()->GetURL(path);
    in_scope_1_ = start_url_.Resolve("page1.html");
    in_scope_2_ = start_url_.Resolve("page2.html");
    scope_ = start_url_.GetWithoutFilename();

    // Create new tab to navigate, install, automatically pop out and then
    // close. This sequence avoids altering the browser window state we started
    // with.
    AddTab(browser(), about_blank_);

    page_load_metrics::PageLoadMetricsTestWaiter metrics_waiter(
        browser()->tab_strip_model()->GetActiveWebContents());
    if (await_metric) {
      metrics_waiter.AddWebFeatureExpectation(
          blink::mojom::WebFeature::kWebAppManifestCaptureLinks);
    }
    BrowserChangeObserver observer(browser(),
                                   BrowserChangeObserver::ChangeType::kAdded);
    app_id_ = web_app::InstallWebAppFromPage(browser(), start_url_);
    if (await_metric)
      metrics_waiter.Wait();

    Browser* app_browser = observer.Wait();
    EXPECT_NE(app_browser, browser());
    EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id_));
    chrome::CloseWindow(app_browser);
  }

  WebAppProviderBase& provider() {
    auto* provider = WebAppProviderBase::GetProviderBase(profile());
    DCHECK(provider);
    return *provider;
  }

  void AddTab(Browser* browser, GURL url) {
    content::TestNavigationObserver observer(url);
    observer.StartWatchingNewWebContents();
    chrome::AddTabAt(browser, url, /*index=*/-1, /*foreground=*/true);
    observer.Wait();
  }

  void Navigate(Browser* browser,
                const GURL& url,
                LinkTarget link_target = LinkTarget::SELF) {
    ClickLinkAndWait(browser->tab_strip_model()->GetActiveWebContents(), url,
                     link_target, "");
  }

  Browser* GetNewBrowserFromNavigation(Browser* browser, const GURL& url) {
    BrowserChangeObserver observer(browser,
                                   BrowserChangeObserver::ChangeType::kAdded);
    Navigate(browser, url);
    return observer.Wait();
  }

  void ExpectTabs(Browser* test_browser, std::vector<GURL> urls) {
    std::string debug_info = "\nOpen browsers:\n";
    for (Browser* open_browser : *BrowserList::GetInstance()) {
      debug_info += "  ";
      if (open_browser == browser())
        debug_info += "Main browser";
      else if (open_browser->app_controller())
        debug_info += "App browser";
      else
        debug_info += "Browser";
      debug_info += ":\n";
      for (int i = 0; i < open_browser->tab_strip_model()->count(); ++i) {
        debug_info += "   - " +
                      open_browser->tab_strip_model()
                          ->GetWebContentsAt(i)
                          ->GetVisibleURL()
                          .spec() +
                      "\n";
      }
    }
    SCOPED_TRACE(debug_info);
    TabStripModel& tab_strip = *test_browser->tab_strip_model();
    ASSERT_EQ(static_cast<size_t>(tab_strip.count()), urls.size());
    for (int i = 0; i < tab_strip.count(); ++i) {
      SCOPED_TRACE(base::StringPrintf("is app browser: %d, tab index: %d",
                                      bool(test_browser->app_controller()), i));
      EXPECT_EQ(
          test_browser->tab_strip_model()->GetWebContentsAt(i)->GetVisibleURL(),
          urls[i]);
    }
  }

  GURL NtpUrl() { return ntp_test_utils::GetFinalNtpUrl(browser()->profile()); }

 protected:
  AppId app_id_;
  GURL start_url_;
  GURL in_scope_1_;
  GURL in_scope_2_;
  GURL scope_;
  GURL out_of_scope_;

  const GURL about_blank_{"about:blank"};

  ScopedOsHooksSuppress os_hooks_supress_;
};

class WebAppTabStripLinkCapturingBrowserTest
    : public WebAppLinkCapturingBrowserTest {
 public:
  WebAppTabStripLinkCapturingBrowserTest() {
    features_.InitWithFeatures({features::kDesktopPWAsTabStrip,
                                features::kDesktopPWAsTabStripLinkCapturing},
                               {});
  }

  void InstallTestApp() {
    WebAppLinkCapturingBrowserTest::InstallTestApp("/web_apps/basic.html",
                                                   /*await_metric=*/false);
    provider().registry_controller().SetExperimentalTabbedWindowMode(
        app_id_, true, /*is_user_action=*/false);
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(WebAppTabStripLinkCapturingBrowserTest,
                       InScopeNavigationsCaptured) {
  InstallTestApp();

  // Start browser at an out of scope page.
  Navigate(browser(), out_of_scope_);

  // In scope navigation should open app window.
  Browser* app_browser = GetNewBrowserFromNavigation(browser(), in_scope_1_);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id_));
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1_});

  // Another in scope navigation should open a new tab in the same app window.
  Navigate(browser(), in_scope_2_);
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1_, in_scope_2_});

  // Whole origin should count as in scope.
  Navigate(browser(), scope_);
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1_, in_scope_2_, scope_});

  // Middle clicking links should not be captured.
  ClickLinkWithModifiersAndWaitForURL(
      browser()->tab_strip_model()->GetActiveWebContents(), scope_, scope_,
      LinkTarget::SELF, "", blink::WebInputEvent::Modifiers::kNoModifiers,
      blink::WebMouseEvent::Button::kMiddle);
  ExpectTabs(browser(), {out_of_scope_, scope_});
  ExpectTabs(app_browser, {in_scope_1_, in_scope_2_, scope_});

  // Out of scope should behave as usual.
  Navigate(browser(), out_of_scope_);
  ExpectTabs(browser(), {out_of_scope_, scope_});
  ExpectTabs(app_browser, {in_scope_1_, in_scope_2_, scope_});
}

// First about:blank captures in scope navigations.
IN_PROC_BROWSER_TEST_F(WebAppTabStripLinkCapturingBrowserTest,
                       AboutBlankNavigationReparented) {
  InstallTestApp();

  ExpectTabs(browser(), {about_blank_});
  content::WebContents* reparent_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigations from a fresh about:blank page should reparent.
  // When no app window is open one should be created.
  Browser* app_browser = GetNewBrowserFromNavigation(browser(), in_scope_1_);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id_));
  ExpectTabs(browser(), {NtpUrl()});
  ExpectTabs(app_browser, {in_scope_1_});
  EXPECT_EQ(reparent_web_contents,
            app_browser->tab_strip_model()->GetActiveWebContents());

  // Navigations from a fresh about:blank page via JavaScript should also
  // reparent. When there is already an app window open we should reparent into
  // it.
  AddTab(browser(), about_blank_);
  ExpectTabs(browser(), {NtpUrl(), about_blank_});
  reparent_web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  {
    content::TestNavigationObserver observer(in_scope_2_);
    observer.WatchExistingWebContents();
    ASSERT_TRUE(content::ExecuteScript(
        reparent_web_contents,
        base::StringPrintf("location = '%s';", in_scope_2_.spec().c_str())));
    observer.Wait();
  }
  ExpectTabs(browser(), {NtpUrl()});
  ExpectTabs(app_browser, {in_scope_1_, in_scope_2_});
  EXPECT_EQ(reparent_web_contents,
            app_browser->tab_strip_model()->GetActiveWebContents());
}

class WebAppDeclarativeLinkCapturingBrowserTest
    : public WebAppLinkCapturingBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  static std::string ParamToString(
      const ::testing::TestParamInfo<bool> param_info) {
    return param_info.param ? "PersistenceOn" : "PersistenceOff";
  }

  WebAppDeclarativeLinkCapturingBrowserTest() {
    if (GetParam()) {
      features_.InitWithFeatures({blink::features::kWebAppEnableLinkCapturing,
                                  features::kIntentPickerPWAPersistence},
                                 {});
    } else {
      features_.InitWithFeatures({blink::features::kWebAppEnableLinkCapturing},
                                 {features::kIntentPickerPWAPersistence});
    }
  }

  bool IsIntentPickerPersistenceEnabled() {
    return base::FeatureList::IsEnabled(features::kIntentPickerPWAPersistence);
  }

  void TurnOnLinkCapturing() {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    proxy->AddPreferredApp(app_id_, start_url_);
    proxy->FlushMojoCallsForTesting();
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_P(WebAppDeclarativeLinkCapturingBrowserTest,
                       CaptureLinksUnset) {
  InstallTestApp("/web_apps/basic.html", /*await_metric=*/false);

  histogram_tester_.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kWebAppManifestCaptureLinks, 0);

  // No link capturing should happen.
  Navigate(browser(), start_url_);
  EXPECT_EQ(browser(), BrowserList::GetInstance()->GetLastActive());
  ExpectTabs(browser(), {start_url_});

  if (IsIntentPickerPersistenceEnabled()) {
    TurnOnLinkCapturing();

    // Users can enable link capturing regardless of declarative link capturing.
    Navigate(browser(), out_of_scope_);
    Browser* app_browser = GetNewBrowserFromNavigation(browser(), in_scope_1_);
    EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id_));
    ExpectTabs(browser(), {out_of_scope_});
    ExpectTabs(app_browser, {in_scope_1_});
  }
}

IN_PROC_BROWSER_TEST_P(WebAppDeclarativeLinkCapturingBrowserTest,
                       CaptureLinksNone) {
  InstallTestApp("/web_apps/capture_links_none.html", /*await_metric=*/true);

  histogram_tester_.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kWebAppManifestCaptureLinks, 1);

  // No link capturing should happen.
  Navigate(browser(), start_url_);
  EXPECT_EQ(browser(), BrowserList::GetInstance()->GetLastActive());
  ExpectTabs(browser(), {start_url_});

  if (IsIntentPickerPersistenceEnabled()) {
    TurnOnLinkCapturing();

    // Users can enable link capturing regardless of declarative link capturing.
    Navigate(browser(), out_of_scope_);
    Browser* app_browser = GetNewBrowserFromNavigation(browser(), in_scope_1_);
    EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id_));
    ExpectTabs(browser(), {out_of_scope_});
    ExpectTabs(app_browser, {in_scope_1_});
  }
}

// Flaky on Linux, crbug.com/1185680
#if defined(OS_LINUX)
#define MAYBE_CaptureLinksNewClient DISABLED_CaptureLinksNewClient
#else
#define MAYBE_CaptureLinksNewClient CaptureLinksNewClient
#endif
IN_PROC_BROWSER_TEST_P(WebAppDeclarativeLinkCapturingBrowserTest,
                       MAYBE_CaptureLinksNewClient) {
  InstallTestApp("/web_apps/capture_links_new_client.html",
                 /*await_metric=*/true);

  histogram_tester_.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kWebAppManifestCaptureLinks, 1);

  if (IsIntentPickerPersistenceEnabled()) {
    // No link capturing should happen until the user turns it on.
    Navigate(browser(), start_url_);
    EXPECT_EQ(browser(), BrowserList::GetInstance()->GetLastActive());
    ExpectTabs(browser(), {start_url_});

    TurnOnLinkCapturing();
  }

  // In scope navigation should open an app window.
  Navigate(browser(), out_of_scope_);
  Browser* app_browser_1 = GetNewBrowserFromNavigation(browser(), in_scope_1_);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser_1, app_id_));
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser_1, {in_scope_1_});

  // In scope navigation should open a new app window.
  Browser* app_browser_2 = GetNewBrowserFromNavigation(browser(), in_scope_2_);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser_2, app_id_));
  EXPECT_NE(app_browser_1, app_browser_2);
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser_1, {in_scope_1_});
  ExpectTabs(app_browser_2, {in_scope_2_});

  // In scope navigation from app window should not capture.
  Navigate(app_browser_1, in_scope_2_);
  EXPECT_EQ(app_browser_2, BrowserList::GetInstance()->GetLastActive());
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser_1, {in_scope_2_});
  ExpectTabs(app_browser_2, {in_scope_2_});
}

IN_PROC_BROWSER_TEST_P(WebAppDeclarativeLinkCapturingBrowserTest,
                       InAppScopeNavigationIgnored) {
  InstallTestApp("/web_apps/capture_links_new_client.html",
                 /*await_metric=*/true);

  histogram_tester_.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kWebAppManifestCaptureLinks, 1);

  if (IsIntentPickerPersistenceEnabled())
    TurnOnLinkCapturing();

  // Put the browser in the app scope.
  AddTab(browser(), start_url_);

  // Navigations that happen inside the app scope should not capture even if
  // done outside of an app window.
  Navigate(browser(), in_scope_1_);
  EXPECT_EQ(browser(), BrowserList::GetInstance()->GetLastActive());
  ExpectTabs(browser(), {about_blank_, in_scope_1_});
}

IN_PROC_BROWSER_TEST_P(WebAppDeclarativeLinkCapturingBrowserTest,
                       CaptureLinksExistingClientNavigate) {
  InstallTestApp("/web_apps/capture_links_existing_client_navigate.html",
                 /*await_metric=*/true);

  histogram_tester_.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kWebAppManifestCaptureLinks, 1);

  if (IsIntentPickerPersistenceEnabled())
    TurnOnLinkCapturing();

  Navigate(browser(), out_of_scope_);

  // In scope navigation should open an app window (because there are none
  // already open).
  Browser* app_browser = GetNewBrowserFromNavigation(browser(), in_scope_1_);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id_));
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1_});

  // In scope navigation should navigate the existing app window.
  Navigate(browser(), in_scope_2_);
  EXPECT_EQ(app_browser, BrowserList::GetInstance()->GetLastActive());
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_2_});

  // target=_blank in scope navigation should navigate the existing app window.
  Navigate(browser(), in_scope_1_, LinkTarget::BLANK);
  // TODO(crbug.com/1209082): The app window should now be focused.
  // EXPECT_EQ(app_browser, BrowserList::GetInstance()->GetLastActive());
  // TODO(crbug.com/1209096): With IntentPickerPWAPersistence we don't close the
  // new about:blank tab after capturing.
  if (!IsIntentPickerPersistenceEnabled()) {
    // Clicking target=_blank will open a new tab that closes asynchronously,
    // wait for that to finish before checking browser tab state.
    AwaitTabCount(browser(), 1);
    ExpectTabs(browser(), {out_of_scope_});
    ExpectTabs(app_browser, {in_scope_1_});
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppDeclarativeLinkCapturingBrowserTest,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    /*persistence=*/testing::Values(true, false),
#else
    // App service intent handling is not yet available outside of Chrome OS.
    /*persistence=*/testing::Values(false),
#endif
    &WebAppDeclarativeLinkCapturingBrowserTest::ParamToString);

class WebAppDeclarativeLinkCapturingOriginTrialBrowserTest
    : public InProcessBrowserTest {
 public:
  WebAppDeclarativeLinkCapturingOriginTrialBrowserTest() {
    features_.InitAndDisableFeature(
        blink::features::kWebAppEnableLinkCapturing);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Using the test public key from docs/origin_trials_integration.md#Testing.
    command_line->AppendSwitchASCII(
        embedder_support::kOriginTrialPublicKey,
        "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=");
  }

 private:
  base::test::ScopedFeatureList features_;
};

namespace {

// Using localhost to avoid the HTTPS requirement for InstallableManager to even
// load the manifest.
constexpr char kTestWebAppUrl[] = "http://127.0.0.1:8000/";
constexpr char kTestWebAppHeaders[] =
    "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n";
constexpr char kTestWebAppBody[] = R"(
  <!DOCTYPE html>
  <head>
    <link rel="manifest" href="manifest.webmanifest">
    <meta http-equiv="origin-trial" content="$1">
  </head>
)";

constexpr char kTestIconUrl[] = "http://127.0.0.1:8000/icon.png";
constexpr char kTestManifestUrl[] =
    "http://127.0.0.1:8000/manifest.webmanifest";
constexpr char kTestManifestHeaders[] =
    "HTTP/1.1 200 OK\nContent-Type: application/json; charset=utf-8\n";
constexpr char kTestManifestBody[] = R"({
  "name": "Test app",
  "display": "standalone",
  "start_url": "/",
  "scope": "/",
  "icons": [{
    "src": "icon.png",
    "sizes": "192x192",
    "type": "image/png"
  }],
  "capture_links": "new-client"
})";

// Generated from script:
// $ tools/origin_trials/generate_token.py http://127.0.0.1:8000 \
// WebAppLinkCapturing --expire-timestamp=2000000000
constexpr char kOriginTrialToken[] =
    "A9FvND2pz57gueYZNHgjh4f5vPfcFyck04vOsOOO+OMqj2naHRG9RwO92Vv1C/"
    "X32R39B+"
    "EaMCn7r3imGvWVvAsAAABbeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgIm"
    "ZlYXR1cmUiOiAiV2ViQXBwTGlua0NhcHR1cmluZyIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ"
    "==";

}  // namespace

IN_PROC_BROWSER_TEST_F(WebAppDeclarativeLinkCapturingOriginTrialBrowserTest,
                       OriginTrial) {
  WebAppProvider& provider = *WebAppProvider::Get(browser()->profile());

  bool serve_token = true;
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&serve_token](
          content::URLLoaderInterceptor::RequestParams* params) -> bool {
        if (params->url_request.url.spec() == kTestWebAppUrl) {
          content::URLLoaderInterceptor::WriteResponse(
              kTestWebAppHeaders,
              base::ReplaceStringPlaceholders(
                  kTestWebAppBody, {serve_token ? kOriginTrialToken : ""},
                  nullptr),
              params->client.get());
          return true;
        }
        if (params->url_request.url.spec() == kTestManifestUrl) {
          content::URLLoaderInterceptor::WriteResponse(
              kTestManifestHeaders, kTestManifestBody, params->client.get());
          return true;
        }
        if (params->url_request.url.spec() == kTestIconUrl) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/web_apps/basic-192.png", params->client.get());
          return true;
        }
        return false;
      }));

  // Install web app with origin trial token.
  content::WebContents* app_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  AppId app_id =
      web_app::InstallWebAppFromPage(browser(), GURL(kTestWebAppUrl));

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Origin trial should grant the app access.
  EXPECT_EQ(provider.registrar().GetAppCaptureLinks(app_id),
            blink::mojom::CaptureLinks::kNewClient);

  // Open the page again with the token missing.
  {
    class UpdateAwaiter : public AppRegistrarObserver {
     public:
      UpdateAwaiter() = default;
      void AwaitUpdate() { run_loop_.Run(); }
      void OnWebAppManifestUpdated(const AppId& app_id,
                                   base::StringPiece old_name) override {
        run_loop_.Quit();
      }

     private:
      base::RunLoop run_loop_;
    } update_awaiter;
    base::ScopedObservation<AppRegistrar, AppRegistrarObserver> observer_scope(
        &update_awaiter);
    observer_scope.Observe(&provider.registrar());

    serve_token = false;
    NavigateToURLAndWait(browser(), GURL(kTestWebAppUrl));

    // Close the app window to unblock updating.
    app_web_contents->Close();

    update_awaiter.AwaitUpdate();
  }

  // The app should update to no longer have capture_links defined without the
  // origin trial.
  EXPECT_EQ(provider.registrar().GetAppCaptureLinks(app_id),
            blink::mojom::CaptureLinks::kUndefined);
#else
  // The origin trial is not available outside of Chrome OS.
  EXPECT_EQ(provider.registrar().GetAppCaptureLinks(app_id),
            blink::mojom::CaptureLinks::kUndefined);

  ALLOW_UNUSED_LOCAL(app_web_contents);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace web_app
