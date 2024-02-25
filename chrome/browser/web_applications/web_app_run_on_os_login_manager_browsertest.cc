// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_run_on_os_login_manager.h"

#include <vector>

#include "base/auto_reset.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

using testing::_;
using testing::AllOf;
using testing::DoAll;
using testing::Eq;
using testing::Field;
using testing::Invoke;
using testing::Property;
using testing::Return;
using testing::SetArgPointee;

namespace web_app {

namespace {

constexpr char kTestApp[] = "https://test.test";

constexpr char kTestAppName[] = "WebApp";

class MockNetworkConnectionTracker : public network::NetworkConnectionTracker {
 public:
  MockNetworkConnectionTracker() = default;
  MockNetworkConnectionTracker(MockNetworkConnectionTracker&) = delete;
  MockNetworkConnectionTracker& operator=(const MockNetworkConnectionTracker&) =
      delete;
  ~MockNetworkConnectionTracker() override = default;

  MOCK_METHOD(bool,
              GetConnectionType,
              (network::mojom::ConnectionType*,
               network::NetworkConnectionTracker::ConnectionTypeCallback),
              (override));

  // Make this function visible so we can simulate a call from the test.
  void OnNetworkChanged(network::mojom::ConnectionType type) override {
    network::NetworkConnectionTracker::OnNetworkChanged(type);
  }
};

class WebAppRunOnOsLoginManagerBrowserTest
    : public WebAppControllerBrowserTest,
      public NotificationDisplayService::Observer {
 public:
  WebAppRunOnOsLoginManagerBrowserTest()
      :  // ROOL startup done manually to ensure that SetUpOnMainThread is run
         // before
        skip_run_on_os_login_startup_(std::make_unique<base::AutoReset<bool>>(
            WebAppRunOnOsLoginManager::SkipStartupForTesting())),
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
    mock_tracker_ = std::make_unique<MockNetworkConnectionTracker>();
    WebAppControllerBrowserTest::SetUpOnMainThread();
    content::SetNetworkConnectionTrackerForTesting(
        /*network_connection_tracker=*/nullptr);
    content::SetNetworkConnectionTrackerForTesting(mock_tracker_.get());
  }

  void TearDownOnMainThread() override {
    content::SetNetworkConnectionTrackerForTesting(nullptr);
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
                  const std::string& run_on_os_login,
                  bool prevent_close = false) {
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
                                .Set(kRunOnOsLogin, run_on_os_login)
                                .Set(kPreventClose, prevent_close));
    prefs->SetList(prefs::kWebAppSettings, std::move(web_app_settings));
    EXPECT_TRUE(policy_refresh_sync_future.Wait());
  }

  void ClearWebAppSettings() {
    profile()->GetPrefs()->SetList(prefs::kWebAppSettings, base::Value::List());
  }

  Browser* FindAppBrowser(GURL app_url) {
    auto web_app = FindAppWithUrlInScope(app_url);
    if (!web_app) {
      return nullptr;
    }
    webapps::AppId app_id = web_app.value();

    return AppBrowserController::FindForWebApp(*profile(), app_id);
  }

  void RunOsLogin() {
    auto& rool_manager = provider().run_on_os_login_manager();
    rool_manager.SetCompletedClosureForTesting(completed_future_.GetCallback());
    rool_manager.Start();
  }

  void WaitForAllCommandsFinished() {
    // Wait until the top-level command is added and done.
    ASSERT_TRUE(completed_future_.Wait());
    // Wait for the triggered sub-commands to be done.
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }

  std::unique_ptr<MockNetworkConnectionTracker> mock_tracker_;
  std::unique_ptr<base::AutoReset<bool>> skip_run_on_os_login_startup_;
  base::AutoReset<bool> skip_preinstalled_web_app_startup_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
  base::test::TestFuture<void> completed_future_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedObservation<NotificationDisplayService,
                          WebAppRunOnOsLoginManagerBrowserTest>
      notification_observation_{this};
};

IN_PROC_BROWSER_TEST_F(
    WebAppRunOnOsLoginManagerBrowserTest,
    WebAppRunOnOsLoginWithInitialPolicyValueLaunchesBrowserWindow) {
  skip_run_on_os_login_startup_ = nullptr;
  EXPECT_CALL(*mock_tracker_, GetConnectionType(_, _))
      .WillRepeatedly(DoAll(
          SetArgPointee<0>(network::mojom::ConnectionType::CONNECTION_ETHERNET),
          Return(true)));

  AddForceInstalledApp(kTestApp, kTestAppName);
  AddRoolApp(kTestApp, kRunWindowed);

  // Wait for ROOL.
  RunOsLogin();
  WaitForAllCommandsFinished();

  // Should have 2 browsers: normal and app.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

  Browser* app_browser = FindAppBrowser(GURL(kTestApp));
  ASSERT_TRUE(app_browser);
}

IN_PROC_BROWSER_TEST_F(
    WebAppRunOnOsLoginManagerBrowserTest,
    WebAppRunOnOsLoginWithForceInstallLaunchesBrowserWindow) {
  skip_run_on_os_login_startup_ = nullptr;
  EXPECT_CALL(*mock_tracker_, GetConnectionType(_, _))
      .WillRepeatedly(DoAll(
          SetArgPointee<0>(network::mojom::ConnectionType::CONNECTION_ETHERNET),
          Return(true)));

  AddForceInstalledApp(kTestApp, kTestAppName);
  AddRoolApp(kTestApp, kRunWindowed);

  // Wait for ROOL.
  RunOsLogin();
  WaitForAllCommandsFinished();

  // Should have 2 browsers: normal and app.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser = FindAppBrowser(GURL(kTestApp));

  ASSERT_TRUE(app_browser);
}

IN_PROC_BROWSER_TEST_F(WebAppRunOnOsLoginManagerBrowserTest,
                       WebAppRunOnOsLoginNetworkNotConnectedCallSynchronous) {
  skip_run_on_os_login_startup_ = nullptr;
  EXPECT_CALL(*mock_tracker_, GetConnectionType(_, _))
      .WillRepeatedly(DoAll(
          SetArgPointee<0>(network::mojom::ConnectionType::CONNECTION_NONE),
          Return(true)));

  AddForceInstalledApp(kTestApp, kTestAppName);
  AddRoolApp(kTestApp, kRunWindowed);

  // Wait for ROOL.
  RunOsLogin();
  base::RunLoop().RunUntilIdle();

  // Should have only the normal browser as there is no network.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));

  // Simulate the network coming back.
  mock_tracker_->OnNetworkChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  WaitForAllCommandsFinished();

  // Should have 2 browsers: normal and app.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser = FindAppBrowser(GURL(kTestApp));

  ASSERT_TRUE(app_browser);
}

IN_PROC_BROWSER_TEST_F(
    WebAppRunOnOsLoginManagerBrowserTest,
    WebAppRunOnOsLoginNetworkNotConnectedCallAsynchronousInitiallyConnected) {
  skip_run_on_os_login_startup_ = nullptr;
  base::OnceCallback<void(network::mojom::ConnectionType)>
      connection_changed_callback;
  EXPECT_CALL(*mock_tracker_, GetConnectionType(_, _))
      .WillRepeatedly(
          DoAll([&connection_changed_callback](
                    network::mojom::ConnectionType*,
                    base::OnceCallback<void(network::mojom::ConnectionType)>
                        callback) {
            connection_changed_callback = std::move(callback);
            return false;
          }));

  AddForceInstalledApp(kTestApp, kTestAppName);
  AddRoolApp(kTestApp, kRunWindowed);

  // Wait for ROOL.
  RunOsLogin();
  base::RunLoop().RunUntilIdle();

  // Should have only the normal browser as there is no network.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));

  // Asynchronously notify that there is a network connection
  std::move(connection_changed_callback)
      .Run(network::mojom::ConnectionType::CONNECTION_WIFI);
  WaitForAllCommandsFinished();

  // Should have 2 browsers: normal and app.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser = FindAppBrowser(GURL(kTestApp));

  ASSERT_TRUE(app_browser);
}

IN_PROC_BROWSER_TEST_F(
    WebAppRunOnOsLoginManagerBrowserTest,
    WebAppRunOnOsLoginNetworkNotConnectedCallAsynchronousInitiallyDisconnected) {
  skip_run_on_os_login_startup_ = nullptr;
  base::OnceCallback<void(network::mojom::ConnectionType)>
      connection_changed_callback;
  EXPECT_CALL(*mock_tracker_, GetConnectionType(_, _))
      .WillRepeatedly(
          DoAll([&connection_changed_callback](
                    network::mojom::ConnectionType*,
                    base::OnceCallback<void(network::mojom::ConnectionType)>
                        callback) {
            connection_changed_callback = std::move(callback);
            return false;
          }));

  AddForceInstalledApp(kTestApp, kTestAppName);
  AddRoolApp(kTestApp, kRunWindowed);

  // Wait for ROOL.
  RunOsLogin();
  base::RunLoop().RunUntilIdle();

  // Should have only the normal browser as there is no network.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));

  // Asynchronously notify that the device is connected.
  std::move(connection_changed_callback)
      .Run(network::mojom::ConnectionType::CONNECTION_NONE);
  base::RunLoop().RunUntilIdle();

  // Should have only the normal browser as there is no network.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));

  // Simulate the network coming back.
  mock_tracker_->OnNetworkChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  WaitForAllCommandsFinished();

  // Should have 2 browsers: normal and app.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser = FindAppBrowser(GURL(kTestApp));

  ASSERT_TRUE(app_browser);
}

struct WebAppRunOnOsLoginNotificationTestParameters {
  size_t number_of_rool_apps;
  size_t number_of_prevent_close_apps;

  std::u16string expected_notification_title;
  std::u16string expected_notification_message;
};

class WebAppRunOnOsLoginNotificationBrowserTest
    : public WebAppRunOnOsLoginManagerBrowserTest,
      public testing::WithParamInterface<
          WebAppRunOnOsLoginNotificationTestParameters> {
 public:
  WebAppRunOnOsLoginNotificationBrowserTest() = default;
  ~WebAppRunOnOsLoginNotificationBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_P(WebAppRunOnOsLoginNotificationBrowserTest,
                       WebAppRunOnOsLoginNotificationOpensManagementUI) {
  skip_run_on_os_login_startup_ = nullptr;
  EXPECT_CALL(*mock_tracker_, GetConnectionType(_, _))
      .WillRepeatedly(DoAll(
          SetArgPointee<0>(network::mojom::ConnectionType::CONNECTION_ETHERNET),
          Return(true)));

  const auto test_params = GetParam();
  for (size_t i = 0; i < test_params.number_of_rool_apps; i++) {
    const auto app_id = base::StrCat({kTestApp, base::ToString(i)});
    AddForceInstalledApp(app_id, kTestAppName);
    AddRoolApp(app_id, kRunWindowed,
               /*prevent_close=*/i < test_params.number_of_prevent_close_apps);
  }
  const absl::Cleanup policy_cleanup = [this]() { ClearWebAppSettings(); };

  // Wait for ROOL.
  RunOsLogin();
  WaitForAllCommandsFinished();

  // Should have `number_of_rool_apps` + 1 browsers: normal and
  // `number_of_rool_apps` apps.
  ASSERT_EQ(test_params.number_of_rool_apps + 1,
            chrome::GetBrowserCount(browser()->profile()));

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
                           Eq("run_on_os_login_notifier")))));

  // Clicking on notification should open "chrome://management" on default
  // browser.
  notification_tester_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                      kRunOnOsLoginNotificationId, std::nullopt,
                                      std::nullopt);
  ui_test_utils::WaitForBrowserSetLastActive(browser());

  content::WebContents* active_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(GURL(chrome::kChromeUIManagementURL), active_contents->GetURL());
}

IN_PROC_BROWSER_TEST_P(WebAppRunOnOsLoginNotificationBrowserTest,
                       WebAppRunOnOsLoginNotification) {
  skip_run_on_os_login_startup_ = nullptr;
  EXPECT_CALL(*mock_tracker_, GetConnectionType(_, _))
      .WillRepeatedly(DoAll(
          SetArgPointee<0>(network::mojom::ConnectionType::CONNECTION_ETHERNET),
          Return(true)));

  const auto test_params = GetParam();
  for (size_t i = 0; i < test_params.number_of_rool_apps; i++) {
    const auto app_id = base::StrCat({kTestApp, base::ToString(i)});
    AddForceInstalledApp(app_id, kTestAppName);
    AddRoolApp(app_id, kRunWindowed,
               /*prevent_close=*/i < test_params.number_of_prevent_close_apps);
  }
  const absl::Cleanup policy_cleanup = [this]() { ClearWebAppSettings(); };

  // Wait for ROOL.
  RunOsLogin();
  WaitForAllCommandsFinished();

  bool notification_shown = base::test::RunUntil([&]() {
    return notification_tester_->GetNotification(kRunOnOsLoginNotificationId)
        .has_value();
  });
  // Should have notification
  ASSERT_TRUE(notification_shown);

  // Should have `number_of_rool_apps` + 1 browsers: normal and
  // `number_of_rool_apps` apps.
  ASSERT_EQ(GetParam().number_of_rool_apps + 1,
            chrome::GetBrowserCount(browser()->profile()));

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
                     Eq(test_params.expected_notification_title)),
            Property(&message_center::Notification::message,
                     Eq(test_params.expected_notification_message))));
}

INSTANTIATE_TEST_SUITE_P(
    WebAppRunOnOsLoginNotificationBrowserTest,
    WebAppRunOnOsLoginNotificationBrowserTest,
    testing::Values(
        WebAppRunOnOsLoginNotificationTestParameters{
            .number_of_rool_apps = 1u,
            .number_of_prevent_close_apps = 0u,
            .expected_notification_title =
                u"\"WebApp\" was started automatically",
            .expected_notification_message =
                u"Your administrator has set up \"WebApp\" to start "
                u"automatically every time you log in."},
        WebAppRunOnOsLoginNotificationTestParameters{
            .number_of_rool_apps = 1u,
            .number_of_prevent_close_apps = 1u,
            .expected_notification_title =
                u"\"WebApp\" was started automatically",
            .expected_notification_message =
                u"Your administrator has set up \"WebApp\" to start "
                u"automatically. This app may not be closed."},
        WebAppRunOnOsLoginNotificationTestParameters{
            .number_of_rool_apps = 2u,
            .number_of_prevent_close_apps = 0u,
            .expected_notification_title = u"2 apps were started automatically",
            .expected_notification_message =
                u"Your administrator has set up \"WebApp\" and 1 other app to "
                u"start automatically every time you log in."},
        WebAppRunOnOsLoginNotificationTestParameters{
            .number_of_rool_apps = 2u,
            .number_of_prevent_close_apps = 1u,
            .expected_notification_title = u"2 apps were started automatically",
            .expected_notification_message =
                u"Your administrator has set up some apps to start "
                u"automatically. Some of these apps may not be closed."},
        WebAppRunOnOsLoginNotificationTestParameters{
            .number_of_rool_apps = 2u,
            .number_of_prevent_close_apps = 2u,
            .expected_notification_title = u"2 apps were started automatically",
            .expected_notification_message =
                u"Your administrator has set up some apps to start "
                u"automatically. Some of these apps may not be closed."},
        WebAppRunOnOsLoginNotificationTestParameters{
            .number_of_rool_apps = 3u,
            .number_of_prevent_close_apps = 0u,
            .expected_notification_title = u"3 apps were started automatically",
            .expected_notification_message =
                u"Your administrator has set up \"WebApp\" and 2 other apps to "
                u"start automatically every time you log in."},
        WebAppRunOnOsLoginNotificationTestParameters{
            .number_of_rool_apps = 3u,
            .number_of_prevent_close_apps = 1u,
            .expected_notification_title = u"3 apps were started automatically",
            .expected_notification_message =
                u"Your administrator has set up some apps to start "
                u"automatically. Some of these apps may not be closed."},
        WebAppRunOnOsLoginNotificationTestParameters{
            .number_of_rool_apps = 3u,
            .number_of_prevent_close_apps = 2u,
            .expected_notification_title = u"3 apps were started automatically",
            .expected_notification_message =
                u"Your administrator has set up some apps to start "
                u"automatically. Some of these apps may not be closed."},
        WebAppRunOnOsLoginNotificationTestParameters{
            .number_of_rool_apps = 3u,
            .number_of_prevent_close_apps = 3u,
            .expected_notification_title = u"3 apps were started automatically",
            .expected_notification_message =
                u"Your administrator has set up some apps to start "
                u"automatically. Some of these apps may not be closed."}));

}  // namespace

}  // namespace web_app
