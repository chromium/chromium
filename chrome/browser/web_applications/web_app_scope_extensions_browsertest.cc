// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/browser/web_applications/app_service/test/loopback_crosapi_app_service_proxy.h"
#endif

namespace web_app {

#if BUILDFLAG(IS_CHROMEOS)

class WebAppScopeExtensionsBrowserTest : public WebAppNavigationBrowserTest {
 public:
  WebAppScopeExtensionsBrowserTest()
      : WebAppScopeExtensionsBrowserTest(/*enabled=*/true) {}
  explicit WebAppScopeExtensionsBrowserTest(bool enabled)
      : primary_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        secondary_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitWithFeatureState(
        blink::features::kWebAppEnableScopeExtensions, enabled);
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
    apps_util::SetSupportedLinksPreferenceAndWait(browser()->profile(), app_id);
#else
    static_assert(
        false,
        "Support WML scope_extensions link capturing once it's implemented");
#endif
  }

  bool WebAppCapturesUrl(const GURL& url) {
    CHECK_NE(url, unrelated_url_);
    NavigateToURLAndWait(browser(), unrelated_url_);

    ui_test_utils::BrowserChangeObserver browser_observer(
        /*browser=*/nullptr,
        ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
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
  absl::optional<LoopbackCrosapiAppServiceProxy> loopback_crosapi_;
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
  OsIntegrationManager::ScopedSuppressForTesting os_hooks_supress_;
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

  ASSERT_TRUE(
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
            /*link_url=*/extended_scope_url,
            /*target_url=*/extended_scope_url);

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

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app
