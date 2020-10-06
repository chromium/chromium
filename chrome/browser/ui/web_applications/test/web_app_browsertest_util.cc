// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/page_transition_types.h"

namespace web_app {

AppId InstallWebApp(Profile* profile,
                    std::unique_ptr<WebApplicationInfo> web_app_info) {
  if (web_app_info->title.empty())
    web_app_info->title = base::ASCIIToUTF16("WebApplicationInfo App Name");

  AppId app_id;
  base::RunLoop run_loop;
  auto* provider = WebAppProviderBase::GetProviderBase(profile);
  DCHECK(provider);
  provider->install_manager().InstallWebAppFromInfo(
      std::move(web_app_info), ForInstallableSite::kYes,
      WebappInstallSource::OMNIBOX_INSTALL_ICON,
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
            app_id = installed_app_id;
            run_loop.Quit();
          }));

  run_loop.Run();
  return app_id;
}

AppId InstallWebAppFromManifest(Browser* browser, const GURL& app_url) {
  NavigateToURLAndWait(browser, app_url);

  AppId app_id;
  base::RunLoop run_loop;

  auto* provider = WebAppProviderBase::GetProviderBase(browser->profile());
  DCHECK(provider);
  provider->install_manager().InstallWebAppFromManifestWithFallback(
      browser->tab_strip_model()->GetActiveWebContents(),
      /*force_shortcut_app=*/true, WebappInstallSource::MENU_BROWSER_TAB,
      base::BindLambdaForTesting(
          [](content::WebContents* initiator_web_contents,
             std::unique_ptr<WebApplicationInfo> web_app_info,
             ForInstallableSite for_installable_site,
             InstallManager::WebAppInstallationAcceptanceCallback
                 acceptance_callback) {
            std::move(acceptance_callback)
                .Run(
                    /*user_accepted=*/true, std::move(web_app_info));
          }),
      base::BindLambdaForTesting(
          [&run_loop, &app_id](const AppId& installed_app_id,
                               InstallResultCode code) {
            DCHECK_EQ(code, InstallResultCode::kSuccessNewInstall);
            app_id = installed_app_id;
            run_loop.Quit();
          }));

  run_loop.Run();
  return app_id;
}

Browser* LaunchWebAppBrowser(Profile* profile, const AppId& app_id) {
  EXPECT_TRUE(
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParams(apps::AppLaunchParams(
              app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
              WindowOpenDisposition::CURRENT_TAB,
              apps::mojom::AppLaunchSource::kSourceTest)));

  Browser* browser = chrome::FindLastActive();
  bool is_correct_app_browser =
      browser && GetAppIdFromApplicationName(browser->app_name()) == app_id;
  EXPECT_TRUE(is_correct_app_browser);

  return is_correct_app_browser ? browser : nullptr;
}

// Launches the app, waits for the app url to load.
Browser* LaunchWebAppBrowserAndWait(Profile* profile, const AppId& app_id) {
  ui_test_utils::UrlLoadObserver url_observer(
      WebAppProviderBase::GetProviderBase(profile)->registrar().GetAppLaunchUrl(
          app_id),
      content::NotificationService::AllSources());
  Browser* const app_browser = LaunchWebAppBrowser(profile, app_id);
  url_observer.Wait();
  return app_browser;
}

Browser* LaunchBrowserForWebAppInTab(Profile* profile, const AppId& app_id) {
  content::WebContents* web_contents =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParams(apps::AppLaunchParams(
              app_id, apps::mojom::LaunchContainer::kLaunchContainerTab,
              WindowOpenDisposition::NEW_FOREGROUND_TAB,
              apps::mojom::AppLaunchSource::kSourceTest));
  DCHECK(web_contents);

  WebAppTabHelperBase* tab_helper =
      WebAppTabHelperBase::FromWebContents(web_contents);
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

InstallResultCode PendingAppManagerInstall(
    Profile* profile,
    ExternalInstallOptions install_options) {
  DCHECK(profile);
  auto* provider = WebAppProviderBase::GetProviderBase(profile);
  DCHECK(provider);
  base::RunLoop run_loop;
  InstallResultCode result_code;

  provider->pending_app_manager().Install(
      std::move(install_options),
      base::BindLambdaForTesting(
          [&result_code, &run_loop](const GURL& provided_url,
                                    InstallResultCode code) {
            result_code = code;
            run_loop.Quit();
          }));
  run_loop.Run();
  return result_code;
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

bool IsBrowserOpen(const Browser* test_browser) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser == test_browser)
      return true;
  }
  return false;
}

void UninstallWebApp(Profile* profile, const AppId& app_id) {
  auto* provider = WebAppProviderBase::GetProviderBase(profile);
  DCHECK(provider);
  DCHECK(provider->install_finalizer().CanUserUninstallExternalApp(app_id));
  provider->install_finalizer().UninstallExternalAppByUser(app_id,
                                                           base::DoNothing());
}

SkColor ReadAppIconPixel(Profile* profile,
                         const AppId& app_id,
                         SquareSizePx size,
                         int x,
                         int y) {
  SkColor result;
  base::RunLoop run_loop;
  WebAppProviderBase::GetProviderBase(profile)->icon_manager().ReadIcons(
      app_id, IconPurpose::ANY, {size},
      base::BindLambdaForTesting(
          [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
            run_loop.Quit();
            result = icon_bitmaps.at(size).getColor(x, y);
          }));
  run_loop.Run();
  return result;
}

}  // namespace web_app
