// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/iwa_dev/iwa_dev_page_handler.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/webui/iwa_dev/iwa_dev_ui.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class IwaDevHandlerBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  IwaDevHandlerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kIsolatedWebAppDevUi);
  }

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
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://iwa-dev")));
    EXPECT_TRUE(content::WaitForLoadStop(web_contents));

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

  web_app::IsolatedWebAppUrlInfo InstallBundle(std::string_view name,
                                               std::string_view version) {
    std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().SetName(name).SetVersion(version))
            .BuildBundle();
    auto result = app->InstallWithSource(
        profile(), &web_app::IsolatedWebAppInstallSource::FromDevUi);
    CHECK(result.has_value()) << result.error();
    return result.value();
  }

  void ExpectBundleInstalledAtProfileDir(const base::FilePath& bundle_path) {
    base::FilePath expected_parent =
        profile()->GetPath().Append(FILE_PATH_LITERAL("iwa"));
    EXPECT_EQ(bundle_path.DirName().DirName(), expected_parent);
    EXPECT_EQ(bundle_path.BaseName().value(), FILE_PATH_LITERAL("main.swbn"));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(IwaDevHandlerBrowserTest,
                       GetInstalledAppsInfo_ProxyApp) {
  std::unique_ptr<net::EmbeddedTestServer> server =
      CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
  auto proxy_app = InstallDevModeProxyIsolatedWebApp(server->GetOrigin());

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  const auto& app = apps[0];

  EXPECT_EQ(app->app_id, proxy_app.app_id());
  EXPECT_EQ(app->name, "Simple Isolated App");
  EXPECT_EQ(app->installed_version, "1.0.0");
  ASSERT_TRUE(app->source->is_proxy_origin());
  EXPECT_EQ(app->source->get_proxy_origin(), server->GetOrigin());
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerBrowserTest,
                       GetInstalledAppsInfo_LocalBundleApp) {
  web_app::IsolatedWebAppUrlInfo app =
      InstallBundle("Local Bundle IWA Name", "1.0.0");

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  const auto& app_info = apps[0];

  EXPECT_EQ(app_info->app_id, app.app_id());
  EXPECT_EQ(app_info->name, "Local Bundle IWA Name");
  EXPECT_EQ(app_info->installed_version, "1.0.0");
  ASSERT_TRUE(app_info->source->is_bundle_path());
  ExpectBundleInstalledAtProfileDir(app_info->source->get_bundle_path());
}

IN_PROC_BROWSER_TEST_F(IwaDevHandlerBrowserTest,
                       GetInstalledAppsInfo_ManifestApp) {
  web_app::IsolatedWebAppUrlInfo app = InstallBundle("Test App", "2.0.0");

  GURL update_manifest_url("https://example.com/update.json");
  SetUpdateInfo(app.app_id(), update_manifest_url);

  auto apps = GetInstalledAppsInfo();
  ASSERT_EQ(apps.size(), 1u);
  const auto& app_info = apps[0];

  EXPECT_EQ(app_info->app_id, app.app_id());
  EXPECT_EQ(app_info->name, "Test App");
  EXPECT_EQ(app_info->installed_version, "2.0.0");
  ASSERT_TRUE(app_info->source->is_update_info());
  const auto& update_info = app_info->source->get_update_info();
  EXPECT_EQ(update_info->update_manifest_url, update_manifest_url);
  EXPECT_EQ(update_info->update_channel, "default");
}
