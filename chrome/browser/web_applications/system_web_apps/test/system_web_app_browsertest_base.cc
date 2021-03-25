// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_apps/test/system_web_app_browsertest_base.h"

#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

SystemWebAppBrowserTestBase::SystemWebAppBrowserTestBase(bool install_mock) {}

SystemWebAppBrowserTestBase::~SystemWebAppBrowserTestBase() = default;

SystemWebAppManager& SystemWebAppBrowserTestBase::GetManager() {
  return WebAppProvider::Get(browser()->profile())->system_web_app_manager();
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
    GetManager().InstallSystemAppsForTesting();
  }

  // Ensure apps are registered with the |AppService| and populated in
  // |AppListModel|. Redirect to the profile that has an AppService that can be
  // flushed. This logic differs from WebAppProviderFactory::GetContextToUse().
  apps::AppServiceProxyFactory::GetForProfileRedirectInIncognito(
      browser()->profile())
      ->FlushMojoCallsForTesting();
}

apps::AppLaunchParams SystemWebAppBrowserTestBase::LaunchParamsForApp(
    SystemAppType system_app_type) {
  base::Optional<AppId> app_id =
      GetManager().GetAppIdForSystemApp(system_app_type);

  CHECK(app_id.has_value());
  return apps::AppLaunchParams(
      *app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::CURRENT_TAB,
      apps::mojom::AppLaunchSource::kSourceAppLauncher);
}

content::WebContents* SystemWebAppBrowserTestBase::LaunchApp(
    apps::AppLaunchParams&& params,
    bool wait_for_load,
    Browser** out_browser) {
  content::TestNavigationObserver navigation_observer(GetStartUrl(params));
  navigation_observer.StartWatchingNewWebContents();

  // AppServiceProxyFactory will DumpWithoutCrash when called with wrong
  // profile. In normal scenarios, no code path should trigger this.
  DCHECK(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      browser()->profile()));

  content::WebContents* web_contents =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
          ->BrowserAppLauncher()
          ->LaunchAppWithParams(std::move(params));

  if (wait_for_load)
    navigation_observer.Wait();

  if (out_browser)
    *out_browser = chrome::FindBrowserWithWebContents(web_contents);

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
             : WebAppProvider::Get(browser()->profile())
                   ->registrar()
                   .GetAppStartUrl(params.app_id);
}

GURL SystemWebAppBrowserTestBase::GetStartUrl() {
  return GetStartUrl(LaunchParamsForApp(GetMockAppType()));
}

SystemWebAppManagerBrowserTest::SystemWebAppManagerBrowserTest(
    bool install_mock)
    : SystemWebAppBrowserTestBase(install_mock) {
  if (install_mock) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp();
  }
}

void SystemWebAppManagerBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  SystemWebAppBrowserTestBase::SetUpCommandLine(command_line);
  if (profile_type() == TestProfileType::kGuest) {
    ConfigureCommandLineForGuestMode(command_line);
  } else if (profile_type() == TestProfileType::kIncognito) {
    command_line->AppendSwitch(::switches::kIncognito);
  }
}

std::string SystemWebAppManagerTestParamsToString(
    const ::testing::TestParamInfo<SystemWebAppManagerTestParams>& param_info) {
  std::string output;

  switch (std::get<0>(param_info.param)) {
    case TestProfileType::kRegular:
      break;
    case TestProfileType::kIncognito:
      output.append("_Incognito");
      break;
    case TestProfileType::kGuest:
      output.append("_Guest");
      break;
  }
  // The framework doesn't accept a blank param
  if (output.empty()) {
    output = "_Default";
  }
  return output;
}

}  // namespace web_app
