// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/power_policy_controller.h"

#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class PowerPolicyControllerTest : public testing::Test {
 public:
  PowerPolicyControllerTest()
      : fake_power_client_(new FakePowerManagerClient) {}

  ~PowerPolicyControllerTest() override = default;

  void SetUp() override {
    PowerPolicyController::Initialize(fake_power_client_.get());
    ASSERT_TRUE(PowerPolicyController::IsInitialized());
    policy_controller_ = PowerPolicyController::Get();
  }

  void TearDown() override {
    if (PowerPolicyController::IsInitialized())
      PowerPolicyController::Shutdown();
  }

 protected:
  std::unique_ptr<FakePowerManagerClient> fake_power_client_;
  PowerPolicyController* policy_controller_;
  base::MessageLoop message_loop_;
};

TEST_F(PowerPolicyControllerTest, Prefs) {
  PowerPolicyController::PrefValues prefs;
  prefs.ac_screen_dim_delay_ms = 600000;
  prefs.ac_screen_off_delay_ms = 660000;
  prefs.ac_idle_delay_ms = 720000;
  prefs.battery_screen_dim_delay_ms = 300000;
  prefs.battery_screen_off_delay_ms = 360000;
  prefs.battery_idle_delay_ms = 420000;
  prefs.ac_idle_action = PowerPolicyController::ACTION_SUSPEND;
  prefs.battery_idle_action = PowerPolicyController::ACTION_STOP_SESSION;
  prefs.lid_closed_action = PowerPolicyController::ACTION_SHUT_DOWN;
  prefs.use_audio_activity = true;
  prefs.use_video_activity = true;
  prefs.ac_brightness_percent = 87.0;
  prefs.battery_brightness_percent = 43.0;
  prefs.enable_auto_screen_lock = false;
  prefs.presentation_screen_dim_delay_factor = 3.0;
  prefs.user_activity_screen_dim_delay_factor = 2.0;
  prefs.wait_for_initial_user_activity = true;
  prefs.force_nonzero_brightness_for_user_activity = false;
  policy_controller_->ApplyPrefs(prefs);

  power_manager::PowerManagementPolicy expected_policy;
  expected_policy.mutable_ac_delays()->set_screen_dim_ms(600000);
  expected_policy.mutable_ac_delays()->set_screen_off_ms(660000);
  expected_policy.mutable_ac_delays()->set_screen_lock_ms(-1);
  expected_policy.mutable_ac_delays()->set_idle_warning_ms(-1);
  expected_policy.mutable_ac_delays()->set_idle_ms(720000);
  expected_policy.mutable_battery_delays()->set_screen_dim_ms(300000);
  expected_policy.mutable_battery_delays()->set_screen_off_ms(360000);
  expected_policy.mutable_battery_delays()->set_screen_lock_ms(-1);
  expected_policy.mutable_battery_delays()->set_idle_warning_ms(-1);
  expected_policy.mutable_battery_delays()->set_idle_ms(420000);
  expected_policy.set_ac_idle_action(
      power_manager::PowerManagementPolicy_Action_SUSPEND);
  expected_policy.set_battery_idle_action(
      power_manager::PowerManagementPolicy_Action_STOP_SESSION);
  expected_policy.set_lid_closed_action(
      power_manager::PowerManagementPolicy_Action_SHUT_DOWN);
  expected_policy.set_use_audio_activity(true);
  expected_policy.set_use_video_activity(true);
  expected_policy.set_ac_brightness_percent(87.0);
  expected_policy.set_battery_brightness_percent(43.0);
  expected_policy.set_presentation_screen_dim_delay_factor(3.0);
  expected_policy.set_user_activity_screen_dim_delay_factor(2.0);
  expected_policy.set_wait_for_initial_user_activity(true);
  expected_policy.set_force_nonzero_brightness_for_user_activity(false);
  expected_policy.set_reason(PowerPolicyController::kPrefsReason);
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));

  // Change some prefs and check that an updated policy is sent.
  prefs.ac_idle_warning_delay_ms = 700000;
  prefs.battery_idle_warning_delay_ms = 400000;
  prefs.lid_closed_action = PowerPolicyController::ACTION_SUSPEND;
  prefs.ac_brightness_percent = -1.0;
  prefs.force_nonzero_brightness_for_user_activity = true;
  policy_controller_->ApplyPrefs(prefs);
  expected_policy.mutable_ac_delays()->set_idle_warning_ms(700000);
  expected_policy.mutable_battery_delays()->set_idle_warning_ms(400000);
  expected_policy.set_lid_closed_action(
      power_manager::PowerManagementPolicy_Action_SUSPEND);
  expected_policy.clear_ac_brightness_percent();
  expected_policy.set_force_nonzero_brightness_for_user_activity(true);
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));

  // The enable-auto-screen-lock pref should force the screen-lock delays to
  // match the screen-off delays plus a constant value.
  prefs.enable_auto_screen_lock = true;
  policy_controller_->ApplyPrefs(prefs);
  expected_policy.mutable_ac_delays()->set_screen_lock_ms(
      660000 + PowerPolicyController::kScreenLockAfterOffDelayMs);
  expected_policy.mutable_battery_delays()->set_screen_lock_ms(
      360000 + PowerPolicyController::kScreenLockAfterOffDelayMs);
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));

  // If the screen-lock-delay prefs are set to lower values than the
  // screen-off delays plus the constant, the lock prefs should take
  // precedence.
  prefs.ac_screen_lock_delay_ms = 70000;
  prefs.battery_screen_lock_delay_ms = 60000;
  policy_controller_->ApplyPrefs(prefs);
  expected_policy.mutable_ac_delays()->set_screen_lock_ms(70000);
  expected_policy.mutable_battery_delays()->set_screen_lock_ms(60000);
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));

  // If the artificial screen-lock delays would exceed the idle delay, they
  // shouldn't be set -- the power manager would ignore them since the
  // idle action should lock the screen in this case.
  prefs.ac_screen_off_delay_ms = prefs.ac_idle_delay_ms - 1;
  prefs.battery_screen_off_delay_ms = prefs.battery_idle_delay_ms - 1;
  prefs.ac_screen_lock_delay_ms = -1;
  prefs.battery_screen_lock_delay_ms = -1;
  policy_controller_->ApplyPrefs(prefs);
  expected_policy.mutable_ac_delays()->set_screen_off_ms(
      prefs.ac_screen_off_delay_ms);
  expected_policy.mutable_battery_delays()->set_screen_off_ms(
      prefs.battery_screen_off_delay_ms);
  expected_policy.mutable_ac_delays()->set_screen_lock_ms(-1);
  expected_policy.mutable_battery_delays()->set_screen_lock_ms(-1);
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));

  // Set the "allow screen wake locks" pref to false and add a screen wake lock.
  // It should be downgraded to a system wake lock, and the pref-supplied delays
  // should be left untouched.
  prefs.allow_screen_wake_locks = false;
  policy_controller_->ApplyPrefs(prefs);
  policy_controller_->AddScreenWakeLock(PowerPolicyController::REASON_OTHER,
                                        "Screen");
  expected_policy.set_system_wake_lock(true);
  expected_policy.set_reason(std::string(PowerPolicyController::kPrefsReason) +
                             ", Screen");
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));

  // Set the "allow wake locks" pref to false and add a screen wake lock.
  // It should be ignored.
  prefs.allow_wake_locks = false;
  policy_controller_->ApplyPrefs(prefs);
  policy_controller_->AddScreenWakeLock(PowerPolicyController::REASON_OTHER,
                                        "Screen");
  expected_policy.clear_system_wake_lock();
  expected_policy.set_reason(std::string(PowerPolicyController::kPrefsReason));
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));
}

TEST_F(PowerPolicyControllerTest, SystemWakeLock) {
  policy_controller_->AddSystemWakeLock(PowerPolicyController::REASON_OTHER,
                                        "1");
  power_manager::PowerManagementPolicy expected_policy;
  expected_policy.set_system_wake_lock(true);
  expected_policy.set_reason("1");
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));
}

TEST_F(PowerPolicyControllerTest, DimWakeLock) {
  policy_controller_->AddDimWakeLock(PowerPolicyController::REASON_OTHER, "1");
  power_manager::PowerManagementPolicy expected_policy;
  expected_policy.set_dim_wake_lock(true);
  expected_policy.set_reason("1");
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));
}

TEST_F(PowerPolicyControllerTest, ScreenWakeLock) {
  policy_controller_->AddScreenWakeLock(PowerPolicyController::REASON_OTHER,
                                        "1");
  power_manager::PowerManagementPolicy expected_policy;
  expected_policy.set_screen_wake_lock(true);
  expected_policy.set_reason("1");
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));
}

TEST_F(PowerPolicyControllerTest, IgnoreMediaWakeLocksWhenRequested) {
  PowerPolicyController::PrefValues prefs;
  policy_controller_->ApplyPrefs(prefs);
  const power_manager::PowerManagementPolicy kDefaultPolicy =
      fake_power_client_->policy();

  // Wake locks created for audio or video playback should be ignored when the
  // |use_audio_activity| or |use_video_activity| prefs are unset.
  prefs.use_audio_activity = false;
  prefs.use_video_activity = false;
  policy_controller_->ApplyPrefs(prefs);

  const int audio_id = policy_controller_->AddSystemWakeLock(
      PowerPolicyController::REASON_AUDIO_PLAYBACK, "audio");
  const int video_id = policy_controller_->AddScreenWakeLock(
      PowerPolicyController::REASON_VIDEO_PLAYBACK, "video");

  power_manager::PowerManagementPolicy expected_policy = kDefaultPolicy;
  expected_policy.set_use_audio_activity(false);
  expected_policy.set_use_video_activity(false);
  expected_policy.set_reason(PowerPolicyController::kPrefsReason);
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));

  // Non-media screen wake locks should still be honored.
  const int other_id = policy_controller_->AddScreenWakeLock(
      PowerPolicyController::REASON_OTHER, "other");

  expected_policy.set_screen_wake_lock(true);
  expected_policy.set_reason(std::string(PowerPolicyController::kPrefsReason) +
                             ", other");
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));

  // Start honoring audio activity and check that the audio wake lock is used.
  policy_controller_->RemoveWakeLock(other_id);
  prefs.use_audio_activity = true;
  policy_controller_->ApplyPrefs(prefs);

  expected_policy = kDefaultPolicy;
  expected_policy.set_use_video_activity(false);
  expected_policy.set_system_wake_lock(true);
  expected_policy.set_reason(std::string(PowerPolicyController::kPrefsReason) +
                             ", audio");
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));

  // Now honor video activity as well.
  prefs.use_video_activity = true;
  policy_controller_->ApplyPrefs(prefs);

  expected_policy = kDefaultPolicy;
  expected_policy.set_screen_wake_lock(true);
  expected_policy.set_system_wake_lock(true);
  expected_policy.set_reason(std::string(PowerPolicyController::kPrefsReason) +
                             ", audio, video");
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));

  policy_controller_->RemoveWakeLock(audio_id);
  policy_controller_->RemoveWakeLock(video_id);
}

TEST_F(PowerPolicyControllerTest, AvoidSendingEmptyPolicies) {
  // Check that empty policies aren't sent when PowerPolicyController is created
  // or destroyed.
  EXPECT_EQ(0, fake_power_client_->num_set_policy_calls());
  PowerPolicyController::Shutdown();
  EXPECT_EQ(0, fake_power_client_->num_set_policy_calls());
}

TEST_F(PowerPolicyControllerTest, DoNothingOnLidClosedWhileSigningOut) {
  PowerPolicyController::PrefValues prefs;
  policy_controller_->ApplyPrefs(prefs);
  const power_manager::PowerManagementPolicy kDefaultPolicy =
      fake_power_client_->policy();

  prefs.lid_closed_action = PowerPolicyController::ACTION_SHUT_DOWN;
  policy_controller_->ApplyPrefs(prefs);

  power_manager::PowerManagementPolicy expected_policy;
  expected_policy = kDefaultPolicy;
  expected_policy.set_lid_closed_action(
      power_manager::PowerManagementPolicy_Action_SHUT_DOWN);
  // Sanity check.
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));

  policy_controller_->NotifyChromeIsExiting();

  expected_policy.set_lid_closed_action(
      power_manager::PowerManagementPolicy_Action_DO_NOTHING);
  // Lid-closed action successfully changed to "do nothing".
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));
}

TEST_F(PowerPolicyControllerTest, SuspendOnLidClosedWhileSignedOut) {
  PowerPolicyController::PrefValues prefs;
  policy_controller_->ApplyPrefs(prefs);
  const power_manager::PowerManagementPolicy kDefaultPolicy =
      fake_power_client_->policy();

  prefs.lid_closed_action = PowerPolicyController::ACTION_SHUT_DOWN;
  policy_controller_->ApplyPrefs(prefs);

  power_manager::PowerManagementPolicy expected_policy;
  expected_policy = kDefaultPolicy;
  expected_policy.set_lid_closed_action(
      power_manager::PowerManagementPolicy_Action_SHUT_DOWN);
  // Sanity check
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));

  policy_controller_->SetEncryptionMigrationActive(true);
  expected_policy.set_lid_closed_action(
      power_manager::PowerManagementPolicy_Action_SUSPEND);
  expected_policy.set_reason("Prefs, encryption migration");
  // Lid-closed action successfully changed to "suspend".
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));
}

TEST_F(PowerPolicyControllerTest, SmartDimEnabledExperimentEnabled) {
  const std::map<std::string, std::string> params = {
      {"dim_threshold", "0.651"}};
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      chromeos::features::kUserActivityPrediction, params);

  PowerPolicyController::PrefValues prefs;
  policy_controller_->ApplyPrefs(prefs);
  const power_manager::PowerManagementPolicy kDefaultPolicy =
      fake_power_client_->policy();

  // First disable smart dim model.
  prefs.smart_dim_enabled = false;
  prefs.presentation_screen_dim_delay_factor = 3.0;
  prefs.user_activity_screen_dim_delay_factor = 2.0;
  policy_controller_->ApplyPrefs(prefs);

  power_manager::PowerManagementPolicy expected_policy = kDefaultPolicy;
  expected_policy.set_presentation_screen_dim_delay_factor(3.0);
  expected_policy.set_user_activity_screen_dim_delay_factor(2.0);
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));

  // Then enable smart dim model.
  prefs.smart_dim_enabled = true;
  policy_controller_->ApplyPrefs(prefs);

  expected_policy.set_presentation_screen_dim_delay_factor(1.0);
  expected_policy.set_user_activity_screen_dim_delay_factor(1.0);
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(expected_policy),
            PowerPolicyController::GetPolicyDebugString(
                fake_power_client_->policy()));
}

TEST_F(PowerPolicyControllerTest, PerSessionScreenBrightnessOverride) {
  const double kAcBrightness = 99.0;
  const double kBatteryBrightness = 77.0;

  PowerPolicyController::PrefValues prefs;
  prefs.ac_brightness_percent = kAcBrightness;
  prefs.battery_brightness_percent = kBatteryBrightness;
  policy_controller_->ApplyPrefs(prefs);

  EXPECT_EQ(kAcBrightness,
            fake_power_client_->policy().ac_brightness_percent());
  EXPECT_EQ(kBatteryBrightness,
            fake_power_client_->policy().battery_brightness_percent());

  // Simulate model triggered brightness change - shouldn't override the policy.
  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(80.0);
  request.set_cause(power_manager::SetBacklightBrightnessRequest_Cause_MODEL);
  fake_power_client_->SetScreenBrightness(request);
  base::RunLoop().RunUntilIdle();
  policy_controller_->ApplyPrefs(prefs);

  EXPECT_EQ(kAcBrightness,
            fake_power_client_->policy().ac_brightness_percent());
  EXPECT_EQ(kBatteryBrightness,
            fake_power_client_->policy().battery_brightness_percent());

  // Simulate user triggered brightness change - should override the policy.
  request.set_percent(80.0);
  request.set_cause(
      power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST);
  fake_power_client_->SetScreenBrightness(request);
  base::RunLoop().RunUntilIdle();
  policy_controller_->ApplyPrefs(prefs);

  EXPECT_FALSE(fake_power_client_->policy().has_ac_brightness_percent());
  EXPECT_FALSE(fake_power_client_->policy().has_battery_brightness_percent());

  // Simulate policy values update that should be ignored.
  prefs.ac_brightness_percent = 98.0;
  prefs.battery_brightness_percent = 76.0;
  policy_controller_->ApplyPrefs(prefs);

  EXPECT_FALSE(fake_power_client_->policy().has_ac_brightness_percent());
  EXPECT_FALSE(fake_power_client_->policy().has_battery_brightness_percent());
}

}  // namespace chromeos
