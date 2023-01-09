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
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/commands/externally_managed_install_command.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
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

using ExternallyManagedInstallCommandBrowserTest = WebAppControllerBrowserTest;

IN_PROC_BROWSER_TEST_F(ExternallyManagedInstallCommandBrowserTest,
                       BasicInstallCommand) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  ExternalInstallOptions install_options(
      kWebAppUrl,
      /*user_display_mode=*/absl::nullopt,
      ExternalInstallSource::kInternalDefault);

  base::test::TestFuture<const AppId&, webapps::InstallResultCode, bool> future;
  provider().scheduler().InstallExternallyManagedApp(
      install_options, future.GetCallback(),
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      std::make_unique<WebAppDataRetriever>());

  const AppId& app_id = future.Get<0>();
  webapps::InstallResultCode result_code = future.Get<1>();
  EXPECT_EQ(result_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().IsLocallyInstalled(app_id));
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
  base::test::TestFuture<const AppId&, webapps::InstallResultCode, bool> future;
  provider().scheduler().InstallExternallyManagedApp(
      install_options, future.GetCallback(),
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      std::make_unique<WebAppDataRetriever>());

  const AppId& app_id = future.Get<0>();
  webapps::InstallResultCode result_code = future.Get<1>();
  EXPECT_EQ(result_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().IsLocallyInstalled(app_id));
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

  base::test::TestFuture<const AppId&, webapps::InstallResultCode, bool> future;
  provider().scheduler().InstallExternallyManagedApp(

      install_options, future.GetCallback(),
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      std::make_unique<WebAppDataRetriever>());

  const AppId& app_id = future.Get<0>();
  webapps::InstallResultCode result_code = future.Get<1>();
  EXPECT_EQ(result_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().IsLocallyInstalled(app_id));
  EXPECT_EQ(
      mojom::UserDisplayMode::kBrowser,
      provider().registrar_unsafe().GetAppUserDisplayMode(app_id).value());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedInstallCommandBrowserTest,
                       InstallAppFromPolicy) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kBrowser,
      ExternalInstallSource::kExternalPolicy);
  base::test::TestFuture<const AppId&, webapps::InstallResultCode, bool> future;

  provider().scheduler().InstallExternallyManagedApp(
      install_options, future.GetCallback(),
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      std::make_unique<WebAppDataRetriever>());

  const AppId& app_id = future.Get<0>();
  webapps::InstallResultCode result_code = future.Get<1>();
  EXPECT_EQ(result_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().IsLocallyInstalled(app_id));
  EXPECT_TRUE(
      provider().registrar_unsafe().GetAppById(app_id)->IsPolicyInstalledApp());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedInstallCommandBrowserTest,
                       InstallFailsWebContentsDestroyed) {
  const GURL kWebAppUrl("https://external_app.com");
  EXPECT_FALSE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  base::test::TestFuture<const AppId&, webapps::InstallResultCode, bool> future;
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kBrowser,
      ExternalInstallSource::kExternalPolicy);
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  provider().scheduler().InstallExternallyManagedApp(
      install_options, future.GetCallback(), web_contents->GetWeakPtr(),
      std::make_unique<WebAppDataRetriever>());

  // Create a new tab to ensure that the browser isn't destroyed with the web
  // contents closing.
  chrome::NewTab(browser());
  web_contents->Close();
  const AppId& app_id = future.Get<0>();
  webapps::InstallResultCode result_code = future.Get<1>();
  EXPECT_EQ(result_code, webapps::InstallResultCode::kWebContentsDestroyed);
  EXPECT_FALSE(provider().registrar_unsafe().IsLocallyInstalled(app_id));
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedInstallCommandBrowserTest,
                       InstallFailsWithInvalidManifest) {
  const GURL kWebAppUrl("https://external_app.com");
  EXPECT_FALSE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  base::test::TestFuture<const AppId&, webapps::InstallResultCode, bool> future;
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kBrowser,
      ExternalInstallSource::kExternalPolicy);
  // This should force the install_params to have a valid manifest, otherwise
  // install will not happen.
  install_options.require_manifest = true;
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  provider().scheduler().InstallExternallyManagedApp(
      install_options, future.GetCallback(), web_contents->GetWeakPtr(),
      std::make_unique<WebAppDataRetriever>());

  const AppId& app_id = future.Get<0>();
  webapps::InstallResultCode result_code = future.Get<1>();
  EXPECT_EQ(result_code,
            webapps::InstallResultCode::kNotValidManifestForWebApp);
  EXPECT_FALSE(provider().registrar_unsafe().IsLocallyInstalled(app_id));
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedInstallCommandBrowserTest,
                       ExternalInstallSourceReinstallOverrideManifestData) {
  const GURL kWebAppUrl = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), kWebAppUrl));

  base::test::TestFuture<const AppId&, webapps::InstallResultCode, bool>
      future_first_install;
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kBrowser,
      ExternalInstallSource::kInternalDefault);

  provider().scheduler().InstallExternallyManagedApp(
      install_options, future_first_install.GetCallback(),
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      std::make_unique<WebAppDataRetriever>());

  const AppId& first_app_id = future_first_install.Get<0>();
  webapps::InstallResultCode first_install_code = future_first_install.Get<1>();
  EXPECT_EQ(first_install_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().IsLocallyInstalled(first_app_id));
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

  base::test::TestFuture<const AppId&, webapps::InstallResultCode, bool>
      future_second_install;
  ExternalInstallOptions install_options_policy(
      kWebAppUrlDifferentManifest, mojom::UserDisplayMode::kBrowser,
      ExternalInstallSource::kExternalPolicy);

  provider().scheduler().InstallExternallyManagedApp(
      install_options_policy, future_second_install.GetCallback(),
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      std::make_unique<WebAppDataRetriever>());

  const AppId& second_app_id = future_second_install.Get<0>();
  webapps::InstallResultCode second_install_code =
      future_second_install.Get<1>();
  EXPECT_EQ(first_app_id, second_app_id);
  EXPECT_EQ(second_install_code,
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().IsLocallyInstalled(second_app_id));
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
  base::test::TestFuture<const AppId&, webapps::InstallResultCode>
      future_first_install;
  provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      /*bypass_service_worker_check=*/false,
      base::BindOnce(test::TestAcceptDialogCallback),
      future_first_install.GetCallback(),
      /*use_fallback=*/false);
  const AppId& first_app_id = future_first_install.Get<0>();
  webapps::InstallResultCode first_install_code = future_first_install.Get<1>();
  EXPECT_EQ(first_install_code, webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().IsLocallyInstalled(first_app_id));

  // Mock installation of the same web_app but with a different install URL
  // and updated manifest values.
  const GURL kWebAppUrlDifferentManifest = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_with_file_handlers.json");
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), kWebAppUrlDifferentManifest));

  base::test::TestFuture<const AppId&, webapps::InstallResultCode, bool>
      future_second_install;
  ExternalInstallOptions install_options_policy(
      kWebAppUrlDifferentManifest, mojom::UserDisplayMode::kBrowser,
      ExternalInstallSource::kExternalPolicy);

  provider().scheduler().InstallExternallyManagedApp(
      install_options_policy, future_second_install.GetCallback(),
      browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      std::make_unique<WebAppDataRetriever>());

  const AppId& second_app_id = future_second_install.Get<0>();
  webapps::InstallResultCode second_install_code =
      future_second_install.Get<1>();
  EXPECT_EQ(first_app_id, second_app_id);
  EXPECT_EQ(second_install_code,
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(provider().registrar_unsafe().IsLocallyInstalled(second_app_id));
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
