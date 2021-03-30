// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace web_app {

class WebAppUninstallBrowserTest : public WebAppControllerBrowserTest {
 public:
  GURL GetSecureAppURL() {
    return https_server()->GetURL("app.com", "/ssl/google.html");
  }

  void UninstallWebApp(const AppId& app_id) {
    WebAppProviderBase* const provider =
        WebAppProviderBase::GetProviderBase(profile());
    base::RunLoop run_loop;

    DCHECK(provider->install_finalizer().CanUserUninstallExternalApp(app_id));
    provider->install_finalizer().UninstallExternalAppByUser(
        app_id, base::BindLambdaForTesting([&](bool uninstalled) {
          EXPECT_TRUE(uninstalled);
          run_loop.Quit();
        }));

    run_loop.Run();
    base::RunLoop().RunUntilIdle();
  }
};

// Tests that app windows are restored in a tab if the app is uninstalled.
IN_PROC_BROWSER_TEST_F(WebAppUninstallBrowserTest,
                       RestoreAppWindowForUninstalledApp) {
  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);

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
      chrome::FindBrowserWithWebContents(restored_web_contents);

  EXPECT_FALSE(restored_browser->is_type_app());
  EXPECT_TRUE(restored_browser->is_type_normal());
}

// Check that uninstalling a PWA with a window opened doesn't crash.
IN_PROC_BROWSER_TEST_F(WebAppUninstallBrowserTest,
                       UninstallPwaWithWindowOpened) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url = GetSecureAppURL();
  const AppId app_id = InstallPWA(app_url);
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
  const AppId app_id = InstallPWA(app_url);
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
  const AppId app_id = InstallPWA(app_url);

  apps::AppLaunchParams params(
      app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW,
      apps::mojom::AppLaunchSource::kSourceTest);

  UninstallWebApp(app_id);
  content::WebContents* const web_contents =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->BrowserAppLauncher()
          ->LaunchAppWithParams(std::move(params));
  EXPECT_EQ(web_contents, nullptr);
}

}  // namespace web_app
