// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/about/device_name_handler.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/device_name/fake_device_name_store.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

class TestDeviceNameHandler : public DeviceNameHandler {
 public:
  explicit TestDeviceNameHandler(content::WebUI* web_ui,
                                 DeviceNameStore* fake_device_name_store)
      : DeviceNameHandler(fake_device_name_store) {
    set_web_ui(web_ui);
  }

  // Raise visibility to public.
  using DeviceNameHandler::HandleAttemptSetDeviceName;
  using DeviceNameHandler::HandleNotifyReadyForDeviceName;
};

class DeviceNameHandlerTest : public testing::Test {
 public:
  DeviceNameHandlerTest() = default;

  void SetUp() override {
    testing::Test::SetUp();

    handler_ = std::make_unique<TestDeviceNameHandler>(
        web_ui(), &fake_device_name_store_);

    web_ui()->ClearTrackedCalls();
    feature_list_.InitAndEnableFeature(ash::features::kEnableHostnameSetting);
  }

  void CallHandleNotifyReadyForDeviceName() {
    // Need to call HandleNotifyReadyForDeviceName() once for handler to start
    // listening to changes in device name metadata.
    base::Value::List args;
    args.Append("callback-id");
    handler()->HandleNotifyReadyForDeviceName(args);

    // On notifying, device name metadata should be received and be equal to the
    // default values.
    VerifyDeviceNameMetadata(FakeDeviceNameStore::kDefaultDeviceName,
                             DeviceNameStore::DeviceNameState::kCanBeModified);
  }

  void TearDown() override { FakeDeviceNameStore::Shutdown(); }

  void VerifyDeviceNameMetadata(
      const std::string& expected_device_name,
      DeviceNameStore::DeviceNameState expected_device_name_state) {
    const content::TestWebUI::CallData& call_data =
        *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.updateDeviceNameMetadata",
              call_data.arg1()->GetString());

    ASSERT_TRUE(call_data.arg2()->is_dict());
    const base::Value::Dict& returned_data = call_data.arg2()->GetDict();

    const std::string* device_name = returned_data.FindString("deviceName");
    ASSERT_TRUE(device_name);
    EXPECT_EQ(expected_device_name, *device_name);

    std::optional<int> device_name_state =
        returned_data.FindInt("deviceNameState");
    ASSERT_TRUE(device_name_state);
    EXPECT_EQ(static_cast<int>(expected_device_name_state), *device_name_state);
  }

  void VerifySetDeviceNameResult(
      const std::string& device_name,
      DeviceNameStore::SetDeviceNameResult expected_result) {
    base::Value::List args;
    args.Append("callback-id");
    args.Append(device_name);
    handler()->HandleAttemptSetDeviceName(args);

    const content::TestWebUI::CallData& call_data =
        *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ("callback-id", call_data.arg1()->GetString());
    EXPECT_TRUE(call_data.arg2()->GetBool());

    EXPECT_EQ(static_cast<int>(expected_result), call_data.arg3()->GetInt());
  }

  void VerifyValuesInDeviceNameStore(
      const std::string& expected_name,
      DeviceNameStore::DeviceNameState expected_state) {
    DeviceNameStore::DeviceNameMetadata metadata =
        fake_device_name_store()->GetDeviceNameMetadata();
    EXPECT_EQ(metadata.device_name, expected_name);
    EXPECT_EQ(metadata.device_name_state, expected_state);
  }

  FakeDeviceNameStore* fake_device_name_store() {
    return &fake_device_name_store_;
  }

  TestDeviceNameHandler* handler() { return handler_.get(); }
  content::TestWebUI* web_ui() { return &web_ui_; }

 private:
  // Run on the UI thread.
  content::BrowserTaskEnvironment task_environment_;

  FakeDeviceNameStore fake_device_name_store_;
  content::TestWebUI web_ui_;
  std::unique_ptr<TestDeviceNameHandler> handler_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(DeviceNameHandlerTest, DeviceNameMetadata_ChangeName) {
  CallHandleNotifyReadyForDeviceName();

  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kSuccess,
            fake_device_name_store()->SetDeviceName("TestName"));
  VerifyDeviceNameMetadata("TestName",
                           DeviceNameStore::DeviceNameState::kCanBeModified);

  // Verify that we can still provide updates to the device name as long as
  // handleNotifyReadyForDeviceName() has been called once during setup.
  EXPECT_EQ(DeviceNameStore::SetDeviceNameResult::kSuccess,
            fake_device_name_store()->SetDeviceName("TestName1"));
  VerifyDeviceNameMetadata("TestName1",
                           DeviceNameStore::DeviceNameState::kCanBeModified);
}

TEST_F(DeviceNameHandlerTest, DeviceNameMetadata_ChangeState) {
  CallHandleNotifyReadyForDeviceName();

  DeviceNameStore::DeviceNameState device_name_state =
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy;
  fake_device_name_store()->SetDeviceNameState(device_name_state);
  VerifyDeviceNameMetadata(FakeDeviceNameStore::kDefaultDeviceName,
                           device_name_state);

  // Verify that we can still provide updates to the device name state as long
  // as handleNotifyReadyForDeviceName() has been called once during setup.
  device_name_state =
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseNotDeviceOwner;
  fake_device_name_store()->SetDeviceNameState(device_name_state);
  VerifyDeviceNameMetadata(FakeDeviceNameStore::kDefaultDeviceName,
                           device_name_state);

  device_name_state = DeviceNameStore::DeviceNameState::kCanBeModified;
  fake_device_name_store()->SetDeviceNameState(device_name_state);
  VerifyDeviceNameMetadata(FakeDeviceNameStore::kDefaultDeviceName,
                           device_name_state);
}

// Verify that DeviceNameHandler::HandleAttemptSetDeviceName() works for all
// possible name update results.
TEST_F(DeviceNameHandlerTest, SetDeviceName) {
  // Verify default values in device name store.
  VerifyValuesInDeviceNameStore(
      "ChromeOS", DeviceNameStore::DeviceNameState::kCanBeModified);

  // Verify that name update is successful and that the name changes in device
  // name store.
  VerifySetDeviceNameResult("TestName",
                            DeviceNameStore::SetDeviceNameResult::kSuccess);
  VerifyValuesInDeviceNameStore(
      "TestName", DeviceNameStore::DeviceNameState::kCanBeModified);

  // Verify that name update is unsuccessful because of invalid name and that
  // the name does not change in device name store.
  VerifySetDeviceNameResult("Invalid Name",
                            DeviceNameStore::SetDeviceNameResult::kInvalidName);
  VerifyValuesInDeviceNameStore(
      "TestName", DeviceNameStore::DeviceNameState::kCanBeModified);

  // Verify that name update is unsuccessful because of policy and that the
  // name does not change in device name store.
  fake_device_name_store()->SetDeviceNameState(
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);
  VerifySetDeviceNameResult(
      "TestName", DeviceNameStore::SetDeviceNameResult::kProhibitedByPolicy);
  VerifyValuesInDeviceNameStore(
      "TestName",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseOfPolicy);

  // Verify that name update is unsuccessful because user is not device owner
  // and that the name does not change in device name store.
  fake_device_name_store()->SetDeviceNameState(
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseNotDeviceOwner);
  VerifySetDeviceNameResult(
      "TestName", DeviceNameStore::SetDeviceNameResult::kNotDeviceOwner);
  VerifyValuesInDeviceNameStore(
      "TestName",
      DeviceNameStore::DeviceNameState::kCannotBeModifiedBecauseNotDeviceOwner);
}

}  // namespace ash::settings
