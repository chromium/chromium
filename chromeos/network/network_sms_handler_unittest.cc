// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_sms_handler.h"

#include <memory>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

class TestObserver : public NetworkSmsHandler::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  void MessageReceived(const base::DictionaryValue& message) override {
    std::string text;
    if (message.GetStringWithoutPathExpansion(
            NetworkSmsHandler::kTextKey, &text)) {
      messages_.insert(text);
    }
  }

  void ClearMessages() {
    messages_.clear();
  }

  int message_count() { return messages_.size(); }
  const std::set<std::string>& messages() const {
    return messages_;
  }

 private:
  std::set<std::string> messages_;
};

}  // namespace

class NetworkSmsHandlerTest : public testing::Test {
 public:
  NetworkSmsHandlerTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}
  ~NetworkSmsHandlerTest() override = default;

  void SetUp() override {
    // Append '--sms-test-messages' to the command line to tell
    // SMSClientStubImpl to generate a series of test SMS messages.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(chromeos::switches::kSmsTestMessages);

    shill_clients::InitializeFakes();
    ShillDeviceClient::TestInterface* device_test =
        ShillDeviceClient::Get()->GetTestInterface();
    ASSERT_TRUE(device_test);
    device_test->AddDevice("/org/freedesktop/ModemManager1/stub/0",
                           shill::kTypeCellular,
                           "stub_cellular_device2");

    // This relies on the stub dbus implementations for ShillManagerClient,
    // ShillDeviceClient, ModemMessagingClient and SMSClient.
    // Initialize a sms handler. The stub dbus clients will not send the
    // first test message until RequestUpdate has been called.
    network_sms_handler_.reset(new NetworkSmsHandler());
    network_sms_handler_->Init();
    test_observer_.reset(new TestObserver());
    network_sms_handler_->AddObserver(test_observer_.get());
    network_sms_handler_->RequestUpdate(true);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_sms_handler_->RemoveObserver(test_observer_.get());
    network_sms_handler_.reset();
    shill_clients::Shutdown();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<NetworkSmsHandler> network_sms_handler_;
  std::unique_ptr<TestObserver> test_observer_;
};

TEST_F(NetworkSmsHandlerTest, SmsHandlerDbusStub) {
  EXPECT_EQ(test_observer_->message_count(), 0);

  // Test that no messages have been received yet
  const std::set<std::string>& messages(test_observer_->messages());
  // Note: The following string corresponds to values in
  // ModemMessagingClientStubImpl and SmsClientStubImpl.
  // TODO(stevenjb): Use a TestInterface to set this up to remove dependency.
  const char kMessage1[] = "FakeSMSClient: Test Message: /SMS/0";
  EXPECT_EQ(messages.find(kMessage1), messages.end());

  // Test for messages delivered by signals.
  test_observer_->ClearMessages();
  network_sms_handler_->RequestUpdate(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_GE(test_observer_->message_count(), 1);
  EXPECT_NE(messages.find(kMessage1), messages.end());
}

}  // namespace chromeos
