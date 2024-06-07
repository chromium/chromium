// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace web_app {

class WebAppUninstallBrowserTest : public WebAppBrowserTestBase {
 public:
  GURL GetSecureAppURL() {
    return https_server()->GetURL("app.com", "/ssl/google.html");
  }

  void UninstallWebApp(const webapps::AppId& app_id) {
    WebAppProvider* const provider = WebAppProvider::GetForTest(profile());

    base::test::TestFuture<webapps::UninstallResultCode> future;
    DCHECK(provider->registrar_unsafe().CanUserUninstallWebApp(app_id));
    provider->scheduler().RemoveUserUninstallableManagements(
        app_id, webapps::WebappUninstallSource::kAppMenu, future.GetCallback());
    EXPECT_EQ(future.Get(), webapps::UninstallResultCode::kAppRemoved);

    base::RunLoop().RunUntilIdle();
  }
};

// Tests that app windows are restored in a tab if the app is uninstalled.
IN_PROC_BROWSER_TEST_F(WebAppUninstallBrowserTest,
                       RestoreAppWindowForUninstalledApp) {
  const GURL app_url = GetSecureAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);

  {
    Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);
    ASSERT_TRUE(app_browser->is_type_app());
    ASSERT_FALSE(app_browser->is_type_normal());
    app_browser->window()->Close();
  }

  UninstallWebApp(app_id);

  content::WebContentsAddedObserver new_contents_observer;

  sessions::TabRestoreService* const service =
      TabRestoreServiceFactory::GetForProfile(profile());
  service->RestoreMostRecentEntry(nullptr);

  content::WebContents* const restored_web_contents =
      new_contents_observer.GetWebContents();
  Browser* const restored_browser =
      chrome::FindBrowserWithTab(restored_web_contents);

  EXPECT_FALSE(restored_browser->is_type_app());
  EXPECT_TRUE(restored_browser->is_type_normal());
}

// Check that uninstalling a PWA with a window opened doesn't crash.
IN_PROC_BROWSER_TEST_F(WebAppUninstallBrowserTest,
                       UninstallPwaWithWindowOpened) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url = GetSecureAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  EXPECT_TRUE(IsBrowserOpen(app_browser));

  UninstallWebApp(app_id);

  EXPECT_FALSE(IsBrowserOpen(app_browser));
}

// PWAs moved to tabbed browsers should not get closed when uninstalled.
IN_PROC_BROWSER_TEST_F(WebAppUninstallBrowserTest,
                       UninstallPwaWithWindowMovedToTab) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url = GetSecureAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  EXPECT_TRUE(IsBrowserOpen(app_browser));

  Browser* const tabbed_browser = chrome::OpenInChrome(app_browser);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsBrowserOpen(tabbed_browser));
  EXPECT_EQ(tabbed_browser, browser());
  EXPECT_FALSE(IsBrowserOpen(app_browser));

  UninstallWebApp(app_id);

  EXPECT_TRUE(IsBrowserOpen(tabbed_browser));
  EXPECT_EQ(tabbed_browser->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL(),
            GetSecureAppURL());
}

IN_PROC_BROWSER_TEST_F(WebAppUninstallBrowserTest, CannotLaunchAfterUninstall) {
  const GURL app_url = GetSecureAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);

  apps::AppLaunchParams params(
      app_id, apps::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest);

  UninstallWebApp(app_id);
  content::WebContents* const web_contents =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(std::move(params));
  EXPECT_EQ(web_contents, nullptr);
}

IN_PROC_BROWSER_TEST_F(WebAppUninstallBrowserTest, TwoUninstallCalls) {
  const GURL app_url = GetSecureAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);

  base::RunLoop run_loop;
  bool quit_run_loop = false;
  bool uninstall_delegate_called = false;

  // Trigger app uninstall without waiting for result.
  WebAppProvider* const provider = WebAppProvider::GetForTest(profile());
  EXPECT_TRUE(provider->registrar_unsafe().IsInstalled(app_id));
  DCHECK(provider->registrar_unsafe().CanUserUninstallWebApp(app_id));
  provider->scheduler().RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        if (quit_run_loop)
          run_loop.Quit();
        quit_run_loop = true;
      }));

  EXPECT_EQ(1u, provider->command_manager().GetCommandCountForTesting());

  // Trigger second uninstall call and wait for result.
  provider->scheduler().RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        if (quit_run_loop)
          run_loop.Quit();
        quit_run_loop = true;
      }));

  EXPECT_EQ(2u, provider->command_manager().GetCommandCountForTesting());

  WebAppInstallManagerObserverAdapter install_observer(
      &provider->install_manager());
  install_observer.SetWebAppWillBeUninstalledDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& uninstall_app_id) {
        EXPECT_EQ(app_id, uninstall_app_id);
        EXPECT_FALSE(uninstall_delegate_called);

        // Validate that uninstalling flag is set
        auto* app = provider->registrar_unsafe().GetAppById(app_id);
        EXPECT_TRUE(app);
        EXPECT_FALSE(app->is_uninstalling());
        uninstall_delegate_called = true;
      }));

  run_loop.Run();
  EXPECT_FALSE(provider->registrar_unsafe().IsInstalled(app_id));
}

}  // namespace web_app
