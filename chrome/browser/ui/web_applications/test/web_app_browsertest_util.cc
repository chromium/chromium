// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/test/service_worker_registration_waiter.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(IS_MAC)
#include <ImageIO/ImageIO.h>
#import "skia/ext/skia_utils_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <shellapi.h>
#include "ui/gfx/icon_util.h"
#endif

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

namespace {

void AutoAcceptDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    WebAppInstallationAcceptanceCallback acceptance_callback) {
  web_app_info->user_display_mode = DisplayMode::kStandalone;
  std::move(acceptance_callback)
      .Run(
          /*user_accepted=*/true, std::move(web_app_info));
}

}  // namespace

SkColor GetIconTopLeftColor(const base::FilePath& shortcut_path) {
#if BUILDFLAG(IS_MAC)
  base::FilePath icon_path =
      shortcut_path.AppendASCII("Contents/Resources/app.icns");
  base::ScopedCFTypeRef<CFDictionaryRef> empty_dict(
      CFDictionaryCreate(NULL, NULL, NULL, 0, NULL, NULL));
  base::ScopedCFTypeRef<CFURLRef> url(CFURLCreateFromFileSystemRepresentation(
      NULL, (const UInt8*)icon_path.value().c_str(), icon_path.value().length(),
      false));
  CGImageSourceRef source = CGImageSourceCreateWithURL(url, NULL);
  // Get the first icon in the .icns file (index 0)
  base::ScopedCFTypeRef<CGImageRef> cg_image(
      CGImageSourceCreateImageAtIndex(source, 0, empty_dict));
  SkBitmap bitmap = skia::CGImageToSkBitmap(cg_image);
  return bitmap.getColor(0, 0);
#else
#if BUILDFLAG(IS_WIN)
  SHFILEINFO file_info = {0};
  if (SHGetFileInfo(shortcut_path.value().c_str(), FILE_ATTRIBUTE_NORMAL,
                    &file_info, sizeof(file_info),
                    SHGFI_ICON | 0 | SHGFI_USEFILEATTRIBUTES)) {
    const SkBitmap bitmap = IconUtil::CreateSkBitmapFromHICON(file_info.hIcon);
    return bitmap.getColor(0, 0);
  }
#endif
  return 0;
#endif
}

AppId InstallWebAppFromPage(Browser* browser, const GURL& app_url) {
  NavigateToURLAndWait(browser, app_url);

  AppId app_id;
  base::RunLoop run_loop;

  auto* provider = WebAppProvider::GetForTest(browser->profile());
  DCHECK(provider);
  test::WaitUntilReady(provider);
  provider->install_manager().InstallWebAppFromManifestWithFallback(
      browser->tab_strip_model()->GetActiveWebContents(),
      WebAppInstallManager::WebAppInstallFlow::kInstallSite,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(&AutoAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&run_loop, &app_id](const AppId& installed_app_id,
                               webapps::InstallResultCode code) {
            DCHECK_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            app_id = installed_app_id;
            run_loop.Quit();
          }));

  run_loop.Run();
  return app_id;
}

AppId InstallWebAppFromManifest(Browser* browser, const GURL& app_url) {
  ServiceWorkerRegistrationWaiter registration_waiter(browser->profile(),
                                                      app_url);
  NavigateToURLAndWait(browser, app_url);
  registration_waiter.AwaitRegistration();

  AppId app_id;
  base::RunLoop run_loop;

  auto* provider = WebAppProvider::GetForTest(browser->profile());
  DCHECK(provider);
  test::WaitUntilReady(provider);
  provider->install_manager().InstallWebAppFromManifestWithFallback(
      browser->tab_strip_model()->GetActiveWebContents(),
      WebAppInstallManager::WebAppInstallFlow::kInstallSite,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(&AutoAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&run_loop, &app_id](const AppId& installed_app_id,
                               webapps::InstallResultCode code) {
            DCHECK_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            app_id = installed_app_id;
            run_loop.Quit();
          }));

  run_loop.Run();
  return app_id;
}

Browser* LaunchWebAppBrowser(Profile* profile, const AppId& app_id) {
  content::WebContents* web_contents =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
              app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
              WindowOpenDisposition::CURRENT_TAB,
              apps::mojom::LaunchSource::kFromTest));
  EXPECT_TRUE(web_contents);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(browser, app_id));
  return browser;
}

// Launches the app, waits for the app url to load.
Browser* LaunchWebAppBrowserAndWait(Profile* profile, const AppId& app_id) {
  ui_test_utils::UrlLoadObserver url_observer(
      WebAppProvider::GetForTest(profile)->registrar().GetAppLaunchUrl(app_id),
      content::NotificationService::AllSources());
  Browser* const app_browser = LaunchWebAppBrowser(profile, app_id);
  url_observer.Wait();
  return app_browser;
}

Browser* LaunchBrowserForWebAppInTab(Profile* profile, const AppId& app_id) {
  content::WebContents* web_contents =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
              app_id, apps::mojom::LaunchContainer::kLaunchContainerTab,
              WindowOpenDisposition::NEW_FOREGROUND_TAB,
              apps::mojom::LaunchSource::kFromTest));
  DCHECK(web_contents);

  WebAppTabHelper* tab_helper = WebAppTabHelper::FromWebContents(web_contents);
  DCHECK(tab_helper);
  EXPECT_EQ(app_id, tab_helper->GetAppId());

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  EXPECT_EQ(browser, chrome::FindLastActive());
  EXPECT_EQ(web_contents, browser->tab_strip_model()->GetActiveWebContents());
  return browser;
}

ExternalInstallOptions CreateInstallOptions(const GURL& url) {
  ExternalInstallOptions install_options(
      url, DisplayMode::kStandalone, ExternalInstallSource::kInternalDefault);
  // Avoid creating real shortcuts in tests.
  install_options.add_to_applications_menu = false;
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;

  return install_options;
}

ExternallyManagedAppManager::InstallResult ExternallyManagedAppManagerInstall(
    Profile* profile,
    ExternalInstallOptions install_options) {
  DCHECK(profile);
  auto* provider = WebAppProvider::GetForTest(profile);
  DCHECK(provider);
  test::WaitUntilReady(provider);
  base::RunLoop run_loop;
  ExternallyManagedAppManager::InstallResult result;

  provider->externally_managed_app_manager().Install(
      std::move(install_options),
      base::BindLambdaForTesting(
          [&result, &run_loop](
              const GURL& provided_url,
              ExternallyManagedAppManager::InstallResult install_result) {
            result = install_result;
            run_loop.Quit();
          }));
  run_loop.Run();
  return result;
}

void NavigateToURLAndWait(Browser* browser,
                          const GURL& url,
                          bool proceed_through_interstitial) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  {
    content::TestNavigationObserver observer(
        web_contents, content::MessageLoopRunner::QuitMode::DEFERRED);
    NavigateParams params(browser, url, ui::PAGE_TRANSITION_LINK);
    ui_test_utils::NavigateToURL(&params);
    observer.WaitForNavigationFinished();
  }

  if (!proceed_through_interstitial)
    return;

  {
    // Need a second TestNavigationObserver; the above one is spent.
    content::TestNavigationObserver observer(
        web_contents, content::MessageLoopRunner::QuitMode::DEFERRED);
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            browser->tab_strip_model()->GetActiveWebContents());
    ASSERT_TRUE(
        helper &&
        helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting());
    std::string javascript = "window.certificateErrorPageController.proceed();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, javascript));
    observer.Wait();
  }
}

// Performs a navigation and then checks that the toolbar visibility is as
// expected.
void NavigateAndCheckForToolbar(Browser* browser,
                                const GURL& url,
                                bool expected_visibility,
                                bool proceed_through_interstitial) {
  NavigateToURLAndWait(browser, url, proceed_through_interstitial);
  EXPECT_EQ(expected_visibility,
            browser->app_controller()->ShouldShowCustomTabBar());
}

AppMenuCommandState GetAppMenuCommandState(int command_id, Browser* browser) {
  DCHECK(!browser->app_controller())
      << "This check only applies to regular browser windows.";
  auto app_menu_model = std::make_unique<AppMenuModel>(nullptr, browser);
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  int index = -1;
  if (!app_menu_model->GetModelAndIndexForCommandId(command_id, &model,
                                                    &index)) {
    return kNotPresent;
  }
  return model->IsEnabledAt(index) ? kEnabled : kDisabled;
}

Browser* FindWebAppBrowser(Profile* profile, const AppId& app_id) {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile)
      continue;

    if (AppBrowserController::IsForWebApp(browser, app_id))
      return browser;
  }

  return nullptr;
}

void CloseAndWait(Browser* browser) {
  BrowserWaiter waiter(browser);
  browser->window()->Close();
  waiter.AwaitRemoved();
}

bool IsBrowserOpen(const Browser* test_browser) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser == test_browser)
      return true;
  }
  return false;
}

void UninstallWebApp(Profile* profile, const AppId& app_id) {
  auto* provider = WebAppProvider::GetForTest(profile);
  DCHECK(provider);
  DCHECK(provider->install_finalizer().CanUserUninstallWebApp(app_id));
  provider->install_finalizer().UninstallWebApp(
      app_id, webapps::WebappUninstallSource::kAppMenu, base::DoNothing());
}

void UninstallWebAppWithCallback(Profile* profile,
                                 const AppId& app_id,
                                 UninstallWebAppCallback callback) {
  auto* provider = WebAppProvider::GetForTest(profile);
  DCHECK(provider);
  DCHECK(provider->install_finalizer().CanUserUninstallWebApp(app_id));
  provider->install_finalizer().UninstallWebApp(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      base::BindOnce(
          [](UninstallWebAppCallback callback,
             webapps::UninstallResultCode code) {
            std::move(callback).Run(code ==
                                    webapps::UninstallResultCode::kSuccess);
          },
          std::move(callback)));
}

BrowserWaiter::BrowserWaiter(Browser* filter) : filter_(filter) {
  BrowserList::AddObserver(this);
}

BrowserWaiter::~BrowserWaiter() {
  BrowserList::RemoveObserver(this);
}

Browser* BrowserWaiter::AwaitAdded() {
  added_run_loop_.Run();
  return added_browser_;
}

Browser* BrowserWaiter::AwaitRemoved() {
  if (!removed_browser_)
    removed_run_loop_.Run();
  return removed_browser_;
}

void BrowserWaiter::OnBrowserAdded(Browser* browser) {
  if (filter_ && browser != filter_)
    return;
  added_browser_ = browser;
  added_run_loop_.Quit();
}
void BrowserWaiter::OnBrowserRemoved(Browser* browser) {
  if (filter_ && browser != filter_)
    return;
  removed_browser_ = browser;
  removed_run_loop_.Quit();
}

UpdateAwaiter::UpdateAwaiter(WebAppInstallManager& install_manager) {
  scoped_observation_.Observe(&install_manager);
}

void UpdateAwaiter::OnWebAppInstallManagerDestroyed() {
  scoped_observation_.Reset();
}

UpdateAwaiter::~UpdateAwaiter() = default;

void UpdateAwaiter::AwaitUpdate() {
  run_loop_.Run();
}

void UpdateAwaiter::OnWebAppManifestUpdated(const AppId& app_id,
                                            base::StringPiece old_name) {
  run_loop_.Quit();
}

}  // namespace web_app
