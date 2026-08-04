// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/iwa_dev/iwa_dev_page_handler.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/webui/iwa_dev/iwa_dev_ui.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace {

constexpr char kAppBaseVersion[] = "1.0.0";
constexpr char kProxyAppName[] = "Simple Isolated App";
constexpr char kLocalBundleName[] = "Local Bundle IWA Name";
constexpr char kManifestAppName[] = "Test App";
constexpr char kUpdateManifestUrl[] = "https://example.com/update.json";

class MockPage : public iwa_dev::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<iwa_dev::mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  void OnAppInstalled(iwa_dev::mojom::IwaDevModeAppInfoPtr app_info) override {
    installed_future_.SetValue(std::move(app_info));
  }

  void OnAppUpdated(iwa_dev::mojom::IwaDevModeAppInfoPtr app_info) override {
    updated_future_.SetValue(std::move(app_info));
  }

  void OnAppUninstalled(const std::string& app_id) override {
    uninstalled_future_.SetValue(app_id);
  }

  base::test::TestFuture<iwa_dev::mojom::IwaDevModeAppInfoPtr>&
  installed_future() {
    return installed_future_;
  }

  base::test::TestFuture<iwa_dev::mojom::IwaDevModeAppInfoPtr>&
  updated_future() {
    return updated_future_;
  }

  base::test::TestFuture<std::string>& uninstalled_future() {
    return uninstalled_future_;
  }

 private:
  mojo::Receiver<iwa_dev::mojom::Page> receiver_{this};
  base::test::TestFuture<iwa_dev::mojom::IwaDevModeAppInfoPtr>
      installed_future_;
  base::test::TestFuture<iwa_dev::mojom::IwaDevModeAppInfoPtr> updated_future_;
  base::test::TestFuture<std::string> uninstalled_future_;
};

}  // namespace

class IwaDevHandlerBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  IwaDevHandlerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kIsolatedWebAppDevUi);
  }

  void SetUpOnMainThread() override {
    web_app::IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://iwa-dev")));
  }

  void TearDownOnMainThread() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    web_app::IsolatedWebAppBrowserTestHarness::TearDownOnMainThread();
  }

  web_app::IsolatedWebAppUrlInfo InstallProxyApp() {
    server_ =
        CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
    return InstallDevModeProxyIsolatedWebApp(server_->GetOrigin());
  }

  web_app::IsolatedWebAppUrlInfo InstallBundleApp() {
    return InstallBundle(kLocalBundleName, kAppBaseVersion);
  }

  web_app::IsolatedWebAppUrlInfo InstallUpdateManifestApp() {
    web_app::IsolatedWebAppUrlInfo app =
        InstallBundle(kManifestAppName, kAppBaseVersion);
    SetUpdateInfo(app.app_id(), GURL(kUpdateManifestUrl));
    return app;
  }

  net::EmbeddedTestServer* server() const { return server_.get(); }

 protected:
  void SetUpdateInfo(const webapps::AppId& app_id,
                     const GURL& update_manifest_url) {
    web_app::ScopedRegistryUpdate update =
        provider().sync_bridge_unsafe().BeginUpdate();
    web_app::WebApp* web_app = update->UpdateApp(app_id);
    CHECK(web_app && web_app->isolation_data());
    web_app->SetIsolationData(
        web_app::IsolationData::Builder(*web_app->isolation_data())
            .SetUpdateManifestUrl(update_manifest_url)
            .SetUpdateChannel(*web_app::UpdateChannel::Create("default"))
            .Build());
  }

  IwaDevPageHandler* GetHandler() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    IwaDevUI* controller =
        static_cast<IwaDevUI*>(web_contents->GetWebUI()->GetController());
    CHECK(controller);
    IwaDevPageHandler* handler = controller->GetHandlerForTesting();
    CHECK(handler);
    return handler;
  }

  std::vector<iwa_dev::mojom::IwaDevModeAppInfoPtr> GetInstalledAppsInfo() {
    base::test::TestFuture<std::vector<iwa_dev::mojom::IwaDevModeAppInfoPtr>>
        future;
    GetHandler()->GetInstalledAppsInfo(future.GetCallback());
    return future.Take();
  }

  bool UninstallApp(const std::string& app_id) {
    extensions::ScopedTestDialogAutoConfirm auto_accept(
        extensions::ScopedTestDialogAutoConfirm::ACCEPT);
    base::test::TestFuture<bool> future;
    GetHandler()->UninstallApp(app_id, future.GetCallback());
    return future.Get();
  }

  std::optional<std::string> CallInstallAppFromDevProxy(const GURL& url) {
    base::test::TestFuture<const std::optional<std::string>&> future;
    GetHandler()->InstallAppFromDevProxy(url, future.GetCallback());
    return future.Take();
  }

  std::optional<std::string> CallSelectAndInstallAppFromLocalWebBundle(
      std::optional<base::FilePath> path) {
    ui::FakeSelectFileDialog::Factory* factory =
        ui::FakeSelectFileDialog::RegisterFactory();

    base::test::TestFuture<void> dialog_opened_future;
    factory->SetOpenCallback(dialog_opened_future.GetRepeatingCallback());

    base::test::TestFuture<const std::optional<std::string>&> future;
    GetHandler()->SelectAndInstallAppFromLocalWebBundle(future.GetCallback());

    if (!dialog_opened_future.Wait()) {
      ADD_FAILURE() << "Timed out waiting for file dialog to open.";
      return "Timed out waiting for file dialog to open.";
    }

    ui::FakeSelectFileDialog* fake_dialog = factory->GetLastDialog();
    if (!fake_dialog) {
      ADD_FAILURE() << "fake_dialog is nullptr.";
      return "fake_dialog is nullptr.";
    }
    if (path.has_value()) {
      EXPECT_TRUE(fake_dialog->CallFileSelected(*path, "swbn"));
    } else {
      fake_dialog->CallFileSelectionCanceled();
    }

    return future.Take();
  }

  web_app::IsolatedWebAppUrlInfo InstallBundle(std::string_view name,
                                               std::string_view version) {
    std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().SetName(name).SetVersion(version))
            .BuildBundle(web_package::test::Ed25519KeyPair::CreateRandom());
    auto result = app->InstallWithSource(
        profile(), &web_app::IsolatedWebAppInstallSource::FromDevUi);
    CHECK(result.has_value()) << result.error();
    return result.value();
  }

  void ExpectBundleInstalledAtTruncatedPath(const base::FilePath& bundle_path) {
    EXPECT_EQ(bundle_path.DirName().DirName().value(),
              FILE_PATH_LITERAL("..."));
    EXPECT_EQ(bundle_path.BaseName().value(), FILE_PATH_LITERAL("main.swbn"));
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(IwaDevHandlerBrowserTest,
                       GetInstalledAppsInfo_ProxyApp) {
  auto proxy_app = InstallProxyApp();

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  const auto& app = apps[0];

  EXPECT_EQ(app->app_id, proxy_app.app_id());
  EXPECT_EQ(app->name, kProxyAppName);
  EXPECT_EQ(app->installed_version, kAppBaseVersion);
  ASSERT_TRUE(app->source->is_proxy_origin());
  EXPECT_EQ(app->source->get_proxy_origin(), server()->GetOrigin());
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerBrowserTest,
                       InstallAppFromDevProxy_Success) {
  auto server =
      CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));

  auto error = CallInstallAppFromDevProxy(server->GetURL("/"));
  EXPECT_FALSE(error.has_value());

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  EXPECT_EQ(apps[0]->name, kProxyAppName);
  ASSERT_TRUE(apps[0]->source->is_proxy_origin());
  EXPECT_EQ(apps[0]->source->get_proxy_origin(), server->GetOrigin());
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerBrowserTest,
                       InstallAppFromDevProxy_Error_NonOriginUrl) {
  auto server =
      CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));

  auto error = CallInstallAppFromDevProxy(server->GetURL("/invalid_path"));

  ASSERT_TRUE(error.has_value());
  EXPECT_THAT(*error, testing::HasSubstr("Non-origin URL provided"));
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerBrowserTest,
                       InstallAppFromDevProxy_Error_MissingManifest) {
  // Point to a directory that does not contain a valid IWA manifest.
  auto empty_server =
      CreateAndStartServer(FILE_PATH_LITERAL("web_apps/empty_dir"));

  auto error = CallInstallAppFromDevProxy(empty_server->GetURL("/"));

  ASSERT_TRUE(error.has_value());
  EXPECT_THAT(*error,
              testing::HasSubstr(
                  "App is not installable: The manifest could not be fetched"));
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerBrowserTest,
                       SelectAndInstallAppFromLocalWebBundle_Success) {
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
      web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder()
                                         .SetName(kLocalBundleName)
                                         .SetVersion(kAppBaseVersion))
          .BuildBundle(web_package::test::Ed25519KeyPair::CreateRandom());

  auto error = CallSelectAndInstallAppFromLocalWebBundle(app->path());
  EXPECT_FALSE(error.has_value());

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  EXPECT_EQ(apps[0]->name, kLocalBundleName);
  ASSERT_TRUE(apps[0]->source->is_bundle_path());
  ExpectBundleInstalledAtTruncatedPath(apps[0]->source->get_bundle_path());
}

IN_PROC_BROWSER_TEST_F(
    IwaDevHandlerBrowserTest,
    SelectAndInstallAppFromLocalWebBundle_Error_NoFileSelected) {
  auto error = CallSelectAndInstallAppFromLocalWebBundle(std::nullopt);
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(*error, "No file selected");
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerBrowserTest,
                       GetInstalledAppsInfo_LocalBundleApp) {
  web_app::IsolatedWebAppUrlInfo app = InstallBundleApp();

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  const auto& app_info = apps[0];

  EXPECT_EQ(app_info->app_id, app.app_id());
  EXPECT_EQ(app_info->name, kLocalBundleName);
  EXPECT_EQ(app_info->installed_version, kAppBaseVersion);
  ASSERT_TRUE(app_info->source->is_bundle_path());
  ExpectBundleInstalledAtTruncatedPath(app_info->source->get_bundle_path());
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerBrowserTest,
                       GetInstalledAppsInfo_ManifestApp) {
  web_app::IsolatedWebAppUrlInfo app = InstallUpdateManifestApp();

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  const auto& app_info = apps[0];

  EXPECT_EQ(app_info->app_id, app.app_id());
  EXPECT_EQ(app_info->name, kManifestAppName);
  EXPECT_EQ(app_info->installed_version, kAppBaseVersion);
  ASSERT_TRUE(app_info->source->is_update_info());
  const auto& update_info = app_info->source->get_update_info();
  EXPECT_EQ(update_info->update_manifest_url, GURL(kUpdateManifestUrl));
  EXPECT_EQ(update_info->update_channel, "default");
}

class IwaDevHandlerUpdateManifestBrowserTest : public IwaDevHandlerBrowserTest {
 public:
  // Builds a BasicHttpResponse with the given status code, content, and content
  // type.
  static std::unique_ptr<net::test_server::HttpResponse> BuildResponse(
      net::HttpStatusCode status,
      std::optional<std::string> content = std::nullopt,
      std::optional<std::string> content_type = std::nullopt) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(status);
    if (content_type) {
      response->set_content_type(*content_type);
    }
    if (content) {
      response->set_content(*content);
    }
    return response;
  }

  // Starts an EmbeddedTestServer that responds to requests matching `path`
  // using `response_builder`.
  std::unique_ptr<net::EmbeddedTestServer> StartServerForPath(
      std::string_view path,
      base::RepeatingCallback<std::unique_ptr<net::test_server::HttpResponse>()>
          response_builder) {
    auto server = std::make_unique<net::EmbeddedTestServer>();
    server->RegisterRequestHandler(base::BindRepeating(
        [](std::string target_path,
           base::RepeatingCallback<
               std::unique_ptr<net::test_server::HttpResponse>()> builder,
           const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.GetURL().path() == target_path) {
            return builder.Run();
          }
          return nullptr;
        },
        std::string(path), response_builder));
    EXPECT_TRUE(server->Start());
    return server;
  }

  std::unique_ptr<net::EmbeddedTestServer> ServeData(
      std::string_view path,
      std::string_view content,
      std::optional<std::string> content_type = std::nullopt) {
    return StartServerForPath(
        path, base::BindRepeating(&BuildResponse, net::HTTP_OK,
                                  std::make_optional<std::string>(content),
                                  content_type));
  }

  std::unique_ptr<net::EmbeddedTestServer> ServeJson(
      std::string_view path,
      std::string_view json_content) {
    return ServeData(path, json_content, "application/json");
  }

  std::optional<std::string> CallInstallAppFromUpdateManifest(
      const GURL& web_bundle_url,
      iwa_dev::mojom::UpdateInfoPtr update_info) {
    base::test::TestFuture<const std::optional<std::string>&> future;
    GetHandler()->InstallAppFromUpdateManifest(
        web_bundle_url, std::move(update_info), future.GetCallback());
    return future.Take();
  }

  base::expected<iwa_dev::mojom::UpdateManifestPtr, std::string>
  CallParseUpdateManifestFromUrl(const GURL& url) {
    base::test::TestFuture<
        base::expected<iwa_dev::mojom::UpdateManifestPtr, std::string>>
        future;
    GetHandler()->ParseUpdateManifestFromUrl(url, future.GetCallback());
    return future.Take();
  }
};

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       ParseUpdateManifestFromUrlWithoutChannels_Success) {
  auto server = ServeJson("/update_manifest.json", R"({
    "versions": [
      {
        "version": "1.0.0",
        "src": "https://example.com/app.swbn"
      }
    ]
  })");

  auto result =
      CallParseUpdateManifestFromUrl(server->GetURL("/update_manifest.json"));
  ASSERT_TRUE(result.has_value());
  const auto& manifest = *result;
  ASSERT_EQ(manifest->versions.size(), 1u);
  EXPECT_EQ(manifest->versions[0]->version, "1.0.0");
  EXPECT_EQ(manifest->versions[0]->src, GURL("https://example.com/app.swbn"));
  EXPECT_THAT(manifest->versions[0]->channels, testing::ElementsAre("default"));

  ASSERT_EQ(manifest->channels.size(), 1u);
  EXPECT_EQ(manifest->channels[0]->channel, "default");
  EXPECT_EQ(manifest->channels[0]->display_name, "default");
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       ParseUpdateManifestFromUrlWithChannels_Success) {
  auto server = ServeJson("/update_manifest.json", R"({
    "channels": {
      "beta": { "name": "Beta Channel" }
    },
    "versions": [
      {
        "version": "1.0.0",
        "src": "https://example.com/app.swbn",
        "channels": ["beta"]
      }
    ]
  })");

  auto result =
      CallParseUpdateManifestFromUrl(server->GetURL("/update_manifest.json"));
  ASSERT_TRUE(result.has_value());
  const auto& manifest = *result;

  ASSERT_EQ(manifest->versions.size(), 1u);
  EXPECT_THAT(manifest->versions[0]->channels, testing::ElementsAre("beta"));

  ASSERT_EQ(manifest->channels.size(), 1u);
  EXPECT_EQ(manifest->channels[0]->channel, "beta");
  EXPECT_EQ(manifest->channels[0]->display_name, "Beta Channel");
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       ParseUpdateManifestFromUrl_Error_DownloadFailed) {
  auto result =
      CallParseUpdateManifestFromUrl(GURL("https://127.0.0.1:0/missing.json"));
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(),
              testing::HasSubstr(web_app::UpdateManifestFetcher::ErrorToString(
                  web_app::UpdateManifestFetcher::Error::kDownloadFailed)));
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       ParseUpdateManifestFromUrl_Error_InvalidJson) {
  auto server = ServeJson("/invalid.json", "invalid json content {{{");

  auto result = CallParseUpdateManifestFromUrl(server->GetURL("/invalid.json"));
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(),
              testing::HasSubstr(web_app::UpdateManifestFetcher::ErrorToString(
                  web_app::UpdateManifestFetcher::Error::kInvalidJson)));
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       ParseUpdateManifestFromUrl_Error_InvalidManifestSchema) {
  auto server = ServeJson("/invalid_manifest.json", R"({ "invalid": true })");

  auto result =
      CallParseUpdateManifestFromUrl(server->GetURL("/invalid_manifest.json"));
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(),
              testing::HasSubstr(web_app::UpdateManifestFetcher::ErrorToString(
                  web_app::UpdateManifestFetcher::Error::kInvalidManifest)));
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       InstallAppFromUpdateManifest_Success) {
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> bundle =
      web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder()
                                         .SetName(kManifestAppName)
                                         .SetVersion(kAppBaseVersion))
          .BuildBundle(web_package::test::Ed25519KeyPair::CreateRandom());
  auto server = ServeData("/app.swbn", bundle->GetBundleData());

  auto update_info =
      iwa_dev::mojom::UpdateInfo::New(GURL(kUpdateManifestUrl), "default");
  auto error = CallInstallAppFromUpdateManifest(server->GetURL("/app.swbn"),
                                                std::move(update_info));
  EXPECT_FALSE(error.has_value()) << *error;

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  EXPECT_EQ(apps[0]->name, kManifestAppName);
  ASSERT_TRUE(apps[0]->source->is_update_info());
  const auto& installed_update_info = apps[0]->source->get_update_info();
  EXPECT_EQ(installed_update_info->update_manifest_url,
            GURL(kUpdateManifestUrl));
  EXPECT_EQ(installed_update_info->update_channel, "default");
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       InstallAppFromUpdateManifestCustomChannel_Success) {
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> bundle =
      web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder()
                                         .SetName(kManifestAppName)
                                         .SetVersion(kAppBaseVersion))
          .BuildBundle(web_package::test::Ed25519KeyPair::CreateRandom());
  auto server = ServeData("/app.swbn", bundle->GetBundleData());

  auto update_info =
      iwa_dev::mojom::UpdateInfo::New(GURL(kUpdateManifestUrl), "beta");
  auto error = CallInstallAppFromUpdateManifest(server->GetURL("/app.swbn"),
                                                std::move(update_info));
  EXPECT_FALSE(error.has_value()) << *error;

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  EXPECT_EQ(apps[0]->name, kManifestAppName);
  ASSERT_TRUE(apps[0]->source->is_update_info());
  const auto& installed_update_info = apps[0]->source->get_update_info();
  EXPECT_EQ(installed_update_info->update_manifest_url,
            GURL(kUpdateManifestUrl));
  EXPECT_EQ(installed_update_info->update_channel, "beta");
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       InstallAppFromUpdateManifest_NonHttpUrlScheme) {
  // Non-HTTP/HTTPS bundle URL
  auto update_info1 = iwa_dev::mojom::UpdateInfo::New(
      GURL("https://example.com/update.json"), "default");
  auto error1 = CallInstallAppFromUpdateManifest(GURL("chrome://settings"),
                                                 std::move(update_info1));
  ASSERT_TRUE(error1.has_value());
  EXPECT_EQ(*error1, "Invalid Web Bundle URL provided.");
  EXPECT_TRUE(GetInstalledAppsInfo().empty());

  // Non-HTTP/HTTPS manifest URL
  auto update_info2 =
      iwa_dev::mojom::UpdateInfo::New(GURL("chrome://settings"), "default");
  auto error2 = CallInstallAppFromUpdateManifest(
      GURL("https://example.com/app.swbn"), std::move(update_info2));
  ASSERT_TRUE(error2.has_value());
  EXPECT_EQ(*error2, "Invalid Update Manifest URL provided.");
  EXPECT_TRUE(GetInstalledAppsInfo().empty());
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       InstallAppFromUpdateManifest_InvalidUpdateChannel) {
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> bundle =
      web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder()
                                         .SetName(kManifestAppName)
                                         .SetVersion(kAppBaseVersion))
          .BuildBundle(web_package::test::Ed25519KeyPair::CreateRandom());
  auto server = ServeData("/app.swbn", bundle->GetBundleData());

  auto update_info =
      iwa_dev::mojom::UpdateInfo::New(GURL(kUpdateManifestUrl), "");
  auto error = CallInstallAppFromUpdateManifest(server->GetURL("/app.swbn"),
                                                std::move(update_info));
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(*error, "Invalid update channel provided.");
  EXPECT_TRUE(GetInstalledAppsInfo().empty());
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       InstallAppFromUpdateManifest_BundleDownloadError) {
  auto update_info =
      iwa_dev::mojom::UpdateInfo::New(GURL(kUpdateManifestUrl), "default");
  auto error = CallInstallAppFromUpdateManifest(
      GURL("https://127.0.0.1:0/missing.swbn"), std::move(update_info));
  ASSERT_TRUE(error.has_value());
  EXPECT_THAT(*error, testing::HasSubstr(
                          "Network error while downloading bundle file"));
  EXPECT_TRUE(GetInstalledAppsInfo().empty());
}

class IwaDevHandlerObserverBrowserTest : public IwaDevHandlerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    IwaDevHandlerBrowserTest::SetUpOnMainThread();

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    handler_ = std::make_unique<IwaDevPageHandler>(
        web_contents->GetWebUI(), mock_page_.BindAndGetRemote(),
        remote_.BindNewPipeAndPassReceiver());
  }

  void TearDownOnMainThread() override {
    handler_.reset();
    IwaDevHandlerBrowserTest::TearDownOnMainThread();
  }

  MockPage& mock_page() { return mock_page_; }

 private:
  MockPage mock_page_;
  mojo::Remote<iwa_dev::mojom::PageHandler> remote_;
  std::unique_ptr<IwaDevPageHandler> handler_;
};

IN_PROC_BROWSER_TEST_F(IwaDevHandlerObserverBrowserTest, OnAppInstalled) {
  web_app::IsolatedWebAppUrlInfo app = InstallBundleApp();

  iwa_dev::mojom::IwaDevModeAppInfoPtr app_info =
      mock_page().installed_future().Take();
  EXPECT_EQ(app_info->app_id, app.app_id());
  EXPECT_EQ(app_info->name, kLocalBundleName);
  EXPECT_EQ(app_info->installed_version, kAppBaseVersion);
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerObserverBrowserTest, OnAppUninstalled) {
  web_app::IsolatedWebAppUrlInfo app = InstallBundleApp();

  web_app::test::UninstallWebApp(profile(), app.app_id());

  EXPECT_EQ(mock_page().uninstalled_future().Get(), app.app_id());
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerObserverBrowserTest, OnAppUpdated) {
  web_app::IsolatedWebAppUrlInfo app = InstallBundleApp();

  provider().install_manager().NotifyWebAppManifestUpdated(app.app_id());

  iwa_dev::mojom::IwaDevModeAppInfoPtr app_info =
      mock_page().updated_future().Take();
  EXPECT_EQ(app_info->app_id, app.app_id());
  EXPECT_EQ(app_info->name, kLocalBundleName);
  EXPECT_EQ(app_info->installed_version, kAppBaseVersion);
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerObserverBrowserTest, IgnoresNormalWebApps) {
  // Install a normal (non-IWA) web app.
  webapps::AppId normal_app_id = web_app::test::InstallDummyWebApp(
      profile(), "Normal Web App", GURL("https://example.com/"));

  mock_page().FlushForTesting();

  // The WebUI should only be notified about Dev Mode IWAs.
  EXPECT_FALSE(mock_page().installed_future().IsReady());

  provider().install_manager().NotifyWebAppManifestUpdated(normal_app_id);
  mock_page().FlushForTesting();
  EXPECT_FALSE(mock_page().updated_future().IsReady());

  provider().install_manager().NotifyWebAppWillBeUninstalled(normal_app_id);
  mock_page().FlushForTesting();
  EXPECT_FALSE(mock_page().uninstalled_future().IsReady());
}

struct IwaDevAppTestCase {
  const char* name;
  web_app::IsolatedWebAppUrlInfo (IwaDevHandlerBrowserTest::*installer)();
};

class IwaDevHandlerAppTypeBrowserTest
    : public IwaDevHandlerBrowserTest,
      public ::testing::WithParamInterface<IwaDevAppTestCase> {};

IN_PROC_BROWSER_TEST_P(IwaDevHandlerAppTypeBrowserTest, UninstallApp) {
  web_app::IsolatedWebAppUrlInfo app = (this->*GetParam().installer)();

  auto apps_before = GetInstalledAppsInfo();
  ASSERT_EQ(apps_before.size(), 1u);
  EXPECT_EQ(apps_before[0]->app_id, app.app_id());

  EXPECT_TRUE(UninstallApp(app.app_id()));

  auto apps_after = GetInstalledAppsInfo();
  EXPECT_TRUE(apps_after.empty());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IwaDevHandlerAppTypeBrowserTest,
    ::testing::Values(
        IwaDevAppTestCase{"ProxyApp",
                          &IwaDevHandlerBrowserTest::InstallProxyApp},
        IwaDevAppTestCase{"LocalBundleApp",
                          &IwaDevHandlerBrowserTest::InstallBundleApp},
        IwaDevAppTestCase{"ManifestApp",
                          &IwaDevHandlerBrowserTest::InstallUpdateManifestApp}),
    [](const testing::TestParamInfo<IwaDevAppTestCase>& info) {
      return info.param.name;
    });
