// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_access/app_access_notifier.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "ash/system/privacy/privacy_indicators_tray_item_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_helper.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/capability_access.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/message_center/message_center.h"

namespace {

constexpr char kPrivacyIndicatorsAppTypeHistogramName[] =
    "Ash.PrivacyIndicators.AppAccessUpdate.Type";
constexpr char kPrivacyIndicatorsLaunchSettingsHistogramName[] =
    "Ash.PrivacyIndicators.LaunchSettings";

// Check the visibility of privacy indicators and their camera/microphone icons
// in all displays.
ash::PrivacyIndicatorsTrayItemView* GetPrivacyIndicatorsView(
    ash::RootWindowController* root_window_controller) {
  return root_window_controller->GetStatusAreaWidget()
      ->notification_center_tray()
      ->privacy_indicators_view();
}

void ExpectPrivacyIndicatorsVisible(bool visible) {
  for (auto* root_window_controller :
       ash::Shell::Get()->GetAllRootWindowControllers()) {
    EXPECT_EQ(GetPrivacyIndicatorsView(root_window_controller)->GetVisible(),
              visible);
  }
}

void ExpectPrivacyIndicatorsCameraIconVisible(bool visible) {
  for (auto* root_window_controller :
       ash::Shell::Get()->GetAllRootWindowControllers()) {
    EXPECT_EQ(GetPrivacyIndicatorsView(root_window_controller)
                  ->camera_icon()
                  ->GetVisible(),
              visible);
  }
}

void ExpectPrivacyIndicatorsMicrophoneIconVisible(bool visible) {
  for (auto* root_window_controller :
       ash::Shell::Get()->GetAllRootWindowControllers()) {
    EXPECT_EQ(GetPrivacyIndicatorsView(root_window_controller)
                  ->microphone_icon()
                  ->GetVisible(),
              visible);
  }
}

}  // namespace

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

class AppAccessNotifierTest : public testing::Test,
                              public ::testing::WithParamInterface<bool> {
 public:
  AppAccessNotifierTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  AppAccessNotifierTest(const AppAccessNotifierTest&) = delete;
  AppAccessNotifierTest& operator=(const AppAccessNotifierTest&) = delete;
  ~AppAccessNotifierTest() override = default;

  bool IsCrosPrivacyHubEnabled() const { return GetParam(); }

  void SetUp() override {
    if (!IsCrosPrivacyHubEnabled()) {
      scoped_feature_list_.InitAndDisableFeature(
          ash::features::kCrosPrivacyHub);
    }

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
        account_id_primary_user_, false, user_manager::UserType::kRegular,
        primary_profile);

    registry_cache_primary_user_.SetAccountId(account_id_primary_user_);
    apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(
        account_id_primary_user_, &registry_cache_primary_user_);
    capability_access_cache_primary_user_.SetAccountId(
        account_id_primary_user_);
    apps::AppCapabilityAccessCacheWrapper::Get().AddAppCapabilityAccessCache(
        account_id_primary_user_, &capability_access_cache_primary_user_);

    SetActiveUserAccountId(/*is_primary=*/true);
    WaitForAppServiceProxyReady(
        apps::AppServiceProxyFactory::GetForProfile(primary_profile));
  }

  void SetupSecondaryUser() {
    auto* secondary_profile = testing_profile_manager_.CreateTestingProfile(
        account_id_secondary_user_.GetUserEmail());
    fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_id_secondary_user_, false, user_manager::UserType::kRegular,
        secondary_profile);

    registry_cache_secondary_user_.SetAccountId(account_id_secondary_user_);
    apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(
        account_id_secondary_user_, &registry_cache_secondary_user_);
    capability_access_cache_secondary_user_.SetAccountId(
        account_id_secondary_user_);
    apps::AppCapabilityAccessCacheWrapper::Get().AddAppCapabilityAccessCache(
        account_id_secondary_user_, &capability_access_cache_secondary_user_);

    SetActiveUserAccountId(/*is_primary=*/false);
    WaitForAppServiceProxyReady(
        apps::AppServiceProxyFactory::GetForProfile(secondary_profile));
  }

  std::vector<std::u16string> GetAppsAccessingCamera() {
    return app_access_notifier_->GetAppsAccessingCamera();
  }

  std::vector<std::u16string> GetAppsAccessingMicrophone() {
    return app_access_notifier_->GetAppsAccessingMicrophone();
  }

  static apps::AppPtr MakeApp(const std::string& app_id,
                              const char* name,
                              apps::AppType app_type) {
    apps::AppPtr app = std::make_unique<apps::App>(app_type, app_id);
    app->name = name;
    app->short_name = name;
    return app;
  }

  static apps::CapabilityAccessPtr MakeCapabilityAccess(
      const std::string& app_id,
      std::optional<bool> camera,
      std::optional<bool> microphone) {
    auto access = std::make_unique<apps::CapabilityAccess>(app_id);
    access->camera = camera;
    access->microphone = microphone;
    return access;
  }

  void LaunchAppUsingCameraOrMicrophone(
      const std::string& id,
      const char* name,
      bool use_camera,
      bool use_microphone,
      apps::AppType app_type = apps::AppType::kChromeApp) {
    apps::AppServiceProxy* proxy = apps::AppServiceProxyFactory::GetForProfile(
        ProfileManager::GetActiveUserProfile());
    apps::AppCapabilityAccessCache* cap_cache =
        app_access_notifier_->GetActiveUserAppCapabilityAccessCache();

    std::vector<apps::AppPtr> registry_deltas;
    registry_deltas.push_back(MakeApp(id, name, app_type));
    proxy->OnApps(std::move(registry_deltas), apps::AppType::kUnknown,
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

  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> fake_user_manager_ =
      nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  // This instance is needed for setting up `ash_test_helper_`.
  // See //docs/threading_and_tasks_testing.md.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingProfileManager testing_profile_manager_;

  // Use this for testing multi-display.
  TestingPrefServiceSimple local_state_;

  ash::AshTestHelper ash_test_helper_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AppAccessNotifierTest,
                         /*IsCrosPrivacyHubEnabled()=*/testing::Bool());

TEST_P(AppAccessNotifierTest, NoAppsLaunched) {
  // The list of apps using mic or camera should be empty.
  EXPECT_EQ(std::vector<std::u16string>(), GetAppsAccessingCamera());
  EXPECT_EQ(std::vector<std::u16string>(), GetAppsAccessingMicrophone());
}

TEST_P(AppAccessNotifierTest, AppLaunchedNotUsingCameraAndMic) {
  LaunchAppUsingCameraOrMicrophone("id_rose", "name_rose", /*use_camera=*/false,
                                   /*use_microphone=*/false);

  // The list of apps using mic or camera should be empty.
  EXPECT_EQ(std::vector<std::u16string>(), GetAppsAccessingCamera());
  EXPECT_EQ(std::vector<std::u16string>(), GetAppsAccessingMicrophone());
}

TEST_P(AppAccessNotifierTest, AppLaunchedUsingCameraAndMic) {
  LaunchAppUsingCameraOrMicrophone("id_rose", "name_rose", /*use_camera=*/true,
                                   /*use_microphone=*/true);

  // List of application names should only contain "name_rose".
  EXPECT_EQ(std::vector<std::u16string>({u"name_rose"}),
            GetAppsAccessingCamera());
  EXPECT_EQ(std::vector<std::u16string>({u"name_rose"}),
            GetAppsAccessingMicrophone());
}

TEST_P(AppAccessNotifierTest, MultipleAppsLaunchedUsingBothCameraAndMic) {
  LaunchAppUsingCameraOrMicrophone("id_rose", "name_rose", /*use_camera=*/true,
                                   /*use_microphone=*/true);
  LaunchAppUsingCameraOrMicrophone("id_mars", "name_mars", /*use_camera=*/true,
                                   /*use_microphone=*/true);
  LaunchAppUsingCameraOrMicrophone("id_zara", "name_zara", /*use_camera=*/true,
                                   /*use_microphone=*/true);
  LaunchAppUsingCameraOrMicrophone(
      "id_oscar", "name_oscar", /*use_camera=*/false, /*use_microphone=*/false);

  // Only the applications using the sensor should be in the respective list and
  // the list should be ordered by the most recently launced app first.
  // "name_oscar" should not be in any of the lists.
  EXPECT_EQ(
      std::vector<std::u16string>({u"name_zara", u"name_mars", u"name_rose"}),
      GetAppsAccessingCamera());
  EXPECT_EQ(
      std::vector<std::u16string>({u"name_zara", u"name_mars", u"name_rose"}),
      GetAppsAccessingMicrophone());

  // Oscar starts using camera, Oscar should be the front element of the camera
  // list now. The mic list should stay the same.
  LaunchAppUsingCameraOrMicrophone(
      "id_oscar", "name_oscar", /*use_camera=*/true, /*use_microphone=*/false);
  EXPECT_EQ(std::vector<std::u16string>(
                {u"name_oscar", u"name_zara", u"name_mars", u"name_rose"}),
            GetAppsAccessingCamera());
  EXPECT_EQ(
      std::vector<std::u16string>({u"name_zara", u"name_mars", u"name_rose"}),
      GetAppsAccessingMicrophone());

  // Oscar starts using mic, Oscar should be the front element of the mic list
  // as well now.
  LaunchAppUsingCameraOrMicrophone(
      "id_oscar", "name_oscar", /*use_camera=*/true, /*use_microphone=*/true);
  EXPECT_EQ(std::vector<std::u16string>(
                {u"name_oscar", u"name_zara", u"name_mars", u"name_rose"}),
            GetAppsAccessingCamera());
  EXPECT_EQ(std::vector<std::u16string>(
                {u"name_oscar", u"name_zara", u"name_mars", u"name_rose"}),
            GetAppsAccessingMicrophone());

  // If we "kill" Oscar (set to no longer be using the mic or camera), Oscar
  // should not be in the returned lists anymore. Zara should be at the front of
  // both of the lists.
  LaunchAppUsingCameraOrMicrophone(
      "id_oscar", "name_oscar", /*use_camera=*/false, /*use_microphone=*/false);
  EXPECT_EQ(
      std::vector<std::u16string>({u"name_zara", u"name_mars", u"name_rose"}),
      GetAppsAccessingCamera());
  EXPECT_EQ(
      std::vector<std::u16string>({u"name_zara", u"name_mars", u"name_rose"}),
      GetAppsAccessingMicrophone());
}

TEST_P(AppAccessNotifierTest, MultipleUsers) {
  // Prepare the secondary user.
  SetupSecondaryUser();

  // Primary user is the active user.
  SetActiveUserAccountId(/*is_primary=*/true);

  // Primary user launches an app using both sensors.
  LaunchAppUsingCameraOrMicrophone("id_primary_user", "name_primary_user",
                                   /*use_camera=*/true,
                                   /*use_microphone=*/true);

  // App we just launched should be the front and only element of both the
  // lists.
  EXPECT_EQ(std::vector<std::u16string>({u"name_primary_user"}),
            GetAppsAccessingCamera());
  EXPECT_EQ(std::vector<std::u16string>({u"name_primary_user"}),
            GetAppsAccessingMicrophone());

  // Secondary user is now the primary user.
  SetActiveUserAccountId(/*is_primary=*/false);

  // Secondary user launches an app using both sensors.
  LaunchAppUsingCameraOrMicrophone("id_secondary_user", "name_secondary_user",
                                   /*use_camera=*/true,
                                   /*use_microphone=*/true);

  // App we just launched should be the front and only element of both the
  // lists.
  EXPECT_EQ(std::vector<std::u16string>({u"name_secondary_user"}),
            GetAppsAccessingCamera());
  EXPECT_EQ(std::vector<std::u16string>({u"name_secondary_user"}),
            GetAppsAccessingMicrophone());

  // Switch back to the primary user and "kill" the app it was running, no app
  // name to show.
  SetActiveUserAccountId(/*is_primary=*/true);
  LaunchAppUsingCameraOrMicrophone("id_primary_user", "name_primary_user",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/false);
  EXPECT_EQ(std::vector<std::u16string>(), GetAppsAccessingCamera());
  EXPECT_EQ(std::vector<std::u16string>(), GetAppsAccessingMicrophone());

  // Now switch back to the secondary user, verify that the same app as before
  // shows up in the lists.
  SetActiveUserAccountId(/*is_primary=*/false);
  EXPECT_EQ(std::vector<std::u16string>({u"name_secondary_user"}),
            GetAppsAccessingCamera());
  EXPECT_EQ(std::vector<std::u16string>({u"name_secondary_user"}),
            GetAppsAccessingMicrophone());

  // Now "kill" our secondary user's app and verify that there's no name to
  // show.
  LaunchAppUsingCameraOrMicrophone("id_secondary_user", "name_secondary_user",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/false);
  EXPECT_EQ(std::vector<std::u16string>(), GetAppsAccessingCamera());
  EXPECT_EQ(std::vector<std::u16string>(), GetAppsAccessingMicrophone());
}

TEST_P(AppAccessNotifierTest, MultipleUsersMultipleApps) {
  // Prepare the secondary user.
  SetupSecondaryUser();

  // Primary user is the active user.
  SetActiveUserAccountId(/*is_primary=*/true);

  // Primary user launches an app using both sensors.
  LaunchAppUsingCameraOrMicrophone("id_primary_user", "name_primary_user",
                                   /*use_camera=*/true,
                                   /*use_microphone=*/true);

  // App we just launched should be the front and only element of both the
  // lists.
  EXPECT_EQ(std::vector<std::u16string>({u"name_primary_user"}),
            GetAppsAccessingCamera());
  EXPECT_EQ(std::vector<std::u16string>({u"name_primary_user"}),
            GetAppsAccessingMicrophone());

  // Primary user launches a second app using both sensors.
  LaunchAppUsingCameraOrMicrophone(
      "id_primary_user_another_app", "name_primary_user_another_app",
      /*use_camera=*/true, /*use_microphone=*/true);

  // The lists should contain two application names ordered by most recently
  // launched.
  EXPECT_EQ(std::vector<std::u16string>(
                {u"name_primary_user_another_app", u"name_primary_user"}),
            GetAppsAccessingCamera());
  EXPECT_EQ(std::vector<std::u16string>(
                {u"name_primary_user_another_app", u"name_primary_user"}),
            GetAppsAccessingMicrophone());

  // Secondary user is now the primary user.
  SetActiveUserAccountId(/*is_primary=*/false);

  // Secondary user launches an app using both sensors.
  LaunchAppUsingCameraOrMicrophone("id_secondary_user", "name_secondary_user",
                                   /*use_camera=*/true,
                                   /*use_microphone=*/true);

  // App we just launched should be the front and only element of both the
  // lists.
  EXPECT_EQ(std::vector<std::u16string>({u"name_secondary_user"}),
            GetAppsAccessingCamera());
  EXPECT_EQ(std::vector<std::u16string>({u"name_secondary_user"}),
            GetAppsAccessingMicrophone());

  // Secondary user launches a second app using both sensors.
  LaunchAppUsingCameraOrMicrophone(
      "id_secondary_user_another_app", "name_secondary_user_another_app",
      /*use_camera=*/true, /*use_microphone=*/true);

  // The lists should contain two application names ordered by most recently
  // launched.
  EXPECT_EQ(std::vector<std::u16string>(
                {u"name_secondary_user_another_app", u"name_secondary_user"}),
            GetAppsAccessingCamera());
  EXPECT_EQ(std::vector<std::u16string>(
                {u"name_secondary_user_another_app", u"name_secondary_user"}),
            GetAppsAccessingMicrophone());

  // Switch back to the primary user.
  SetActiveUserAccountId(/*is_primary=*/true);

  // Both of the apps we launced for the primary user should be in the lists
  // ordered by most recently launched.
  EXPECT_EQ(std::vector<std::u16string>(
                {u"name_primary_user_another_app", u"name_primary_user"}),
            GetAppsAccessingCamera());
  EXPECT_EQ(std::vector<std::u16string>(
                {u"name_primary_user_another_app", u"name_primary_user"}),
            GetAppsAccessingMicrophone());
}

TEST_P(AppAccessNotifierTest, GetShortNameFromAppId) {
  // Test that GetAppShortNameFromAppId works properly.
  const std::string id = "test_app_id";
  LaunchAppUsingCameraOrMicrophone(id, "test_app_name", /*use_camera=*/false,
                                   /*use_microphone=*/true);
  EXPECT_EQ(AppAccessNotifier::GetAppShortNameFromAppId(id), u"test_app_name");
}

TEST_P(AppAccessNotifierTest, AppAccessNotification) {
  // Test that notifications get created/removed when an app is accessing camera
  // or microphone.
  const std::string id1 = "test_app_id_1";
  const std::string id2 = "test_app_id_2";
  const std::string notification_id1 =
      ash::GetPrivacyIndicatorsNotificationId(id1);
  const std::string notification_id2 =
      ash::GetPrivacyIndicatorsNotificationId(id2);

  LaunchAppUsingCameraOrMicrophone(id1, "test_app_name", /*use_camera=*/false,
                                   /*use_microphone=*/true);
  LaunchAppUsingCameraOrMicrophone(id2, "test_app_name", /*use_camera=*/true,
                                   /*use_microphone=*/false);
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id1));
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id2));

  LaunchAppUsingCameraOrMicrophone(id1, "test_app_name", /*use_camera=*/false,
                                   /*use_microphone=*/false);
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment_.FastForwardBy(
      ash::PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  LaunchAppUsingCameraOrMicrophone(id2, "test_app_name", /*use_camera=*/false,
                                   /*use_microphone=*/false);
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment_.FastForwardBy(
      ash::PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id1));
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id2));

  LaunchAppUsingCameraOrMicrophone(id1, "test_app_name", /*use_camera=*/true,
                                   /*use_microphone=*/true);
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id1));
}

TEST_P(AppAccessNotifierTest, PrivacyIndicatorsVisibility) {
  // Uses normal animation duration so that the icons would not be immediately
  // hidden after the animation.
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Make sure privacy indicators work on multiple displays.
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("800x700,801+0-800x700");

  ExpectPrivacyIndicatorsVisible(/*visible=*/false);

  // Privacy indicators should show up if at least camera or microphone is being
  // accessed. The icons should show up accordingly (only at the start of the
  // animation).
  LaunchAppUsingCameraOrMicrophone("test_app_id", "test_app_name",
                                   /*use_camera=*/true,
                                   /*use_microphone=*/true);
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment_.FastForwardBy(
      ash::PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  ExpectPrivacyIndicatorsVisible(/*visible=*/true);
  ExpectPrivacyIndicatorsCameraIconVisible(/*visible=*/true);
  ExpectPrivacyIndicatorsMicrophoneIconVisible(/*visible=*/true);

  LaunchAppUsingCameraOrMicrophone("test_app_id", "test_app_name",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/false);
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment_.FastForwardBy(
      ash::PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  ExpectPrivacyIndicatorsVisible(/*visible=*/false);
  ExpectPrivacyIndicatorsCameraIconVisible(/*visible=*/false);
  ExpectPrivacyIndicatorsMicrophoneIconVisible(/*visible=*/false);

  LaunchAppUsingCameraOrMicrophone("test_app_id", "test_app_name",
                                   /*use_camera=*/true,
                                   /*use_microphone=*/false);
  ExpectPrivacyIndicatorsVisible(/*visible=*/true);
  ExpectPrivacyIndicatorsCameraIconVisible(/*visible=*/true);
  ExpectPrivacyIndicatorsMicrophoneIconVisible(/*visible=*/false);

  LaunchAppUsingCameraOrMicrophone("test_app_id", "test_app_name",
                                   /*use_camera=*/false,
                                   /*use_microphone=*/true);
  ExpectPrivacyIndicatorsVisible(/*visible=*/true);
  ExpectPrivacyIndicatorsCameraIconVisible(/*visible=*/false);
  ExpectPrivacyIndicatorsMicrophoneIconVisible(/*visible=*/true);
}

TEST_P(AppAccessNotifierTest, RecordAppType) {
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

TEST_P(AppAccessNotifierTest, RecordLaunchSettings) {
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

// Tests that the privacy indicators notification of a system web app should not
// have a launch settings callback (thus it will not have a launch settings
// button).
TEST_P(AppAccessNotifierTest, SystemWebAppWithoutSettingsCallback) {
  const std::string app_id = "test_app_id";
  LaunchAppUsingCameraOrMicrophone(app_id, "test_app_name",
                                   /*use_camera=*/true,
                                   /*use_microphone=*/false,
                                   /*app_type=*/apps::AppType::kSystemWeb);
  const std::string notification_id =
      ash::GetPrivacyIndicatorsNotificationId(app_id);

  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);
  ASSERT_TRUE(notification);

  auto* delegate = static_cast<ash::PrivacyIndicatorsNotificationDelegate*>(
      notification->delegate());
  EXPECT_FALSE(delegate->launch_settings_callback());
}
