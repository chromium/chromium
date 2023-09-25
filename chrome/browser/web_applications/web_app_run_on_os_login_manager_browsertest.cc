// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_run_on_os_login_notification.h"
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
  WebAppRunOnOsLoginManagerBrowserTest()
      :  // ROOL startup done manually to ensure that SetUpOnMainThread is run
         // before
        skip_run_on_os_login_startup_(
            WebAppRunOnOsLoginManager::SkipStartupForTesting()),
        skip_preinstalled_web_app_startup_(
            PreinstalledWebAppManager::SkipStartupForTesting()) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kDesktopPWAsEnforceWebAppSettingsPolicy,
                              features::kDesktopPWAsRunOnOsLogin},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    notification_tester_ = std::make_unique<NotificationDisplayServiceTester>(
        /*profile=*/profile());
  }

 protected:
  void SetForceInstallPref() {
    profile()->GetPrefs()->SetList(
        prefs::kWebAppInstallForceList,
        base::Value::List().Append(
            base::Value::Dict()
                .Set(kUrlKey, kTestApp)
                .Set(kDefaultLaunchContainerKey,
                     kDefaultLaunchContainerWindowValue)));
  }

  void SetWebAppSettingsPref() {
    profile()->GetPrefs()->SetList(
        prefs::kWebAppSettings,
        base::Value::List().Append(base::Value::Dict()
                                       .Set(kManifestId, kTestApp)
                                       .Set(kRunOnOsLogin, kRunWindowed)));
  }

  void InstallWebApp() { InstallPWA(GURL(kTestApp)); }

  Browser* FindAppBrowser() {
    auto web_app = FindAppWithUrlInScope(GURL(kTestApp));
    if (!web_app) {
      return nullptr;
    }
    webapps::AppId app_id = web_app.value();

    return AppBrowserController::FindForWebApp(*profile(), app_id);
  }

  void AwaitPolicyAppsSynchronizedAndRunOnOsLoginComplete() {
    base::test::TestFuture<void> future;
    provider().on_external_managers_synchronized().Post(FROM_HERE,
                                                        future.GetCallback());
    EXPECT_TRUE(future.Wait());

    provider().run_on_os_login_manager().RunAppsOnOsLoginForTesting();

    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }

  base::AutoReset<bool> skip_run_on_os_login_startup_;
  base::AutoReset<bool> skip_preinstalled_web_app_startup_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
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
  AwaitPolicyAppsSynchronizedAndRunOnOsLoginComplete();

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
  AwaitPolicyAppsSynchronizedAndRunOnOsLoginComplete();

  // Should have 2 browsers: normal and app.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

  Browser* app_browser = FindAppBrowser();

  ASSERT_TRUE(app_browser);
}

IN_PROC_BROWSER_TEST_F(WebAppRunOnOsLoginManagerBrowserTest,
                       PRE_WebAppRunOnOsLoginNotificationOpensManagementUI) {
  // WebAppSettings to use during next launch.
  SetWebAppSettingsPref();
  // Install WebApp here so following test run will launch them.
  InstallWebApp();
}

IN_PROC_BROWSER_TEST_F(WebAppRunOnOsLoginManagerBrowserTest,
                       WebAppRunOnOsLoginNotificationOpensManagementUI) {
  // Wait for force-install and ROOL.
  AwaitPolicyAppsSynchronizedAndRunOnOsLoginComplete();

  // Should have 2 browsers: normal and app.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

  // Should have notification
  ASSERT_TRUE(notification_tester_->GetNotification(kRunOnOsLoginNotificationId)
                  .has_value());

  notification_tester_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                      kRunOnOsLoginNotificationId,
                                      absl::nullopt, absl::nullopt);

  content::WebContents* active_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(GURL(chrome::kChromeUIManagementURL), active_contents->GetURL());
}

}  // namespace

}  // namespace web_app
