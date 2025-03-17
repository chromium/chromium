// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/privacy/privacy_hub_handler.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/adapters.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
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
};
}  // namespace

class PrivacyHubHandlerTest : public AshTestBase {
 public:
  PrivacyHubHandlerTest()
      : this_test_name_(
            testing::UnitTest::GetInstance()->current_test_info()->name()) {
    feature_list_.InitWithFeatures({features::kCrosPrivacyHub}, {});
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    privacy_hub_handler_ = std::make_unique<TestPrivacyHubHandler>();
    privacy_hub_handler_->set_web_ui(&web_ui_);
    privacy_hub_handler_->AllowJavascriptForTesting();
  }

  void TearDown() override {
    privacy_hub_handler_.reset();
    AshTestBase::TearDown();
  }

  [[nodiscard]] base::Value GetLastWebUIListenerData(
      const std::string& callback) const {
    return GetLastWebUIData("cr.webUIListenerCallback", callback);
  }

  [[nodiscard]] base::Value GetLastWebUIResponse(
      const std::string& callback) const {
    return GetLastWebUIData("cr.webUIResponse", callback);
  }

  std::unique_ptr<TestPrivacyHubHandler> privacy_hub_handler_;
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

  content::TestWebUI web_ui_;
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
  privacy_hub_handler_->MicrophoneHardwareToggleChanged(GetParam());

  const base::Value data =
      GetLastWebUIListenerData("microphone-hardware-toggle-changed");

  ExpectValueMatchesBoolParam(data);
}

TEST_P(PrivacyHubHandlerMicrophoneTest, HandleInitialMicrophoneSwitchState) {
  SetParamValueMicrophoneMute();

  privacy_hub_handler_->HandleInitialMicrophoneSwitchState(
      base::Value::List().Append(this_test_name_));

  const base::Value data = GetLastWebUIResponse(this_test_name_);

  ExpectValueMatchesBoolParam(data);
}

TEST_P(PrivacyHubHandlerCameraSwitchTest,
       ForceDisableCameraSwitchSwitchChanged) {
  privacy_hub_handler_->SetForceDisableCameraSwitch(GetParam());

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

  privacy_hub_handler_->HandleInitialCameraLedFallbackState(args);

  const base::Value data = GetLastWebUIResponse(this_test_name_);

  ExpectValueMatchesBoolParam(data);
}

INSTANTIATE_TEST_SUITE_P(CameraLedFallback,
                         PrivacyHubHandlerCameraLedFallbackTest,
                         testing::Values(true, false),
                         testing::PrintToStringParamName());

TEST_F(PrivacyHubHandlerTest, MicrophoneMutedBySecurityCurtainChanged) {
  privacy_hub_handler_->OnInputMutedBySecurityCurtainChanged(true);

  ExpectValueMatchesBoolParam(
      true,
      GetLastWebUIListenerData("microphone-muted-by-security-curtain-changed"));

  privacy_hub_handler_->OnInputMutedBySecurityCurtainChanged(false);

  ExpectValueMatchesBoolParam(
      false,
      GetLastWebUIListenerData("microphone-muted-by-security-curtain-changed"));
}

#if DCHECK_IS_ON()
using PrivacyHubHandlerDeathTest = PrivacyHubHandlerTest;

TEST_F(PrivacyHubHandlerDeathTest,
       HandleInitialMicrophoneSwitchStateNoCallbackId) {
  base::Value::List args;

  EXPECT_DEATH(privacy_hub_handler_->HandleInitialMicrophoneSwitchState(args),
               ".*");
}

TEST_F(PrivacyHubHandlerDeathTest, HandleInitialMicrophoneSwitchStateWithArgs) {
  base::Value::List args;
  args.Append(this_test_name_);
  args.Append(base::Value());

  EXPECT_DEATH(privacy_hub_handler_->HandleInitialMicrophoneSwitchState(args),
               ".*");
}

#endif

}  // namespace ash::settings
