// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_name_handler.h"

#include <memory>
#include <string>

#include "chrome/browser/chromeos/device_name_store.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

class TestDeviceNameHandler : public DeviceNameHandler {
 public:
  explicit TestDeviceNameHandler(content::WebUI* web_ui) : DeviceNameHandler() {
    set_web_ui(web_ui);
  }

  // Raise visibility to public.
  void HandleGetDeviceNameMetadata(const base::ListValue* args) {
    DeviceNameHandler::HandleGetDeviceNameMetadata(args);
  }
};

class DeviceNameHandlerTest : public testing::Test {
 public:
  DeviceNameHandlerTest() = default;

  void SetUp() override {
    testing::Test::SetUp();

    handler_ = std::make_unique<TestDeviceNameHandler>(web_ui());
    handler()->AllowJavascriptForTesting();
    web_ui()->ClearTrackedCalls();

    DeviceNameStore::RegisterLocalStatePrefs(local_state_.registry());

    local_state()->SetString(prefs::kDeviceName, "TestDeviceName");
    DeviceNameStore::GetInstance()->Initialize(&local_state_);
  }

  TestDeviceNameHandler* handler() { return handler_.get(); }
  content::TestWebUI* web_ui() { return &web_ui_; }
  TestingPrefServiceSimple* local_state() { return &local_state_; }

 private:
  // Run on the UI thread.
  content::BrowserTaskEnvironment task_environment_;

  // Test backing store for prefs.
  TestingPrefServiceSimple local_state_;

  content::TestWebUI web_ui_;
  std::unique_ptr<TestDeviceNameHandler> handler_;
};

TEST_F(DeviceNameHandlerTest, DeviceNameMetadata_DeviceName) {
  base::ListValue args;
  args.AppendString("callback-id");
  handler()->HandleGetDeviceNameMetadata(&args);

  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("callback-id", call_data.arg1()->GetString());
  EXPECT_TRUE(call_data.arg2()->GetBool());

  const base::DictionaryValue* returned_data;
  ASSERT_TRUE(call_data.arg3()->GetAsDictionary(&returned_data));

  std::string device_name;
  ASSERT_TRUE(returned_data->GetString("deviceName", &device_name));
  EXPECT_EQ("TestDeviceName", device_name);
}

}  // namespace settings
}  // namespace chromeos
