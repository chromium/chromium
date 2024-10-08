// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/hats_office_trigger.h"

#include "ash/constants/ash_switches.h"
#include "ash/constants/web_app_id_constants.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/safe_browsing/url_lookup_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::cloud_upload {

namespace {
constexpr char kFakeUserEmail[] = "test-user@example.com";
}  // namespace

class HatsOfficeTriggerTestBase : public testing::Test {
 public:
  HatsOfficeTriggerTestBase() {
    scoped_feature_list_.InitAndEnableFeature(kHatsOfficeSurvey.feature);

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(
        ash::switches::kForceHappinessTrackingSystem,
        ::features::kHappinessTrackingOffice.name);
  }
  ~HatsOfficeTriggerTestBase() override = default;

  // testing::Test:
  void SetUp() override {
    user_manager_ = std::make_unique<FakeChromeUserManager>();
    user_manager_->Initialize();
    // Login user.
    user_manager_->AddUser(AccountId::FromUserEmail(kFakeUserEmail));
  }

  void TearDown() override {
    user_manager_->Destroy();
    user_manager_.reset();
  }

  bool IsDelayTriggerActive() {
    return hats_office_trigger_.IsDelayTriggerActiveForTesting();
  }

  bool IsAppStateTriggerActive() {
    return hats_office_trigger_.IsAppStateTriggerActiveForTesting();
  }

  const HatsNotificationController* GetHatsNotificationController() const {
    return hats_office_trigger_.GetHatsNotificationControllerForTesting();
  }

  base::Time Now() const { return task_environment_.GetMockClock()->Now(); }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  session_manager::SessionManager session_manager_;
  std::unique_ptr<FakeChromeUserManager> user_manager_;

  HatsOfficeTrigger hats_office_trigger_;
};

class HatsOfficeTriggerTest : public HatsOfficeTriggerTestBase {
 public:
  void SetUp() override {
    HatsOfficeTriggerTestBase::SetUp();

    const AccountId account_id = AccountId::FromUserEmail(kFakeUserEmail);

    user_manager_->LoginUser(account_id);
    user_manager_->SwitchActiveUser(account_id);

    ASSERT_TRUE(test_profile_manager_.SetUp());
    // Use sign-in profile to simulate the login screen.
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
    test_profile_manager_.DeleteAllTestingProfiles();

    HatsOfficeTriggerTestBase::TearDown();
  }

  bool IsHatsNotificationActive() const {
    return display_service_
        ->GetNotification(HatsNotificationController::kNotificationId)
        .has_value();
  }

  void OnTrackedDocsInstance(apps::InstanceState state) {
    apps::InstanceRegistry& registry =
        apps::AppServiceProxyFactory::GetForProfile(profile_)
            ->InstanceRegistry();
    auto instance = std::make_unique<apps::Instance>(
        web_app::kGoogleDocsAppId, tracked_instance_id_, nullptr);
    instance->UpdateState(state, Now());
    registry.OnInstance(std::move(instance));
  }

  void OnIgnoredDocsInstance(apps::InstanceState state) {
    apps::InstanceRegistry& registry =
        apps::AppServiceProxyFactory::GetForProfile(profile_)
            ->InstanceRegistry();
    auto instance = std::make_unique<apps::Instance>(
        web_app::kGoogleDocsAppId, ignored_instance_id_, nullptr);
    instance->UpdateState(state, Now());
    registry.OnInstance(std::move(instance));
  }

  TestingProfileManager test_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  raw_ptr<Profile> profile_ = nullptr;

  base::UnguessableToken tracked_instance_id_ =
      base::UnguessableToken::Create();
  base::UnguessableToken ignored_instance_id_ =
      base::UnguessableToken::Create();

  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
};

class HatsOfficeTriggerLoginScreenTest : public HatsOfficeTriggerTestBase {
 public:
  void SetUp() override {
    HatsOfficeTriggerTestBase::SetUp();
    ash::ProfileHelper::Get();  // Instantiate ProfileHelper.
    session_manager_.SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);
  }
};

TEST_F(HatsOfficeTriggerTest, ShowSurveyAfterDelaySuccess) {
  ASSERT_FALSE(IsHatsNotificationActive());

  base::test::TestFuture<void> future;
  display_service_->SetNotificationAddedClosure(future.GetRepeatingCallback());
  hats_office_trigger_.ShowSurveyAfterDelay(
      HatsOfficeLaunchingApp::kQuickOffice);

  ASSERT_TRUE(IsDelayTriggerActive());
  ASSERT_FALSE(IsHatsNotificationActive());
  ASSERT_FALSE(GetHatsNotificationController());

  ASSERT_TRUE(future.Wait());

  ASSERT_FALSE(IsDelayTriggerActive());
  ASSERT_TRUE(GetHatsNotificationController());
  ASSERT_TRUE(IsHatsNotificationActive());
}

TEST_F(HatsOfficeTriggerTest, ShowSurveyAfterAppInactiveSuccess) {
  ASSERT_FALSE(IsHatsNotificationActive());
  ASSERT_FALSE(GetHatsNotificationController());

  base::test::TestFuture<void> future;
  display_service_->SetNotificationAddedClosure(future.GetRepeatingCallback());
  hats_office_trigger_.ShowSurveyAfterAppInactive(
      web_app::kGoogleDocsAppId, HatsOfficeLaunchingApp::kDrive);

  // Simulate receiving updates from an instance that shouldn't be tracked.
  OnIgnoredDocsInstance(
      apps::InstanceState(apps::kStarted | apps::kRunning | apps::kVisible));
  task_environment_.FastForwardBy(kDebounceDelay);
  // Simulate receiving instance updates. The first few updates, when the app is
  // not "active" yet, should be debounced.
  OnTrackedDocsInstance(apps::InstanceState(apps::kStarted | apps::kRunning));
  task_environment_.FastForwardBy(kDebounceDelay / 2);
  OnTrackedDocsInstance(
      apps::InstanceState(apps::kStarted | apps::kRunning | apps::kVisible));
  task_environment_.FastForwardBy(kDebounceDelay / 2);

  ASSERT_FALSE(IsHatsNotificationActive());
  ASSERT_FALSE(GetHatsNotificationController());

  // Simulate the app being active. The survey shouldn't be triggered.
  OnTrackedDocsInstance(apps::InstanceState(apps::kStarted | apps::kRunning |
                                            apps::kVisible | apps::kActive));
  task_environment_.FastForwardBy(kDebounceDelay);

  ASSERT_FALSE(IsHatsNotificationActive());
  ASSERT_FALSE(GetHatsNotificationController());

  // Simulate another update from the app that isn't tracked, it shouldn't
  // trigger the survey either.
  OnIgnoredDocsInstance(
      apps::InstanceState(apps::kStarted | apps::kRunning | apps::kVisible));
  task_environment_.FastForwardBy(kDebounceDelay);

  ASSERT_FALSE(IsHatsNotificationActive());
  ASSERT_FALSE(GetHatsNotificationController());

  // Simulate the app being no longer active, it will this time trigger the
  // survey.
  OnTrackedDocsInstance(
      apps::InstanceState(apps::kStarted | apps::kRunning | apps::kVisible));

  ASSERT_FALSE(future.IsReady());
  task_environment_.FastForwardBy(kDebounceDelay);
  ASSERT_TRUE(future.IsReady());

  ASSERT_TRUE(GetHatsNotificationController());
  ASSERT_TRUE(IsHatsNotificationActive());
}

TEST_F(HatsOfficeTriggerTest, NoAppUpdateTimeout) {
  ASSERT_FALSE(IsAppStateTriggerActive());
  ASSERT_FALSE(IsHatsNotificationActive());
  ASSERT_FALSE(GetHatsNotificationController());

  base::test::TestFuture<void> future;
  display_service_->SetNotificationAddedClosure(future.GetRepeatingCallback());

  hats_office_trigger_.ShowSurveyAfterAppInactive(
      web_app::kGoogleDocsAppId, HatsOfficeLaunchingApp::kDrive);

  ASSERT_TRUE(IsAppStateTriggerActive());

  // The expected initial app event (started and running) isn't received during
  // the initial delay.
  OnTrackedDocsInstance(apps::kStarted);
  task_environment_.FastForwardBy(kFirstAppStateEventTimeout);

  ASSERT_FALSE(IsAppStateTriggerActive());
  ASSERT_FALSE(IsHatsNotificationActive());
  ASSERT_FALSE(GetHatsNotificationController());

  // Simulate receiving the expected instance updates. They shouldn't trigger
  // the survey.
  OnTrackedDocsInstance(apps::InstanceState(apps::kStarted | apps::kRunning));
  task_environment_.FastForwardBy(kDebounceDelay / 2);
  // Running and active.
  OnTrackedDocsInstance(apps::InstanceState(apps::kStarted | apps::kRunning |
                                            apps::kVisible | apps::kActive));
  task_environment_.FastForwardBy(kDebounceDelay * 2);
  // Destroyed.
  OnTrackedDocsInstance(apps::kDestroyed);
  task_environment_.FastForwardBy(kDebounceDelay);

  ASSERT_FALSE(future.IsReady());
  ASSERT_FALSE(IsHatsNotificationActive());
  ASSERT_FALSE(GetHatsNotificationController());
}

TEST_F(HatsOfficeTriggerTest, ShowSurveyOnlyOnce) {
  ASSERT_FALSE(IsHatsNotificationActive());

  // Show survey once
  base::test::TestFuture<void> future;
  display_service_->SetNotificationAddedClosure(future.GetRepeatingCallback());

  hats_office_trigger_.ShowSurveyAfterDelay(
      HatsOfficeLaunchingApp::kQuickOfficeClippyOff);

  ASSERT_TRUE(IsDelayTriggerActive());
  ASSERT_FALSE(IsHatsNotificationActive());
  ASSERT_FALSE(GetHatsNotificationController());

  ASSERT_TRUE(future.Wait());

  ASSERT_FALSE(IsDelayTriggerActive());
  const HatsNotificationController* hats_notification_controller =
      GetHatsNotificationController();
  EXPECT_NE(hats_notification_controller, nullptr);
  ASSERT_TRUE(IsHatsNotificationActive());

  // Trigger survey again but the controller shouldn't be a new instance.
  hats_office_trigger_.ShowSurveyAfterDelay(HatsOfficeLaunchingApp::kDrive);

  ASSERT_FALSE(IsDelayTriggerActive());
  EXPECT_EQ(hats_notification_controller, GetHatsNotificationController());
}

TEST_F(HatsOfficeTriggerLoginScreenTest, NoActiveUser) {
  hats_office_trigger_.ShowSurveyAfterDelay(HatsOfficeLaunchingApp::kMS365);
  ASSERT_FALSE(IsDelayTriggerActive());
  ASSERT_FALSE(GetHatsNotificationController());
}

TEST_F(HatsOfficeTriggerTest, NoSurveyForManagedProfile) {
  profile_->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  ASSERT_FALSE(IsHatsNotificationActive());

  hats_office_trigger_.ShowSurveyAfterDelay(HatsOfficeLaunchingApp::kMS365);
  ASSERT_FALSE(IsDelayTriggerActive());
  ASSERT_FALSE(GetHatsNotificationController());
}

TEST_F(HatsOfficeTriggerTest, NoSurveyIfSessionNotActive) {
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);
  ASSERT_FALSE(IsHatsNotificationActive());

  hats_office_trigger_.ShowSurveyAfterDelay(HatsOfficeLaunchingApp::kMS365);

  ASSERT_TRUE(IsDelayTriggerActive());
  task_environment_.FastForwardBy(kDelayTriggerTimeout);
  ASSERT_FALSE(IsDelayTriggerActive());
  ASSERT_FALSE(GetHatsNotificationController());
}

}  // namespace ash::cloud_upload
