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

namespace web_app {

class WebAppScopeExtensionsBrowserTest
    : public WebAppNavigationBrowserTest,
      public testing::WithParamInterface<
          apps::test::LinkCapturingFeatureVersion> {
 public:
  WebAppScopeExtensionsBrowserTest()
      : WebAppScopeExtensionsBrowserTest(/*enabled=*/true) {}

  explicit WebAppScopeExtensionsBrowserTest(bool enabled)
      : primary_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        secondary_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        apps::test::GetFeaturesToEnableLinkCapturingUX(GetParam());
    enabled_features.emplace_back(
        features::kPwaNavigationCapturingWithScopeExtensions,
        base::FieldTrialParams());

    feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }
  ~WebAppScopeExtensionsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebAppNavigationBrowserTest::SetUpOnMainThread();

    primary_server_.AddDefaultHandlers(GetChromeTestDataDir());
    primary_server_.RegisterRequestHandler(
        base::BindRepeating(&WebAppScopeExtensionsBrowserTest::RequestHandler,
                            base::Unretained(this)));
    ASSERT_TRUE(primary_server_.Start());
    primary_origin_ = primary_server_.GetOrigin();
    primary_scope_ = primary_server_.GetURL("/web_apps/basic.html");

    secondary_server_.AddDefaultHandlers(GetChromeTestDataDir());
    secondary_server_.RegisterRequestHandler(
        base::BindRepeating(&WebAppScopeExtensionsBrowserTest::RequestHandler,
                            base::Unretained(this)));
    ASSERT_TRUE(secondary_server_.Start());
    secondary_origin_ = secondary_server_.GetOrigin();
    secondary_scope_ = secondary_server_.GetURL("/web_apps/basic.html");

    unrelated_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(unrelated_server_.Start());
    unrelated_url_ = unrelated_server_.GetURL("/simple.html");
  }

  void TearDownOnMainThread() override { app_ = nullptr; }

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

    ui_test_utils::BrowserCreatedObserver browser_created_observer;

    // This always creates a new top level browsing context which is essential
    // to trigger navigation capturing.
    WebAppNavigationBrowserTest::ClickLinkAndWaitForURL(
        browser()->tab_strip_model()->GetActiveWebContents(),
        /*link_url=*/url,
        /*target_url=*/url, WebAppNavigationBrowserTest::LinkTarget::BLANK,
        /*rel=*/"");

    // If `ClickLinkAndWaitForURL()` does not perform navigation capturing, then
    // it will open a new tab in the same browser, and the active web contents
    // will change.
    if (browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL() ==
        url) {
      return false;
    }

    Browser* app_browser = browser_created_observer.Wait();
    EXPECT_EQ(
        app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
        url);
    chrome::CloseWindow(app_browser);
    return true;
  }

 protected:
  net::EmbeddedTestServer primary_server_;
  url::Origin primary_origin_;
  GURL primary_scope_;

  net::EmbeddedTestServer secondary_server_;
  url::Origin secondary_origin_;
  GURL secondary_scope_;

  net::EmbeddedTestServer unrelated_server_;
  GURL unrelated_url_;

  std::map<GURL, std::string> url_overrides_;

  raw_ptr<const WebApp> app_ = nullptr;

  base::test::ScopedFeatureList feature_list_;
  content::ContentMockCertVerifier cert_verifier_;
};

IN_PROC_BROWSER_TEST_P(WebAppScopeExtensionsBrowserTest,
                       ExtendedLinkCapturingProperlyLimitsScope) {
  InstallScopeExtendedWebApp(
      /*manifest_file=*/base::ReplaceStringPlaceholders(
          R"(
          {
            "Name": "Test app",
            "start_url": "/",
            "scope": "/",
            "scope_extensions": [{
              "type": "origin", "origin": "$1"
            }]
          })",
          {secondary_origin_.Serialize()}, nullptr),
      /*association_file=*/base::ReplaceStringPlaceholders(
          R"(
          {
            "$1" : { "scope": "/scope-limiter" }
          })",
          {primary_origin_.Serialize()}, nullptr));

  EXPECT_THAT(app_->scope_extensions(),
              testing::ElementsAre(
                  ScopeExtensionInfo::CreateForOrigin(secondary_origin_)));

  // We expect that validated scope extensions differ from the requested
  // scope_extension defined in the app manifest.
  EXPECT_NE(app_->scope_extensions(), app_->validated_scope_extensions());
  GURL limited_scope(secondary_origin_.Serialize() + "/scope-limiter");
  EXPECT_THAT(
      app_->validated_scope_extensions(),
      testing::ElementsAre(ScopeExtensionInfo::CreateForScope(limited_scope)));

  // primary_server_ is the web app's server
  GURL primary_server_launch_url =
      primary_server_.GetURL("/web_apps/basic.html");
  EXPECT_TRUE(WebAppCapturesUrl(primary_server_launch_url));

  // secondary_server_ is the associate's server. We expect this navigation to
  // not capture since it is not in the extended scope "/scope-limiter"
  GURL secondary_server_launch_url =
      secondary_server_.GetURL("/web_apps/basic.html");
  EXPECT_FALSE(WebAppCapturesUrl(secondary_server_launch_url));
}

IN_PROC_BROWSER_TEST_P(WebAppScopeExtensionsBrowserTest,
                       ExtendedLinkCapturingBasic) {
  InstallScopeExtendedWebApp(
      /*manifest_file=*/base::ReplaceStringPlaceholders(
          R"(
          {
            "Name": "Test app",
            "start_url": "/",
            "scope": "/",
            "scope_extensions": [{
              "type": "origin", "origin": "$1"
            }]
          })",
          {secondary_origin_.Serialize()}, nullptr),
      /*association_file=*/base::ReplaceStringPlaceholders(
          R"(
          {
            "$1" : { "scope": "/web_apps/basic.html" }
          })",
          {primary_origin_.Serialize()}, nullptr));

  EXPECT_THAT(app_->scope_extensions(),
              testing::ElementsAre(
                  ScopeExtensionInfo::CreateForOrigin(secondary_origin_)));

  EXPECT_THAT(app_->validated_scope_extensions(),
              testing::ElementsAre(
                  ScopeExtensionInfo::CreateForScope(secondary_scope_)));

  EXPECT_TRUE(
      WebAppCapturesUrl(primary_server_.GetURL("/web_apps/basic.html")));
  EXPECT_TRUE(
      WebAppCapturesUrl(secondary_server_.GetURL("/web_apps/basic.html")));
}

IN_PROC_BROWSER_TEST_P(WebAppScopeExtensionsBrowserTest,
                       ExtendedLinkCapturingFocusExisting) {
  InstallScopeExtendedWebApp(
      /*manifest_file=*/base::ReplaceStringPlaceholders(
          R"(
          {
            "Name": "Test app",
            "start_url": "/simple.html",
            "scope": "/",
            "scope_extensions": [{
              "type": "origin", "origin": "$1"
            }],
            "launch_handler": {
              "client_mode": "focus-existing"
            }
          })",
          {secondary_origin_.Serialize()}, nullptr),
      /*association_file=*/base::ReplaceStringPlaceholders(
          R"(
          {
            "$1" : {}
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
            /*link_url=*/extended_scope_url, LinkTarget::BLANK);

  // Await the second LaunchParams in the same app document.
  EXPECT_EQ(
      EvalJs(app_web_contents, "window.launchParamsPromise").ExtractString(),
      extended_scope_url.spec());
  // The document should not have navigated due to "focus-existing".
  EXPECT_EQ(app_web_contents->GetVisibleURL(), app_->start_url().spec());
}

IN_PROC_BROWSER_TEST_P(WebAppScopeExtensionsBrowserTest,
                       ExtendedLinkCapturingBadAssociationFile) {
  InstallScopeExtendedWebApp(
      /*manifest_file=*/base::ReplaceStringPlaceholders(
          R"(
          {
            "Name": "Test app",
            "start_url": "/",
            "scope": "/",
            "scope_extensions": [{
              "type": "origin", "origin": "$1"
            }]
          })",
          {secondary_origin_.Serialize()}, nullptr),
      /*association_file=*/"garbage");

  EXPECT_TRUE(
      WebAppCapturesUrl(primary_server_.GetURL("/web_apps/basic.html")));
  EXPECT_FALSE(
      WebAppCapturesUrl(secondary_server_.GetURL("/web_apps/basic.html")));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppScopeExtensionsBrowserTest,
#if BUILDFLAG(IS_CHROMEOS)
    testing::Values(apps::test::LinkCapturingFeatureVersion::kV1DefaultOff,
                    apps::test::LinkCapturingFeatureVersion::kV2DefaultOff)
#else
    testing::Values(apps::test::LinkCapturingFeatureVersion::kV2DefaultOff,
                    apps::test::LinkCapturingFeatureVersion::kV2DefaultOn)
#endif  // BUILDFLAG(IS_CHROMEOS)
        ,
    apps::test::LinkCapturingVersionToString);

class WebAppScopeExtensionsDisabledBrowserTest
    : public WebAppScopeExtensionsBrowserTest {
 public:
  WebAppScopeExtensionsDisabledBrowserTest()
      : WebAppScopeExtensionsBrowserTest(/*enabled=*/false) {}
};

IN_PROC_BROWSER_TEST_P(WebAppScopeExtensionsDisabledBrowserTest,
                       ExtendedLinkCapturing) {
  InstallScopeExtendedWebApp(
      /*manifest_file=*/base::ReplaceStringPlaceholders(
          R"(
          {
            "Name": "Test app",
            "start_url": "/",
            "scope": "/",
            "scope_extensions": [{
              "type": "origin", "origin": "$1"
            }]
          })",
          {secondary_origin_.Serialize()}, nullptr),
      /*association_file=*/base::ReplaceStringPlaceholders(
          R"(
          {
            "$1" : {}
          })",
          {primary_origin_.Serialize()}, nullptr));

  EXPECT_FALSE(app_->scope_extensions().empty());
  EXPECT_FALSE(app_->validated_scope_extensions().empty());

  ASSERT_TRUE(
      WebAppCapturesUrl(primary_server_.GetURL("/web_apps/basic.html")));
  EXPECT_TRUE(
      WebAppCapturesUrl(secondary_server_.GetURL("/web_apps/basic.html")));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppScopeExtensionsDisabledBrowserTest,
#if BUILDFLAG(IS_CHROMEOS)
    testing::Values(apps::test::LinkCapturingFeatureVersion::kV1DefaultOff,
                    apps::test::LinkCapturingFeatureVersion::kV2DefaultOff)
#else
    testing::Values(apps::test::LinkCapturingFeatureVersion::kV2DefaultOff,
                    apps::test::LinkCapturingFeatureVersion::kV2DefaultOn)
#endif  // BUILDFLAG(IS_CHROMEOS)
        ,
    apps::test::LinkCapturingVersionToString);

}  // namespace web_app
