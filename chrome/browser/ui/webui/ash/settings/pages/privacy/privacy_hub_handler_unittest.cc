// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/privacy/privacy_hub_handler.h"

#include "ash/constants/ash_features.h"
#include "base/containers/adapters.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_hats_trigger.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

namespace {
class TestPrivacyHubHandler : public PrivacyHubHandler {
 public:
  using content::WebUIMessageHandler::set_web_ui;

  using PrivacyHubHandler::HandleInitialCameraLedFallbackState;
  using PrivacyHubHandler::HandleInitialMicrophoneSwitchState;
  using PrivacyHubHandler::HandlePrivacyPageClosed;
  using PrivacyHubHandler::HandlePrivacyPageOpened;
};

using cps = cros::mojom::CameraPrivacySwitchState;
}  // namespace

class PrivacyHubHandlerTest : public testing::Test {
  // This has to go before privacy_hub_handler_ because in the
  // CameraHalDispatcherImpl constructor a call to
  // base::SequencedTaskRunner::GetCurrentDefault() is made which requires a
  // task_environment. Initialization order of the members takes care
  // of providing this before privacy_hub_handler_ is constructed.
  base::test::SingleThreadTaskEnvironment task_environment_;

  // This has to go before privacy_hub_handler_ because PrivacyHubHandler
  // constructor  requires CrasAudioHandler to be initialized.
  // ScopedCrasAudioHandlerForTesting is a helper class that initializes
  // CrasAudioHandler in it's constructor.
  ScopedCrasAudioHandlerForTesting cras_audio_handler_;

  // Has to go before privacy_hub_handler_ as it references its
  // address and destruction order guarantees no invalid pointers.
  content::TestWebUI web_ui_;

 public:
  PrivacyHubHandlerTest()
      : this_test_name_(
            testing::UnitTest::GetInstance()->current_test_info()->name()) {
    privacy_hub_handler_.set_web_ui(&web_ui_);
    privacy_hub_handler_.AllowJavascriptForTesting();

    feature_list_.InitWithFeatures({features::kCrosPrivacyHub}, {});
  }

  [[nodiscard]] base::Value GetLastWebUIListenerData(
      const std::string& callback) const {
    return GetLastWebUIData("cr.webUIListenerCallback", callback);
  }

  [[nodiscard]] base::Value GetLastWebUIResponse(
      const std::string& callback) const {
    return GetLastWebUIData("cr.webUIResponse", callback);
  }

  TestPrivacyHubHandler privacy_hub_handler_;
  const std::string this_test_name_;

 protected:
  void ExpectValueMatchesBoolParam(bool param, const base::Value& value) const {
    ASSERT_TRUE(value.is_bool());
    EXPECT_EQ(param, value.GetBool());
  }

 private:
  [[nodiscard]] base::Value GetLastWebUIData(
      const std::string& function_name,
      const std::string& callback_name) const {
    for (const auto& data : base::Reversed(web_ui_.call_data())) {
      const std::string* name = data->arg1()->GetIfString();

      if (data->function_name() != function_name || !name ||
          *name != callback_name) {
        continue;
      }

      // Assume that the data is stored in the last valid arg.
      for (const auto& arg : base::Reversed(data->args())) {
        if (&arg != data->arg1()) {
          return arg.Clone();
        }
      }
    }

    ADD_FAILURE() << "None of the " << web_ui_.call_data().size()
                  << " CallData objects matched for function '" << function_name
                  << "' and callback '" << callback_name
                  << "' with a valid arg";
    return base::Value();
  }

  base::test::ScopedFeatureList feature_list_;
};

class PrivacyHubHandlerMicrophoneTest
    : public PrivacyHubHandlerTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetParamValueMicrophoneMute() const {
    // Have to set it to not-param once before to ensure the observers are
    // triggered from MicrophoneMuteSwitchMonitor.
    ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(
        !GetParam());
    ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(
        GetParam());
  }

  void ExpectValueMatchesBoolParam(const base::Value& value) const {
    PrivacyHubHandlerTest::ExpectValueMatchesBoolParam(GetParam(), value);
  }
};

class PrivacyHubHandlerCameraLedFallbackTest
    : public PrivacyHubHandlerTest,
      public testing::WithParamInterface<bool> {
 public:
  PrivacyHubHandlerCameraLedFallbackTest()
      : scoped_camera_led_fallback_(GetParam()) {}

  void ExpectValueMatchesBoolParam(const base::Value& value) const {
    PrivacyHubHandlerTest::ExpectValueMatchesBoolParam(GetParam(), value);
  }

 private:
  privacy_hub_util::ScopedCameraLedFallbackForTesting
      scoped_camera_led_fallback_;
};

class PrivacyHubHandlerHatsTest : public PrivacyHubHandlerTest {
 public:
  PrivacyHubHandlerHatsTest() {
    feature_list_.InitAndEnableFeature(
        ::features::kHappinessTrackingPrivacyHubPostLaunch);
  }

  bool IsTimerStarted() {
    return PrivacyHubHatsTrigger::Get().GetTimerForTesting().IsRunning();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class PrivacyHubHandlerCameraSwitchTest
    : public PrivacyHubHandlerTest,
      public testing::WithParamInterface<bool> {
 public:
  void ExpectValueMatchesBoolParam(const base::Value& value) const {
    PrivacyHubHandlerTest::ExpectValueMatchesBoolParam(GetParam(), value);
  }
};

TEST_P(PrivacyHubHandlerMicrophoneTest,
       MicrophoneHardwarePrivacySwitchChanged) {
  privacy_hub_handler_.MicrophoneHardwareToggleChanged(GetParam());

  const base::Value data =
      GetLastWebUIListenerData("microphone-hardware-toggle-changed");

  ExpectValueMatchesBoolParam(data);
}

TEST_P(PrivacyHubHandlerMicrophoneTest, HandleInitialMicrophoneSwitchState) {
  SetParamValueMicrophoneMute();

  privacy_hub_handler_.HandleInitialMicrophoneSwitchState(
      base::Value::List().Append(this_test_name_));

  const base::Value data = GetLastWebUIResponse(this_test_name_);

  ExpectValueMatchesBoolParam(data);
}

TEST_P(PrivacyHubHandlerCameraSwitchTest,
       ForceDisableCameraSwitchSwitchChanged) {
  privacy_hub_handler_.SetForceDisableCameraSwitch(GetParam());

  const base::Value data =
      GetLastWebUIListenerData("force-disable-camera-switch");

  ExpectValueMatchesBoolParam(data);
}

INSTANTIATE_TEST_SUITE_P(HardwareSwitchStates,
                         PrivacyHubHandlerMicrophoneTest,
                         testing::Values(true, false),
                         testing::PrintToStringParamName());

INSTANTIATE_TEST_SUITE_P(HardwareSwitchStates,
                         PrivacyHubHandlerCameraSwitchTest,
                         testing::Values(true, false),
                         testing::PrintToStringParamName());

TEST_P(PrivacyHubHandlerCameraLedFallbackTest,
       HandleInitialCameraLedCallbackState) {
  base::Value::List args;
  args.Append(this_test_name_);

  privacy_hub_handler_.HandleInitialCameraLedFallbackState(args);

  const base::Value data = GetLastWebUIResponse(this_test_name_);

  ExpectValueMatchesBoolParam(data);
}

INSTANTIATE_TEST_SUITE_P(CameraLedFallback,
                         PrivacyHubHandlerCameraLedFallbackTest,
                         testing::Values(true, false),
                         testing::PrintToStringParamName());

TEST_F(PrivacyHubHandlerHatsTest, OnlyTriggerHatsIfPageWasVisitedLongEnough) {
  const base::Value::List args;

  EXPECT_FALSE(IsTimerStarted());

  // We trigger the HaTS survey on the leave event but the user hasn't visited
  // the page yet.
  privacy_hub_handler_.HandlePrivacyPageClosed(args);
  EXPECT_FALSE(IsTimerStarted());

  // User goes to the page.
  privacy_hub_handler_.HandlePrivacyPageOpened(args);
  EXPECT_FALSE(IsTimerStarted());

  // Simulate the user stays on the page for 5 seconds.
  privacy_hub_handler_.SetPrivacyPageOpenedTimeStampForTesting(
      base::TimeTicks::Now() - base::Seconds(5));
  EXPECT_FALSE(IsTimerStarted());

  // And leaves it again, now the survey should be triggered.
  privacy_hub_handler_.HandlePrivacyPageClosed(args);
  EXPECT_TRUE(IsTimerStarted());
}

TEST_F(PrivacyHubHandlerHatsTest, DontTriggerHatsIfUserLeftEarly) {
  const base::Value::List args;

  EXPECT_FALSE(IsTimerStarted());

  // We trigger the HaTS survey on the leave event but the user hasn't visited
  // the page yet.
  privacy_hub_handler_.HandlePrivacyPageClosed(args);
  EXPECT_FALSE(IsTimerStarted());

  // User goes to the page.
  privacy_hub_handler_.HandlePrivacyPageOpened(args);
  EXPECT_FALSE(IsTimerStarted());

  // And leaves it again immediately, now the survey shouldn't be triggered.
  privacy_hub_handler_.HandlePrivacyPageClosed(args);
  EXPECT_FALSE(IsTimerStarted());
}

TEST_F(PrivacyHubHandlerTest, MicrophoneMutedBySecurityCurtainChanged) {
  privacy_hub_handler_.OnInputMutedBySecurityCurtainChanged(true);

  ExpectValueMatchesBoolParam(
      true,
      GetLastWebUIListenerData("microphone-muted-by-security-curtain-changed"));

  privacy_hub_handler_.OnInputMutedBySecurityCurtainChanged(false);

  ExpectValueMatchesBoolParam(
      false,
      GetLastWebUIListenerData("microphone-muted-by-security-curtain-changed"));
}

#if DCHECK_IS_ON()
using PrivacyHubHandlerDeathTest = PrivacyHubHandlerTest;

TEST_F(PrivacyHubHandlerDeathTest,
       HandleInitialMicrophoneSwitchStateNoCallbackId) {
  base::Value::List args;

  EXPECT_DEATH(privacy_hub_handler_.HandleInitialMicrophoneSwitchState(args),
               ".*Callback ID is required.*");
}

TEST_F(PrivacyHubHandlerDeathTest, HandleInitialMicrophoneSwitchStateWithArgs) {
  base::Value::List args;
  args.Append(this_test_name_);
  args.Append(base::Value());

  EXPECT_DEATH(privacy_hub_handler_.HandleInitialMicrophoneSwitchState(args),
               ".*Did not expect arguments.*");
}

TEST_F(PrivacyHubHandlerDeathTest, HandlePrivacyPageOpened) {
  base::Value::List args;
  args.Append(this_test_name_);

  EXPECT_DEATH(privacy_hub_handler_.HandlePrivacyPageOpened(args), ".*empty.*");
}

TEST_F(PrivacyHubHandlerDeathTest, HandlePrivacyPageClosed) {
  base::Value::List args;
  args.Append(this_test_name_);

  EXPECT_DEATH(privacy_hub_handler_.HandlePrivacyPageClosed(args), ".*empty.*");
}

TEST_F(PrivacyHubHandlerDeathTest, OnlyTriggerHatsIfFeatureIsEnabled) {
  const base::Value::List args;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      ::features::kHappinessTrackingPrivacyHubPostLaunch);

  // User goes to the page.
  EXPECT_DEATH(privacy_hub_handler_.HandlePrivacyPageOpened(args),
               "base::FeatureList::IsEnabled");
  EXPECT_DEATH(privacy_hub_handler_.HandlePrivacyPageClosed(args),
               "base::FeatureList::IsEnabled");
}

#endif

}  // namespace ash::settings
