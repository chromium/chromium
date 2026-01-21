// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <map>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// Map of mime_type to file_extensions required for file handling
using AcceptMap = std::map<std::string, base::flat_set<std::string>>;

std::vector<apps::FileHandler::AcceptEntry> GetAcceptEntriesForFileHandler(
    const AcceptMap& accept_map) {
  std::vector<apps::FileHandler::AcceptEntry> entries;
  for (const auto& elem : accept_map) {
    apps::FileHandler::AcceptEntry accept_entry;
    accept_entry.mime_type = elem.first;
    accept_entry.file_extensions.insert(elem.second.begin(), elem.second.end());
    entries.push_back(accept_entry);
  }
  return entries;
}

}  // namespace

class ExternalAppResolutionCommandBrowserTest : public WebAppBrowserTestBase {
 public:
  ExternalAppResolutionCommandBrowserTest() = default;
  ExternalAppResolutionCommandBrowserTest(
      const ExternalAppResolutionCommandBrowserTest&) = delete;
  ExternalAppResolutionCommandBrowserTest& operator=(
      const ExternalAppResolutionCommandBrowserTest&) = delete;
};

IN_PROC_BROWSER_TEST_F(ExternalAppResolutionCommandBrowserTest,
                       BasicInstallCommand) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  // TODO(crbug.com/381408483): Review usage of ExternalInstallOptions in tests.
  ExternalInstallOptions install_options(
      kWebAppUrl,
      /*user_display_mode=*/std::nullopt,
      ExternalInstallSource::kInternalDefault);

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  provider().scheduler().InstallExternallyManagedApp(
      install_options,
      /*installed_placeholder_app_id=*/std::nullopt, future.GetCallback());

  const ExternallyManagedAppManager::InstallResult& result =
      future.Get<ExternallyManagedAppManager::InstallResult>();
  const webapps::AppId& app_id = *result.app_id;
  webapps::InstallResultCode install_code = result.code;
  EXPECT_EQ(install_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));
}

IN_PROC_BROWSER_TEST_F(ExternalAppResolutionCommandBrowserTest,
                       ExternalInstallWindowMode) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  // TODO(crbug.com/381408483): Review usage of ExternalInstallOptions in tests.
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalDefault);
  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  provider().scheduler().InstallExternallyManagedApp(
      install_options,
      /*installed_placeholder_app_id=*/std::nullopt, future.GetCallback());

  const ExternallyManagedAppManager::InstallResult& result =
      future.Get<ExternallyManagedAppManager::InstallResult>();
  const webapps::AppId& app_id = *result.app_id;
  webapps::InstallResultCode install_code = result.code;
  EXPECT_EQ(install_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));
  EXPECT_EQ(
      mojom::UserDisplayMode::kStandalone,
      provider().registrar_unsafe().GetAppUserDisplayMode(app_id).value());
}

IN_PROC_BROWSER_TEST_F(ExternalAppResolutionCommandBrowserTest,
                       ExternalInstallBrowserMode) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  // TODO(crbug.com/381408483): Review usage of ExternalInstallOptions in tests.
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kBrowser,
      ExternalInstallSource::kInternalDefault);

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  provider().scheduler().InstallExternallyManagedApp(
      install_options,
      /*installed_placeholder_app_id=*/std::nullopt, future.GetCallback());

  const ExternallyManagedAppManager::InstallResult& result =
      future.Get<ExternallyManagedAppManager::InstallResult>();
  const webapps::AppId& app_id = *result.app_id;
  webapps::InstallResultCode install_code = result.code;
  EXPECT_EQ(install_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));
  EXPECT_EQ(
      mojom::UserDisplayMode::kBrowser,
      provider().registrar_unsafe().GetAppUserDisplayMode(app_id).value());
}

IN_PROC_BROWSER_TEST_F(ExternalAppResolutionCommandBrowserTest,
                       InstallAppFromPolicy) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");

  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kBrowser,
      ExternalInstallSource::kExternalPolicy);
  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;

  provider().scheduler().InstallExternallyManagedApp(
      install_options,
      /*installed_placeholder_app_id=*/std::nullopt, future.GetCallback());

  const ExternallyManagedAppManager::InstallResult& result =
      future.Get<ExternallyManagedAppManager::InstallResult>();
  const webapps::AppId& app_id = *result.app_id;
  webapps::InstallResultCode install_code = result.code;
  EXPECT_EQ(install_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));
  EXPECT_TRUE(
      provider().registrar_unsafe().GetAppById(app_id)->IsPolicyInstalledApp());
}

IN_PROC_BROWSER_TEST_F(ExternalAppResolutionCommandBrowserTest,
                       InstallFailsWithInvalidManifest) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "invalid_manifest_test_page.html");

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kBrowser,
      ExternalInstallSource::kExternalPolicy);
  // This should force the install_params to have a valid manifest, otherwise
  // install will not happen.
  install_options.require_manifest = true;

  provider().scheduler().InstallExternallyManagedApp(
      install_options,
      /*installed_placeholder_app_id=*/std::nullopt, future.GetCallback());

  const ExternallyManagedAppManager::InstallResult& result =
      future.Get<ExternallyManagedAppManager::InstallResult>();
  const webapps::AppId& app_id = GenerateAppId(std::nullopt, kWebAppUrl);
  webapps::InstallResultCode install_code = result.code;
  EXPECT_EQ(install_code,
            webapps::InstallResultCode::kNotValidManifestForWebApp);
  EXPECT_FALSE(
      provider().registrar_unsafe().GetInstallState(app_id).has_value());
}

IN_PROC_BROWSER_TEST_F(
    ExternalAppResolutionCommandBrowserTest,
    DISABLED_ExternalInstallSourceReinstallOverrideManifestData) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult>
      future_first_install;
  // TODO(crbug.com/381408483): Review usage of ExternalInstallOptions in tests.
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kBrowser,
      ExternalInstallSource::kInternalDefault);

  provider().scheduler().InstallExternallyManagedApp(
      install_options,
      /*installed_placeholder_app_id=*/std::nullopt,
      future_first_install.GetCallback());

  const ExternallyManagedAppManager::InstallResult& first_result =
      future_first_install.Get<0>();
  const webapps::AppId& first_app_id = *first_result.app_id;
  webapps::InstallResultCode first_install_code = first_result.code;
  EXPECT_EQ(first_install_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      first_app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));
  EXPECT_EQ(mojom::UserDisplayMode::kBrowser,
            provider()
                .registrar_unsafe()
                .GetAppUserDisplayMode(first_app_id)
                .value());

  // Now install the same web_app with a different manifest (with updated file
  // handler information) and a different install_url.
  const GURL kWebAppUrlDifferentManifest = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_with_file_handlers.json");
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), kWebAppUrlDifferentManifest));

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult>
      future_second_install;
  ExternalInstallOptions install_options_policy(
      kWebAppUrlDifferentManifest, mojom::UserDisplayMode::kBrowser,
      ExternalInstallSource::kExternalPolicy);

  provider().scheduler().InstallExternallyManagedApp(
      install_options_policy,
      /*installed_placeholder_app_id=*/std::nullopt,
      future_second_install.GetCallback());

  const ExternallyManagedAppManager::InstallResult& second_result =
      future_second_install.Get<0>();
  const webapps::AppId& second_app_id = *second_result.app_id;
  webapps::InstallResultCode second_install_code = second_result.code;
  EXPECT_EQ(first_app_id, second_app_id);
  EXPECT_EQ(second_install_code,
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      second_app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));
  EXPECT_EQ(mojom::UserDisplayMode::kBrowser,
            provider()
                .registrar_unsafe()
                .GetAppUserDisplayMode(second_app_id)
                .value());

  // Verify that the file handlers are correctly updated after a
  // second installation. The file handlers should match the ones in
  // manifest_with_file_handlers.json.
  const apps::FileHandlers& handlers =
      *provider().registrar_unsafe().GetAppFileHandlers(second_app_id);
  EXPECT_EQ(2u, handlers.size());

  // Verify the first file handler matches.
  EXPECT_EQ(https_server()->GetURL("/open-foo"), handlers[0].action);
  EXPECT_EQ(
      GetAcceptEntriesForFileHandler(
          {{"application/foo", {".foo"}}, {"application/foobar", {".foobar"}}}),
      handlers[0].accept);

  // Verify the second file handler matches.
  EXPECT_EQ(https_server()->GetURL("/open-bar"), handlers[1].action);
  EXPECT_EQ(
      GetAcceptEntriesForFileHandler({{"application/bar", {".bar", ".baz"}}}),
      handlers[1].accept);
}

IN_PROC_BROWSER_TEST_F(ExternalAppResolutionCommandBrowserTest,
                       UserInstallReinstallOverrideManifestData) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  // Mock an user installing an app by clicking on the omnibox.
  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      future_first_install;
  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      base::BindOnce(test::TestAcceptDialogCallback),
      future_first_install.GetCallback(),
      FallbackBehavior::kCraftedManifestOnly);

  const webapps::AppId& first_app_id = future_first_install.Get<0>();
  webapps::InstallResultCode first_install_code = future_first_install.Get<1>();
  EXPECT_EQ(first_install_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      first_app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));

  // Mock installation of the same web_app but with a different install URL
  // and updated manifest values.
  const GURL kWebAppUrlDifferentManifest = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_with_file_handlers.json");
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), kWebAppUrlDifferentManifest));

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult>
      future_second_install;
  ExternalInstallOptions install_options_policy(
      kWebAppUrlDifferentManifest, mojom::UserDisplayMode::kBrowser,
      ExternalInstallSource::kExternalPolicy);

  provider().scheduler().InstallExternallyManagedApp(
      install_options_policy,
      /*installed_placeholder_app_id=*/std::nullopt,
      future_second_install.GetCallback());

  const ExternallyManagedAppManager::InstallResult& second_result =
      future_second_install.Get<0>();
  const webapps::AppId& second_app_id = *second_result.app_id;
  webapps::InstallResultCode second_install_code = second_result.code;
  EXPECT_EQ(first_app_id, second_app_id);
  EXPECT_EQ(second_install_code,
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      second_app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));
  EXPECT_EQ(mojom::UserDisplayMode::kBrowser,
            provider()
                .registrar_unsafe()
                .GetAppUserDisplayMode(second_app_id)
                .value());

  // Verify that the file handlers are correctly updated after a
  // second installation. The file handlers should match the ones in
  // manifest_with_file_handlers.json.
  const apps::FileHandlers& handlers =
      *provider().registrar_unsafe().GetAppFileHandlers(second_app_id);
  EXPECT_EQ(2u, handlers.size());

  // Verify the first file handler matches.
  EXPECT_EQ(https_server()->GetURL("/open-foo"), handlers[0].action);
  EXPECT_EQ(
      GetAcceptEntriesForFileHandler(
          {{"application/foo", {".foo"}}, {"application/foobar", {".foobar"}}}),
      handlers[0].accept);

  // Verify the second file handler matches.
  EXPECT_EQ(https_server()->GetURL("/open-bar"), handlers[1].action);
  EXPECT_EQ(
      GetAcceptEntriesForFileHandler({{"application/bar", {".bar", ".baz"}}}),
      handlers[1].accept);
}

IN_PROC_BROWSER_TEST_F(ExternalAppResolutionCommandBrowserTest,
                       PlaceholderInstallWithCustomIconLoadSuccessful) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page1.html");

  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  install_options.install_placeholder = true;
  install_options.force_reinstall = true;
  // Set a custom icon.
  install_options.override_icon_url = https_server()->GetURL(
      "/banners/"
      "128x128-green.png");

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  provider().scheduler().InstallExternallyManagedApp(
      install_options,
      /*installed_placeholder_app_id=*/"someplaceholderappid",
      future.GetCallback());

  const ExternallyManagedAppManager::InstallResult& result =
      future.Get<ExternallyManagedAppManager::InstallResult>();
  const webapps::AppId& app_id = *result.app_id;
  webapps::InstallResultCode install_code = result.code;
  EXPECT_EQ(install_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));
  base::test::TestFuture<WebAppIconManager::WebAppBitmaps> icon_future;
  provider().icon_manager().ReadAllIcons(app_id, icon_future.GetCallback());
  const WebAppIconManager::WebAppBitmaps bitmaps = icon_future.Get();

  EXPECT_FALSE(bitmaps.manifest_icons.empty());
  EXPECT_FALSE(bitmaps.trusted_icons.empty());
}

IN_PROC_BROWSER_TEST_F(
    ExternalAppResolutionCommandBrowserTest,
    PlaceholderInstallWithCustomIconLoadFailedStillSuccessful) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page1.html");

  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  install_options.install_placeholder = true;
  install_options.force_reinstall = true;
  // Set a custom icon.
  install_options.override_icon_url = https_server()->GetURL(
      "/banners/"
      "non-existing-icon.png");

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  provider().scheduler().InstallExternallyManagedApp(
      install_options,
      /*installed_placeholder_app_id=*/"someplaceholderappid",
      future.GetCallback());

  const ExternallyManagedAppManager::InstallResult& result =
      future.Get<ExternallyManagedAppManager::InstallResult>();
  const webapps::AppId& app_id = *result.app_id;
  webapps::InstallResultCode install_code = result.code;
  EXPECT_EQ(install_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));

  base::test::TestFuture<WebAppIconManager::WebAppBitmaps> icon_future;
  provider().icon_manager().ReadAllIcons(app_id, icon_future.GetCallback());
  const WebAppIconManager::WebAppBitmaps bitmaps = icon_future.Get();

  EXPECT_TRUE(bitmaps.manifest_icons.empty());
  EXPECT_TRUE(bitmaps.trusted_icons.empty());
}

class ExternalAppResolutionCommandCspBrowserTest
    : public ExternalAppResolutionCommandBrowserTest {
 public:
  struct ResponseMap {
    std::queue<net::HttpStatusCode> manifest_response_codes;
    std::queue<net::HttpStatusCode> icon_response_codes;
  };

  void SetUp() override {
    https_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [&](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          std::unique_ptr<net::test_server::BasicHttpResponse> response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          if (request.relative_url == "/manifest_test_page.html") {
            EXPECT_FALSE(response_map_.manifest_response_codes.empty());
            response->set_code(response_map_.manifest_response_codes.front());
            response_map_.manifest_response_codes.pop();
            return std::move(response);
          } else if (request.relative_url ==
                     "/manifest_test_page_with_strict_csp.html") {
            EXPECT_FALSE(response_map_.manifest_response_codes.empty());
            response->set_code(response_map_.manifest_response_codes.front());
            response_map_.manifest_response_codes.pop();
            // This CSP prevents icon loads from any url.
            response->set_content(R"(
              <!DOCTYPE html>
              <html>
                <head>
                    <meta http-equiv="Content-Security-Policy"
                      content="default-src 'self';
                      img-src 'none';">
                </head>
                <body>
                </body>
              </html>
              )");
            return std::move(response);
          } else if (request.relative_url == "/icon.png") {
            EXPECT_FALSE(response_map_.icon_response_codes.empty());
            const net::HttpStatusCode status_code =
                response_map_.icon_response_codes.front();
            response_map_.icon_response_codes.pop();
            response->set_code(status_code);
            if (status_code == net::HttpStatusCode::HTTP_OK) {
              response->set_content_type("image/x-icon");
              std::string icon_data = std::string(
                  "\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x20\x00\x2C"
                  "\x00\x00\x00\x16\x00\x00\x00\x28\x00\x00\x00\x01\x00\x00\x00"
                  "\x02\x00\x00\x00\x01\x00\x20\x00\x00\x00\x00\x00\x04\x00\x00"
                  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
                  70);
              response->set_content(icon_data);
            }
          } else {
            response->set_code(net::HTTP_NOT_FOUND);
          }
          return response;
        }));
    ExternalAppResolutionCommandBrowserTest::SetUp();
  }

  void SetResponseMap(ResponseMap response_map) {
    response_map_ = std::move(response_map);
  }

 private:
  ResponseMap response_map_;
};

IN_PROC_BROWSER_TEST_F(
    ExternalAppResolutionCommandCspBrowserTest,
    PlaceholderInstallAfterRedirectWithCustomIconLoadSuccessful) {
  const GURL kWebAppUrl = https_server()->GetURL("/manifest_test_page.html");

  std::queue<net::HttpStatusCode> manifest_response_codes(
      {net::HttpStatusCode::HTTP_PERMANENT_REDIRECT});
  std::queue<net::HttpStatusCode> icon_response_codes(
      {// First URL navigation.
       net::HttpStatusCode::HTTP_OK,
       // Icon load.
       net::HttpStatusCode::HTTP_OK});
  SetResponseMap({.manifest_response_codes = std::move(manifest_response_codes),
                  .icon_response_codes = icon_response_codes});

  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  install_options.install_placeholder = true;
  install_options.force_reinstall = true;
  // Set a custom icon.
  install_options.override_icon_url = https_server()->GetURL("/icon.png");

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  provider().scheduler().InstallExternallyManagedApp(
      install_options,
      /*installed_placeholder_app_id=*/"someplaceholderappid",
      future.GetCallback());

  const ExternallyManagedAppManager::InstallResult& result =
      future.Get<ExternallyManagedAppManager::InstallResult>();
  const webapps::AppId& app_id = *result.app_id;
  webapps::InstallResultCode install_code = result.code;
  EXPECT_EQ(install_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));

  base::test::TestFuture<WebAppIconManager::WebAppBitmaps> icon_future;
  provider().icon_manager().ReadAllIcons(app_id, icon_future.GetCallback());
  const WebAppIconManager::WebAppBitmaps bitmaps = icon_future.Get();

  EXPECT_FALSE(bitmaps.manifest_icons.empty());
  EXPECT_FALSE(bitmaps.trusted_icons.empty());
}

IN_PROC_BROWSER_TEST_F(
    ExternalAppResolutionCommandCspBrowserTest,
    PlaceholderInstallAfterRedirectWithCustomIconLoadNavigationFailsAndRetriesSuccessful) {
  const GURL kWebAppUrl = https_server()->GetURL("/manifest_test_page.html");

  std::queue<net::HttpStatusCode> manifest_response_codes(
      {net::HttpStatusCode::HTTP_PERMANENT_REDIRECT});
  std::queue<net::HttpStatusCode> icon_response_codes(
      {// First URL navigation fails --> triggers retry.
       net::HttpStatusCode::HTTP_NOT_FOUND,
       // Second URL navigation succeeds.
       net::HttpStatusCode::HTTP_OK,
       // Icon load succeeds.
       net::HttpStatusCode::HTTP_OK});
  SetResponseMap({.manifest_response_codes = std::move(manifest_response_codes),
                  .icon_response_codes = icon_response_codes});

  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  install_options.install_placeholder = true;
  install_options.force_reinstall = true;
  // Set a custom icon.
  install_options.override_icon_url = https_server()->GetURL("/icon.png");

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  provider().scheduler().InstallExternallyManagedApp(
      install_options,
      /*installed_placeholder_app_id=*/"someplaceholderappid",
      future.GetCallback());

  const ExternallyManagedAppManager::InstallResult& result =
      future.Get<ExternallyManagedAppManager::InstallResult>();
  const webapps::AppId& app_id = *result.app_id;
  webapps::InstallResultCode install_code = result.code;
  EXPECT_EQ(install_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));

  base::test::TestFuture<WebAppIconManager::WebAppBitmaps> icon_future;
  provider().icon_manager().ReadAllIcons(app_id, icon_future.GetCallback());
  const WebAppIconManager::WebAppBitmaps bitmaps = icon_future.Get();

  EXPECT_FALSE(bitmaps.manifest_icons.empty());
  EXPECT_FALSE(bitmaps.trusted_icons.empty());
}

IN_PROC_BROWSER_TEST_F(
    ExternalAppResolutionCommandCspBrowserTest,
    PlaceholderInstallAfterRedirectWithCustomIconLoadFailsAndRetriesSuccessful) {
  const GURL kWebAppUrl = https_server()->GetURL("/manifest_test_page.html");

  std::queue<net::HttpStatusCode> manifest_response_codes(
      {net::HttpStatusCode::HTTP_PERMANENT_REDIRECT});
  std::queue<net::HttpStatusCode> icon_response_codes(
      {// URL navigation succeeds.
       net::HttpStatusCode::HTTP_OK,
       // First icon load fails --> triggers retry.
       net::HttpStatusCode::HTTP_NOT_FOUND,
       // URL navigation succeeds.
       net::HttpStatusCode::HTTP_OK,
       // Second icon load succeeds.
       net::HttpStatusCode::HTTP_OK});
  SetResponseMap({.manifest_response_codes = std::move(manifest_response_codes),
                  .icon_response_codes = icon_response_codes});

  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  install_options.install_placeholder = true;
  install_options.force_reinstall = true;
  // Set a custom icon.
  install_options.override_icon_url = https_server()->GetURL("/icon.png");

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  provider().scheduler().InstallExternallyManagedApp(
      install_options,
      /*installed_placeholder_app_id=*/"someplaceholderappid",
      future.GetCallback());

  const ExternallyManagedAppManager::InstallResult& result =
      future.Get<ExternallyManagedAppManager::InstallResult>();
  const webapps::AppId& app_id = *result.app_id;
  webapps::InstallResultCode install_code = result.code;
  EXPECT_EQ(install_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));

  base::test::TestFuture<WebAppIconManager::WebAppBitmaps> icon_future;
  provider().icon_manager().ReadAllIcons(app_id, icon_future.GetCallback());
  const WebAppIconManager::WebAppBitmaps bitmaps = icon_future.Get();

  EXPECT_FALSE(bitmaps.manifest_icons.empty());
  EXPECT_FALSE(bitmaps.trusted_icons.empty());
}

IN_PROC_BROWSER_TEST_F(
    ExternalAppResolutionCommandCspBrowserTest,
    PlaceholderInstallCustomIconLoadWithStrictCspSuccessful) {
  const GURL kWebAppUrl =
      https_server()->GetURL("/manifest_test_page_with_strict_csp.html");

  std::queue<net::HttpStatusCode> manifest_response_codes(
      {net::HttpStatusCode::HTTP_PERMANENT_REDIRECT});
  std::queue<net::HttpStatusCode> icon_response_codes(
      {// URL navigation succeeds.
       net::HttpStatusCode::HTTP_OK,
       // Icon load succeeds.
       net::HttpStatusCode::HTTP_OK});
  SetResponseMap({.manifest_response_codes = std::move(manifest_response_codes),
                  .icon_response_codes = icon_response_codes});

  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  install_options.install_placeholder = true;
  install_options.force_reinstall = true;
  // Set a custom icon. This URL is blocked by the manifest test page csp
  // (manifest_test_page_with_strict_csp.html). We expect the custom icon load
  // to still work correctly.
  install_options.override_icon_url = https_server()->GetURL("/icon.png");

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  provider().scheduler().InstallExternallyManagedApp(
      install_options,
      /*installed_placeholder_app_id=*/"someplaceholderappid",
      future.GetCallback());

  const ExternallyManagedAppManager::InstallResult& result =
      future.Get<ExternallyManagedAppManager::InstallResult>();
  const webapps::AppId& app_id = *result.app_id;
  webapps::InstallResultCode install_code = result.code;
  EXPECT_EQ(install_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));

  base::test::TestFuture<WebAppIconManager::WebAppBitmaps> icon_future;
  provider().icon_manager().ReadAllIcons(app_id, icon_future.GetCallback());
  const WebAppIconManager::WebAppBitmaps bitmaps = icon_future.Get();

  EXPECT_FALSE(bitmaps.manifest_icons.empty());
  EXPECT_FALSE(bitmaps.trusted_icons.empty());
}

}  // namespace web_app
