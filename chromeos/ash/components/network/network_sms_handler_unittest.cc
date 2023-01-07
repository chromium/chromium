// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_sms_handler.h"

#include <memory>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/shill/modem_messaging_client.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kCellularDevicePath[] = "/org/freedesktop/ModemManager1/stub/0";
const char kCellularDeviceObjectPath1[] =
    "/org/freedesktop/ModemManager1/stub/0/Modem/0";
const char kCellularDeviceObjectPath2[] =
    "/org/freedesktop/ModemManager1/stub/0/Modem/1";
const char kSmsPath[] = "/SMS/0";

class TestObserver : public NetworkSmsHandler::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  void MessageReceived(const base::Value& message) override {
    const std::string* text =
        message.FindStringKey(NetworkSmsHandler::kTextKey);
    if (text)
      messages_.insert(*text);
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
    device_test_ = ShillDeviceClient::Get()->GetTestInterface();
    ASSERT_TRUE(device_test_);

    // We want to have only 1 cellular device.
    device_test_->ClearDevices();
    device_test_->AddDevice(kCellularDevicePath, shill::kTypeCellular,
                            "stub_cellular_device2");
    device_test_->SetDeviceProperty(
        kCellularDevicePath, shill::kDBusObjectProperty,
        base::Value(kCellularDeviceObjectPath1), /*notify_changed=*/false);
    modem_messaging_test_ = ModemMessagingClient::Get()->GetTestInterface();

    // This relies on the stub dbus implementations for ShillManagerClient,
    // ShillDeviceClient, ModemMessagingClient and SMSClient.
    network_sms_handler_.reset(new NetworkSmsHandler());
    network_sms_handler_->Init();
    test_observer_ = std::make_unique<TestObserver>();
    network_sms_handler_->AddObserver(test_observer_.get());
    network_sms_handler_->RequestUpdate();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_sms_handler_->RemoveObserver(test_observer_.get());
    network_sms_handler_.reset();
    shill_clients::Shutdown();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ShillDeviceClient::TestInterface* device_test_;
  ModemMessagingClient::TestInterface* modem_messaging_test_;
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
  modem_messaging_test_->ReceiveSms(
      dbus::ObjectPath(kCellularDeviceObjectPath1), dbus::ObjectPath(kSmsPath));
  base::RunLoop().RunUntilIdle();

  network_sms_handler_->RequestUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_GE(test_observer_->message_count(), 1);
  EXPECT_NE(messages.find(kMessage1), messages.end());
}

TEST_F(NetworkSmsHandlerTest, SmsHandlerEmptyDbusObjectPath) {
  // This test verifies no crash should occur when the device dbus object path
  // is an empty value.
  device_test_->SetDeviceProperty(kCellularDevicePath,
                                  shill::kDBusObjectProperty, base::Value(""),
                                  /*notify_changed=*/true);
  base::RunLoop().RunUntilIdle();
  network_sms_handler_->RequestUpdate();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(test_observer_->message_count(), 0);
}

TEST_F(NetworkSmsHandlerTest, SmsHandlerDeviceObjectPathChange) {
  // Fake the SIM being switched to a different SIM.
  device_test_->SetDeviceProperty(
      kCellularDevicePath, shill::kDBusObjectProperty,
      base::Value(kCellularDeviceObjectPath2), /*notify_changed=*/true);
  base::RunLoop().RunUntilIdle();
  network_sms_handler_->RequestUpdate();
  base::RunLoop().RunUntilIdle();

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
  modem_messaging_test_->ReceiveSms(
      dbus::ObjectPath(kCellularDeviceObjectPath2), dbus::ObjectPath(kSmsPath));
  base::RunLoop().RunUntilIdle();

  network_sms_handler_->RequestUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_GE(test_observer_->message_count(), 1);
  EXPECT_NE(messages.find(kMessage1), messages.end());
}

}  // namespace ash
