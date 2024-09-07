// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "components/webapps/services/web_app_origin_association/test/test_web_app_origin_association_fetcher.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/browser/web_applications/app_service/test/loopback_crosapi_app_service_proxy.h"
#endif

namespace web_app {

class WebAppScopeExtensionsBrowserTest : public WebAppNavigationBrowserTest {
 public:
  WebAppScopeExtensionsBrowserTest()
      : WebAppScopeExtensionsBrowserTest(/*enabled=*/true) {}
  explicit WebAppScopeExtensionsBrowserTest(bool enabled)
      : primary_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        secondary_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        apps::test::GetFeaturesToEnableLinkCapturingUX();
    enabled_features.emplace_back(
        features::kPwaNavigationCapturingWithScopeExtensions,
        base::FieldTrialParams());

    std::vector<base::test::FeatureRef> disabled_features;
    if (enabled) {
      enabled_features.emplace_back(
          blink::features::kWebAppEnableScopeExtensions,
          base::FieldTrialParams());
    } else {
      disabled_features.push_back(
          blink::features::kWebAppEnableScopeExtensions);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }
  ~WebAppScopeExtensionsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebAppNavigationBrowserTest::SetUpOnMainThread();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    extensions::SetEmptyAshKeeplistForTest();
    loopback_crosapi_.emplace(browser()->profile());
#endif

    primary_server_.AddDefaultHandlers(GetChromeTestDataDir());
    primary_server_.RegisterRequestHandler(
        base::BindRepeating(&WebAppScopeExtensionsBrowserTest::RequestHandler,
                            base::Unretained(this)));
    ASSERT_TRUE(primary_server_.Start());
    primary_origin_ = primary_server_.GetOrigin();

    secondary_server_.AddDefaultHandlers(GetChromeTestDataDir());
    secondary_server_.RegisterRequestHandler(
        base::BindRepeating(&WebAppScopeExtensionsBrowserTest::RequestHandler,
                            base::Unretained(this)));
    ASSERT_TRUE(secondary_server_.Start());
    secondary_origin_ = secondary_server_.GetOrigin();

    unrelated_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(unrelated_server_.Start());
    unrelated_url_ = unrelated_server_.GetURL("/simple.html");
  }

  void TearDownOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    loopback_crosapi_.reset();
#endif

    app_ = nullptr;
  }

  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    auto it = url_overrides_.find(request.GetURL());
    if (it == url_overrides_.end()) {
      return nullptr;
    }
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(it->second);
    return http_response;
  }

  WebAppProvider& provider() {
    return *WebAppProvider::GetForTest(browser()->profile());
  }

  void InstallScopeExtendedWebApp(std::string manifest_file,
                                  std::string association_file) {
    GURL manifest_url = primary_server_.GetURL("/web_apps/manifest.json");
    GURL association_url =
        secondary_server_.GetURL("/.well-known/web-app-origin-association");

    url_overrides_[manifest_url] = manifest_file;
    url_overrides_[association_url] = association_file;

    webapps::AppId app_id = InstallWebAppFromPageAndCloseAppBrowser(
        browser(),
        primary_server_.GetURL("/web_apps/get_manifest.html?manifest.json"));

    app_ = provider().registrar_unsafe().GetAppById(app_id);

    // Turn on link capturing.
#if BUILDFLAG(IS_CHROMEOS)
    apps::AppReadinessWaiter(browser()->profile(), app_id).Await();
#endif
    EXPECT_THAT(
        apps::test::EnableLinkCapturingByUser(browser()->profile(), app_id),
        base::test::HasValue());
  }

  bool WebAppCapturesUrl(const GURL& url) {
    CHECK_NE(url, unrelated_url_);
    NavigateViaLinkClickToURLAndWait(browser(), unrelated_url_);

    ui_test_utils::BrowserChangeObserver browser_observer(
        /*browser=*/nullptr,
        ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    // Note: The 'self' target will likely soon be not supported as capturable
    // on non-CrOS, so this method & it's functionality will have to change
    // slightly. https://crbug.com/339095686.
    WebAppNavigationBrowserTest::ClickLinkAndWaitForURL(
        web_contents,
        /*link_url=*/url,
        /*target_url=*/url, WebAppNavigationBrowserTest::LinkTarget::SELF,
        /*rel=*/"");

    // Navigation happened in the browser tab instead of being link captured.
    if (web_contents->GetVisibleURL() == url) {
      return false;
    }

    Browser* app_browser = browser_observer.Wait();
    EXPECT_EQ(
        app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
        url);
    chrome::CloseWindow(app_browser);
    return true;
  }

 protected:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::optional<LoopbackCrosapiAppServiceProxy> loopback_crosapi_;
#endif

  net::EmbeddedTestServer primary_server_;
  url::Origin primary_origin_;

  net::EmbeddedTestServer secondary_server_;
  url::Origin secondary_origin_;

  net::EmbeddedTestServer unrelated_server_;
  GURL unrelated_url_;

  std::map<GURL, std::string> url_overrides_;

  raw_ptr<const WebApp> app_ = nullptr;

  base::test::ScopedFeatureList feature_list_;
  content::ContentMockCertVerifier cert_verifier_;
};

IN_PROC_BROWSER_TEST_F(WebAppScopeExtensionsBrowserTest,
                       ExtendedLinkCapturingBasic) {
  InstallScopeExtendedWebApp(
      /*manifest_file=*/base::ReplaceStringPlaceholders(
          R"(
          {
            "Name": "Test app",
            "start_url": "/",
            "scope": "/",
            "scope_extensions": [{
              "origin": "$1"
            }]
          })",
          {secondary_origin_.Serialize()}, nullptr),
      /*association_file=*/base::ReplaceStringPlaceholders(
          R"(
          {
            "web_apps": [{
              "web_app_identity": "$1"
            }]
          })",
          {primary_origin_.Serialize()}, nullptr));

  EXPECT_THAT(
      app_->scope_extensions(),
      testing::ElementsAre(ScopeExtensionInfo{.origin = secondary_origin_}));
  EXPECT_EQ(app_->scope_extensions(), app_->validated_scope_extensions());

  EXPECT_TRUE(
      WebAppCapturesUrl(primary_server_.GetURL("/web_apps/basic.html")));
  EXPECT_TRUE(
      WebAppCapturesUrl(secondary_server_.GetURL("/web_apps/basic.html")));
}

IN_PROC_BROWSER_TEST_F(WebAppScopeExtensionsBrowserTest,
                       ExtendedLinkCapturingFocusExisting) {
  InstallScopeExtendedWebApp(
      /*manifest_file=*/base::ReplaceStringPlaceholders(
          R"(
          {
            "Name": "Test app",
            "start_url": "/simple.html",
            "scope": "/",
            "scope_extensions": [{
              "origin": "$1"
            }],
            "launch_handler": {
              "client_mode": "focus-existing"
            }
          })",
          {secondary_origin_.Serialize()}, nullptr),
      /*association_file=*/base::ReplaceStringPlaceholders(
          R"(
          {
            "web_apps": [{
              "web_app_identity": "$1"
            }]
          })",
          {primary_server_.GetURL("/simple.html").spec()}, nullptr));

  Browser* app_browser = LaunchWebAppBrowserAndWait(app_->app_id());
  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  // Await the first LaunchParams.
  const char* script = R"(
    window.launchParamsPromise = new Promise(resolve => {
      window.resolveLaunchParamsPromise = resolve;
    });
    launchQueue.setConsumer(launchParams => {
      window.resolveLaunchParamsPromise(launchParams.targetURL);
      window.resolveLaunchParamsPromise = null;
    });
    window.launchParamsPromise;
  )";
  EXPECT_EQ(EvalJs(app_web_contents, script).ExtractString(),
            app_->start_url().spec());

  // Set up the next LaunchParams promise.
  script = R"(
    window.launchParamsPromise = new Promise(resolve => {
      window.resolveLaunchParamsPromise = resolve;
    });
    true;
  )";
  EXPECT_TRUE(EvalJs(app_web_contents, script).ExtractBool());

  // Link capture an extended scope URL.
  GURL extended_scope_url =
      secondary_server_.GetURL("/url/that/does/not/get/navigated/to");
  ClickLink(browser()->tab_strip_model()->GetActiveWebContents(),
            /*link_url=*/extended_scope_url);

  // Await the second LaunchParams in the same app document.
  EXPECT_EQ(
      EvalJs(app_web_contents, "window.launchParamsPromise").ExtractString(),
      extended_scope_url.spec());
  // The document should not have navigated due to "focus-existing".
  EXPECT_EQ(app_web_contents->GetVisibleURL(), app_->start_url().spec());
}

IN_PROC_BROWSER_TEST_F(WebAppScopeExtensionsBrowserTest,
                       ExtendedLinkCapturingBadAssociationFile) {
  InstallScopeExtendedWebApp(
      /*manifest_file=*/base::ReplaceStringPlaceholders(
          R"(
          {
            "Name": "Test app",
            "start_url": "/",
            "scope": "/",
            "scope_extensions": [{
              "origin": "$1"
            }]
          })",
          {secondary_origin_.Serialize()}, nullptr),
      /*association_file=*/"garbage");

  EXPECT_TRUE(
      WebAppCapturesUrl(primary_server_.GetURL("/web_apps/basic.html")));
  EXPECT_FALSE(
      WebAppCapturesUrl(secondary_server_.GetURL("/web_apps/basic.html")));
}

class WebAppScopeExtensionsDisabledBrowserTest
    : public WebAppScopeExtensionsBrowserTest {
 public:
  WebAppScopeExtensionsDisabledBrowserTest()
      : WebAppScopeExtensionsBrowserTest(/*enabled=*/false) {}
};

IN_PROC_BROWSER_TEST_F(WebAppScopeExtensionsDisabledBrowserTest,
                       NoExtendedLinkCapturing) {
  InstallScopeExtendedWebApp(
      /*manifest_file=*/base::ReplaceStringPlaceholders(
          R"(
          {
            "Name": "Test app",
            "start_url": "/",
            "scope": "/",
            "scope_extensions": [{
              "origin": "$1"
            }]
          })",
          {secondary_origin_.Serialize()}, nullptr),
      /*association_file=*/base::ReplaceStringPlaceholders(
          R"(
          {
            "web_apps": [{
              "web_app_identity": "$1"
            }]
          })",
          {primary_origin_.Serialize()}, nullptr));

  EXPECT_TRUE(app_->scope_extensions().empty());
  EXPECT_TRUE(app_->validated_scope_extensions().empty());

  ASSERT_TRUE(
      WebAppCapturesUrl(primary_server_.GetURL("/web_apps/basic.html")));
  EXPECT_FALSE(
      WebAppCapturesUrl(secondary_server_.GetURL("/web_apps/basic.html")));
}

class WebAppScopeExtensionsOriginTrialBrowserTest
    : public WebAppBrowserTestBase {
 public:
  WebAppScopeExtensionsOriginTrialBrowserTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kWebAppEnableScopeExtensions);
  }
  ~WebAppScopeExtensionsOriginTrialBrowserTest() override = default;

  // WebAppBrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Using the test public key from docs/origin_trials_integration.md#Testing.
    command_line->AppendSwitchASCII(
        embedder_support::kOriginTrialPublicKey,
        "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=");
  }
  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    web_app::test::WaitUntilReady(
        web_app::WebAppProvider::GetForTest(browser()->profile()));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};
namespace {

// InstallableManager requires https or localhost to load the manifest. Go with
// localhost to avoid having to set up cert servers.
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
  "display_override": ["tabbed"],
  "start_url": "/",
  "scope": "/",
  "icons": [{
    "src": "icon.png",
    "sizes": "192x192",
    "type": "image/png"
  }],
  "scope_extensions": [
    {
      "origin": "https://test.com"
    }
  ]
})";
constexpr char kTestAssociatedOrigin[] = "https://test.com/";
constexpr char kTestOriginAssociationFile[] = R"({
  "web_apps": [{
    "web_app_identity": "http://127.0.0.1:8000/"
  }]
})";

// Generated from script:
// $ tools/origin_trials/generate_token.py http://127.0.0.1:8000
// "WebAppScopeExtensions" --expire-timestamp=2000000000
constexpr char kOriginTrialToken[] =
    "A6wt8IeZJ7M9rThrMsExahxtxjgVGPp1f2k6AdCzj2+Nl+"
    "74sf4z9YYU1ChSCI5qDFf44q3Lff42UnCCbUunwQQAAABdeyJvcmlnaW4iOiAiaHR0cDovLzEy"
    "Ny4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiV2ViQXBwU2NvcGVFeHRlbnNpb25zIiwgImV4cG"
    "lyeSI6IDIwMDAwMDAwMDB9";

}  // namespace

IN_PROC_BROWSER_TEST_F(WebAppScopeExtensionsOriginTrialBrowserTest,
                       OriginTrial) {
  ManifestUpdateManager::ScopedBypassWindowCloseWaitingForTesting
      bypass_window_close_waiting;
  WebAppProvider& provider = *WebAppProvider::GetForTest(browser()->profile());

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
  auto origin_association_fetcher =
      std::make_unique<webapps::TestWebAppOriginAssociationFetcher>();
  origin_association_fetcher->SetData(
      {{url::Origin::Create(GURL(kTestAssociatedOrigin)),
        kTestOriginAssociationFile}});
  provider.origin_association_manager().SetFetcherForTest(
      std::move(origin_association_fetcher));

  // Install web app with origin trial token.
  webapps::AppId app_id =
      web_app::InstallWebAppFromPage(browser(), GURL(kTestWebAppUrl));

  // Origin trial should grant the app access.
  base::flat_set<ScopeExtensionInfo> expected_scope_extensions = {
      ScopeExtensionInfo(url::Origin::Create(GURL(kTestAssociatedOrigin)),
                         /*has_origin_wildcard=*/false)};
  EXPECT_EQ(expected_scope_extensions,
            provider.registrar_unsafe().GetValidatedScopeExtensions(app_id));
  EXPECT_TRUE(provider.registrar_unsafe().IsUrlInAppExtendedScope(
      GURL(kTestAssociatedOrigin), app_id));

  // Out of scope bar should not be shown for extended scope.
  Browser* app_browser = LaunchWebAppToURL(browser()->profile(), app_id,
                                           GURL(kTestAssociatedOrigin));
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());

  // Open the page again with the token missing.
  {
    UpdateAwaiter update_awaiter(provider.install_manager());
    serve_token = false;
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTestWebAppUrl)));
    update_awaiter.AwaitUpdate();
  }

  // The app should update to no longer parsing scope_extensions without the
  // origin trial active.
  EXPECT_TRUE(provider.registrar_unsafe().GetScopeExtensions(app_id).empty());
  EXPECT_FALSE(provider.registrar_unsafe().IsUrlInAppExtendedScope(
      GURL(kTestAssociatedOrigin), app_id));
}

}  // namespace web_app
