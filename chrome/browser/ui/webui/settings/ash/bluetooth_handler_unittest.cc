// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/bluetooth_handler.h"

#include "content/public/test/test_web_ui.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

namespace {

class TestBluetoothHandler : public BluetoothHandler {
 public:
  TestBluetoothHandler() : BluetoothHandler() {}
  ~TestBluetoothHandler() override = default;

  // Make public for testing.
  using BluetoothHandler::AllowJavascript;
  using BluetoothHandler::RegisterMessages;
  using BluetoothHandler::set_web_ui;
};

}  // namespace

class BluetoothHandlerTest : public testing::Test {
 protected:
  BluetoothHandlerTest() {}
  BluetoothHandlerTest(const BluetoothHandlerTest&) = delete;
  BluetoothHandlerTest& operator=(const BluetoothHandlerTest&) = delete;
  ~BluetoothHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    mock_adapter_ = new testing::NiceMock<device::MockBluetoothAdapter>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);

    test_web_ui_ = std::make_unique<content::TestWebUI>();
    handler_ = std::make_unique<TestBluetoothHandler>();
    handler_->set_web_ui(test_web_ui_.get());
    handler_->RegisterMessages();
  }

  content::TestWebUI* test_web_ui() { return test_web_ui_.get(); }

  const content::TestWebUI::CallData& CallDataAtIndex(size_t index) {
    return *test_web_ui_->call_data()[index];
  }

 private:
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  std::unique_ptr<TestBluetoothHandler> handler_;
};

TEST_F(BluetoothHandlerTest, GetRequestFastPairDeviceSupport) {
  size_t call_data_count_before_call = test_web_ui()->call_data().size();

  base::Value::List args;
  test_web_ui()->HandleReceivedMessage("requestFastPairDeviceSupportStatus",
                                       args);

  ASSERT_EQ(call_data_count_before_call + 1u,
            test_web_ui()->call_data().size());
  const content::TestWebUI::CallData& call_data =
      CallDataAtIndex(call_data_count_before_call);
  EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
  EXPECT_EQ("fast-pair-device-supported-status", call_data.arg1()->GetString());
  EXPECT_FALSE(call_data.arg2()->GetBool());
}

}  // namespace ash::settings
