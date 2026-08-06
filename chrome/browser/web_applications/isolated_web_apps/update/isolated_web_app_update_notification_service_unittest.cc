// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_notification_service.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/components/kiosk/kiosk_test_utils.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace web_app {

// This test is ChromeOS-only (configured in BUILD.gn) because update pending
// notifications rely on the Ash feature flag kIsolatedWebAppInlineUpdate.
class IsolatedWebAppUpdateNotificationServiceTest : public IsolatedWebAppTest {
 public:
  void SetUp() override {
    IsolatedWebAppTest::SetUp();
    tester_ = std::make_unique<NotificationDisplayServiceTester>(
        /*profile=*/profile());
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  FakeWebAppUiManager& ui_manager() {
    return static_cast<FakeWebAppUiManager&>(provider().ui_manager());
  }

  IsolatedWebAppUpdateManager& update_manager() {
    return provider().isolated_web_app_update_manager();
  }

  IsolatedWebAppUrlInfo InstallIwaWithPendingUpdate(
      const std::string& app_name,
      const std::string& current_version,
      const std::string& pending_version) {
    std::unique_ptr<ScopedBundledIsolatedWebApp> app =
        IsolatedWebAppBuilder(
            ManifestBuilder().SetName(app_name).SetVersion(current_version))
            .BuildBundle(test::GetDefaultEd25519KeyPair());
    app->FakeInstallPageState(profile());
    IsolatedWebAppUrlInfo url_info = app->InstallChecked(profile());

    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* web_app = update->UpdateApp(url_info.app_id());
    CHECK(web_app);
    web_app->SetIsolationData(
        IsolationData::Builder(*web_app->isolation_data())
            .SetPendingUpdateInfo(IsolationData::PendingUpdateInfo(
                IwaStorageOwnedBundle{"iwa_update_dir", /*dev_mode=*/false},
                *IwaVersion::Create(pending_version)))
            .Build());

    return url_info;
  }

 protected:
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(IsolatedWebAppUpdateNotificationServiceTest,
       ShowsNotificationWhenFeatureEnabled) {
  feature_list_.InitAndEnableFeature(
      ash::features::kIsolatedWebAppInlineUpdate);

  IsolatedWebAppUrlInfo url_info =
      InstallIwaWithPendingUpdate("IWA App", "1.0.0", "1.2.0");

  update_manager().update_notification_service()->ShowUpdatePendingNotification(
      url_info.app_id());

  auto notifications = tester_->GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(notifications.size(), 1u);

  const auto& notification = notifications[0];
  EXPECT_EQ(notification.id(), "iwa_pending_update_" + url_info.app_id());
  EXPECT_EQ(notification.title(), u"Update available for IWA App");
  EXPECT_EQ(notification.message(),
            u"A new version (1.2.0) is ready. Restart the app to apply the "
            u"update. Unsaved data may be lost.");
  ASSERT_EQ(notification.buttons().size(), 1u);
  EXPECT_EQ(notification.buttons()[0].title, u"Restart app");
}

TEST_F(IsolatedWebAppUpdateNotificationServiceTest, ClickRestartAppButton) {
  feature_list_.InitAndEnableFeature(
      ash::features::kIsolatedWebAppInlineUpdate);

  IsolatedWebAppUrlInfo url_info =
      InstallIwaWithPendingUpdate("IWA App", "1.0.0", "1.2.0");

  ui_manager().SetNumWindowsForApp(url_info.app_id(), 1);

  update_manager().update_notification_service()->ShowUpdatePendingNotification(
      url_info.app_id());

  auto notifications = tester_->GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(notifications.size(), 1u);

  tester_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                         "iwa_pending_update_" + url_info.app_id(),
                         /*action_index=*/0, /*reply=*/std::nullopt);

  EXPECT_EQ(ui_manager().GetNumWindowsForApp(url_info.app_id()), 0u);
  EXPECT_TRUE(tester_
                  ->GetDisplayedNotificationsForType(
                      NotificationHandler::Type::TRANSIENT)
                  .empty());
}

TEST_F(IsolatedWebAppUpdateNotificationServiceTest, CloseNotification) {
  feature_list_.InitAndEnableFeature(
      ash::features::kIsolatedWebAppInlineUpdate);

  IsolatedWebAppUrlInfo url_info =
      InstallIwaWithPendingUpdate("IWA App", "1.0.0", "1.2.0");

  update_manager().update_notification_service()->ShowUpdatePendingNotification(
      url_info.app_id());
  EXPECT_EQ(tester_
                ->GetDisplayedNotificationsForType(
                    NotificationHandler::Type::TRANSIENT)
                .size(),
            1u);

  update_manager().update_notification_service()->CloseNotification(
      url_info.app_id());
  EXPECT_TRUE(tester_
                  ->GetDisplayedNotificationsForType(
                      NotificationHandler::Type::TRANSIENT)
                  .empty());
}

TEST_F(IsolatedWebAppUpdateNotificationServiceTest, OnWebAppUninstalled) {
  feature_list_.InitAndEnableFeature(
      ash::features::kIsolatedWebAppInlineUpdate);

  IsolatedWebAppUrlInfo url_info =
      InstallIwaWithPendingUpdate("IWA App", "1.0.0", "1.2.0");

  update_manager().update_notification_service()->ShowUpdatePendingNotification(
      url_info.app_id());
  EXPECT_EQ(tester_
                ->GetDisplayedNotificationsForType(
                    NotificationHandler::Type::TRANSIENT)
                .size(),
            1u);

  test::UninstallWebApp(profile(), url_info.app_id());
  EXPECT_TRUE(tester_
                  ->GetDisplayedNotificationsForType(
                      NotificationHandler::Type::TRANSIENT)
                  .empty());
}

TEST_F(IsolatedWebAppUpdateNotificationServiceTest,
       NoNotificationWhenFeatureDisabled) {
  feature_list_.InitAndDisableFeature(
      ash::features::kIsolatedWebAppInlineUpdate);

  IsolatedWebAppUrlInfo url_info =
      InstallIwaWithPendingUpdate("IWA App", "1.0.0", "1.2.0");

  update_manager().update_notification_service()->ShowUpdatePendingNotification(
      url_info.app_id());

  EXPECT_TRUE(tester_
                  ->GetDisplayedNotificationsForType(
                      NotificationHandler::Type::TRANSIENT)
                  .empty());
}

TEST_F(IsolatedWebAppUpdateNotificationServiceTest,
       NoNotificationInKioskSession) {
  feature_list_.InitAndEnableFeature(
      ash::features::kIsolatedWebAppInlineUpdate);

  user_manager::ScopedUserManager user_manager(
      std::make_unique<user_manager::FakeUserManager>(
          TestingBrowserProcess::GetGlobal()->local_state()));
  chromeos::SetUpFakeIwaKioskSession();

  IsolatedWebAppUrlInfo url_info =
      InstallIwaWithPendingUpdate("IWA App", "1.0.0", "1.2.0");

  update_manager().update_notification_service()->ShowUpdatePendingNotification(
      url_info.app_id());

  EXPECT_TRUE(tester_
                  ->GetDisplayedNotificationsForType(
                      NotificationHandler::Type::TRANSIENT)
                  .empty());
}

TEST_F(IsolatedWebAppUpdateNotificationServiceTest,
       RestartsOnlyMainWindowWhenMultipleWindowsOpen) {
  feature_list_.InitAndEnableFeature(
      ash::features::kIsolatedWebAppInlineUpdate);

  IsolatedWebAppUrlInfo url_info =
      InstallIwaWithPendingUpdate("IWA App", "1.0.0", "1.2.0");
  webapps::AppId app_id = url_info.app_id();

  ui_manager().SetNumWindowsForApp(app_id, 3);

  update_manager().update_notification_service()->ShowUpdatePendingNotification(
      app_id);

  auto notifications = tester_->GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(notifications.size(), 1u);

  base::test::TestFuture<apps::AppLaunchParams, LaunchWebAppWindowSetting>
      launch_future;
  ui_manager().SetOnLaunchWebAppCallback(launch_future.GetRepeatingCallback());

  tester_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                         "iwa_pending_update_" + app_id,
                         /*action_index=*/0, /*reply=*/std::nullopt);

  EXPECT_EQ(ui_manager().GetNumWindowsForApp(app_id), 0u);
  EXPECT_FALSE(launch_future.IsReady());

  static_cast<IsolatedWebAppUpdateManager::Observer*>(
      update_manager().update_notification_service())
      ->OnUpdateApplyTaskCompleted(app_id, base::ok());

  auto [params, launch_setting] = launch_future.Take();
  EXPECT_EQ(params.app_id, app_id);
  EXPECT_EQ(params.launch_source, apps::LaunchSource::kFromChromeInternal);
  EXPECT_EQ(params.container, apps::LaunchContainer::kLaunchContainerNone);
}

}  // namespace web_app
