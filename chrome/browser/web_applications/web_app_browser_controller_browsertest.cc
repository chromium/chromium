// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"

#include <optional>
#include <vector>

#include "base/auto_reset.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_scope.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace web_app {

class WebAppBrowserControllerBrowserTest : public WebAppBrowserTestBase {
 public:
  WebAppBrowserControllerBrowserTest() {
    CHECK(embedded_test_server()->Start());
    std::vector<ExternalInstallOptions> preinstall_options;
    {
      GURL install_url =
          embedded_test_server()->GetURL("/web_apps/scope_updating/page.html");
      ExternalInstallOptions options(install_url,
                                     mojom::UserDisplayMode::kStandalone,
                                     ExternalInstallSource::kExternalDefault);
      preinstall_options.push_back(std::move(options));
    }
    custom_preinstalls_ = PreinstalledWebAppManager::SetParsedConfigsForTesting(
        std::move(preinstall_options));
  }
  ~WebAppBrowserControllerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    test::WaitUntilWebAppProviderAndSubsystemsReady(&provider());
  }

  Profile* profile() { return browser()->profile(); }

  std::optional<base::AutoReset<std::vector<ExternalInstallOptions>>>
      custom_preinstalls_;
};

IN_PROC_BROWSER_TEST_F(WebAppBrowserControllerBrowserTest,
                       ToolbarUpdatedOnReinstall) {
  webapps::ManifestId manifest_id =
      embedded_test_server()->GetURL("/web_apps/scope_updating/");
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/scope_updating/page.html");
  GURL out_of_scope_url = embedded_test_server()->GetURL(
      "/web_apps/scope_updating/out-of-scope.html");

  // 1. App should be preinstalled.
  const WebApp* web_app = provider().registrar_unsafe().GetAppById(
      GenerateAppIdFromManifestId(manifest_id));
  ASSERT_TRUE(web_app);
  webapps::AppId app_id = web_app->app_id();

  // 2. Launch the app and navigate out of scope.
  Browser* app_browser = web_app::LaunchWebAppBrowser(profile(), app_id);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, out_of_scope_url));
  WebAppBrowserController* controller =
      app_browser->app_controller()->AsWebAppBrowserController();
  EXPECT_TRUE(controller->ShouldShowCustomTabBar());

  // 3. "User" installs the app with a wider scope.
  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      install_future;
  GURL update_url = embedded_test_server()->GetURL(
      "/web_apps/scope_updating/page_update.html");
  auto install_info =
      std::make_unique<WebAppInstallInfo>(manifest_id, start_url);
  install_info->scope =
      embedded_test_server()->GetURL("/web_apps/scope_updating/");
  install_info->title = u"New Title";
  provider().scheduler().InstallFromInfoWithParams(
      std::move(install_info),
      /*overwrite_existing_manifest_fields=*/true,
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      install_future.GetCallback(), WebAppInstallParams());
  ASSERT_TRUE(install_future.Wait());
  EXPECT_EQ(install_future.Get<webapps::AppId>(), app_id);
  EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
            webapps::InstallResultCode::kSuccessNewInstall);

  // 4. Verify the toolbar is now hidden.
  EXPECT_FALSE(controller->ShouldShowCustomTabBar());
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserControllerBrowserTest,
                       ToolbarAppearsOnReinstallNarrowing) {
  webapps::ManifestId manifest_id =
      embedded_test_server()->GetURL("/web_apps/scope_updating/");
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/scope_updating/page.html");
  GURL out_of_scope_url = embedded_test_server()->GetURL(
      "/web_apps/scope_updating/out-of-scope.html");
  GURL updating_url = embedded_test_server()->GetURL(
      "/web_apps/scope_updating/page_update.html");

  // 1. App should be preinstalled.
  const WebApp* web_app = provider().registrar_unsafe().GetAppById(
      GenerateAppIdFromManifestId(manifest_id));
  ASSERT_TRUE(web_app);
  webapps::AppId app_id = web_app->app_id();

  // 2. Launch the app to a page that will update it's scope..
  UpdateAwaiter update_awaiter(provider().install_manager());
  Browser* app_browser =
      web_app::LaunchWebAppToURL(profile(), app_id, updating_url);
  ASSERT_TRUE(app_browser);
  update_awaiter.AwaitUpdate();
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // 3. Check that the scope changed
  WebAppScope effective_scope =
      *provider().registrar_unsafe().GetEffectiveScope(app_id);
  EXPECT_TRUE(effective_scope.IsInScope(out_of_scope_url));

  // 4. Navigate to the otherwise out of scope url, but with the updated scope
  // the app should now be in scope.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, out_of_scope_url));
  WebAppBrowserController* controller =
      app_browser->app_controller()->AsWebAppBrowserController();
  EXPECT_FALSE(controller->ShouldShowCustomTabBar());

  // 4. "User" installs the app with a narrower scope.
  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      install_future;
  GURL update_url =
      embedded_test_server()->GetURL("/web_apps/scope_updating/page.html");
  auto install_info =
      std::make_unique<WebAppInstallInfo>(manifest_id, start_url);
  install_info->scope =
      embedded_test_server()->GetURL("/web_apps/scope_updating/page.html");
  install_info->title = u"New Title";
  provider().scheduler().InstallFromInfoWithParams(
      std::move(install_info),
      /*overwrite_existing_manifest_fields=*/true,
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      install_future.GetCallback(), WebAppInstallParams());
  ASSERT_TRUE(install_future.Wait());
  EXPECT_EQ(install_future.Get<webapps::AppId>(), app_id);
  EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
            webapps::InstallResultCode::kSuccessNewInstall);

  // 4. Verify the toolbar is now shown.
  EXPECT_TRUE(controller->ShouldShowCustomTabBar());
}

}  // namespace web_app
