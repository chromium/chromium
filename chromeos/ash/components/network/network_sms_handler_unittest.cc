// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_sms_handler.h"

#include <memory>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/fake_sms_client.h"
#include "chromeos/ash/components/dbus/shill/modem_messaging_client.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

const char kCellularDevicePath[] = "/org/freedesktop/ModemManager1/stub/0";
const char kCellularDeviceObjectPath1[] =
    "/org/freedesktop/ModemManager1/stub/0/Modem/0";
const char kCellularDeviceObjectPath2[] =
    "/org/freedesktop/ModemManager1/stub/0/Modem/1";
const char kSmsPath[] = "/SMS/0";
constexpr char kTestCellularServicePath1[] = "/service/stub/0";
constexpr char kTestCellularServicePath2[] = "/service/stub/1";
constexpr char kTestGuid1[] = "1";
constexpr char kTestGuid2[] = "2";
constexpr char kTestIccid1[] = "000000000000000000";
constexpr char kTestIccid2[] = "000000000000000001";

class TestObserver : public NetworkSmsHandler::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  void MessageReceived(const base::Value::Dict& message) override {
    const std::string* text = message.FindString(NetworkSmsHandler::kTextKey);
    if (text)
      messages_.insert(*text);
  }

  void MessageReceivedFromNetwork(const std::string& guid,
                                  const TextMessageData& mesage_data) override {
    if (mesage_data.text.has_value()) {
      messages_.insert(*mesage_data.text);
      guid_messages_map_[guid].insert(*mesage_data.text);
    }
    if (mesage_data.number.has_value()) {
      EXPECT_EQ(FakeSMSClient::kNumber, *mesage_data.number);
    }

    if (mesage_data.timestamp.has_value()) {
      EXPECT_EQ(FakeSMSClient::kTimestamp, *mesage_data.timestamp);
    }
  }

  void ClearMessages() {
    messages_.clear();
    guid_messages_map_.clear();
  }

  // Returns the count for messages received across all cellular networks.
  int message_count() { return messages_.size(); }

  // Returns all received messages.
  const std::set<std::string>& messages() const {
    return messages_;
  }

  // Returns messages for the network with the given |guid|.
  const std::set<std::string>& messages(const std::string& guid) {
    return guid_messages_map_[guid];
  }

 private:
  std::set<std::string> messages_;
  base::flat_map<std::string, std::set<std::string>> guid_messages_map_;
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
    service_test_ = ShillServiceClient::Get()->GetTestInterface();
    // We want to have only 1 cellular device.
    device_test_->ClearDevices();
    device_test_->AddDevice(kCellularDevicePath, shill::kTypeCellular,
                            "stub_cellular_device2");
    device_test_->SetDeviceProperty(
        kCellularDevicePath, shill::kDBusObjectProperty,
        base::Value(kCellularDeviceObjectPath1), /*notify_changed=*/false);
    SetupCellularModem(kCellularDeviceObjectPath1, kTestCellularServicePath1,
                       kTestGuid1, kTestIccid1, shill::kStateOnline);
    modem_messaging_test_ = ModemMessagingClient::Get()->GetTestInterface();

    // This relies on the stub dbus implementations for ShillManagerClient,
    // ShillDeviceClient, ModemMessagingClient and SMSClient.
    network_sms_handler_.reset(new NetworkSmsHandler());
    network_state_handler_ = NetworkStateHandler::InitializeForTest();
      network_sms_handler_->Init(network_state_handler_.get());

    test_observer_ = std::make_unique<TestObserver>();
    network_sms_handler_->AddObserver(test_observer_.get());
    network_sms_handler_->RequestUpdate();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_sms_handler_->RemoveObserver(test_observer_.get());
    network_sms_handler_.reset();
    network_state_handler_.reset();
    service_test_ = nullptr;  // Destroyed by shill_clients::Shutdown() below.
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

  void SetupCellularModem(const std::string& object_path,
                          const std::string& service_path,
                          const std::string& guid,
                          const std::string& iccid,
                          const std::string& state) {
    device_test_->SetDeviceProperty(
        kCellularDevicePath, shill::kDBusObjectProperty,
        base::Value(object_path), /*notify_changed=*/true);
    device_test_->SetDeviceProperty(kCellularDevicePath, shill::kIccidProperty,
                                    base::Value(iccid),
                                    /*notify_changed=*/true);
    service_test_->AddService(service_path, guid, "", shill::kTypeCellular,
                              state,
                              /*visible=*/true);
    service_test_->SetServiceProperty(service_path, shill::kIccidProperty,
                                      base::Value(iccid));
  }

  void UpdateNetworkState(const std::string& service_path,
                          const std::string& state) {
    service_test_->SetServiceProperty(service_path, shill::kStateProperty,
                                      base::Value(state));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<ShillDeviceClient::TestInterface, DanglingUntriaged> device_test_;
  raw_ptr<ModemMessagingClient::TestInterface, DanglingUntriaged>
      modem_messaging_test_;
  raw_ptr<ShillServiceClient::TestInterface> service_test_;
  std::unique_ptr<NetworkSmsHandler> network_sms_handler_;
  std::unique_ptr<TestObserver> test_observer_;

 private:
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  base::test::ScopedFeatureList features_;
  raw_ptr<FakeSMSClient, DanglingUntriaged> fake_sms_client_;
};

TEST_F(NetworkSmsHandlerTest, DbusStub) {
  EXPECT_EQ(test_observer_->message_count(), 0);

  // Test that no messages have been received yet
  const std::set<std::string>& messages(test_observer_->messages());
  // Note: The following string corresponds to values in
  // ModemMessagingClientStubImpl and SmsClientStubImpl.
  // TODO(stevenjb): Use a TestInterface to set this up to remove dependency.
  const char kMessage1[] = "FakeSMSClient: Test Message: /SMS/0";
  EXPECT_FALSE(base::Contains(messages, kMessage1));

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
  EXPECT_TRUE(base::Contains(messages, kMessage1));

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
  EXPECT_FALSE(base::Contains(messages, kMessage1));

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
  EXPECT_TRUE(base::Contains(messages, kMessage1));

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
  EXPECT_FALSE(base::Contains(messages, kMessage1));

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
  EXPECT_FALSE(base::Contains(messages, kMessage1));
  EXPECT_TRUE(base::Contains(messages, "FakeSMSClient: Test Message: /SMS/2"));

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
  EXPECT_FALSE(base::Contains(messages, kMessage1));

  // Test for messages delivered by signals.
  test_observer_->ClearMessages();
  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath2),
             dbus::ObjectPath(kSmsPath));
  CompleteReceiveSms();
  base::RunLoop().RunUntilIdle();

  network_sms_handler_->RequestUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_GE(test_observer_->message_count(), 1);
  EXPECT_TRUE(base::Contains(messages, kMessage1));
}

TEST_F(NetworkSmsHandlerTest, NetworkGuidTest) {
  // Test that no messages have been received yet
  EXPECT_EQ(test_observer_->message_count(), 0);

  const char kMessage1[] = "FakeSMSClient: Test Message: /SMS/0";
  const char kMessage2[] = "FakeSMSClient: Test Message: /SMS/1";
  EXPECT_EQ(test_observer_->messages().find(kMessage1),
            test_observer_->messages().end());

  // Test for messages for the first network.
  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath1),
             dbus::ObjectPath(kSmsPath));
  CompleteReceiveSms();
  base::RunLoop().RunUntilIdle();

  network_sms_handler_->RequestUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->messages(kTestGuid1).size());
  EXPECT_NE(test_observer_->messages(kTestGuid1).find(kMessage1),
            test_observer_->messages(kTestGuid1).end());
  modem_messaging_test_->CompletePendingDeleteRequest(/*success=*/true);
  base::RunLoop().RunUntilIdle();

  // Switch to a different modem.
  UpdateNetworkState(kTestCellularServicePath1, shill::kStateDisconnecting);
  SetupCellularModem(kCellularDeviceObjectPath2, kTestCellularServicePath2,
                     kTestGuid2, kTestIccid2, shill::kStateOnline);

  base::RunLoop().RunUntilIdle();
  network_sms_handler_->RequestUpdate();

  // Test for messages for messages from the second network.
  EXPECT_EQ(0u, test_observer_->messages(kTestGuid2).size());
  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath2),
             dbus::ObjectPath("/SMS/1"));
  CompleteReceiveSms();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, test_observer_->messages().size());
  EXPECT_EQ(1u, test_observer_->messages(kTestGuid2).size());
  EXPECT_NE(test_observer_->messages(kTestGuid2).find(kMessage2),
            test_observer_->messages(kTestGuid2).end());
  EXPECT_EQ(test_observer_->messages(kTestGuid1).find(kMessage2),
            test_observer_->messages(kTestGuid1).end());
}

TEST_F(NetworkSmsHandlerTest, NetworkDelayedActiveNetworkTest) {
  SetupCellularModem(kCellularDeviceObjectPath2, kTestCellularServicePath2,
                     kTestGuid2, kTestIccid2, shill::kStateIdle);
  base::RunLoop().RunUntilIdle();
  network_sms_handler_->RequestUpdate();
  EXPECT_EQ(0u, test_observer_->messages().size());
  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath2),
             dbus::ObjectPath(kSmsPath));
  CompleteReceiveSms();
  base::RunLoop().RunUntilIdle();
  // The message will be sent with the GUID of the currently connected
  // network which is kTestGuid1.
  EXPECT_EQ(1u, test_observer_->messages(kTestGuid1).size());
  EXPECT_EQ(0u, test_observer_->messages(kTestGuid2).size());

  UpdateNetworkState(kTestCellularServicePath1, shill::kStateDisconnecting);
  base::RunLoop().RunUntilIdle();
  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath2),
             dbus::ObjectPath("/SMS/1"));
  CompleteReceiveSms();
  base::RunLoop().RunUntilIdle();
  // No connected network, the GUID of the last connected
  // network will be used.
  EXPECT_EQ(2u, test_observer_->messages(kTestGuid1).size());
  EXPECT_EQ(0u, test_observer_->messages(kTestGuid2).size());

  UpdateNetworkState(kTestCellularServicePath2, shill::kStateOnline);
  base::RunLoop().RunUntilIdle();

  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath2),
             dbus::ObjectPath("/SMS/2"));
  CompleteReceiveSms();
  base::RunLoop().RunUntilIdle();
  // After updating the state, we see that the message is sent with the GUID
  // associated with the last active network.
  EXPECT_EQ(2u, test_observer_->messages(kTestGuid1).size());
  EXPECT_EQ(1u, test_observer_->messages(kTestGuid2).size());
}

TEST_F(NetworkSmsHandlerTest, MessageReceivedNeverConnectedNetwork) {
  UpdateNetworkState(kTestCellularServicePath1, shill::kStateDisconnecting);
  SetupCellularModem(kCellularDeviceObjectPath2, kTestCellularServicePath2,
                     kTestGuid2, kTestIccid2, shill::kStateDisconnecting);

  base::RunLoop().RunUntilIdle();
  network_sms_handler_->RequestUpdate();
  EXPECT_EQ(0u, test_observer_->messages().size());
  ReceiveSms(dbus::ObjectPath(kCellularDeviceObjectPath2),
             dbus::ObjectPath(kSmsPath));
  CompleteReceiveSms();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->messages(kTestGuid2).size());
}

}  // namespace ash
