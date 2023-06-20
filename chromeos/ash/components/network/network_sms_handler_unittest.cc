// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_sms_handler.h"

#include <memory>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/shill/fake_sms_client.h"
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

  void MessageReceived(const base::Value::Dict& message) override {
    const std::string* text = message.FindString(NetworkSmsHandler::kTextKey);
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
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~NetworkSmsHandlerTest() override = default;

  void SetUp() override {
    // Append '--sms-test-messages' to the command line to tell
    // SMSClientStubImpl to generate a series of test SMS messages.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(chromeos::switches::kSmsTestMessages);

    shill_clients::InitializeFakes();
    fake_sms_client_ = static_cast<FakeSMSClient*>(SMSClient::Get());
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

  void ReceiveSms(const dbus::ObjectPath& object_path,
                  const dbus::ObjectPath& sms_path) {
    modem_messaging_test_->ReceiveSms(object_path, sms_path);
  }

  void CompleteReceiveSms() { fake_sms_client_->CompleteGetAll(); }

  void FastForwardReceiveSmsTimeout() {
    task_environment_.FastForwardBy(NetworkSmsHandler::kFetchSmsDetailsTimeout);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<ShillDeviceClient::TestInterface, ExperimentalAsh> device_test_;
  raw_ptr<ModemMessagingClient::TestInterface, ExperimentalAsh>
      modem_messaging_test_;
  std::unique_ptr<NetworkSmsHandler> network_sms_handler_;
  std::unique_ptr<TestObserver> test_observer_;

 private:
  raw_ptr<FakeSMSClient, ExperimentalAsh> fake_sms_client_;
};

TEST_F(NetworkSmsHandlerTest, DbusStub) {
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
  EXPECT_TRUE(modem_messaging_test_->GetPendingDeleteRequestSmsPath().empty());
  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath1),
             dbus::ObjectPath(kSmsPath));
  CompleteReceiveSms();
  base::RunLoop().RunUntilIdle();

  network_sms_handler_->RequestUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(test_observer_->message_count(), 1);
  EXPECT_NE(messages.find(kMessage1), messages.end());

  // There should be a request to delete the message after the message has been
  // received. Complete the request.
  EXPECT_EQ(kSmsPath, modem_messaging_test_->GetPendingDeleteRequestSmsPath());
  modem_messaging_test_->CompletePendingDeleteRequest(/*success=*/true);
  base::RunLoop().RunUntilIdle();

  // There should be no more messages to delete.
  EXPECT_TRUE(modem_messaging_test_->GetPendingDeleteRequestSmsPath().empty());

  // Receive two more messages. We shouldn't attempt to delete messages until
  // after we've finished receiving all messages.
  const std::string sms_path_1 = "/SMS/1";
  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath1),
             dbus::ObjectPath(sms_path_1));
  EXPECT_TRUE(modem_messaging_test_->GetPendingDeleteRequestSmsPath().empty());
  const std::string sms_path_2 = "/SMS/2";
  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath1),
             dbus::ObjectPath(sms_path_2));
  EXPECT_TRUE(modem_messaging_test_->GetPendingDeleteRequestSmsPath().empty());

  // Simulate no more messages being received. The last SMS added should be the
  // first one attempted to be deleted.
  CompleteReceiveSms();
  base::RunLoop().RunUntilIdle();
  CompleteReceiveSms();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(sms_path_2,
            modem_messaging_test_->GetPendingDeleteRequestSmsPath());
  network_sms_handler_->RequestUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(test_observer_->message_count(), 3);

  // Complete the deletion, another delete request should occur.
  modem_messaging_test_->CompletePendingDeleteRequest(/*success=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(sms_path_1,
            modem_messaging_test_->GetPendingDeleteRequestSmsPath());

  // Receive another message before completing the delete request.
  const std::string sms_path_3 = "/SMS/3";
  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath1),
             dbus::ObjectPath(sms_path_3));
  CompleteReceiveSms();
  EXPECT_EQ(sms_path_1,
            modem_messaging_test_->GetPendingDeleteRequestSmsPath());
  network_sms_handler_->RequestUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(test_observer_->message_count(), 4);

  // Complete the deletion, the last delete request should occur.
  EXPECT_EQ(sms_path_1,
            modem_messaging_test_->GetPendingDeleteRequestSmsPath());
  modem_messaging_test_->CompletePendingDeleteRequest(/*success=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(sms_path_3,
            modem_messaging_test_->GetPendingDeleteRequestSmsPath());

  // Complete the last delete request.
  modem_messaging_test_->CompletePendingDeleteRequest(/*success=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(modem_messaging_test_->GetPendingDeleteRequestSmsPath().empty());
  network_sms_handler_->RequestUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(test_observer_->message_count(), 4);
}

TEST_F(NetworkSmsHandlerTest, DeleteFailure) {
  EXPECT_EQ(test_observer_->message_count(), 0);

  // Test that no messages have been received yet
  const std::set<std::string>& messages(test_observer_->messages());
  // Note: The following string corresponds to values in
  // ModemMessagingClientStubImpl and SmsClientStubImpl.
  const char kMessage1[] = "FakeSMSClient: Test Message: /SMS/0";
  EXPECT_EQ(messages.find(kMessage1), messages.end());

  // Test for messages delivered by signals.
  test_observer_->ClearMessages();
  EXPECT_TRUE(modem_messaging_test_->GetPendingDeleteRequestSmsPath().empty());
  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath1),
             dbus::ObjectPath(kSmsPath));
  CompleteReceiveSms();
  base::RunLoop().RunUntilIdle();

  network_sms_handler_->RequestUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_GE(test_observer_->message_count(), 1);
  EXPECT_NE(messages.find(kMessage1), messages.end());

  // There should be a request to delete the message after the message has been
  // received.
  EXPECT_EQ(kSmsPath, modem_messaging_test_->GetPendingDeleteRequestSmsPath());

  // Simulate the deletion failing. The delete request should be placed back in
  // the queue, but not invoked.
  modem_messaging_test_->CompletePendingDeleteRequest(/*success=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(modem_messaging_test_->GetPendingDeleteRequestSmsPath().empty());

  // Receive two more messages. We shouldn't attempt to delete messages until
  // after we've finished receiving all messages.
  const std::string sms_path_1 = "/SMS/1";
  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath1),
             dbus::ObjectPath(sms_path_1));
  EXPECT_TRUE(modem_messaging_test_->GetPendingDeleteRequestSmsPath().empty());
  const std::string sms_path_2 = "/SMS/2";
  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath1),
             dbus::ObjectPath(sms_path_2));
  EXPECT_TRUE(modem_messaging_test_->GetPendingDeleteRequestSmsPath().empty());

  // Simulate no more messages being received. The last SMS added should be the
  // first one attempted to be deleted.
  CompleteReceiveSms();
  base::RunLoop().RunUntilIdle();
  CompleteReceiveSms();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(sms_path_2,
            modem_messaging_test_->GetPendingDeleteRequestSmsPath());
  network_sms_handler_->RequestUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_GE(test_observer_->message_count(), 3);

  // Simulate the deletion failing, the SMS should be placed back in the queue,
  // but no deletion invoked.
  modem_messaging_test_->CompletePendingDeleteRequest(/*success=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(modem_messaging_test_->GetPendingDeleteRequestSmsPath().empty());

  // Receive another message, it should be the first message attempted to be
  // deleted.
  const std::string sms_path_3 = "/SMS/3";
  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath1),
             dbus::ObjectPath(sms_path_3));
  CompleteReceiveSms();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(sms_path_3,
            modem_messaging_test_->GetPendingDeleteRequestSmsPath());
  network_sms_handler_->RequestUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_GE(test_observer_->message_count(), 4);

  // Simulate the deletion succeeding, the second SMS should be attempted to be
  // deleted.
  EXPECT_EQ(sms_path_3,
            modem_messaging_test_->GetPendingDeleteRequestSmsPath());
  modem_messaging_test_->CompletePendingDeleteRequest(/*success=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(sms_path_1,
            modem_messaging_test_->GetPendingDeleteRequestSmsPath());

  // Simulate the deletion succeeding, the first SMS should be attempted to be
  // deleted.
  modem_messaging_test_->CompletePendingDeleteRequest(/*success=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kSmsPath, modem_messaging_test_->GetPendingDeleteRequestSmsPath());

  // Simulate the deletion succeeding, the third SMS should be attempted to be
  // deleted.
  modem_messaging_test_->CompletePendingDeleteRequest(/*success=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(sms_path_2,
            modem_messaging_test_->GetPendingDeleteRequestSmsPath());
}

TEST_F(NetworkSmsHandlerTest, ReceiveSmsTimeout) {
  EXPECT_EQ(test_observer_->message_count(), 0);

  // Test that no messages have been received yet
  const std::set<std::string>& messages(test_observer_->messages());
  // Note: The following string corresponds to values in
  // ModemMessagingClientStubImpl and SmsClientStubImpl.
  // TODO(stevenjb): Use a TestInterface to set this up to remove dependency.
  const char kMessage1[] = "FakeSMSClient: Test Message: /SMS/0";
  EXPECT_EQ(messages.find(kMessage1), messages.end());

  // Receive 2 SMSes.
  test_observer_->ClearMessages();
  EXPECT_TRUE(modem_messaging_test_->GetPendingDeleteRequestSmsPath().empty());
  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath1),
             dbus::ObjectPath(kSmsPath));
  const std::string sms_path_2 = "/SMS/2";
  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath1),
             dbus::ObjectPath(sms_path_2));
  EXPECT_TRUE(modem_messaging_test_->GetPendingDeleteRequestSmsPath().empty());

  // Simulate the timeout passing for the first SMS. The next SMS in the queue
  // should be processed.
  FastForwardReceiveSmsTimeout();

  // Complete fetching the second SMS' details.
  CompleteReceiveSms();
  base::RunLoop().RunUntilIdle();

  // The second SMS should be found in |messages| but not the first SMS.
  network_sms_handler_->RequestUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(test_observer_->message_count(), 1);
  EXPECT_EQ(messages.find(kMessage1), messages.end());
  EXPECT_NE(messages.find("FakeSMSClient: Test Message: /SMS/2"),
            messages.end());

  // There should be a request to delete the second SMS. Complete the request.
  EXPECT_EQ(sms_path_2,
            modem_messaging_test_->GetPendingDeleteRequestSmsPath());
  modem_messaging_test_->CompletePendingDeleteRequest(/*success=*/true);
  base::RunLoop().RunUntilIdle();

  // There should be a request to delete the first SMS. Complete the request.
  EXPECT_EQ(kSmsPath, modem_messaging_test_->GetPendingDeleteRequestSmsPath());
  modem_messaging_test_->CompletePendingDeleteRequest(/*success=*/true);
  base::RunLoop().RunUntilIdle();

  // There should be no more messages to delete.
  EXPECT_TRUE(modem_messaging_test_->GetPendingDeleteRequestSmsPath().empty());
}

TEST_F(NetworkSmsHandlerTest, EmptyDbusObjectPath) {
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

TEST_F(NetworkSmsHandlerTest, DeviceObjectPathChange) {
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
  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath2),
             dbus::ObjectPath(kSmsPath));
  CompleteReceiveSms();
  base::RunLoop().RunUntilIdle();

  network_sms_handler_->RequestUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_GE(test_observer_->message_count(), 1);
  EXPECT_NE(messages.find(kMessage1), messages.end());
}

}  // namespace ash
