// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_forward.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_run_on_os_login_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "content/public/test/browser_test.h"

namespace web_app {

namespace {

constexpr char kTestApp[] = "https://test.test/";

class WebAppRunOnOsLoginManagerBrowserTest
    : public WebAppControllerBrowserTest {
 public:
  WebAppRunOnOsLoginManagerBrowserTest() {
    BuildAndInitFeatureList();
    PreinstalledWebAppManager::SkipStartupForTesting();
  }

 protected:
  void BuildAndInitFeatureList() {
    scoped_feature_list_.Reset();
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    enabled_features.push_back(
        features::kDesktopPWAsEnforceWebAppSettingsPolicy);
    enabled_features.push_back(features::kDesktopPWAsRunOnOsLogin);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetForceInstallPref() {
    base::Value item(base::Value::Type::DICT);
    item.SetKey(kUrlKey, base::Value(kTestApp));
    item.SetKey(kDefaultLaunchContainerKey,
                base::Value(kDefaultLaunchContainerWindowValue));
    base::Value::List list;
    list.Append(std::move(item));
    profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                   std::move(list));
  }

  void SetWebAppSettingsPref() {
    base::Value item(base::Value::Type::DICT);
    item.SetKey(kManifestId, base::Value(kTestApp));
    item.SetKey(kRunOnOsLogin, base::Value(kRunWindowed));
    base::Value::List list;
    list.Append(std::move(item));
    profile()->GetPrefs()->SetList(prefs::kWebAppSettings, std::move(list));
  }

  void InstallWebApp() { InstallPWA(GURL(kTestApp)); }

  Browser* FindAppBrowser() {
    auto web_app = FindAppWithUrlInScope(GURL(kTestApp));
    if (!web_app) {
      return nullptr;
    }
    AppId app_id = web_app.value();

    return AppBrowserController::FindForWebApp(*profile(), app_id);
  }

  void AwaitPolicyAppsSynchronizedAndCommandsComplete() {
    base::test::TestFuture<void> future;
    provider().on_external_managers_synchronized().Post(FROM_HERE,
                                                        future.GetCallback());
    EXPECT_TRUE(future.Wait());

    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    WebAppRunOnOsLoginManagerBrowserTest,
    PRE_WebAppRunOnOsLoginWithInitialPolicyValueLaunchesBrowserWindow) {
  // WebAppSettings to use during next launch.
  SetWebAppSettingsPref();
  // Install WebApp here so following test run will launch them.
  InstallWebApp();
}

IN_PROC_BROWSER_TEST_F(
    WebAppRunOnOsLoginManagerBrowserTest,
    WebAppRunOnOsLoginWithInitialPolicyValueLaunchesBrowserWindow) {
  // Wait for ROOL.
  AwaitPolicyAppsSynchronizedAndCommandsComplete();

  // Should have 2 browsers: normal and app.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

  Browser* app_browser = FindAppBrowser();

  ASSERT_TRUE(app_browser);
}

IN_PROC_BROWSER_TEST_F(
    WebAppRunOnOsLoginManagerBrowserTest,
    PRE_WebAppRunOnOsLoginWithForceInstallLaunchesBrowserWindow) {
  // WebAppSettings to use during next launch.
  SetWebAppSettingsPref();
  // Set force-installs for following test to install app before ROOL.
  SetForceInstallPref();
}

IN_PROC_BROWSER_TEST_F(
    WebAppRunOnOsLoginManagerBrowserTest,
    WebAppRunOnOsLoginWithForceInstallLaunchesBrowserWindow) {
  // Wait for force-install and ROOL.
  AwaitPolicyAppsSynchronizedAndCommandsComplete();

  // Should have 2 browsers: normal and app.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

  Browser* app_browser = FindAppBrowser();

  ASSERT_TRUE(app_browser);
}

}  // namespace

}  // namespace web_app
