// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_access_notifier.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/privacy/privacy_indicators_tray_item_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_helper.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/message_center/message_center.h"

namespace {

// Check the visibility of privacy indicators in all displays.
void ExpectPrivacyIndicatorsVisible(bool visible) {
  for (ash::RootWindowController* root_window_controller :
       ash::Shell::Get()->GetAllRootWindowControllers()) {
    EXPECT_EQ(root_window_controller->GetStatusAreaWidget()
                  ->unified_system_tray()
                  ->privacy_indicators_view()
                  ->GetVisible(),
              visible);
  }
}

}  // namespace

const char kPrivacyIndicatorsNotificationIdPrefix[] = "privacy-indicators";

class TestAppAccessNotifier : public AppAccessNotifier {
 public:
  TestAppAccessNotifier() = default;
  TestAppAccessNotifier(const TestAppAccessNotifier&) = delete;
  TestAppAccessNotifier& operator=(const TestAppAccessNotifier&) = delete;
  ~TestAppAccessNotifier() override = default;

  void SetFakeActiveUserAccountId(AccountId id) {
    user_account_id_ = id;
    CheckActiveUserChanged();
  }

  AccountId GetActiveUserAccountId() override { return user_account_id_; }

 private:
  AccountId user_account_id_ = EmptyAccountId();
};

class AppAccessNotifierBaseTest : public testing::Test {
 public:
  AppAccessNotifierBaseTest() = default;
  AppAccessNotifierBaseTest(const AppAccessNotifierBaseTest&) = delete;
  AppAccessNotifierBaseTest& operator=(const AppAccessNotifierBaseTest&) =
      delete;
  ~AppAccessNotifierBaseTest() override = default;

  void SetUp() override {
    // Setting ash prefs for testing multi-display.
    ash::RegisterLocalStatePrefs(local_state_.registry(), /*for_test=*/true);

    ash::AshTestHelper::InitParams params;
    params.local_state = &local_state_;
    ash_test_helper_.SetUp(std::move(params));

    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    microphone_mute_notification_delegate_ =
        std::make_unique<TestAppAccessNotifier>();

    SetupPrimaryUser();
  }

  void TearDown() override {
    microphone_mute_notification_delegate_.reset();
    ash_test_helper_.TearDown();
  }

  void SetupPrimaryUser() {
    registry_cache_primary_user_.SetAccountId(account_id_primary_user_);
    apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(
        account_id_primary_user_, &registry_cache_primary_user_);
    capability_access_cache_primary_user_.SetAccountId(
        account_id_primary_user_);
    apps::AppCapabilityAccessCacheWrapper::Get().AddAppCapabilityAccessCache(
        account_id_primary_user_, &capability_access_cache_primary_user_);
    microphone_mute_notification_delegate_->SetFakeActiveUserAccountId(
        account_id_primary_user_);
  }

  void SetupSecondaryUser() {
    registry_cache_secondary_user_.SetAccountId(account_id_secondary_user_);
    apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(
        account_id_secondary_user_, &registry_cache_secondary_user_);
    capability_access_cache_secondary_user_.SetAccountId(
        account_id_secondary_user_);
    apps::AppCapabilityAccessCacheWrapper::Get().AddAppCapabilityAccessCache(
        account_id_secondary_user_, &capability_access_cache_secondary_user_);
    microphone_mute_notification_delegate_->SetFakeActiveUserAccountId(
        account_id_secondary_user_);
  }

  absl::optional<std::u16string> GetAppAccessingMicrophone() {
    apps::AppCapabilityAccessCache* cap_cache =
        (microphone_mute_notification_delegate_->GetActiveUserAccountId() ==
         account_id_primary_user_)
            ? &capability_access_cache_primary_user_
            : &capability_access_cache_secondary_user_;
    apps::AppRegistryCache* reg_cache =
        (microphone_mute_notification_delegate_->GetActiveUserAccountId() ==
         account_id_primary_user_)
            ? &registry_cache_primary_user_
            : &registry_cache_secondary_user_;
    return microphone_mute_notification_delegate_
        ->GetMostRecentAppAccessingMicrophone(cap_cache, reg_cache);
  }

  static apps::AppPtr MakeApp(const std::string app_id, const char* name) {
    apps::AppPtr app =
        std::make_unique<apps::App>(apps::AppType::kChromeApp, app_id);
    app->name = name;
    app->short_name = name;
    return app;
  }

  static apps::mojom::CapabilityAccessPtr MakeCapabilityAccess(
      const std::string app_id,
      apps::mojom::OptionalBool camera,
      apps::mojom::OptionalBool microphone) {
    apps::mojom::CapabilityAccessPtr access =
        apps::mojom::CapabilityAccess::New();
    access->app_id = app_id;
    access->camera = camera;
    access->microphone = microphone;
    return access;
  }

  void LaunchAppUsingCameraOrMicrophone(const std::string id,
                                        const char* name,
                                        bool use_camera,
                                        bool use_microphone) {
    bool is_primary_user =
        (microphone_mute_notification_delegate_->GetActiveUserAccountId() ==
         account_id_primary_user_);
    apps::AppRegistryCache* reg_cache = is_primary_user
                                            ? &registry_cache_primary_user_
                                            : &registry_cache_secondary_user_;
    apps::AppCapabilityAccessCache* cap_cache =
        is_primary_user ? &capability_access_cache_primary_user_
                        : &capability_access_cache_secondary_user_;

    std::vector<apps::AppPtr> registry_deltas;
    registry_deltas.push_back(MakeApp(id, name));
    reg_cache->OnApps(std::move(registry_deltas), apps::AppType::kUnknown,
                      /*should_notify_initialized=*/false);

    std::vector<apps::mojom::CapabilityAccessPtr> capability_access_deltas;
    capability_access_deltas.push_back(MakeCapabilityAccess(
        id,
        use_camera ? apps::mojom::OptionalBool::kTrue
                   : apps::mojom::OptionalBool::kFalse,
        use_microphone ? apps::mojom::OptionalBool::kTrue
                       : apps::mojom::OptionalBool::kFalse));
    cap_cache->OnCapabilityAccesses(std::move(capability_access_deltas));
  }

  void SetActiveUserAccountId(AccountId id) {
    microphone_mute_notification_delegate_->SetFakeActiveUserAccountId(id);
  }

  const std::string kPrimaryProfileName = "primary_profile";
  const AccountId account_id_primary_user_ =
      AccountId::FromUserEmail(kPrimaryProfileName);
  const std::string kSecondaryProfileName = "secondary_profile";
  const AccountId account_id_secondary_user_ =
      AccountId::FromUserEmail(kSecondaryProfileName);

  std::unique_ptr<TestAppAccessNotifier> microphone_mute_notification_delegate_;

  apps::AppRegistryCache registry_cache_primary_user_;
  apps::AppCapabilityAccessCache capability_access_cache_primary_user_;
  apps::AppRegistryCache registry_cache_secondary_user_;
  apps::AppCapabilityAccessCache capability_access_cache_secondary_user_;

  user_manager::FakeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  // This instance is needed for setting up `ash_test_helper_`.
  // See //docs/threading_and_tasks_testing.md.
  content::BrowserTaskEnvironment task_environment_;

  // Use this for testing multi-display.
  TestingPrefServiceSimple local_state_;

  ash::AshTestHelper ash_test_helper_;
};

class AppAccessNotifierParameterizedTest
    : public AppAccessNotifierBaseTest,
      public testing::WithParamInterface<bool> {
 public:
  AppAccessNotifierParameterizedTest() = default;
  AppAccessNotifierParameterizedTest(
      const AppAccessNotifierParameterizedTest&) = delete;
  AppAccessNotifierParameterizedTest& operator=(
      const AppAccessNotifierParameterizedTest&) = delete;
  ~AppAccessNotifierParameterizedTest() override = default;

  // AppAccessNotifierBaseTest:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        ash::features::kPrivacyIndicators, IsPrivacyIndicatorsFeatureEnabled());
    AppAccessNotifierBaseTest::SetUp();
  }

  bool IsPrivacyIndicatorsFeatureEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class AppAccessNotifierPrivacyIndicatorTest : public AppAccessNotifierBaseTest {
 public:
  AppAccessNotifierPrivacyIndicatorTest() = default;
  AppAccessNotifierPrivacyIndicatorTest(
      const AppAccessNotifierPrivacyIndicatorTest&) = delete;
  AppAccessNotifierPrivacyIndicatorTest& operator=(
      const AppAccessNotifierPrivacyIndicatorTest&) = delete;
  ~AppAccessNotifierPrivacyIndicatorTest() override = default;

  // AppAccessNotifierBaseTest:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(ash::features::kPrivacyIndicators,
                                              true);
    AppAccessNotifierBaseTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AppAccessNotifierParameterizedTest,
    /*IsPrivacyIndicatorsFeatureEnabled()=*/::testing::Bool());

TEST_P(AppAccessNotifierParameterizedTest, NoAppsLaunched) {
  // Should return a completely value-free app_name.
  absl::optional<std::u16string> app_name = GetAppAccessingMicrophone();
  EXPECT_FALSE(app_name.has_value());
}

TEST_P(AppAccessNotifierParameterizedTest, AppLaunchedNotUsingMicrophone) {
  LaunchAppUsingCameraOrMicrophone("id_rose", "name_rose", /*use_camera=*/false,
                                   /*use_microphone=*/false);

  // Should return a completely value-free app_name.
  absl::optional<std::u16string> app_name = GetAppAccessingMicrophone();
  EXPECT_FALSE(app_name.has_value());
}

TEST_P(AppAccessNotifierParameterizedTest, AppLaunchedUsingMicrophone) {
  LaunchAppUsingCameraOrMicrophone("id_rose", "name_rose", /*use_camera=*/false,
                                   /*use_microphone=*/true);

  // Should return the name of our app.
  absl::optional<std::u16string> app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_rose");
}

TEST_P(AppAccessNotifierParameterizedTest,
       MultipleAppsLaunchedUsingMicrophone) {
  LaunchAppUsingCameraOrMicrophone("id_rose", "name_rose", /*use_camera=*/false,
                                   /*use_microphone=*/true);
  LaunchAppUsingCameraOrMicrophone("id_mars", "name_mars", /*use_camera=*/false,
                                   /*use_microphone=*/true);
  LaunchAppUsingCameraOrMicrophone("id_zara", "name_zara", /*use_camera=*/false,
                                   /*use_microphone=*/true);
  LaunchAppUsingCameraOrMicrophone(
      "id_oscar", "name_oscar", /*use_camera=*/false, /*use_microphone=*/false);

  // Most recently launched mic-using app should be the one we use for the
  // notification.
  absl::optional<std::u16string> app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_zara");

  // Oscar starts using the mic, Oscar shows up in the notification.
  LaunchAppUsingCameraOrMicrophone(
      "id_oscar", "name_oscar", /*use_camera=*/false, /*use_microphone=*/true);
  app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_oscar");

  // If we "kill" Oscar (set to no longer be using the mic or camera),
  // the notification shows Zara again.
  LaunchAppUsingCameraOrMicrophone(
      "id_oscar", "name_oscar", /*use_camera=*/false, /*use_microphone=*/false);
  app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_zara");
}

TEST_P(AppAccessNotifierParameterizedTest, MultipleUsers) {
  // Prepare the secondary user.
  SetupSecondaryUser();

  // Primary user is the active user.
  SetActiveUserAccountId(account_id_primary_user_);

  // Primary user launches a mic-using app.
  LaunchAppUsingCameraOrMicrophone("id_primary_user", "name_primary_user",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/true);

  // App we just launched should show up in the notification.
  absl::optional<std::u16string> app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_primary_user");

  // Secondary user is now the primary user.
  SetActiveUserAccountId(account_id_secondary_user_);

  // Secondary user launches a mic-using app.
  LaunchAppUsingCameraOrMicrophone("id_secondary_user", "name_secondary_user",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/true);

  // App we just launched should show up in the notification.
  app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_secondary_user");

  // Switch back to the primary user and "kill" the app it was running, no app
  // name to show.
  SetActiveUserAccountId(account_id_primary_user_);
  LaunchAppUsingCameraOrMicrophone("id_primary_user", "name_primary_user",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/false);
  app_name = GetAppAccessingMicrophone();
  EXPECT_FALSE(app_name.has_value());

  // Now switch back to the secondary user, verify that the same app as before
  // shows up in the notification.
  SetActiveUserAccountId(account_id_secondary_user_);
  app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_secondary_user");

  // Now "kill" our secondary user's app and verify that there's no name to
  // show.
  LaunchAppUsingCameraOrMicrophone("id_secondary_user", "name_secondary_user",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/false);
  app_name = GetAppAccessingMicrophone();
  EXPECT_FALSE(app_name.has_value());
}

TEST_P(AppAccessNotifierParameterizedTest, MultipleUsersMultipleApps) {
  // Prepare the secondary user.
  SetupSecondaryUser();

  // Primary user is the active user.
  SetActiveUserAccountId(account_id_primary_user_);

  // Primary user launches a mic-using app.
  LaunchAppUsingCameraOrMicrophone("id_primary_user", "name_primary_user",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/true);

  // App we just launched should show up in the notification.
  absl::optional<std::u16string> app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_primary_user");

  // Primary user launches a second mic-using app.
  LaunchAppUsingCameraOrMicrophone(
      "id_primary_user", "name_primary_user_another_app", /*use_camera=*/false,
      /*use_microphone=*/true);

  // App we just launched should show up in the notification.
  app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_primary_user_another_app");

  // Secondary user is now the primary user.
  SetActiveUserAccountId(account_id_secondary_user_);

  // Secondary user launches a mic-using app.
  LaunchAppUsingCameraOrMicrophone("id_secondary_user", "name_secondary_user",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/true);

  // App we just launched should show up in the notification.
  app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_secondary_user");

  // Secondary user launches a second mic-using app.
  LaunchAppUsingCameraOrMicrophone(
      "id_secondary_user", "name_secondary_user_another_app",
      /*use_camera=*/false, /*use_microphone=*/true);

  // App we just launched should show up in the notification.
  app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_secondary_user_another_app");

  // Switch back to the primary user.
  SetActiveUserAccountId(account_id_primary_user_);

  // App we just launched should show up in the notification.
  app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_primary_user_another_app");
}

TEST_P(AppAccessNotifierParameterizedTest, GetShortNameFromAppId) {
  // Test that GetAppShortNameFromAppId works properly.
  const std::string id = "test_app_id";
  LaunchAppUsingCameraOrMicrophone(id, "test_app_name", /*use_camera=*/false,
                                   /*use_microphone=*/true);
  EXPECT_EQ(AppAccessNotifier::GetAppShortNameFromAppId(
                id, &registry_cache_primary_user_),
            u"test_app_name");
}

TEST_F(AppAccessNotifierPrivacyIndicatorTest, AppAccessNotification) {
  // Test that notifications get created/removed when an app is accessing camera
  // or microphone.
  const std::string id1 = "test_app_id_1";
  const std::string id2 = "test_app_id_2";

  LaunchAppUsingCameraOrMicrophone(id1, "test_app_name", /*use_camera=*/false,
                                   /*use_microphone=*/true);
  LaunchAppUsingCameraOrMicrophone(id2, "test_app_name", /*use_camera=*/true,
                                   /*use_microphone=*/false);
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      kPrivacyIndicatorsNotificationIdPrefix + id1));
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      kPrivacyIndicatorsNotificationIdPrefix + id2));

  LaunchAppUsingCameraOrMicrophone(id1, "test_app_name", /*use_camera=*/false,
                                   /*use_microphone=*/false);
  LaunchAppUsingCameraOrMicrophone(id2, "test_app_name", /*use_camera=*/false,
                                   /*use_microphone=*/false);
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      kPrivacyIndicatorsNotificationIdPrefix + id1));
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      kPrivacyIndicatorsNotificationIdPrefix + id2));

  LaunchAppUsingCameraOrMicrophone(id1, "test_app_name", /*use_camera=*/true,
                                   /*use_microphone=*/true);
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      kPrivacyIndicatorsNotificationIdPrefix + id1));
}

TEST_F(AppAccessNotifierPrivacyIndicatorTest, PrivacyIndicatorsVisibility) {
  // Make sure privacy indicators work on multiple displays.
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("800x800,801+0-800x800");

  ExpectPrivacyIndicatorsVisible(/*visible=*/false);

  // Privacy indicators should show up if at least camera or microphone is being
  // accessed.
  LaunchAppUsingCameraOrMicrophone("test_app_id", "test_app_name",
                                   /*use_camera=*/true,
                                   /*use_microphone=*/true);
  ExpectPrivacyIndicatorsVisible(/*visible=*/true);

  LaunchAppUsingCameraOrMicrophone("test_app_id", "test_app_name",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/false);
  ExpectPrivacyIndicatorsVisible(/*visible=*/false);

  LaunchAppUsingCameraOrMicrophone("test_app_id", "test_app_name",
                                   /*use_camera=*/true,
                                   /*use_microphone=*/false);
  ExpectPrivacyIndicatorsVisible(/*visible=*/true);

  LaunchAppUsingCameraOrMicrophone("test_app_id", "test_app_name",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/true);
  ExpectPrivacyIndicatorsVisible(/*visible=*/true);
}
