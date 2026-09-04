// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/iwa_dev/iwa_dev_page_handler.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/test_future.h"
#include "base/thread_annotations.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
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
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "mojo/public/mojom/base/empty.mojom.h"
#include "mojo/public/mojom/base/error.mojom.h"
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
    return BuildAndInstallBundle(kLocalBundleName, kAppBaseVersion);
  }

  web_app::IsolatedWebAppUrlInfo InstallUpdateManifestApp() {
    web_app::IsolatedWebAppUrlInfo app =
        BuildAndInstallBundle(kManifestAppName, kAppBaseVersion);
    SetUpdateInfo(app.app_id(), GURL(kUpdateManifestUrl));
    return app;
  }

  net::EmbeddedTestServer* server() const { return server_.get(); }

 protected:
  void SetUpdateInfo(const webapps::AppId& app_id,
                     const GURL& update_manifest_url,
                     std::string_view update_channel = "default") {
    web_app::ScopedRegistryUpdate update =
        provider().sync_bridge_unsafe().BeginUpdate();
    web_app::WebApp* web_app = update->UpdateApp(app_id);
    CHECK(web_app && web_app->isolation_data());
    web_app->SetIsolationData(
        web_app::IsolationData::Builder(*web_app->isolation_data())
            .SetUpdateManifestUrl(update_manifest_url)
            .SetUpdateChannel(
                *web_app::UpdateChannel::Create(std::string(update_channel)))
            .Build());
  }

  IwaDevPageHandler* GetHandler() {
    content::WebContents* web_contents =
        browser()->GetTabStripModel()->GetActiveWebContents();
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

  base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
  CallInstallAppFromDevProxy(const GURL& url) {
    base::test::TestFuture<
        base::expected<std::monostate, mojo_base::mojom::ErrorPtr>>
        future;
    GetHandler()->InstallAppFromDevProxy(url, future.GetCallback());
    return future.Take();
  }

  base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
  CallUpdateDevProxyInstalledApp(const std::string& app_id) {
    base::test::TestFuture<
        base::expected<std::monostate, mojo_base::mojom::ErrorPtr>>
        future;
    GetHandler()->UpdateDevProxyInstalledApp(app_id, future.GetCallback());
    return future.Take();
  }

  web_app::IsolatedWebAppUrlInfo BuildAndInstallBundle(
      std::string_view name,
      std::string_view version,
      const web_package::test::Ed25519KeyPair& key_pair =
          web_package::test::Ed25519KeyPair::CreateRandom()) {
    std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().SetName(name).SetVersion(version))
            .BuildBundle(key_pair);
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

  auto result = CallInstallAppFromDevProxy(server->GetURL("/"));
  EXPECT_TRUE(result.has_value());

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

  auto result = CallInstallAppFromDevProxy(server->GetURL("/invalid_path"));

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error()->message,
              testing::HasSubstr("Non-origin URL provided"));
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerBrowserTest,
                       InstallAppFromDevProxy_Error_MissingManifest) {
  // Point to a directory that does not contain a valid IWA manifest.
  auto empty_server =
      CreateAndStartServer(FILE_PATH_LITERAL("web_apps/empty_dir"));

  auto result = CallInstallAppFromDevProxy(empty_server->GetURL("/"));

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error()->message,
              testing::HasSubstr(
                  "App is not installable: The manifest could not be fetched"));
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerBrowserTest,
                       UpdateDevProxyInstalledApp_Error_SameVersion) {
  auto proxy_app = InstallProxyApp();

  auto result = CallUpdateDevProxyInstalledApp(proxy_app.app_id());
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error()->message,
              testing::HasSubstr("Installed app is already on version 1.0.0."));
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerBrowserTest,
                       UpdateDevProxyInstalledApp_Error_AppNotInstalled) {
  auto result = CallUpdateDevProxyInstalledApp("invalid_app_id");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->message, "App not found.");
}

class IwaDevHandlerLocalBundleBrowserTest : public IwaDevHandlerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    IwaDevHandlerBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  std::unique_ptr<web_app::BundledIsolatedWebApp> BuildBundle(
      std::string_view name = kLocalBundleName,
      std::string_view version = kAppBaseVersion,
      const web_package::test::Ed25519KeyPair& key_pair =
          web_package::test::Ed25519KeyPair::CreateRandom()) {
    base::FilePath bundle_path = temp_dir_.GetPath().AppendASCII(
        base::StrCat({name, "_", version, ".swbn"}));
    return web_app::IsolatedWebAppBuilder(
               web_app::ManifestBuilder().SetName(name).SetVersion(version))
        .BuildBundle(bundle_path, key_pair);
  }

 protected:
  base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
  CallSelectAndInstallAppFromLocalWebBundle(
      std::optional<base::FilePath> path) {
    ui::FakeSelectFileDialog::Factory* factory =
        ui::FakeSelectFileDialog::RegisterFactory();

    base::test::TestFuture<void> dialog_opened_future;
    factory->SetOpenCallback(dialog_opened_future.GetRepeatingCallback());

    base::test::TestFuture<
        base::expected<std::monostate, mojo_base::mojom::ErrorPtr>>
        future;
    GetHandler()->SelectAndInstallAppFromLocalWebBundle(future.GetCallback());

    if (!dialog_opened_future.Wait()) {
      ADD_FAILURE() << "Timed out waiting for file dialog to open.";
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInvalidArgument,
          "Timed out waiting for file dialog to open."));
    }

    ui::FakeSelectFileDialog* fake_dialog = factory->GetLastDialog();
    if (!fake_dialog) {
      ADD_FAILURE() << "fake_dialog is nullptr.";
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInvalidArgument, "fake_dialog is nullptr."));
    }
    if (path.has_value()) {
      EXPECT_TRUE(fake_dialog->CallFileSelected(*path, "swbn"));
    } else {
      fake_dialog->CallFileSelectionCanceled();
    }

    return future.Take();
  }

  base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
  CallSelectAndUpdateAppFromLocalWebBundle(const std::string& app_id,
                                           std::optional<base::FilePath> path) {
    ui::FakeSelectFileDialog::Factory* factory =
        ui::FakeSelectFileDialog::RegisterFactory();

    base::test::TestFuture<void> dialog_opened_future;
    factory->SetOpenCallback(dialog_opened_future.GetRepeatingCallback());

    base::test::TestFuture<
        base::expected<std::monostate, mojo_base::mojom::ErrorPtr>>
        future;
    GetHandler()->SelectAndUpdateAppFromLocalWebBundle(app_id,
                                                       future.GetCallback());

    if (!dialog_opened_future.Wait()) {
      ADD_FAILURE() << "Timed out waiting for file dialog to open.";
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInvalidArgument,
          "Timed out waiting for file dialog to open."));
    }

    ui::FakeSelectFileDialog* fake_dialog = factory->GetLastDialog();
    if (!fake_dialog) {
      ADD_FAILURE() << "fake_dialog is nullptr.";
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInvalidArgument, "fake_dialog is nullptr."));
    }
    if (path.has_value()) {
      EXPECT_TRUE(fake_dialog->CallFileSelected(*path, "swbn"));
    } else {
      fake_dialog->CallFileSelectionCanceled();
    }

    return future.Take();
  }

 private:
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(IwaDevHandlerLocalBundleBrowserTest,
                       SelectAndInstallAppFromLocalWebBundle_Success) {
  auto app = BuildBundle();

  auto result = CallSelectAndInstallAppFromLocalWebBundle(app->path());
  EXPECT_TRUE(result.has_value());

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  EXPECT_EQ(apps[0]->name, kLocalBundleName);
  ASSERT_TRUE(apps[0]->source->is_bundle_path());
  ExpectBundleInstalledAtTruncatedPath(apps[0]->source->get_bundle_path());
}

IN_PROC_BROWSER_TEST_F(
    IwaDevHandlerLocalBundleBrowserTest,
    SelectAndInstallAppFromLocalWebBundle_Error_NoFileSelected) {
  auto result = CallSelectAndInstallAppFromLocalWebBundle(std::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->message, "No file selected");
}

IN_PROC_BROWSER_TEST_F(
    IwaDevHandlerLocalBundleBrowserTest,
    SelectAndInstallAppFromLocalWebBundle_Error_InvalidFileType) {
  auto result = CallSelectAndInstallAppFromLocalWebBundle(
      base::FilePath(FILE_PATH_LITERAL("app.json")));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      result.error()->message,
      "Invalid file type. Please select a Signed Web Bundle (.swbn) file.");
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerLocalBundleBrowserTest,
                       SelectAndUpdateAppFromLocalWebBundle_Success) {
  web_package::test::Ed25519KeyPair key_pair =
      web_package::test::Ed25519KeyPair::CreateRandom();
  auto app = BuildBundle(kLocalBundleName, kAppBaseVersion, key_pair);
  auto install_result = app->InstallWithSource(
      profile(), &web_app::IsolatedWebAppInstallSource::FromDevUi);
  ASSERT_TRUE(install_result.has_value());

  auto updated_app = BuildBundle(kLocalBundleName, "2.0.0", key_pair);

  auto result = CallSelectAndUpdateAppFromLocalWebBundle(
      install_result->app_id(), updated_app->path());
  EXPECT_TRUE(result.has_value());

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  EXPECT_EQ(apps[0]->name, kLocalBundleName);
  EXPECT_EQ(apps[0]->installed_version, "2.0.0");
  ASSERT_TRUE(apps[0]->source->is_bundle_path());
}

IN_PROC_BROWSER_TEST_F(
    IwaDevHandlerLocalBundleBrowserTest,
    SelectAndUpdateAppFromLocalWebBundle_Error_NoFileSelected) {
  web_app::IsolatedWebAppUrlInfo app = InstallBundleApp();

  auto result =
      CallSelectAndUpdateAppFromLocalWebBundle(app.app_id(), std::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->message, "No file selected");
}

IN_PROC_BROWSER_TEST_F(
    IwaDevHandlerLocalBundleBrowserTest,
    SelectAndUpdateAppFromLocalWebBundle_Error_InvalidFileType) {
  web_app::IsolatedWebAppUrlInfo app = InstallBundleApp();

  auto result = CallSelectAndUpdateAppFromLocalWebBundle(
      app.app_id(), base::FilePath(FILE_PATH_LITERAL("app.json")));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      result.error()->message,
      "Invalid file type. Please select a Signed Web Bundle (.swbn) file.");
}

IN_PROC_BROWSER_TEST_F(
    IwaDevHandlerLocalBundleBrowserTest,
    SelectAndUpdateAppFromLocalWebBundle_Error_AppNotInstalled) {
  auto app = BuildBundle();

  auto result =
      CallSelectAndUpdateAppFromLocalWebBundle("invalid_app_id", app->path());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->message, "App not found.");
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

  std::unique_ptr<net::EmbeddedTestServer> BuildAndServeBundle(
      const std::string& version = kAppBaseVersion,
      const web_package::test::Ed25519KeyPair& key_pair =
          web_package::test::Ed25519KeyPair::CreateRandom()) {
    std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> bundle =
        web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder()
                                           .SetName(kManifestAppName)
                                           .SetVersion(version))
            .BuildBundle(key_pair);
    return ServeData("/app.swbn", bundle->GetBundleData());
  }

  std::unique_ptr<net::EmbeddedTestServer> ServeUpdateManifest(
      const std::string& version,
      const GURL& bundle_url) {
    return ServeJson(
        "/update_manifest.json",
        base::StringPrintf(R"({
      "versions": [
        {
          "version": "%s",
          "src": "%s"
        }
      ]
    })",
                           version.c_str(), bundle_url.spec().c_str()));
  }

  base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
  CallInstallAppFromUpdateManifest(const GURL& web_bundle_url,
                                   iwa_dev::mojom::UpdateInfoPtr update_info) {
    base::test::TestFuture<
        base::expected<std::monostate, mojo_base::mojom::ErrorPtr>>
        future;
    GetHandler()->InstallAppFromUpdateManifest(
        web_bundle_url, std::move(update_info), future.GetCallback());
    return future.Take();
  }

  base::expected<iwa_dev::mojom::UpdateManifestPtr, mojo_base::mojom::ErrorPtr>
  CallParseUpdateManifestFromUrl(const GURL& url) {
    base::test::TestFuture<base::expected<iwa_dev::mojom::UpdateManifestPtr,
                                          mojo_base::mojom::ErrorPtr>>
        future;
    GetHandler()->ParseUpdateManifestFromUrl(url, future.GetCallback());
    return future.Take();
  }

  base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
  CallUpdateManifestInstalledApp(
      const std::string& app_id,
      iwa_dev::mojom::UpdateManifestOptionsPtr options) {
    base::test::TestFuture<
        base::expected<std::monostate, mojo_base::mojom::ErrorPtr>>
        future;
    GetHandler()->UpdateManifestInstalledApp(app_id, std::move(options),
                                             future.GetCallback());
    return future.Take();
  }

  base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
  CallUpdateManifestInstalledApp(
      const std::string& app_id,
      std::optional<std::string> pinned_version = std::nullopt,
      bool allow_downgrades = false) {
    return CallUpdateManifestInstalledApp(
        app_id, iwa_dev::mojom::UpdateManifestOptions::New(
                    allow_downgrades, std::move(pinned_version)));
  }

  base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
  CallSetUpdateChannel(const std::string& app_id,
                       const std::string& update_channel) {
    base::test::TestFuture<
        base::expected<std::monostate, mojo_base::mojom::ErrorPtr>>
        future;
    GetHandler()->SetUpdateChannel(app_id, update_channel,
                                   future.GetCallback());
    return future.Take();
  }

  void ExpectUpdateChannel(std::string_view expected_channel) {
    auto apps = GetInstalledAppsInfo();
    ASSERT_EQ(apps.size(), 1u);
    ASSERT_TRUE(apps[0]->source->is_update_info());
    EXPECT_EQ(apps[0]->source->get_update_info()->update_channel,
              expected_channel);
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
  EXPECT_THAT(result.error()->message,
              testing::HasSubstr(web_app::UpdateManifestFetcher::ErrorToString(
                  web_app::UpdateManifestFetcher::Error::kDownloadFailed)));
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       ParseUpdateManifestFromUrl_Error_InvalidJson) {
  auto server = ServeJson("/invalid.json", "invalid json content {{{");

  auto result = CallParseUpdateManifestFromUrl(server->GetURL("/invalid.json"));
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error()->message,
              testing::HasSubstr(web_app::UpdateManifestFetcher::ErrorToString(
                  web_app::UpdateManifestFetcher::Error::kInvalidJson)));
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       ParseUpdateManifestFromUrl_Error_InvalidManifestSchema) {
  auto server = ServeJson("/invalid_manifest.json", R"({ "invalid": true })");

  auto result =
      CallParseUpdateManifestFromUrl(server->GetURL("/invalid_manifest.json"));
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error()->message,
              testing::HasSubstr(web_app::UpdateManifestFetcher::ErrorToString(
                  web_app::UpdateManifestFetcher::Error::kInvalidManifest)));
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       InstallAppFromUpdateManifest_Success) {
  auto server = BuildAndServeBundle();

  auto update_info =
      iwa_dev::mojom::UpdateInfo::New(GURL(kUpdateManifestUrl), "default");
  auto result = CallInstallAppFromUpdateManifest(server->GetURL("/app.swbn"),
                                                 std::move(update_info));
  EXPECT_TRUE(result.has_value());

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
  auto server = BuildAndServeBundle();

  auto update_info =
      iwa_dev::mojom::UpdateInfo::New(GURL(kUpdateManifestUrl), "beta");
  auto result = CallInstallAppFromUpdateManifest(server->GetURL("/app.swbn"),
                                                 std::move(update_info));
  EXPECT_TRUE(result.has_value());

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
  auto result1 = CallInstallAppFromUpdateManifest(GURL("chrome://settings"),
                                                  std::move(update_info1));
  ASSERT_FALSE(result1.has_value());
  EXPECT_EQ(result1.error()->message, "Invalid Web Bundle URL provided.");
  EXPECT_TRUE(GetInstalledAppsInfo().empty());

  // Non-HTTP/HTTPS manifest URL
  auto update_info2 =
      iwa_dev::mojom::UpdateInfo::New(GURL("chrome://settings"), "default");
  auto result2 = CallInstallAppFromUpdateManifest(
      GURL("https://example.com/app.swbn"), std::move(update_info2));
  ASSERT_FALSE(result2.has_value());
  EXPECT_EQ(result2.error()->message, "Invalid Update Manifest URL provided.");
  EXPECT_TRUE(GetInstalledAppsInfo().empty());
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       InstallAppFromUpdateManifest_InvalidUpdateChannel) {
  auto server = BuildAndServeBundle();

  auto update_info =
      iwa_dev::mojom::UpdateInfo::New(GURL(kUpdateManifestUrl), "");
  auto result = CallInstallAppFromUpdateManifest(server->GetURL("/app.swbn"),
                                                 std::move(update_info));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->message, "Invalid update channel provided.");
  EXPECT_TRUE(GetInstalledAppsInfo().empty());
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       InstallAppFromUpdateManifest_BundleDownloadError) {
  auto update_info =
      iwa_dev::mojom::UpdateInfo::New(GURL(kUpdateManifestUrl), "default");
  auto result = CallInstallAppFromUpdateManifest(
      GURL("https://127.0.0.1:0/missing.swbn"), std::move(update_info));
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(
      result.error()->message,
      testing::HasSubstr("Network error while downloading bundle file"));
  EXPECT_TRUE(GetInstalledAppsInfo().empty());
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       UpdateManifestInstalledApp_Success) {
  web_package::test::Ed25519KeyPair key_pair =
      web_package::test::Ed25519KeyPair::CreateRandom();
  web_app::IsolatedWebAppUrlInfo app =
      BuildAndInstallBundle(kManifestAppName, kAppBaseVersion, key_pair);

  auto bundle_server = BuildAndServeBundle("2.0.0", key_pair);
  auto manifest_server =
      ServeUpdateManifest("2.0.0", bundle_server->GetURL("/app.swbn"));
  SetUpdateInfo(app.app_id(), manifest_server->GetURL("/update_manifest.json"));

  auto result = CallUpdateManifestInstalledApp(app.app_id());
  EXPECT_TRUE(result.has_value());

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  EXPECT_EQ(apps[0]->installed_version, "2.0.0");
}

IN_PROC_BROWSER_TEST_F(
    IwaDevHandlerUpdateManifestBrowserTest,
    UpdateManifestInstalledApp_SubsequentUpdateRefreshesManifest) {
  web_package::test::Ed25519KeyPair key_pair =
      web_package::test::Ed25519KeyPair::CreateRandom();
  web_app::IsolatedWebAppUrlInfo app =
      BuildAndInstallBundle(kManifestAppName, kAppBaseVersion, key_pair);

  auto bundle_server_v2 = BuildAndServeBundle("2.0.0", key_pair);
  auto bundle_server_v3 = BuildAndServeBundle("3.0.0", key_pair);

  // Thread-safe dynamic manifest state since EmbeddedTestServer runs on a
  // worker thread.
  struct ManifestState {
    base::Lock lock;
    std::string version GUARDED_BY(lock);
    GURL bundle_url GUARDED_BY(lock);
  } manifest_state;

  {
    base::AutoLock lock(manifest_state.lock);
    manifest_state.version = "2.0.0";
    manifest_state.bundle_url = bundle_server_v2->GetURL("/app.swbn");
  }

  auto manifest_server = StartServerForPath(
      "/update_manifest.json",
      base::BindRepeating(
          [](ManifestState* state)
              -> std::unique_ptr<net::test_server::HttpResponse> {
            base::AutoLock lock(state->lock);
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content_type("application/json");
            // Set aggressive caching header to simulate a caching proxy/server.
            response->AddCustomHeader("Cache-Control", "max-age=3600");
            response->set_content(base::StringPrintf(
                R"({
              "versions": [
                {
                  "version": "%s",
                  "src": "%s"
                }
              ]
            })",
                state->version.c_str(), state->bundle_url.spec().c_str()));
            return response;
          },
          base::Unretained(&manifest_state)));

  SetUpdateInfo(app.app_id(), manifest_server->GetURL("/update_manifest.json"));

  // First update: from 1.0.0 to 2.0.0.
  auto result_v2 = CallUpdateManifestInstalledApp(app.app_id());
  EXPECT_TRUE(result_v2.has_value());

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  EXPECT_EQ(apps[0]->installed_version, "2.0.0");

  // Developer updates the manifest on the server to 3.0.0.
  {
    base::AutoLock lock(manifest_state.lock);
    manifest_state.version = "3.0.0";
    manifest_state.bundle_url = bundle_server_v3->GetURL("/app.swbn");
  }

  // Second update: despite Cache-Control: max-age=3600, the manifest must be
  // re-fetched and the app updated to 3.0.0.
  auto result_v3 = CallUpdateManifestInstalledApp(app.app_id());
  EXPECT_TRUE(result_v3.has_value());

  apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  EXPECT_EQ(apps[0]->installed_version, "3.0.0");
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       UpdateManifestInstalledApp_AlreadyPending) {
  web_package::test::Ed25519KeyPair key_pair =
      web_package::test::Ed25519KeyPair::CreateRandom();
  web_app::IsolatedWebAppUrlInfo app =
      BuildAndInstallBundle(kManifestAppName, kAppBaseVersion, key_pair);

  auto bundle_server = BuildAndServeBundle("2.0.0", key_pair);
  auto manifest_server =
      ServeUpdateManifest("2.0.0", bundle_server->GetURL("/app.swbn"));
  SetUpdateInfo(app.app_id(), manifest_server->GetURL("/update_manifest.json"));

  provider().isolated_web_app_update_manager().DiscoverAndPrepareUpdatesNow();

  auto result = CallUpdateManifestInstalledApp(app.app_id());
  EXPECT_TRUE(result.has_value());

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  EXPECT_EQ(apps[0]->installed_version, "2.0.0");
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       UpdateManifestInstalledApp_NoUpdateAvailable) {
  web_app::IsolatedWebAppUrlInfo app = InstallUpdateManifestApp();

  auto manifest_server = ServeJson("/update_manifest.json", R"({
    "versions": [
      {
        "version": "1.0.0",
        "src": "https://example.com/app.swbn"
      }
    ]
  })");

  SetUpdateInfo(app.app_id(), manifest_server->GetURL("/update_manifest.json"));

  auto result = CallUpdateManifestInstalledApp(app.app_id());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->message, "App is already on the latest version.");
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       UpdateManifestInstalledApp_PinnedVersion_Success) {
  web_package::test::Ed25519KeyPair key_pair =
      web_package::test::Ed25519KeyPair::CreateRandom();
  web_app::IsolatedWebAppUrlInfo app =
      BuildAndInstallBundle(kManifestAppName, kAppBaseVersion, key_pair);

  auto bundle_server2 = BuildAndServeBundle("2.0.0", key_pair);
  auto bundle_server3 = BuildAndServeBundle("3.0.0", key_pair);
  auto manifest_server = ServeJson(
      "/update_manifest.json",
      base::StringPrintf(R"({
    "versions": [
      { "version": "2.0.0", "src": "%s" },
      { "version": "3.0.0", "src": "%s" }
    ]
  })",
                         bundle_server2->GetURL("/app.swbn").spec().c_str(),
                         bundle_server3->GetURL("/app.swbn").spec().c_str()));
  SetUpdateInfo(app.app_id(), manifest_server->GetURL("/update_manifest.json"));

  EXPECT_TRUE(
      CallUpdateManifestInstalledApp(app.app_id(), "2.0.0").has_value());

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  EXPECT_EQ(apps[0]->installed_version, "2.0.0");
}

IN_PROC_BROWSER_TEST_F(
    IwaDevHandlerUpdateManifestBrowserTest,
    UpdateManifestInstalledApp_PinnedVersion_DowngradeIgnored) {
  web_package::test::Ed25519KeyPair key_pair =
      web_package::test::Ed25519KeyPair::CreateRandom();
  web_app::IsolatedWebAppUrlInfo app =
      BuildAndInstallBundle(kManifestAppName, "2.0.0", key_pair);

  auto bundle_server1 = BuildAndServeBundle("1.0.0", key_pair);
  auto manifest_server =
      ServeUpdateManifest("1.0.0", bundle_server1->GetURL("/app.swbn"));
  SetUpdateInfo(app.app_id(), manifest_server->GetURL("/update_manifest.json"));

  auto result = CallUpdateManifestInstalledApp(app.app_id(), "1.0.0",
                                               /*allow_downgrades=*/false);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->message, "Version downgrade is not allowed.");

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  EXPECT_EQ(apps[0]->installed_version, "2.0.0");
}

IN_PROC_BROWSER_TEST_F(
    IwaDevHandlerUpdateManifestBrowserTest,
    UpdateManifestInstalledApp_PinnedVersion_DowngradeAllowed) {
  web_package::test::Ed25519KeyPair key_pair =
      web_package::test::Ed25519KeyPair::CreateRandom();
  web_app::IsolatedWebAppUrlInfo app =
      BuildAndInstallBundle(kManifestAppName, "2.0.0", key_pair);

  auto bundle_server1 = BuildAndServeBundle("1.0.0", key_pair);
  auto manifest_server =
      ServeUpdateManifest("1.0.0", bundle_server1->GetURL("/app.swbn"));
  SetUpdateInfo(app.app_id(), manifest_server->GetURL("/update_manifest.json"));

  EXPECT_TRUE(CallUpdateManifestInstalledApp(app.app_id(), "1.0.0",
                                             /*allow_downgrades=*/true)
                  .has_value());

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  EXPECT_EQ(apps[0]->installed_version, "1.0.0");
}

IN_PROC_BROWSER_TEST_F(
    IwaDevHandlerUpdateManifestBrowserTest,
    UpdateManifestInstalledApp_PinnedVersion_InvalidVersion) {
  web_app::IsolatedWebAppUrlInfo app = InstallUpdateManifestApp();

  auto empty_result = CallUpdateManifestInstalledApp(app.app_id(), "");
  ASSERT_FALSE(empty_result.has_value());
  EXPECT_EQ(empty_result.error()->message, "Invalid pinned version provided.");

  auto invalid_result =
      CallUpdateManifestInstalledApp(app.app_id(), "invalid_version");
  ASSERT_FALSE(invalid_result.has_value());
  EXPECT_EQ(invalid_result.error()->message,
            "Invalid pinned version provided.");
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       UpdateManifestInstalledApp_PinnedVersion_NotInManifest) {
  web_app::IsolatedWebAppUrlInfo app = InstallUpdateManifestApp();

  auto manifest_server =
      ServeUpdateManifest("1.0.0", GURL("https://example.com/app.swbn"));
  SetUpdateInfo(app.app_id(), manifest_server->GetURL("/update_manifest.json"));

  auto result = CallUpdateManifestInstalledApp(app.app_id(), "1.5.0");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->message,
            "Pinned version not found in update manifest.");

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  EXPECT_EQ(apps[0]->installed_version, "1.0.0");
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       UpdateManifestInstalledApp_Error_AppNotInstalled) {
  auto result = CallUpdateManifestInstalledApp("invalid_app_id");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->message, "App not found.");
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       SetUpdateChannel_ExistingChannel_Success) {
  web_package::test::Ed25519KeyPair key_pair =
      web_package::test::Ed25519KeyPair::CreateRandom();
  web_app::IsolatedWebAppUrlInfo app =
      BuildAndInstallBundle(kManifestAppName, kAppBaseVersion, key_pair);

  auto bundle_server = BuildAndServeBundle(kAppBaseVersion, key_pair);
  auto manifest_server = ServeJson(
      "/update_manifest.json",
      base::StringPrintf(R"({
    "channels": {
      "beta": { "name": "Beta Channel" }
    },
    "versions": [
      {
        "version": "%s",
        "src": "%s",
        "channels": ["default", "beta"]
      }
    ]
  })",
                         kAppBaseVersion,
                         bundle_server->GetURL("/app.swbn").spec().c_str()));

  SetUpdateInfo(app.app_id(), manifest_server->GetURL("/update_manifest.json"));
  ExpectUpdateChannel("default");
  EXPECT_TRUE(CallSetUpdateChannel(app.app_id(), "beta").has_value());
  ExpectUpdateChannel("beta");
  EXPECT_TRUE(CallSetUpdateChannel(app.app_id(), "default").has_value());
  ExpectUpdateChannel("default");
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       SetUpdateChannel_NotifiesObserver) {
  MockPage mock_page;
  mojo::Remote<iwa_dev::mojom::PageHandler> remote;
  auto handler = std::make_unique<IwaDevPageHandler>(
      browser()->GetTabStripModel()->GetActiveWebContents()->GetWebUI(),
      mock_page.BindAndGetRemote(), remote.BindNewPipeAndPassReceiver());

  web_app::IsolatedWebAppUrlInfo app = InstallUpdateManifestApp();
  EXPECT_TRUE(CallSetUpdateChannel(app.app_id(), "beta").has_value());

  iwa_dev::mojom::IwaDevModeAppInfoPtr app_info =
      mock_page.updated_future().Take();
  EXPECT_EQ(app_info->app_id, app.app_id());
  ASSERT_TRUE(app_info->source->is_update_info());
  EXPECT_EQ(app_info->source->get_update_info()->update_channel, "beta");
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       SetUpdateChannel_ChannelNotInManifest_Success) {
  web_app::IsolatedWebAppUrlInfo app = InstallUpdateManifestApp();
  ExpectUpdateChannel("default");

  // Setting a channel that isn't in the manifest succeeds without requiring a
  // server check.
  EXPECT_TRUE(CallSetUpdateChannel(app.app_id(), "dev").has_value());
  ExpectUpdateChannel("dev");
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       SetUpdateChannel_NotManifestApp) {
  web_app::IsolatedWebAppUrlInfo bundle_app = InstallBundleApp();
  auto bundle_result = CallSetUpdateChannel(bundle_app.app_id(), "beta");
  ASSERT_FALSE(bundle_result.has_value());
  EXPECT_EQ(bundle_result.error()->message,
            "App was not installed from an update manifest.");

  web_app::IsolatedWebAppUrlInfo proxy_app = InstallProxyApp();
  auto proxy_result = CallSetUpdateChannel(proxy_app.app_id(), "beta");
  ASSERT_FALSE(proxy_result.has_value());
  EXPECT_EQ(proxy_result.error()->message,
            "App was not installed from an update manifest.");
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       SetUpdateChannel_InvalidChannel) {
  web_app::IsolatedWebAppUrlInfo app = InstallUpdateManifestApp();

  auto empty_result = CallSetUpdateChannel(app.app_id(), "");
  ASSERT_FALSE(empty_result.has_value());
  EXPECT_EQ(empty_result.error()->message, "Invalid update channel provided.");

  auto non_utf8_result = CallSetUpdateChannel(app.app_id(), "\xff\xff");
  ASSERT_FALSE(non_utf8_result.has_value());
  EXPECT_EQ(non_utf8_result.error()->message,
            "Invalid update channel provided.");
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerUpdateManifestBrowserTest,
                       SetUpdateChannel_AppNotFound) {
  auto result = CallSetUpdateChannel("invalid_app_id", "beta");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->message, "App not found.");
}

class IwaDevHandlerObserverBrowserTest : public IwaDevHandlerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    IwaDevHandlerBrowserTest::SetUpOnMainThread();

    content::WebContents* web_contents =
        browser()->GetTabStripModel()->GetActiveWebContents();

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
