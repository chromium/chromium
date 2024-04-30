// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/ephemeral_network_configuration_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/time/time.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/mock_managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using testing::Return;

class EphemeralNetworkConfigurationHandlerTest : public testing::Test {
 public:
  EphemeralNetworkConfigurationHandlerTest() {
    policy_util::SetEphemeralNetworkPoliciesEnabled();

    LoginState::Initialize();
    LoginState::Get()->set_always_logged_in(false);
  }

  ~EphemeralNetworkConfigurationHandlerTest() override {
    ephemeral_network_configuration_handler_.reset();

    LoginState::Shutdown();
  }

  EphemeralNetworkConfigurationHandlerTest(
      const EphemeralNetworkConfigurationHandlerTest&) = delete;
  EphemeralNetworkConfigurationHandlerTest& operator=(
      const EphemeralNetworkConfigurationHandlerTest&) = delete;

 protected:
  void NotifyScreenIdleOffChanged(bool off) {
    power_manager::ScreenIdleState proto;
    proto.set_off(off);
    fake_power_manager_client_.SendScreenIdleStateChanged(proto);
  }

  MockManagedNetworkConfigurationHandler
      mock_managed_network_configuration_handler_;
  chromeos::FakePowerManagerClient fake_power_manager_client_;
  std::unique_ptr<EphemeralNetworkConfigurationHandler>
      ephemeral_network_configuration_handler_;
};

// If an ephemeral network config action is active, the device is on the sign-in
// screen (logged-in state .*_NONE) and network policy has already been applied,
// the EphemeralNetworkConfigurationHandler triggrers
// TriggerEphemeralNetworkConfigActions once on construction.
TEST_F(EphemeralNetworkConfigurationHandlerTest,
       PolicyAlreadyApplied_SignInScreen_Active) {
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_NONE,
                                      LoginState::LOGGED_IN_USER_NONE);
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              RecommendedValuesAreEphemeral())
      .WillRepeatedly(Return(true));

  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions());

  ephemeral_network_configuration_handler_ =
      EphemeralNetworkConfigurationHandler::TryCreate(
          &mock_managed_network_configuration_handler_,
          /*was_enterprise_managed_at_startup=*/true);
  ASSERT_TRUE(ephemeral_network_configuration_handler_);

  testing::Mock::VerifyAndClearExpectations(
      &mock_managed_network_configuration_handler_);

  // A follow-up policy change doesn't trigger
  // TriggerEphemeralNetworkConfigActions anymore (it is set to Times(1) above).
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(0);
  ephemeral_network_configuration_handler_->TriggerPoliciesChangedForTesting(
      /*userhash=*/std::string());
}

// If an ephemeral network config action is active, the device is on the sign-in
// screen (logged-in state .*_NONE) and network policy is applied, the
// EphemeralNetworkConfigurationHandler triggrers
// TriggerEphemeralNetworkConfigActions once.
TEST_F(EphemeralNetworkConfigurationHandlerTest,
       InitialPolicyChange_SignInScreen_Active) {
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_NONE,
                                      LoginState::LOGGED_IN_USER_NONE);

  // At EphemeralNetworkConfigurationHandler construction time the policies have
  // not been applied yet.
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              RecommendedValuesAreEphemeral())
      .WillRepeatedly(Return(false));

  ephemeral_network_configuration_handler_ =
      EphemeralNetworkConfigurationHandler::TryCreate(
          &mock_managed_network_configuration_handler_,
          /*was_enterprise_managed_at_startup=*/true);
  ASSERT_TRUE(ephemeral_network_configuration_handler_);

  // Apply policies enabling ephemeral network policy actions.
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              RecommendedValuesAreEphemeral())
      .WillRepeatedly(Return(true));

  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions());
  ephemeral_network_configuration_handler_->TriggerPoliciesChangedForTesting(
      /*userhash=*/std::string());

  testing::Mock::VerifyAndClearExpectations(
      &mock_managed_network_configuration_handler_);

  // A follow-up policy change doesn't trigger
  // TriggerEphemeralNetworkConfigActions anymore (it is set to Times(1) above).
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(0);
  ephemeral_network_configuration_handler_->TriggerPoliciesChangedForTesting(
      /*userhash=*/std::string());
}

// If the device enterprise-enrolls on the sign-in screen, no ephemeral network
// config actions are triggered even if the ephemeral network policies are
// enabled according to the newly applied device policy.
TEST_F(EphemeralNetworkConfigurationHandlerTest,
       InitialPolicyChange_SignInScreen_Enrollment) {
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_NONE,
                                      LoginState::LOGGED_IN_USER_NONE);

  // At EphemeralNetworkConfigurationHandler construction time the policies have
  // not been applied yet.
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              RecommendedValuesAreEphemeral())
      .WillRepeatedly(Return(false));

  ephemeral_network_configuration_handler_ =
      EphemeralNetworkConfigurationHandler::TryCreate(
          &mock_managed_network_configuration_handler_,
          /*was_enterprise_managed_at_startup=*/false);
  ASSERT_TRUE(ephemeral_network_configuration_handler_);

  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(0);

  // Mark as enterprise-enrolled and apply policies enabling ephemeral network
  // policy actions.
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              RecommendedValuesAreEphemeral())
      .WillRepeatedly(Return(true));

  ephemeral_network_configuration_handler_->TriggerPoliciesChangedForTesting(
      /*userhash=*/std::string());

  testing::Mock::VerifyAndClearExpectations(
      &mock_managed_network_configuration_handler_);

  // A follow-up policy change also doesn't trigger
  // TriggerEphemeralNetworkConfigActions.
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(0);
  ephemeral_network_configuration_handler_->TriggerPoliciesChangedForTesting(
      /*userhash=*/std::string());
}

// The device is not on the sign-in screen, so the
// EphemeralNetworkConfigurationHandler does not triggrer
// TriggerEphemeralNetworkConfigActions on network policy application.
TEST_F(EphemeralNetworkConfigurationHandlerTest,
       InitialPolicyChange_NotSigninScreen_Active) {
  LoginState::Get()->SetLoggedInState(
      LoginState::LOGGED_IN_ACTIVE, LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);

  // At EphemeralNetworkConfigurationHandler construction time the policies have
  // not been applied yet.
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              RecommendedValuesAreEphemeral())
      .WillRepeatedly(Return(false));

  ephemeral_network_configuration_handler_ =
      EphemeralNetworkConfigurationHandler::TryCreate(
          &mock_managed_network_configuration_handler_,
          /*was_enterprise_managed_at_startup=*/true);
  ASSERT_TRUE(ephemeral_network_configuration_handler_);

  EXPECT_CALL(mock_managed_network_configuration_handler_,
              RecommendedValuesAreEphemeral())
      .WillRepeatedly(Return(true));

  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(0);
  ephemeral_network_configuration_handler_->TriggerPoliciesChangedForTesting(
      /*userhash=*/std::string());
}

// No "ephemeral network config" policy is active, so the
// EphemeralNetworkConfigurationHandler does not triggrer
// TriggerEphemeralNetworkConfigActions on network policy application.
TEST_F(EphemeralNetworkConfigurationHandlerTest,
       InitialPolicyChange_SignInScreen_NotActive) {
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_NONE,
                                      LoginState::LOGGED_IN_USER_NONE);
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              RecommendedValuesAreEphemeral())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              UserCreatedNetworkConfigurationsAreEphemeral())
      .WillRepeatedly(Return(false));

  ephemeral_network_configuration_handler_ =
      EphemeralNetworkConfigurationHandler::TryCreate(
          &mock_managed_network_configuration_handler_,
          /*was_enterprise_managed_at_startup=*/true);
  ASSERT_TRUE(ephemeral_network_configuration_handler_);

  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(0);
  ephemeral_network_configuration_handler_->TriggerPoliciesChangedForTesting(
      /*userhash=*/std::string());
}

// When the device wakes up from suspend and a "ephemeral network config"
// (RecommendedValuesAreEphemeral) policy is active,
// TriggerEphemeralNetworkConfigActions is triggered.
TEST_F(EphemeralNetworkConfigurationHandlerTest,
       SuspendDoneReal_Active_Recommended) {
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_NONE,
                                      LoginState::LOGGED_IN_USER_NONE);

  ephemeral_network_configuration_handler_ =
      EphemeralNetworkConfigurationHandler::TryCreate(
          &mock_managed_network_configuration_handler_,
          /*was_enterprise_managed_at_startup=*/true);
  ASSERT_TRUE(ephemeral_network_configuration_handler_);

  EXPECT_CALL(mock_managed_network_configuration_handler_,
              RecommendedValuesAreEphemeral())
      .WillRepeatedly(Return(true));

  // Initial policy application
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(1);
  ephemeral_network_configuration_handler_->TriggerPoliciesChangedForTesting(
      /*userhash=*/std::string());

  testing::Mock::VerifyAndClearExpectations(
      &mock_managed_network_configuration_handler_);

  // Wake up from suspend on sign-in screen
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(1);
  fake_power_manager_client_.SendSuspendDone(base::Minutes(10));

  testing::Mock::VerifyAndClearExpectations(
      &mock_managed_network_configuration_handler_);

  // After entering an active session, waking up from suspend doesn't trigger
  // TriggerEphemeralNetworkConfigActions - it should only happen on the sign-in
  // screen.
  LoginState::Get()->SetLoggedInState(
      LoginState::LOGGED_IN_ACTIVE, LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(0);
  fake_power_manager_client_.SendSuspendDone(base::Minutes(10));
}

// When the device wakes up from suspend and a "ephemeral network config"
// (UserCreatedNetworkConfigurationsAreEphemeral)  policy is active,
// TriggerEphemeralNetworkConfigActions is triggered.
TEST_F(EphemeralNetworkConfigurationHandlerTest,
       SuspendDoneReal_Active_Unmanaged) {
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_NONE,
                                      LoginState::LOGGED_IN_USER_NONE);

  ephemeral_network_configuration_handler_ =
      EphemeralNetworkConfigurationHandler::TryCreate(
          &mock_managed_network_configuration_handler_,
          /*was_enterprise_managed_at_startup=*/true);
  ASSERT_TRUE(ephemeral_network_configuration_handler_);

  EXPECT_CALL(mock_managed_network_configuration_handler_,
              UserCreatedNetworkConfigurationsAreEphemeral())
      .WillRepeatedly(Return(true));

  // Initial policy application
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(1);
  ephemeral_network_configuration_handler_->TriggerPoliciesChangedForTesting(
      /*userhash=*/std::string());

  testing::Mock::VerifyAndClearExpectations(
      &mock_managed_network_configuration_handler_);

  // Wake up from suspend on sign-in screen
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(1);
  fake_power_manager_client_.SendSuspendDone(base::Minutes(10));

  testing::Mock::VerifyAndClearExpectations(
      &mock_managed_network_configuration_handler_);

  // After entering an active session, waking up from suspend doesn't trigger
  // TriggerEphemeralNetworkConfigActions - it should only happen on the sign-in
  // screen.
  LoginState::Get()->SetLoggedInState(
      LoginState::LOGGED_IN_ACTIVE, LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(0);
  fake_power_manager_client_.SendSuspendDone(base::Minutes(10));
}

// When a "spurious" (sleep time 0) suspend done event comes,
// TriggerEphemeralNetworkConfigActions is not triggered.
TEST_F(EphemeralNetworkConfigurationHandlerTest, SuspendDoneSpurious_Active) {
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_NONE,
                                      LoginState::LOGGED_IN_USER_NONE);

  ephemeral_network_configuration_handler_ =
      EphemeralNetworkConfigurationHandler::TryCreate(
          &mock_managed_network_configuration_handler_,
          /*was_enterprise_managed_at_startup=*/true);
  ASSERT_TRUE(ephemeral_network_configuration_handler_);

  EXPECT_CALL(mock_managed_network_configuration_handler_,
              RecommendedValuesAreEphemeral())
      .WillRepeatedly(Return(true));
  ephemeral_network_configuration_handler_->TriggerPoliciesChangedForTesting(
      /*userhash=*/std::string());

  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(0);
  fake_power_manager_client_.SendSuspendDone(base::TimeDelta());
}

// When the device wakes up from suspend and no "ephemeral network config"
// policy is active, TriggerEphemeralNetworkConfigActions is not triggered.
TEST_F(EphemeralNetworkConfigurationHandlerTest, SuspendDoneReal_NotActive) {
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_NONE,
                                      LoginState::LOGGED_IN_USER_NONE);

  ephemeral_network_configuration_handler_ =
      EphemeralNetworkConfigurationHandler::TryCreate(
          &mock_managed_network_configuration_handler_,
          /*was_enterprise_managed_at_startup=*/true);
  ASSERT_TRUE(ephemeral_network_configuration_handler_);

  EXPECT_CALL(mock_managed_network_configuration_handler_,
              RecommendedValuesAreEphemeral())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              UserCreatedNetworkConfigurationsAreEphemeral())
      .WillRepeatedly(Return(false));
  ephemeral_network_configuration_handler_->TriggerPoliciesChangedForTesting(
      /*userhash=*/std::string());

  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(0);
  fake_power_manager_client_.SendSuspendDone(base::Minutes(10));
}

// When the device wakes up from suspend and no policy is present yet,
// TriggerEphemeralNetworkConfigActions is not triggered.
TEST_F(EphemeralNetworkConfigurationHandlerTest, SuspendDoneReal_NoPolicy) {
  LoginState::Get()->SetLoggedInState(
      LoginState::LOGGED_IN_ACTIVE, LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);

  ephemeral_network_configuration_handler_ =
      EphemeralNetworkConfigurationHandler::TryCreate(
          &mock_managed_network_configuration_handler_,
          /*was_enterprise_managed_at_startup=*/true);
  ASSERT_TRUE(ephemeral_network_configuration_handler_);

  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(0);
  fake_power_manager_client_.SendSuspendDone(base::Minutes(10));
}

// When the screen is turned off and a "ephemeral network config"
// (RecommendedValuesAreEphemeral) policy is active,
// TriggerEphemeralNetworkConfigActions is triggered.
TEST_F(EphemeralNetworkConfigurationHandlerTest, ScreenIdleState_Active) {
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_NONE,
                                      LoginState::LOGGED_IN_USER_NONE);

  ephemeral_network_configuration_handler_ =
      EphemeralNetworkConfigurationHandler::TryCreate(
          &mock_managed_network_configuration_handler_,
          /*was_enterprise_managed_at_startup=*/true);
  ASSERT_TRUE(ephemeral_network_configuration_handler_);

  EXPECT_CALL(mock_managed_network_configuration_handler_,
              RecommendedValuesAreEphemeral())
      .WillRepeatedly(Return(true));

  // Initial policy application
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(1);
  ephemeral_network_configuration_handler_->TriggerPoliciesChangedForTesting(
      /*userhash=*/std::string());

  testing::Mock::VerifyAndClearExpectations(
      &mock_managed_network_configuration_handler_);

  // Screen turns off due to inactivity.
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(1);
  NotifyScreenIdleOffChanged(/*off=*/true);

  testing::Mock::VerifyAndClearExpectations(
      &mock_managed_network_configuration_handler_);

  // Other notifications (such as turn screen on) don't have an effect.
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(0);

  NotifyScreenIdleOffChanged(/*off=*/false);

  testing::Mock::VerifyAndClearExpectations(
      &mock_managed_network_configuration_handler_);

  // But turning screen off on the sign-in screen has a repeated effect.
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(1);
  NotifyScreenIdleOffChanged(/*off=*/true);

  testing::Mock::VerifyAndClearExpectations(
      &mock_managed_network_configuration_handler_);

  // After entering an active session, the screen turning off does not trigger
  // TriggerEphemeralNetworkConfigActions - it should only happen on the sign-in
  // screen.
  LoginState::Get()->SetLoggedInState(
      LoginState::LOGGED_IN_ACTIVE, LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(0);
  NotifyScreenIdleOffChanged(/*off=*/true);
}

// When the screen is turned off due to inactivity no "ephemeral network config"
// policy is active, TriggerEphemeralNetworkConfigActions is not triggered.
TEST_F(EphemeralNetworkConfigurationHandlerTest, ScreenIdleState_NotActive) {
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_NONE,
                                      LoginState::LOGGED_IN_USER_NONE);

  ephemeral_network_configuration_handler_ =
      EphemeralNetworkConfigurationHandler::TryCreate(
          &mock_managed_network_configuration_handler_,
          /*was_enterprise_managed_at_startup=*/true);
  ASSERT_TRUE(ephemeral_network_configuration_handler_);

  EXPECT_CALL(mock_managed_network_configuration_handler_,
              RecommendedValuesAreEphemeral())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_managed_network_configuration_handler_,
              UserCreatedNetworkConfigurationsAreEphemeral())
      .WillRepeatedly(Return(false));
  ephemeral_network_configuration_handler_->TriggerPoliciesChangedForTesting(
      /*userhash=*/std::string());

  EXPECT_CALL(mock_managed_network_configuration_handler_,
              TriggerEphemeralNetworkConfigActions())
      .Times(0);
  NotifyScreenIdleOffChanged(/*off=*/true);
}

}  // namespace ash
