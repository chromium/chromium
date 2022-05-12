// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_apps/test/system_web_app_browsertest_base.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

SystemWebAppBrowserTestBase::SystemWebAppBrowserTestBase(bool install_mock) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EnableSystemWebAppsInLacrosForTesting();
#endif
}

SystemWebAppBrowserTestBase::~SystemWebAppBrowserTestBase() = default;

SystemWebAppManager& SystemWebAppBrowserTestBase::GetManager() {
  return WebAppProvider::GetForSystemWebApps(browser()->profile())
      ->system_web_app_manager();
}

SystemAppType SystemWebAppBrowserTestBase::GetMockAppType() {
  CHECK(maybe_installation_);
  return maybe_installation_->GetType();
}

void SystemWebAppBrowserTestBase::WaitForTestSystemAppInstall() {
  // Wait for the System Web Apps to install.
  if (maybe_installation_) {
    maybe_installation_->WaitForAppInstall();
  } else {
    // Avoid recreating system apps in tests since AppBrowserController keeps a
    // reference to SystemWebAppDelegates.
    if (!GetManager().GetRegisteredSystemAppsForTesting().empty())
      return;
    GetManager().InstallSystemAppsForTesting();
  }

  // Ensure apps are registered with the |AppService| and populated in
  // |AppListModel|.
  web_app::FlushSystemWebAppLaunchesForTesting(browser()->profile());
}

apps::AppLaunchParams SystemWebAppBrowserTestBase::LaunchParamsForApp(
    SystemAppType system_app_type) {
  absl::optional<AppId> app_id =
      GetManager().GetAppIdForSystemApp(system_app_type);

  CHECK(app_id.has_value());
  return apps::AppLaunchParams(
      *app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::CURRENT_TAB,
      apps::mojom::LaunchSource::kFromAppListGrid);
}

content::WebContents* SystemWebAppBrowserTestBase::LaunchApp(
    apps::AppLaunchParams&& params,
    bool wait_for_load,
    Browser** out_browser) {
  content::TestNavigationObserver navigation_observer(GetStartUrl(params));
  navigation_observer.StartWatchingNewWebContents();

  // AppServiceProxyFactory will DCHECK when called with wrong profile. In
  // normal scenarios, no code path should trigger this.
  DCHECK(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      browser()->profile()));

  if (!params.launch_files.empty()) {
    // SWA browser tests bypass the code in `WebAppPublisherHelper` that fills
    // in `override_url`, so fill it in here, assuming the file handler action
    // URL matches the start URL.
    params.override_url =
        WebAppProvider::GetForSystemWebApps(browser()->profile())
            ->registrar()
            .GetAppStartUrl(params.app_id);
  }

  content::WebContents* web_contents =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(std::move(params));

  if (wait_for_load) {
    navigation_observer.Wait();
    DCHECK(navigation_observer.last_navigation_succeeded());
  }

  if (out_browser) {
    *out_browser = web_contents
                       ? chrome::FindBrowserWithWebContents(web_contents)
                       : nullptr;
  }

  return web_contents;
}

content::WebContents* SystemWebAppBrowserTestBase::LaunchApp(
    apps::AppLaunchParams&& params,
    Browser** browser) {
  return LaunchApp(std::move(params), /* wait_for_load */ true, browser);
}

content::WebContents* SystemWebAppBrowserTestBase::LaunchApp(
    SystemAppType type,
    Browser** browser) {
  return LaunchApp(LaunchParamsForApp(type), browser);
}

content::WebContents* SystemWebAppBrowserTestBase::LaunchAppWithoutWaiting(
    apps::AppLaunchParams&& params,
    Browser** browser) {
  return LaunchApp(std::move(params), /* wait_for_load */ false, browser);
}

content::WebContents* SystemWebAppBrowserTestBase::LaunchAppWithoutWaiting(
    SystemAppType type,
    Browser** browser) {
  return LaunchAppWithoutWaiting(LaunchParamsForApp(type), browser);
}

GURL SystemWebAppBrowserTestBase::GetStartUrl(
    const apps::AppLaunchParams& params) {
  return params.override_url.is_valid()
             ? params.override_url
             : WebAppProvider::GetForSystemWebApps(browser()->profile())
                   ->registrar()
                   .GetAppStartUrl(params.app_id);
}

GURL SystemWebAppBrowserTestBase::GetStartUrl(SystemAppType type) {
  return GetStartUrl(LaunchParamsForApp(type));
}

GURL SystemWebAppBrowserTestBase::GetStartUrl() {
  return GetStartUrl(LaunchParamsForApp(GetMockAppType()));
}

size_t SystemWebAppBrowserTestBase::GetSystemWebAppBrowserCount(
    SystemAppType type) {
  auto* browser_list = BrowserList::GetInstance();
  return std::count_if(
      browser_list->begin(), browser_list->end(), [&](Browser* browser) {
        return web_app::IsBrowserForSystemWebApp(browser, type);
      });
}

SystemWebAppManagerBrowserTest::SystemWebAppManagerBrowserTest(
    bool install_mock)
    : TestProfileTypeMixin<SystemWebAppBrowserTestBase>(install_mock) {
  if (install_mock) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp();
  }
}

}  // namespace web_app
