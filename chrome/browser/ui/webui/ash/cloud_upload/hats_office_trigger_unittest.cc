// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/hats_office_trigger.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::cloud_upload {

namespace {
constexpr char kFakeUserEmail[] = "test-user@example.com";
}  // namespace

class HatsOfficeTriggerTest : public testing::Test {
 public:
  HatsOfficeTriggerTest() {
    scoped_feature_list_.InitAndEnableFeature(kHatsOfficeSurvey.feature);

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(
        ash::switches::kForceHappinessTrackingSystem,
        ::features::kHappinessTrackingOffice.name);
  }
  ~HatsOfficeTriggerTest() override = default;

  // testing::Test:
  void SetUp() override {
    user_manager_ = std::make_unique<FakeChromeUserManager>();
    user_manager_->Initialize();
    // Login user.
    const AccountId account_id(AccountId::FromUserEmail(kFakeUserEmail));
    user_manager_->AddUser(account_id);
    user_manager_->LoginUser(account_id);
    user_manager_->SwitchActiveUser(account_id);

    ASSERT_TRUE(test_profile_manager_.SetUp());
    profile_ = test_profile_manager_.CreateTestingProfile(kFakeUserEmail);
    session_manager_.SetSessionState(session_manager::SessionState::ACTIVE);

    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_);

    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();

    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    // Remove the hats notification if it is showing. This allows the
    // scopedref-ed HatsNotificationController to get destroyed at the end of
    // the test.
    if (display_service_
            ->GetNotification(HatsNotificationController::kNotificationId)
            .has_value()) {
      display_service_->RemoveNotification(
          NotificationHandler::Type::TRANSIENT,
          HatsNotificationController::kNotificationId, /*by_user=*/true);
    }
    network_handler_test_helper_.reset();
    display_service_.reset();
    profile_ = nullptr;
    test_profile_manager_.DeleteTestingProfile(kFakeUserEmail);
    user_manager_->Destroy();
    user_manager_.reset();
  }

  bool IsHatsNotificationActive() const {
    return display_service_
        ->GetNotification(HatsNotificationController::kNotificationId)
        .has_value();
  }

  const HatsNotificationController* GetHatsNotificationController() const {
    return hats_office_trigger_.GetHatsNotificationControllerForTesting();
  }

  base::OneShotTimer& GetTimer() {
    return hats_office_trigger_.GetTimerForTesting();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  session_manager::SessionManager session_manager_;
  std::unique_ptr<FakeChromeUserManager> user_manager_;
  TestingProfileManager test_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  raw_ptr<Profile> profile_;

  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;

  HatsOfficeTrigger hats_office_trigger_;
};

TEST_F(HatsOfficeTriggerTest, ShowSurveySuccess) {
  EXPECT_FALSE(IsHatsNotificationActive());

  base::test::TestFuture<void> future;
  display_service_->SetNotificationAddedClosure(future.GetRepeatingCallback());
  hats_office_trigger_.ShowSurveyAfterDelay(
      HatsOfficeLaunchingApp::kQuickOffice);

  EXPECT_TRUE(GetTimer().IsRunning());
  EXPECT_FALSE(IsHatsNotificationActive());
  EXPECT_FALSE(GetHatsNotificationController());

  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(GetTimer().IsRunning());
  EXPECT_TRUE(GetHatsNotificationController());
  EXPECT_TRUE(IsHatsNotificationActive());
}

TEST_F(HatsOfficeTriggerTest, ShowSurveyOnlyOnce) {
  EXPECT_FALSE(IsHatsNotificationActive());

  // Show survey once
  base::test::TestFuture<void> future;
  display_service_->SetNotificationAddedClosure(future.GetRepeatingCallback());

  hats_office_trigger_.ShowSurveyAfterDelay(
      HatsOfficeLaunchingApp::kQuickOfficeClippyOff);

  EXPECT_TRUE(GetTimer().IsRunning());
  EXPECT_FALSE(IsHatsNotificationActive());
  EXPECT_FALSE(GetHatsNotificationController());

  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(GetTimer().IsRunning());
  const HatsNotificationController* hats_notification_controller =
      GetHatsNotificationController();
  EXPECT_NE(hats_notification_controller, nullptr);
  EXPECT_TRUE(IsHatsNotificationActive());

  // Trigger survey again but the controller shouldn't be a new instance.
  hats_office_trigger_.ShowSurveyAfterDelay(HatsOfficeLaunchingApp::kDrive);

  EXPECT_FALSE(GetTimer().IsRunning());
  EXPECT_EQ(hats_notification_controller, GetHatsNotificationController());
}

TEST_F(HatsOfficeTriggerTest, NoActiveProfile) {
  // Remove the current active user.
  const AccountId account_id(
      AccountId::FromUserEmail(profile_->GetProfileUserName()));
  user_manager_->RemoveUser(account_id,
                            user_manager::UserRemovalReason::UNKNOWN);
  EXPECT_FALSE(IsHatsNotificationActive());

  hats_office_trigger_.ShowSurveyAfterDelay(HatsOfficeLaunchingApp::kMS365);

  EXPECT_TRUE(GetTimer().IsRunning());
  task_environment_.FastForwardBy(kHatsSurveyTimeout);
  EXPECT_FALSE(GetTimer().IsRunning());
  EXPECT_FALSE(GetHatsNotificationController());
}

TEST_F(HatsOfficeTriggerTest, NoSurveyForManagedProfile) {
  profile_->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  EXPECT_FALSE(IsHatsNotificationActive());

  hats_office_trigger_.ShowSurveyAfterDelay(HatsOfficeLaunchingApp::kMS365);

  EXPECT_TRUE(GetTimer().IsRunning());
  task_environment_.FastForwardBy(kHatsSurveyTimeout);
  EXPECT_FALSE(GetTimer().IsRunning());
  EXPECT_FALSE(GetHatsNotificationController());
}

TEST_F(HatsOfficeTriggerTest, NoSurveyIfSessionNotActive) {
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);
  EXPECT_FALSE(IsHatsNotificationActive());

  hats_office_trigger_.ShowSurveyAfterDelay(HatsOfficeLaunchingApp::kMS365);

  EXPECT_TRUE(GetTimer().IsRunning());
  task_environment_.FastForwardBy(kHatsSurveyTimeout);
  EXPECT_FALSE(GetTimer().IsRunning());
  EXPECT_FALSE(GetHatsNotificationController());
}

}  // namespace ash::cloud_upload
