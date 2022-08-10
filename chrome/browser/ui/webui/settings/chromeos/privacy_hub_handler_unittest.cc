// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/privacy_hub_handler.h"

#include "base/containers/adapters.h"
#include "base/test/task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::settings {

namespace {
class TestPrivacyHubHandler : public PrivacyHubHandler {
 public:
  using content::WebUIMessageHandler::set_web_ui;

  using PrivacyHubHandler::HandleInitialCameraSwitchState;
  using PrivacyHubHandler::HandleInitialMicrophoneSwitchState;
  using PrivacyHubHandler::OnCameraHWPrivacySwitchStatusChanged;
};

using cps = cros::mojom::CameraPrivacySwitchState;
}  // namespace

class PrivacyHubHandlerTest : public testing::Test {
  // This has to go before privacy_hub_handler_ because in the
  // CameraHalDispatcherImpl constructor a call to
  // base::SequencedTaskRunnerHandle::Get() is made which requires a
  // task_environment. Initialization order of the members takes care
  // of providing this before privacy_hub_handler_ is constructed.
  base::test::SingleThreadTaskEnvironment task_environment_;

  // Has to go before privacy_hub_handler_ as it references its
  // address and destruction order guarantees no invalid pointers.
  content::TestWebUI web_ui_;

 public:
  PrivacyHubHandlerTest()
      : this_test_name_(
            testing::UnitTest::GetInstance()->current_test_info()->name()) {
    privacy_hub_handler_.set_web_ui(&web_ui_);
    privacy_hub_handler_.AllowJavascriptForTesting();
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
        if (&arg != data->arg1())
          return arg.Clone();
      }
    }

    ADD_FAILURE() << "None of the " << web_ui_.call_data().size()
                  << " CallData objects matched for function '" << function_name
                  << "' and callback '" << callback_name
                  << "' with a valid arg";
    return base::Value();
  }
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

class PrivacyHubHandlerCameraTest : public PrivacyHubHandlerTest,
                                    public testing::WithParamInterface<cps> {
 public:
  void ExpectValueMatchesEnumParam(const base::Value& value) const {
    if (GetParam() == cps::UNKNOWN) {
      EXPECT_TRUE(value.is_none());
    } else {
      ExpectValueMatchesBoolParam(GetParam() == cps::ON, value);
    }
  }
};

TEST_P(PrivacyHubHandlerCameraTest, CameraHardwarePrivacySwitchChanged) {
  privacy_hub_handler_.OnCameraHWPrivacySwitchStatusChanged(/*camera_id=*/0,
                                                            GetParam());

  const base::Value data =
      GetLastWebUIListenerData("camera-hardware-toggle-changed");

  ExpectValueMatchesEnumParam(data);
}

INSTANTIATE_TEST_SUITE_P(HardwareSwitchStates,
                         PrivacyHubHandlerCameraTest,
                         testing::Values(cps::ON, cps::OFF, cps::UNKNOWN),
                         testing::PrintToStringParamName());

TEST_F(PrivacyHubHandlerCameraTest, HandleInitialCameraSwitchState) {
  base::Value::List args;
  args.Append(this_test_name_);

  privacy_hub_handler_.HandleInitialCameraSwitchState(args);

  const base::Value data = GetLastWebUIResponse(this_test_name_);

  // The initial state is always UNKNOWN which is communicated as NONE
  EXPECT_TRUE(data.is_none());
}

TEST_P(PrivacyHubHandlerMicrophoneTest,
       MicrophoneHardwarePrivacySwitchChanged) {
  SetParamValueMicrophoneMute();

  const base::Value data =
      GetLastWebUIListenerData("microphone-hardware-toggle-changed");

  ExpectValueMatchesBoolParam(data);
}

TEST_P(PrivacyHubHandlerMicrophoneTest, HandleInitialMicrophoneSwitchState) {
  SetParamValueMicrophoneMute();

  base::Value::List args;
  args.Append(this_test_name_);

  privacy_hub_handler_.HandleInitialMicrophoneSwitchState(args);

  const base::Value data = GetLastWebUIResponse(this_test_name_);

  ExpectValueMatchesBoolParam(data);
}

INSTANTIATE_TEST_SUITE_P(HardwareSwitchStates,
                         PrivacyHubHandlerMicrophoneTest,
                         testing::Values(true, false),
                         testing::PrintToStringParamName());

#if DCHECK_IS_ON()
using PrivacyHubHandlerDeathTest = PrivacyHubHandlerTest;

TEST_F(PrivacyHubHandlerDeathTest, HandleInitialCameraSwitchStateNoCallbackId) {
  base::Value::List args;

  EXPECT_DEATH(privacy_hub_handler_.HandleInitialCameraSwitchState(args),
               ".*Callback ID is required.*");
}

TEST_F(PrivacyHubHandlerDeathTest, HandleInitialCameraSwitchStateWithArgs) {
  base::Value::List args;
  args.Append(this_test_name_);
  args.Append(base::Value());

  EXPECT_DEATH(privacy_hub_handler_.HandleInitialCameraSwitchState(args),
               ".*Did not expect arguments.*");
}

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
#endif
}  // namespace chromeos::settings
