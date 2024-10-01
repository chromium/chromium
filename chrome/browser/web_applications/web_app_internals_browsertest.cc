// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals.mojom.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals_handler.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals_ui.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_server_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Pointee;

constexpr char kBadIconErrorTemplate[] = R"({
   "!url": "$1banners/manifest_test_page.html",
   "background_installation": false,
   "install_surface": 15,
   "stages": [ {
      "!stage": "OnIconsRetrieved",
      "icons_downloaded_result": "Completed",
      "icons_http_results": [ {
         "http_code_desc": "Not Found",
         "http_status_code": 404,
         "icon_size": "0x0",
         "icon_url": "$1banners/bad_icon.png"
      }, {
         "http_code_desc": "Not Found",
         "http_status_code": 404,
         "icon_size": "0x0",
         "icon_url": "$1favicon.ico"
      } ],
      "is_generated_icon": true
   } ]
}
)";

// Drops all CR and LF characters.
std::string TrimLineEndings(std::string_view text) {
  return base::CollapseWhitespaceASCII(
      text,
      /*trim_sequences_with_line_breaks=*/true);
}

}  // namespace

class WebAppInternalsBrowserTest : public WebAppBrowserTestBase {
 public:
  WebAppInternalsBrowserTest() = default;
  WebAppInternalsBrowserTest(const WebAppInternalsBrowserTest&) = delete;
  WebAppInternalsBrowserTest& operator=(const WebAppInternalsBrowserTest&) =
      delete;

  ~WebAppInternalsBrowserTest() override = default;

  void SetUp() override {
    embedded_test_server()->AddDefaultHandlers(GetChromeTestDataDir());
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&WebAppInternalsBrowserTest::RequestHandlerOverride,
                            base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    WebAppBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    test::WaitUntilReady(WebAppProvider::GetForTest(browser()->profile()));
    WebAppBrowserTestBase::SetUpOnMainThread();
  }

  webapps::AppId InstallWebApp(const GURL& app_url) {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

    webapps::AppId app_id;
    base::RunLoop run_loop;
    GetProvider().scheduler().FetchManifestAndInstall(
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
        base::BindOnce(test::TestAcceptDialogCallback),
        base::BindLambdaForTesting([&](const webapps::AppId& new_app_id,
                                       webapps::InstallResultCode code) {
          EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
          app_id = new_app_id;
          run_loop.Quit();
        }),
        FallbackBehavior::kAllowFallbackDataAlways);

    run_loop.Run();
    return app_id;
  }

  WebAppProvider& GetProvider() {
    return *WebAppProvider::GetForTest(browser()->profile());
  }

  std::unique_ptr<net::test_server::HttpResponse> RequestHandlerOverride(
      const net::test_server::HttpRequest& request) {
    if (request_override_)
      return request_override_.Run(request);
    return nullptr;
  }

  void OverrideHttpRequest(GURL url, net::HttpStatusCode http_status_code) {
    request_override_ = base::BindLambdaForTesting(
        [url = std::move(url),
         http_status_code](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.GetURL() != url)
            return nullptr;
          auto http_response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          http_response->set_code(http_status_code);
          return std::move(http_response);
        });
  }

 private:
  net::EmbeddedTestServer::HandleRequestCallback request_override_;

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kRecordWebAppDebugInfo};
};

IN_PROC_BROWSER_TEST_F(WebAppInternalsBrowserTest,
                       PRE_InstallManagerErrorsPersist) {
  OverrideHttpRequest(embedded_test_server()->GetURL("/banners/bad_icon.png"),
                      net::HTTP_NOT_FOUND);

  webapps::AppId app_id = InstallWebApp(embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest=manifest_bad_icon.json"));

  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(web_app);
  EXPECT_TRUE(web_app->is_generated_icon());

  const std::string expected_error = base::ReplaceStringPlaceholders(
      kBadIconErrorTemplate, {embedded_test_server()->base_url().spec()},
      nullptr);

  ASSERT_TRUE(GetProvider().install_manager().error_log());
  ASSERT_EQ(1u, GetProvider().install_manager().error_log()->size());

  const base::Value& error_log =
      (*GetProvider().install_manager().error_log())[0];
  EXPECT_TRUE(error_log.is_dict());
  EXPECT_EQ(4u, error_log.GetDict().size());

  EXPECT_EQ(TrimLineEndings(expected_error),
            TrimLineEndings(error_log.DebugString()));
}

IN_PROC_BROWSER_TEST_F(WebAppInternalsBrowserTest,
                       InstallManagerErrorsPersist) {
  test::WaitUntilReady(WebAppProvider::GetForTest(browser()->profile()));

  ASSERT_TRUE(GetProvider().install_manager().error_log());
  ASSERT_EQ(1u, GetProvider().install_manager().error_log()->size());

  const base::Value& error_log =
      (*GetProvider().install_manager().error_log())[0];
  EXPECT_TRUE(error_log.is_dict());
  EXPECT_EQ(4u, error_log.GetDict().size());

  // Parses base url from the log: the port for embedded_test_server() changes
  // on every test run.
  const std::string* url_value = error_log.GetDict().FindString("!url");
  ASSERT_TRUE(url_value);
  GURL url{*url_value};
  ASSERT_TRUE(url.is_valid());

  const std::string expected_error = base::ReplaceStringPlaceholders(
      kBadIconErrorTemplate, {url.GetWithEmptyPath().spec()}, nullptr);

  EXPECT_EQ(TrimLineEndings(expected_error),
            TrimLineEndings(error_log.DebugString()));
}

class WebAppInternalsIwaInstallationBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 protected:
  WebAppInternalsHandler* OpenWebAppInternals() {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL("chrome://web-app-internals")));
    return static_cast<WebAppInternalsUI*>(browser()
                                               ->tab_strip_model()
                                               ->GetActiveWebContents()
                                               ->GetWebUI()
                                               ->GetController())
        ->GetHandlerForTesting();
  }

  IsolatedWebAppUpdateServerMixin update_server_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(WebAppInternalsIwaInstallationBrowserTest,
                       FetchUpdateManifestAndInstallIwaAndUpdate) {
  update_server_mixin_.AddBundle(
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
          .BuildBundle(test::GetDefaultEd25519KeyPair()));

  auto* handler = OpenWebAppInternals();

  const GURL& update_manifest_url = update_server_mixin_.GetUpdateManifestUrl(
      test::GetDefaultEd25519WebBundleId());
  base::test::TestFuture<::mojom::ParseUpdateManifestFromUrlResultPtr>
      um_future;
  handler->ParseUpdateManifestFromUrl(update_manifest_url,
                                      um_future.GetCallback());

  auto um_result = um_future.Take();
  ASSERT_TRUE(um_result->is_update_manifest());

  const auto& update_manifest = *um_result->get_update_manifest();

  ASSERT_THAT(update_manifest,
              Field(&::mojom::UpdateManifest::versions,
                    ElementsAre(Pointee(
                        Field(&::mojom::VersionEntry::version, Eq("1.0.0"))))));

  const GURL& web_bundle_url = update_manifest.versions[0]->web_bundle_url;

  base::test::TestFuture<::mojom::InstallIsolatedWebAppResultPtr>
      install_future;
  auto params = ::mojom::InstallFromBundleUrlParams::New();
  params->web_bundle_url = web_bundle_url;
  params->update_manifest_url = update_manifest_url;
  handler->InstallIsolatedWebAppFromBundleUrl(std::move(params),
                                              install_future.GetCallback());
  ASSERT_TRUE(install_future.Take()->is_success());

  webapps::AppId app_id = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                              test::GetDefaultEd25519WebBundleId())
                              .app_id();
  {
    ASSERT_OK_AND_ASSIGN(
        const WebApp& iwa,
        GetIsolatedWebAppById(provider().registrar_unsafe(), app_id));

    EXPECT_EQ(iwa.isolation_data()->version(), base::Version("1.0.0"));
    EXPECT_EQ(iwa.isolation_data()->update_manifest_url(), update_manifest_url);
  }

  // Run an update check on the same manifest.
  {
    base::test::TestFuture<std::string> update_future;
    handler->UpdateManifestInstalledIsolatedWebApp(
        app_id, update_future.GetCallback<const std::string&>());
    EXPECT_THAT(update_future.Get(),
                HasSubstr("app is already on the latest version"));
  }

  // Now add a new entry to the manifest and re-run the update check.
  update_server_mixin_.AddBundle(
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("2.0.0"))
          .BuildBundle(test::GetDefaultEd25519KeyPair()));
  {
    base::test::TestFuture<std::string> update_future;
    handler->UpdateManifestInstalledIsolatedWebApp(
        app_id, update_future.GetCallback<const std::string&>());
    EXPECT_THAT(update_future.Get(), HasSubstr("Update to v2.0.0 successful"));

    ASSERT_OK_AND_ASSIGN(
        const WebApp& iwa,
        GetIsolatedWebAppById(provider().registrar_unsafe(), app_id));

    EXPECT_EQ(iwa.isolation_data()->version(), base::Version("2.0.0"));
    EXPECT_EQ(iwa.isolation_data()->update_manifest_url(), update_manifest_url);
  }
}

IN_PROC_BROWSER_TEST_F(WebAppInternalsIwaInstallationBrowserTest,
                       ParseUpdateManifestFromUrlFailsWithIncorrectUrl) {
  auto* handler = OpenWebAppInternals();

  base::test::TestFuture<::mojom::ParseUpdateManifestFromUrlResultPtr>
      um_future;

  // Select some dummy URL that certainly doesn't host an update manifest.
  handler->ParseUpdateManifestFromUrl(GURL("https://example.com"),
                                      um_future.GetCallback());
  ASSERT_TRUE(um_future.Take()->is_error());
}

IN_PROC_BROWSER_TEST_F(
    WebAppInternalsIwaInstallationBrowserTest,
    InstallIsolatedWebAppFromBundleUrlFailsWithIncorrectUrl) {
  auto* handler = OpenWebAppInternals();

  base::test::TestFuture<::mojom::InstallIsolatedWebAppResultPtr>
      install_future;
  auto params = ::mojom::InstallFromBundleUrlParams::New();

  // Select some dummy URL that certainly doesn't host a web bundle.
  params->web_bundle_url = GURL("https://example.com");
  handler->InstallIsolatedWebAppFromBundleUrl(std::move(params),
                                              install_future.GetCallback());
  ASSERT_TRUE(install_future.Take()->is_error());
}

}  // namespace web_app
