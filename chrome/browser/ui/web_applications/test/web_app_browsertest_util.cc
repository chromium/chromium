// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper.h"
#include "chrome/common/web_application_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

AppId InstallWebApp(Profile* profile,
                    std::unique_ptr<WebApplicationInfo> web_app_info) {
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

Browser* LaunchWebAppBrowser(Profile* profile, const AppId& app_id) {
  EXPECT_TRUE(
      apps::LaunchService::Get(profile)->OpenApplication(apps::AppLaunchParams(
          app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::CURRENT_TAB,
          apps::mojom::AppLaunchSource::kSourceTest)));

  Browser* browser = chrome::FindLastActive();
  bool is_correct_app_browser =
      browser && GetAppIdFromApplicationName(browser->app_name()) == app_id;
  EXPECT_TRUE(is_correct_app_browser);

  return is_correct_app_browser ? browser : nullptr;
}

Browser* LaunchBrowserForWebAppInTab(Profile* profile, const AppId& app_id) {
  content::WebContents* web_contents =
      apps::LaunchService::Get(profile)->OpenApplication(apps::AppLaunchParams(
          app_id, apps::mojom::LaunchContainer::kLaunchContainerTab,
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          apps::mojom::AppLaunchSource::kSourceTest));
  DCHECK(web_contents);

  WebAppTabHelper* tab_helper = WebAppTabHelper::FromWebContents(web_contents);
  DCHECK(tab_helper);
  EXPECT_EQ(app_id, tab_helper->app_id());

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

InstallResultCode InstallApp(Profile* profile,
                             ExternalInstallOptions install_options) {
  base::RunLoop run_loop;
  InstallResultCode result_code;

  WebAppProviderBase::GetProviderBase(profile)->pending_app_manager().Install(
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

}  // namespace web_app
