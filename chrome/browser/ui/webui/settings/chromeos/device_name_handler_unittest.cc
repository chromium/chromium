// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_name_handler.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/fake_device_name_store.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

class TestDeviceNameHandler : public DeviceNameHandler {
 public:
  explicit TestDeviceNameHandler(content::WebUI* web_ui,
                                 DeviceNameStore* fake_device_name_store)
      : DeviceNameHandler(fake_device_name_store) {
    set_web_ui(web_ui);
  }

  // Raise visibility to public.
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
    base::Value args(base::Value::Type::LIST);
    args.Append("callback-id");
    handler()->HandleNotifyReadyForDeviceName(&base::Value::AsListValue(args));

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

    const base::DictionaryValue* returned_data;
    ASSERT_TRUE(call_data.arg2()->GetAsDictionary(&returned_data));

    std::string device_name;
    returned_data->GetString("deviceName", &device_name);
    EXPECT_EQ(expected_device_name, device_name);

    int device_name_state;
    returned_data->GetInteger("deviceNameState", &device_name_state);
    EXPECT_EQ(static_cast<int>(expected_device_name_state), device_name_state);
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

}  // namespace settings
}  // namespace chromeos
