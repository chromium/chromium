// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/internet/internet_handler.h"

#include <memory>

#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/ash/components/tether/fake_gms_core_notifications_state_tracker.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

namespace {

const char kWebCallbackFunctionName[] = "cr.webUIListenerCallback";
const char kSendDeviceNamesMessageType[] =
    "sendGmsCoreNotificationsDisabledDeviceNames";

class TestInternetHandler : public InternetHandler {
 public:
  // Pull WebUIMessageHandler::set_web_ui() into public so SetUp() can call it.
  using InternetHandler::set_web_ui;

  explicit TestInternetHandler(TestingProfile* profile)
      : InternetHandler(profile) {}
};

}  // namespace

class InternetHandlerTest : public BrowserWithTestWindowTest {
 public:
  InternetHandlerTest(const InternetHandlerTest&) = delete;
  InternetHandlerTest& operator=(const InternetHandlerTest&) = delete;

 protected:
  InternetHandlerTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    web_ui_ = std::make_unique<content::TestWebUI>();

    handler_ = std::make_unique<TestInternetHandler>(profile());
    handler_->set_web_ui(web_ui_.get());
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();

    fake_tracker_ =
        std::make_unique<tether::FakeGmsCoreNotificationsStateTracker>();
    handler_->SetGmsCoreNotificationsStateTrackerForTesting(
        fake_tracker_.get());
  }

  void RequestGmsCoreNotificationsDisabledDeviceNames() {
    handler_->RequestGmsCoreNotificationsDisabledDeviceNames(
        base::Value::List());
  }

  void VerifyMostRecentDeviceNamesSent(
      const std::vector<std::string>& expected_device_names,
      size_t expected_num_updates) {
    EXPECT_EQ(expected_num_updates, web_ui_->call_data().size());

    const content::TestWebUI::CallData* last_call_data =
        web_ui_->call_data()[expected_num_updates - 1].get();
    EXPECT_TRUE(last_call_data);

    // The call is structured such that the function name is the "web callback"
    // name and the first argument is the name of the message being sent.
    EXPECT_EQ(kWebCallbackFunctionName, last_call_data->function_name());
    EXPECT_EQ(kSendDeviceNamesMessageType, last_call_data->arg1()->GetString());

    std::vector<std::string> actual_device_names;
    for (const auto& device_name_value : last_call_data->arg2()->GetList()) {
      actual_device_names.push_back(device_name_value.GetString());
    }
    EXPECT_EQ(expected_device_names, actual_device_names);
  }

  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<tether::FakeGmsCoreNotificationsStateTracker> fake_tracker_;
  std::unique_ptr<TestInternetHandler> handler_;
};

TEST_F(InternetHandlerTest, TestSendsDeviceNames) {
  RequestGmsCoreNotificationsDisabledDeviceNames();
  VerifyMostRecentDeviceNamesSent({} /* expected_device_names */,
                                  1u /* expected_num_updates */);

  // Set two unique names.
  fake_tracker_->set_device_names({"device1", "device2"});
  fake_tracker_->NotifyGmsCoreNotificationStateChanged();
  VerifyMostRecentDeviceNamesSent(
      {"device1", "device2"} /* expected_device_names */,
      2u /* expected_num_updates */);

  // Set three names, two of them identical. Devices with the same name should
  // be supported, since it is possible for a user to have two phones of the
  // same model.
  fake_tracker_->set_device_names({"device1", "device1", "device3"});
  fake_tracker_->NotifyGmsCoreNotificationStateChanged();
  VerifyMostRecentDeviceNamesSent(
      {"device1", "device1", "device3"} /* expected_device_names */,
      3u /* expected_num_updates */);
}

TEST_F(InternetHandlerTest, TestSendsDeviceNames_StartsWithDevices) {
  // Start with two devices before the handler requests any names.
  fake_tracker_->set_device_names({"device1", "device2"});

  RequestGmsCoreNotificationsDisabledDeviceNames();
  VerifyMostRecentDeviceNamesSent(
      {"device1", "device2"} /* expected_device_names */,
      1u /* expected_num_updates */);
}

}  // namespace ash::settings
