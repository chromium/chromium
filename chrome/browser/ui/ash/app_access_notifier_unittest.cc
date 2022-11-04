// Copyright 2021 The Chromium Authors
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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/capability_access.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/message_center/message_center.h"

namespace {

constexpr char kPrivacyIndicatorsAppTypeHistogramName[] =
    "Ash.PrivacyIndicators.AppAccessUpdate.Type";
constexpr char kPrivacyIndicatorsLaunchSettingsHistogramName[] =
    "Ash.PrivacyIndicators.LaunchSettings";

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
  AppAccessNotifierBaseTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  AppAccessNotifierBaseTest(const AppAccessNotifierBaseTest&) = delete;
  AppAccessNotifierBaseTest& operator=(const AppAccessNotifierBaseTest&) =
      delete;
  ~AppAccessNotifierBaseTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    // Setting ash prefs for testing multi-display.
    ash::RegisterLocalStatePrefs(local_state_.registry(), /*for_test=*/true);

    ash::AshTestHelper::InitParams params;
    params.local_state = &local_state_;
    ash_test_helper_.SetUp(std::move(params));

    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    app_access_notifier_ = std::make_unique<TestAppAccessNotifier>();

    SetupPrimaryUser();
  }

  void TearDown() override {
    app_access_notifier_.reset();
    ash_test_helper_.TearDown();
  }

  void SetupPrimaryUser() {
    auto* primary_profile = testing_profile_manager_.CreateTestingProfile(
        account_id_primary_user_.GetUserEmail());
    fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_id_primary_user_, false, user_manager::USER_TYPE_REGULAR,
        primary_profile);

    registry_cache_primary_user_.SetAccountId(account_id_primary_user_);
    apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(
        account_id_primary_user_, &registry_cache_primary_user_);
    capability_access_cache_primary_user_.SetAccountId(
        account_id_primary_user_);
    apps::AppCapabilityAccessCacheWrapper::Get().AddAppCapabilityAccessCache(
        account_id_primary_user_, &capability_access_cache_primary_user_);

    SetActiveUserAccountId(/*is_primary=*/true);
  }

  void SetupSecondaryUser() {
    auto* secondary_profile = testing_profile_manager_.CreateTestingProfile(
        account_id_secondary_user_.GetUserEmail());
    fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_id_secondary_user_, false, user_manager::USER_TYPE_REGULAR,
        secondary_profile);

    registry_cache_secondary_user_.SetAccountId(account_id_secondary_user_);
    apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(
        account_id_secondary_user_, &registry_cache_secondary_user_);
    capability_access_cache_secondary_user_.SetAccountId(
        account_id_secondary_user_);
    apps::AppCapabilityAccessCacheWrapper::Get().AddAppCapabilityAccessCache(
        account_id_secondary_user_, &capability_access_cache_secondary_user_);

    SetActiveUserAccountId(/*is_primary=*/false);
  }

  absl::optional<std::u16string> GetAppAccessingMicrophone() {
    return app_access_notifier_->GetAppAccessingMicrophone();
  }

  static apps::AppPtr MakeApp(const std::string app_id,
                              const char* name,
                              apps::AppType app_type) {
    apps::AppPtr app = std::make_unique<apps::App>(app_type, app_id);
    app->name = name;
    app->short_name = name;
    return app;
  }

  static apps::CapabilityAccessPtr MakeCapabilityAccess(
      const std::string app_id,
      absl::optional<bool> camera,
      absl::optional<bool> microphone) {
    auto access = std::make_unique<apps::CapabilityAccess>(app_id);
    access->camera = camera;
    access->microphone = microphone;
    return access;
  }

  void LaunchAppUsingCameraOrMicrophone(
      const std::string id,
      const char* name,
      bool use_camera,
      bool use_microphone,
      apps::AppType app_type = apps::AppType::kChromeApp) {
    apps::AppRegistryCache* reg_cache =
        app_access_notifier_->GetActiveUserAppRegistryCache();
    apps::AppCapabilityAccessCache* cap_cache =
        app_access_notifier_->GetActiveUserAppCapabilityAccessCache();

    std::vector<apps::AppPtr> registry_deltas;
    registry_deltas.push_back(MakeApp(id, name, app_type));
    reg_cache->OnApps(std::move(registry_deltas), apps::AppType::kUnknown,
                      /*should_notify_initialized=*/false);

    std::vector<apps::CapabilityAccessPtr> capability_access_deltas;
    capability_access_deltas.push_back(
        MakeCapabilityAccess(id, use_camera, use_microphone));
    cap_cache->OnCapabilityAccesses(std::move(capability_access_deltas));
  }

  // Set the active account, whether to use the primary or secondary fake user
  // account.
  void SetActiveUserAccountId(bool is_primary) {
    auto id =
        is_primary ? account_id_primary_user_ : account_id_secondary_user_;
    app_access_notifier_->SetFakeActiveUserAccountId(id);

    fake_user_manager_->LoginUser(id);
    fake_user_manager_->SwitchActiveUser(id);
  }

  const AccountId account_id_primary_user_ =
      AccountId::FromUserEmail("primary_profile");
  const AccountId account_id_secondary_user_ =
      AccountId::FromUserEmail("secondary_profile");

  std::unique_ptr<TestAppAccessNotifier> app_access_notifier_;

  apps::AppRegistryCache registry_cache_primary_user_;
  apps::AppCapabilityAccessCache capability_access_cache_primary_user_;
  apps::AppRegistryCache registry_cache_secondary_user_;
  apps::AppCapabilityAccessCache capability_access_cache_secondary_user_;

  ash::FakeChromeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  // This instance is needed for setting up `ash_test_helper_`.
  // See //docs/threading_and_tasks_testing.md.
  content::BrowserTaskEnvironment task_environment_;

  TestingProfileManager testing_profile_manager_;

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
    if (IsPrivacyIndicatorsFeatureEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {apps::kAppServiceCapabilityAccessWithoutMojom,
           ash::features::kPrivacyIndicators},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {apps::kAppServiceCapabilityAccessWithoutMojom},
          {ash::features::kPrivacyIndicators});
    }
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
    scoped_feature_list_.InitWithFeatures(
        {apps::kAppServiceCapabilityAccessWithoutMojom,
         ash::features::kPrivacyIndicators},
        {});
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
  SetActiveUserAccountId(/*is_primary=*/true);

  // Primary user launches a mic-using app.
  LaunchAppUsingCameraOrMicrophone("id_primary_user", "name_primary_user",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/true);

  // App we just launched should show up in the notification.
  absl::optional<std::u16string> app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_primary_user");

  // Secondary user is now the primary user.
  SetActiveUserAccountId(/*is_primary=*/false);

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
  SetActiveUserAccountId(/*is_primary=*/true);
  LaunchAppUsingCameraOrMicrophone("id_primary_user", "name_primary_user",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/false);
  app_name = GetAppAccessingMicrophone();
  EXPECT_FALSE(app_name.has_value());

  // Now switch back to the secondary user, verify that the same app as before
  // shows up in the notification.
  SetActiveUserAccountId(/*is_primary=*/false);
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
  SetActiveUserAccountId(/*is_primary=*/true);

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
  SetActiveUserAccountId(/*is_primary=*/false);

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
  SetActiveUserAccountId(/*is_primary=*/true);

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
  EXPECT_EQ(AppAccessNotifier::GetAppShortNameFromAppId(id), u"test_app_name");
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

TEST_F(AppAccessNotifierPrivacyIndicatorTest, RecordAppType) {
  base::HistogramTester histograms;
  LaunchAppUsingCameraOrMicrophone("test_app_id1", "test_app_name",
                                   /*use_camera=*/true,
                                   /*use_microphone=*/false,
                                   /*app_type=*/apps::AppType::kArc);
  histograms.ExpectBucketCount(kPrivacyIndicatorsAppTypeHistogramName,
                               apps::AppType::kArc, 1);

  LaunchAppUsingCameraOrMicrophone("test_app_id2", "test_app_name",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/true,
                                   /*app_type=*/apps::AppType::kChromeApp);
  histograms.ExpectBucketCount(kPrivacyIndicatorsAppTypeHistogramName,
                               apps::AppType::kChromeApp, 1);

  LaunchAppUsingCameraOrMicrophone("test_app_id3", "test_app_name",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/false,
                                   /*app_type=*/apps::AppType::kChromeApp);
  histograms.ExpectBucketCount(kPrivacyIndicatorsAppTypeHistogramName,
                               apps::AppType::kChromeApp, 2);

  LaunchAppUsingCameraOrMicrophone("test_app_id4", "test_app_name",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/true,
                                   /*app_type=*/apps::AppType::kSystemWeb);
  histograms.ExpectBucketCount(kPrivacyIndicatorsAppTypeHistogramName,
                               apps::AppType::kSystemWeb, 1);
}

TEST_F(AppAccessNotifierPrivacyIndicatorTest, RecordLaunchSettings) {
  // Make sure histograms with app type is being recorded after launching
  // settings.
  base::HistogramTester histograms;
  LaunchAppUsingCameraOrMicrophone("test_app_id1", "test_app_name",
                                   /*use_camera=*/true,
                                   /*use_microphone=*/false,
                                   /*app_type=*/apps::AppType::kArc);
  AppAccessNotifier::LaunchAppSettings("test_app_id1");
  histograms.ExpectBucketCount(kPrivacyIndicatorsLaunchSettingsHistogramName,
                               apps::AppType::kArc, 1);

  LaunchAppUsingCameraOrMicrophone("test_app_id2", "test_app_name",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/true,
                                   /*app_type=*/apps::AppType::kChromeApp);
  AppAccessNotifier::LaunchAppSettings("test_app_id2");
  histograms.ExpectBucketCount(kPrivacyIndicatorsLaunchSettingsHistogramName,
                               apps::AppType::kChromeApp, 1);
}
