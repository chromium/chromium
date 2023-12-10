// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/startup/first_run_service.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_run_on_os_login_notification.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_run_on_os_login_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::AllOf;
using testing::Eq;
using testing::Field;
using testing::Property;

namespace web_app {

namespace {

constexpr char kTestApp1[] = "https://test.test1/";
constexpr char kTestApp2[] = "https://test.test2/";
constexpr char kTestApp3[] = "https://test.test3/";
constexpr char kTestApp4[] = "https://test.test4/";

constexpr char kTestAppName[] = "A Web App";

class WebAppRunOnOsLoginManagerBrowserTest
    : public WebAppControllerBrowserTest,
      public NotificationDisplayService::Observer {
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
    WebAppControllerBrowserTest::SetUpOnMainThread();

    // WebAppSettings to use during next launch.
    AddForceInstalledApp(kTestApp1, kTestAppName);
    AddRoolApp(kTestApp1, kRunWindowed);
  }

  // NotificationDisplayService::Observer:
  MOCK_METHOD(void,
              OnNotificationDisplayed,
              (const message_center::Notification&,
               const NotificationCommon::Metadata* const),
              (override));
  MOCK_METHOD(void,
              OnNotificationClosed,
              (const std::string& notification_id),
              (override));

  void OnNotificationDisplayServiceDestroyed(
      NotificationDisplayService* service) override {
    notification_observation_.Reset();
  }

 protected:
  void AddForceInstalledApp(const std::string& manifest_id,
                            const std::string& app_name) {
    base::test::TestFuture<void> app_sync_future;
    provider()
        .policy_manager()
        .SetOnAppsSynchronizedCompletedCallbackForTesting(
            app_sync_future.GetCallback());
    PrefService* prefs = profile()->GetPrefs();
    base::Value::List install_force_list =
        prefs->GetList(prefs::kWebAppInstallForceList).Clone();
    install_force_list.Append(
        base::Value::Dict()
            .Set(kUrlKey, manifest_id)
            .Set(kDefaultLaunchContainerKey, kDefaultLaunchContainerWindowValue)
            .Set(kFallbackAppNameKey, app_name));
    profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                   std::move(install_force_list));
    EXPECT_TRUE(app_sync_future.Wait());
  }

  void AddRoolApp(const std::string& manifest_id,
                  const std::string& run_on_os_login) {
    base::test::TestFuture<void> policy_refresh_sync_future;
    provider()
        .policy_manager()
        .SetRefreshPolicySettingsCompletedCallbackForTesting(
            policy_refresh_sync_future.GetCallback());
    PrefService* prefs = profile()->GetPrefs();
    base::Value::List web_app_settings =
        prefs->GetList(prefs::kWebAppSettings).Clone();
    web_app_settings.Append(base::Value::Dict()
                                .Set(kManifestId, manifest_id)
                                .Set(kRunOnOsLogin, run_on_os_login));
    prefs->SetList(prefs::kWebAppSettings, std::move(web_app_settings));
    EXPECT_TRUE(policy_refresh_sync_future.Wait());
  }

  Browser* FindAppBrowser() {
    auto web_app = FindAppWithUrlInScope(GURL(kTestApp1));
    if (!web_app) {
      return nullptr;
    }
    webapps::AppId app_id = web_app.value();

    return AppBrowserController::FindForWebApp(*profile(), app_id);
  }

  void RunOsLoginAndWait() {
    provider().run_on_os_login_manager().RunAppsOnOsLoginForTesting();
    base::test::TestFuture<void> future;
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }

  base::AutoReset<bool> skip_run_on_os_login_startup_;
  base::AutoReset<bool> skip_preinstalled_web_app_startup_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedObservation<NotificationDisplayService,
                          WebAppRunOnOsLoginManagerBrowserTest>
      notification_observation_{this};
};

IN_PROC_BROWSER_TEST_F(
    WebAppRunOnOsLoginManagerBrowserTest,
    WebAppRunOnOsLoginWithInitialPolicyValueLaunchesBrowserWindow) {
  // Wait for ROOL.
  RunOsLoginAndWait();

  // Should have 2 browsers: normal and app.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

  Browser* app_browser = FindAppBrowser();

  ASSERT_TRUE(app_browser);
}

IN_PROC_BROWSER_TEST_F(
    WebAppRunOnOsLoginManagerBrowserTest,
    WebAppRunOnOsLoginWithForceInstallLaunchesBrowserWindow) {
  // Wait for ROOL.
  RunOsLoginAndWait();

  // Should have 2 browsers: normal and app.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

  Browser* app_browser = FindAppBrowser();

  ASSERT_TRUE(app_browser);
}

IN_PROC_BROWSER_TEST_F(WebAppRunOnOsLoginManagerBrowserTest,
                       WebAppRunOnOsLoginNotificationOpensManagementUI) {
  // Wait for ROOL.
  RunOsLoginAndWait();

  // Should have 2 browsers: normal and app.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

  bool notification_shown = base::test::RunUntil([&]() {
    return notification_tester_->GetNotification(kRunOnOsLoginNotificationId)
        .has_value();
  });
  // Should have notification
  ASSERT_TRUE(notification_shown);

  message_center::Notification notification =
      notification_tester_->GetNotification(kRunOnOsLoginNotificationId)
          .value();
  EXPECT_THAT(
      notification,
      AllOf(Property(&message_center::Notification::id, Eq("run_on_os_login")),
            Property(&message_center::Notification::notifier_id,
                     Field(&message_center::NotifierId::id,
                           Eq("run_on_os_login_notifier"))),
            Property(&message_center::Notification::title,
                     Eq(u"A Web App was started automatically")),
            Property(&message_center::Notification::message,
                     Eq(u"Your administrator has set A Web App to start "
                        u"automatically every time you log in."))));

  notification_tester_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                      kRunOnOsLoginNotificationId,
                                      absl::nullopt, absl::nullopt);

  content::WebContents* active_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(GURL(chrome::kChromeUIManagementURL), active_contents->GetURL());
}

IN_PROC_BROWSER_TEST_F(WebAppRunOnOsLoginManagerBrowserTest,
                       WebAppRunOnOsLoginNotificationWithTwoApps) {
  AddForceInstalledApp(kTestApp2, kTestAppName);

  AddRoolApp(kTestApp2, kRunWindowed);

  // Wait for ROOL.
  RunOsLoginAndWait();

  bool notification_shown = base::test::RunUntil([&]() {
    return notification_tester_->GetNotification(kRunOnOsLoginNotificationId)
        .has_value();
  });
  // Should have notification
  ASSERT_TRUE(notification_shown);

  // Should have 3 browsers: normal and 2 apps.
  ASSERT_EQ(3u, chrome::GetBrowserCount(browser()->profile()));

  message_center::Notification notification =
      notification_tester_->GetNotification(kRunOnOsLoginNotificationId)
          .value();

  EXPECT_THAT(
      notification,
      AllOf(Property(&message_center::Notification::id, Eq("run_on_os_login")),
            Property(&message_center::Notification::notifier_id,
                     Field(&message_center::NotifierId::id,
                           Eq("run_on_os_login_notifier"))),
            Property(&message_center::Notification::title,
                     Eq(u"2 apps were started automatically")),
            Property(&message_center::Notification::message,
                     Eq(u"Your administrator has set A Web App and A Web App "
                        u"to start automatically every time you log in."))));
}

IN_PROC_BROWSER_TEST_F(WebAppRunOnOsLoginManagerBrowserTest,
                       WebAppRunOnOsLoginNotificationWithFourApps) {
  AddForceInstalledApp(kTestApp2, kTestAppName);
  AddForceInstalledApp(kTestApp3, kTestAppName);
  AddForceInstalledApp(kTestApp4, kTestAppName);
  AddRoolApp(kTestApp2, kRunWindowed);
  AddRoolApp(kTestApp3, kRunWindowed);
  AddRoolApp(kTestApp4, kRunWindowed);

  // Wait for ROOL.
  RunOsLoginAndWait();

  bool notification_shown = base::test::RunUntil([&]() {
    return notification_tester_->GetNotification(kRunOnOsLoginNotificationId)
        .has_value();
  });
  // Should have notification
  ASSERT_TRUE(notification_shown);

  // Should have 5 browsers: normal and 4 apps.
  ASSERT_EQ(5u, chrome::GetBrowserCount(browser()->profile()));

  message_center::Notification notification =
      notification_tester_->GetNotification(kRunOnOsLoginNotificationId)
          .value();
  EXPECT_THAT(
      notification,
      AllOf(Property(&message_center::Notification::id, Eq("run_on_os_login")),
            Property(&message_center::Notification::notifier_id,
                     Field(&message_center::NotifierId::id,
                           Eq("run_on_os_login_notifier"))),
            Property(&message_center::Notification::title,
                     Eq(u"4 apps were started automatically")),
            Property(&message_center::Notification::message,
                     Eq(u"Your administrator has set A Web App, A Web App "
                        u"and 2 other apps to start automatically every time "
                        u"you log in."))));
}

}  // namespace

}  // namespace web_app
