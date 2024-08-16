// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
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

class ExternallyManagedInstallCommandBrowserTest
    : public WebAppBrowserTestBase {
 public:
  ExternallyManagedInstallCommandBrowserTest() = default;
  ExternallyManagedInstallCommandBrowserTest(
      const ExternallyManagedInstallCommandBrowserTest&) = delete;
  ExternallyManagedInstallCommandBrowserTest& operator=(
      const ExternallyManagedInstallCommandBrowserTest&) = delete;
};

IN_PROC_BROWSER_TEST_F(ExternallyManagedInstallCommandBrowserTest,
                       BasicInstallCommand) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

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
  EXPECT_TRUE(provider().registrar_unsafe().IsInstallState(
      app_id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
               proto::INSTALLED_WITH_OS_INTEGRATION}));
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedInstallCommandBrowserTest,
                       ExternalInstallWindowMode) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

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
  EXPECT_TRUE(provider().registrar_unsafe().IsInstallState(
      app_id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
               proto::INSTALLED_WITH_OS_INTEGRATION}));
  EXPECT_EQ(
      mojom::UserDisplayMode::kStandalone,
      provider().registrar_unsafe().GetAppUserDisplayMode(app_id).value());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedInstallCommandBrowserTest,
                       ExternalInstallBrowserMode) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

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
  EXPECT_TRUE(provider().registrar_unsafe().IsInstallState(
      app_id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
               proto::INSTALLED_WITH_OS_INTEGRATION}));
  EXPECT_EQ(
      mojom::UserDisplayMode::kBrowser,
      provider().registrar_unsafe().GetAppUserDisplayMode(app_id).value());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedInstallCommandBrowserTest,
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
  EXPECT_TRUE(provider().registrar_unsafe().IsInstallState(
      app_id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
               proto::INSTALLED_WITH_OS_INTEGRATION}));
  EXPECT_TRUE(
      provider().registrar_unsafe().GetAppById(app_id)->IsPolicyInstalledApp());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedInstallCommandBrowserTest,
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
  EXPECT_FALSE(provider().registrar_unsafe().IsInstallState(
      app_id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
               proto::INSTALLED_WITH_OS_INTEGRATION}));
}

IN_PROC_BROWSER_TEST_F(
    ExternallyManagedInstallCommandBrowserTest,
    DISABLED_ExternalInstallSourceReinstallOverrideManifestData) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult>
      future_first_install;
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
  EXPECT_TRUE(provider().registrar_unsafe().IsInstallState(
      first_app_id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                     proto::INSTALLED_WITH_OS_INTEGRATION}));
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
  EXPECT_TRUE(provider().registrar_unsafe().IsInstallState(
      second_app_id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                      proto::INSTALLED_WITH_OS_INTEGRATION}));
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

IN_PROC_BROWSER_TEST_F(ExternallyManagedInstallCommandBrowserTest,
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
  EXPECT_TRUE(provider().registrar_unsafe().IsInstallState(
      first_app_id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                     proto::INSTALLED_WITH_OS_INTEGRATION}));

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
  EXPECT_TRUE(provider().registrar_unsafe().IsInstallState(
      second_app_id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                      proto::INSTALLED_WITH_OS_INTEGRATION}));
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

}  // namespace web_app
