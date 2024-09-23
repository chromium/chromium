// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/ble_weave_client_connection.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/services/secure_channel/ble_weave_packet_generator.h"
#include "chromeos/ash/services/secure_channel/ble_weave_packet_receiver.h"
#include "chromeos/ash/services/secure_channel/connection_observer.h"
#include "chromeos/ash/services/secure_channel/fake_wire_message.h"
#include "chromeos/ash/services/secure_channel/wire_message.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_notify_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel::weave {

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::WithArgs;

typedef BluetoothLowEnergyWeaveClientConnection::SubStatus SubStatus;
typedef BluetoothLowEnergyWeavePacketReceiver::State ReceiverState;
typedef BluetoothLowEnergyWeavePacketReceiver::ReceiverError ReceiverError;
typedef BluetoothLowEnergyWeavePacketReceiver::ReceiverType ReceiverType;

const char kTestFeature[] = "testFeature";

const char kServiceUUID[] = "DEADBEEF-CAFE-FEED-FOOD-D15EA5EBEEEF";
const char kTXCharacteristicUUID[] = "977c6674-1239-4e72-993b-502369b8bb5a";
const char kRXCharacteristicUUID[] = "f4b904a2-a030-43b3-98a8-221c536c03cb";

const char kServiceID[] = "service id";
const char kTXCharacteristicID[] = "TX characteristic id";
const char kRXCharacteristicID[] = "RX characteristic id";

const char kTestRemoteDeviceBluetoothAddress[] = "AA:BB:CC:DD:EE:FF";

const device::BluetoothRemoteGattCharacteristic::Properties
    kCharacteristicProperties =
        device::BluetoothRemoteGattCharacteristic::PROPERTY_BROADCAST |
        device::BluetoothRemoteGattCharacteristic::PROPERTY_READ |
        device::BluetoothRemoteGattCharacteristic::
            PROPERTY_WRITE_WITHOUT_RESPONSE |
        device::BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE;

const int kMaxNumberOfTries = 3;
const uint16_t kLargeMaxPacketSize = 30;

const uint8_t kDataHeader = 0;
const uint8_t kConnectionRequestHeader = 1;
const uint8_t kSmallConnectionResponseHeader = 2;
const uint8_t kLargeConnectionResponseHeader = 3;
const uint8_t kConnectionCloseHeader = 4;
const uint8_t kErroneousHeader = 5;

const char kSmallMessage[] = "bb";
const char kLargeMessage[] = "aaabbb";

const Packet kConnectionRequest{kConnectionRequestHeader};
const Packet kSmallConnectionResponse{kSmallConnectionResponseHeader};
const Packet kLargeConnectionResponse{kLargeConnectionResponseHeader};
const Packet kConnectionCloseSuccess{kConnectionCloseHeader,
                                     ReasonForClose::CLOSE_WITHOUT_ERROR};
const Packet kConnectionCloseUnknownError{kConnectionCloseHeader,
                                          ReasonForClose::UNKNOWN_ERROR};
const Packet kConnectionCloseApplicationError{
    kConnectionCloseHeader, ReasonForClose::APPLICATION_ERROR};

const Packet kSmallPackets0 = Packet{kDataHeader, 'b', 'b'};
const Packet kLargePackets0 = Packet{kDataHeader, 'a', 'a', 'a'};
const Packet kLargePackets1 = Packet{kDataHeader, 'b', 'b', 'b'};
const Packet kErroneousPacket = Packet{kErroneousHeader};

const std::vector<Packet> kSmallPackets{kSmallPackets0};
const std::vector<Packet> kLargePackets{kLargePackets0, kLargePackets1};

class MockBluetoothLowEnergyWeavePacketGenerator
    : public BluetoothLowEnergyWeavePacketGenerator {
 public:
  MockBluetoothLowEnergyWeavePacketGenerator()
      : max_packet_size_(kDefaultMaxPacketSize) {}

  Packet CreateConnectionRequest() override { return kConnectionRequest; }

  Packet CreateConnectionResponse() override {
    NOTIMPLEMENTED();
    return Packet();
  }

  Packet CreateConnectionClose(ReasonForClose reason_for_close) override {
    return Packet{kConnectionCloseHeader,
                  static_cast<uint8_t>(reason_for_close)};
  }

  void SetMaxPacketSize(uint16_t size) override { max_packet_size_ = size; }

  std::vector<Packet> EncodeDataMessage(std::string message) override {
    if (message == (std::string(kTestFeature) + "," + kSmallMessage) &&
        max_packet_size_ == kDefaultMaxPacketSize) {
      return kSmallPackets;
    } else if (message == (std::string(kTestFeature) + "," + kLargeMessage) &&
               max_packet_size_ == kLargeMaxPacketSize) {
      return kLargePackets;
    } else {
      NOTREACHED_IN_MIGRATION();
      return std::vector<Packet>();
    }
  }

  uint16_t GetMaxPacketSize() { return max_packet_size_; }

 private:
  uint16_t max_packet_size_;
};

class MockBluetoothLowEnergyWeavePacketReceiver
    : public BluetoothLowEnergyWeavePacketReceiver {
 public:
  MockBluetoothLowEnergyWeavePacketReceiver()
      : BluetoothLowEnergyWeavePacketReceiver(ReceiverType::CLIENT),
        state_(State::CONNECTING),
        max_packet_size_(kDefaultMaxPacketSize),
        reason_for_close_(ReasonForClose::CLOSE_WITHOUT_ERROR),
        reason_to_close_(ReasonForClose::CLOSE_WITHOUT_ERROR) {}

  ReceiverState GetState() override { return state_; }

  uint16_t GetMaxPacketSize() override { return max_packet_size_; }

  ReasonForClose GetReasonForClose() override { return reason_for_close_; }

  ReasonForClose GetReasonToClose() override { return reason_to_close_; }

  std::string GetDataMessage() override {
    if (max_packet_size_ == kDefaultMaxPacketSize) {
      return kSmallMessage;
    } else {
      return kLargeMessage;
    }
  }

  ReceiverError GetReceiverError() override {
    return ReceiverError::NO_ERROR_DETECTED;
  }

  ReceiverState ReceivePacket(const Packet& packet) override {
    switch (packet[0]) {
      case kSmallConnectionResponseHeader:
        max_packet_size_ = kDefaultMaxPacketSize;
        state_ = ReceiverState::WAITING;
        break;
      case kLargeConnectionResponseHeader:
        max_packet_size_ = kLargeMaxPacketSize;
        state_ = ReceiverState::WAITING;
        break;
      case kConnectionCloseHeader:
        state_ = ReceiverState::CONNECTION_CLOSED;
        reason_for_close_ = static_cast<ReasonForClose>(packet[1]);
        break;
      case kDataHeader:
        if (packet == kSmallPackets0 || packet == kLargePackets1) {
          state_ = ReceiverState::DATA_READY;
        } else {
          state_ = ReceiverState::RECEIVING_DATA;
        }
        break;
      default:
        reason_to_close_ = ReasonForClose::APPLICATION_ERROR;
        state_ = ReceiverState::ERROR_DETECTED;
    }
    return state_;
  }

 private:
  ReceiverState state_;
  uint16_t max_packet_size_;
  ReasonForClose reason_for_close_;
  ReasonForClose reason_to_close_;
};

class TestBluetoothLowEnergyWeaveClientConnection
    : public BluetoothLowEnergyWeaveClientConnection {
 public:
  TestBluetoothLowEnergyWeaveClientConnection(
      multidevice::RemoteDeviceRef remote_device,
      scoped_refptr<device::BluetoothAdapter> adapter,
      const device::BluetoothUUID remote_service_uuid,
      const std::string& device_address,
      bool should_set_low_connection_latency)
      : BluetoothLowEnergyWeaveClientConnection(
            remote_device,
            adapter,
            remote_service_uuid,
            device_address,
            should_set_low_connection_latency) {}

  TestBluetoothLowEnergyWeaveClientConnection(
      const TestBluetoothLowEnergyWeaveClientConnection&) = delete;
  TestBluetoothLowEnergyWeaveClientConnection& operator=(
      const TestBluetoothLowEnergyWeaveClientConnection&) = delete;

  ~TestBluetoothLowEnergyWeaveClientConnection() override {}

  bool should_set_low_connection_latency() {
    return BluetoothLowEnergyWeaveClientConnection::
        should_set_low_connection_latency();
  }

  BluetoothLowEnergyCharacteristicsFinder* CreateCharacteristicsFinder(
      BluetoothLowEnergyCharacteristicsFinder::SuccessCallback success,
      base::OnceClosure error) override {
    return CreateCharacteristicsFinder_(success, error);
  }
  MOCK_METHOD2(
      CreateCharacteristicsFinder_,
      BluetoothLowEnergyCharacteristicsFinder*(
          BluetoothLowEnergyCharacteristicsFinder::SuccessCallback& success,
          base::OnceClosure& error));

  MOCK_METHOD1(OnBytesReceived, void(const std::string& bytes));

  // Exposing inherited protected methods for testing.
  using BluetoothLowEnergyWeaveClientConnection::DestroyConnection;
  using BluetoothLowEnergyWeaveClientConnection::GattCharacteristicValueChanged;
  using BluetoothLowEnergyWeaveClientConnection::SetupTestDoubles;

  // Exposing inherited protected fields for testing.
  using BluetoothLowEnergyWeaveClientConnection::status;
  using BluetoothLowEnergyWeaveClientConnection::sub_status;
};

class MockBluetoothLowEnergyCharacteristicsFinder
    : public BluetoothLowEnergyCharacteristicsFinder {
 public:
  MockBluetoothLowEnergyCharacteristicsFinder(
      multidevice::RemoteDeviceRef remote_device)
      : BluetoothLowEnergyCharacteristicsFinder(remote_device) {}

  MockBluetoothLowEnergyCharacteristicsFinder(
      const MockBluetoothLowEnergyCharacteristicsFinder&) = delete;
  MockBluetoothLowEnergyCharacteristicsFinder& operator=(
      const MockBluetoothLowEnergyCharacteristicsFinder&) = delete;

  ~MockBluetoothLowEnergyCharacteristicsFinder() override {}
};

class MockConnectionObserver : public ConnectionObserver {
 public:
  explicit MockConnectionObserver(Connection* connection)
      : connection_(connection),
        num_send_completed_(0),
        delete_on_disconnect_(false),
        delete_on_message_sent_(false) {}

  std::string GetLastDeserializedMessage() {
    return last_deserialized_message_;
  }

  bool last_send_success() { return last_send_success_; }

  int num_send_completed() { return num_send_completed_; }

  bool delete_on_disconnect() { return delete_on_disconnect_; }

  void set_delete_on_disconnect(bool delete_on_disconnect) {
    delete_on_disconnect_ = delete_on_disconnect;
  }

  void set_delete_on_message_sent(bool delete_on_message_sent) {
    delete_on_message_sent_ = delete_on_message_sent;
  }

  // ConnectionObserver:
  void OnConnectionStatusChanged(Connection* connection,
                                 Connection::Status old_status,
                                 Connection::Status new_status) override {
    if (new_status == Connection::Status::DISCONNECTED && delete_on_disconnect_)
      delete connection_;
  }

  void OnMessageReceived(const Connection& connection,
                         const WireMessage& message) override {}

  void OnSendCompleted(const Connection& connection,
                       const WireMessage& message,
                       bool success) override {
    last_deserialized_message_ = message.payload();
    last_send_success_ = success;
    num_send_completed_++;

    if (delete_on_message_sent_)
      delete connection_;
  }

 private:
  raw_ptr<Connection, DanglingUntriaged> connection_;
  std::string last_deserialized_message_;
  bool last_send_success_;
  int num_send_completed_;
  bool delete_on_disconnect_;
  bool delete_on_message_sent_;
};

}  // namespace

class SecureChannelBluetoothLowEnergyWeaveClientConnectionTest
    : public testing::Test {
 public:
  SecureChannelBluetoothLowEnergyWeaveClientConnectionTest()
      : remote_device_(multidevice::CreateRemoteDeviceRefForTest()),
        service_uuid_(device::BluetoothUUID(kServiceUUID)),
        tx_characteristic_uuid_(device::BluetoothUUID(kTXCharacteristicUUID)),
        rx_characteristic_uuid_(device::BluetoothUUID(kRXCharacteristicUUID)) {}

  SecureChannelBluetoothLowEnergyWeaveClientConnectionTest(
      const SecureChannelBluetoothLowEnergyWeaveClientConnectionTest&) = delete;
  SecureChannelBluetoothLowEnergyWeaveClientConnectionTest& operator=(
      const SecureChannelBluetoothLowEnergyWeaveClientConnectionTest&) = delete;

  ~SecureChannelBluetoothLowEnergyWeaveClientConnectionTest() override {}

  void SetUp() override {
    test_timer_ = nullptr;
    generator_ = nullptr;
    receiver_ = nullptr;
    has_verified_connection_result_ = false;
    connection_observer_.reset();

    adapter_ = base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();

    mock_bluetooth_device_ =
        std::make_unique<NiceMock<device::MockBluetoothDevice>>(
            adapter_.get(), 0, multidevice::kTestRemoteDeviceName,
            kTestRemoteDeviceBluetoothAddress, false, false);
    service_ = std::make_unique<NiceMock<device::MockBluetoothGattService>>(
        mock_bluetooth_device_.get(), kServiceID, service_uuid_,
        /*is_primary=*/true);
    tx_characteristic_ =
        std::make_unique<NiceMock<device::MockBluetoothGattCharacteristic>>(
            service_.get(), kTXCharacteristicID, tx_characteristic_uuid_,
            kCharacteristicProperties,
            device::BluetoothRemoteGattCharacteristic::PERMISSION_NONE);
    rx_characteristic_ =
        std::make_unique<NiceMock<device::MockBluetoothGattCharacteristic>>(
            service_.get(), kRXCharacteristicID, rx_characteristic_uuid_,
            kCharacteristicProperties,
            device::BluetoothRemoteGattCharacteristic::PERMISSION_NONE);

    std::vector<raw_ptr<const device::BluetoothDevice, VectorExperimental>>
        devices;
    devices.push_back(mock_bluetooth_device_.get());
    ON_CALL(*adapter_, GetDevices()).WillByDefault(Return(devices));
    ON_CALL(*adapter_, GetDevice(kTestRemoteDeviceBluetoothAddress))
        .WillByDefault(Return(mock_bluetooth_device_.get()));
    ON_CALL(*mock_bluetooth_device_, GetGattService(kServiceID))
        .WillByDefault(Return(service_.get()));
    ON_CALL(*mock_bluetooth_device_, IsConnected()).WillByDefault(Return(true));
    ON_CALL(*mock_bluetooth_device_, GetConnectionInfo(_))
        .WillByDefault(Invoke(
            this, &SecureChannelBluetoothLowEnergyWeaveClientConnectionTest::
                      MockGetConnectionInfo));
    ON_CALL(*service_, GetCharacteristic(kRXCharacteristicID))
        .WillByDefault(Return(rx_characteristic_.get()));
    ON_CALL(*service_, GetCharacteristic(kTXCharacteristicID))
        .WillByDefault(Return(tx_characteristic_.get()));

    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
  }

  void TearDown() override { connection_observer_.reset(); }

  // Creates a BluetoothLowEnergyWeaveClientConnection and verifies it's in
  // DISCONNECTED state.
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> CreateConnection(
      bool should_set_low_connection_latency) {
    EXPECT_CALL(*adapter_, AddObserver(_));
    EXPECT_CALL(*adapter_, RemoveObserver(_));

    std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
        new TestBluetoothLowEnergyWeaveClientConnection(
            remote_device_, adapter_, service_uuid_,
            kTestRemoteDeviceBluetoothAddress,
            should_set_low_connection_latency));

    EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
    EXPECT_EQ(connection->status(), Connection::Status::DISCONNECTED);

    // Add the mock observer to observe on OnDidMessageSend.
    connection_observer_ =
        base::WrapUnique(new MockConnectionObserver(connection.get()));
    connection->AddObserver(connection_observer_.get());

    test_timer_ = new base::MockOneShotTimer();
    generator_ = new NiceMock<MockBluetoothLowEnergyWeavePacketGenerator>();
    receiver_ = new NiceMock<MockBluetoothLowEnergyWeavePacketReceiver>();
    connection->SetupTestDoubles(
        task_runner_, base::WrapUnique(test_timer_.get()),
        base::WrapUnique(generator_.get()), base::WrapUnique(receiver_.get()));

    return connection;
  }

  // Transitions |connection| from DISCONNECTED to WAITING_CHARACTERISTICS
  // state, without an existing GATT connection.
  void ConnectGatt(TestBluetoothLowEnergyWeaveClientConnection* connection) {
    if (connection->should_set_low_connection_latency()) {
      EXPECT_CALL(*mock_bluetooth_device_,
                  SetConnectionLatency(
                      device::BluetoothDevice::CONNECTION_LATENCY_LOW, _, _))
          .WillOnce(WithArgs<1, 2>(
              [&](base::OnceClosure callback,
                  device::BluetoothDevice::ErrorCallback error_callback) {
                connection_latency_callback_ = std::move(callback);
                connection_latency_error_callback_ = std::move(error_callback);
              }));
    }

    // Preparing |connection| for a CreateGattConnection call.
    EXPECT_CALL(*mock_bluetooth_device_, CreateGattConnection(_, _))
        .WillOnce(DoAll(MoveArg<0>(&create_gatt_connection_callback_)));

    connection->Connect();

    if (connection->should_set_low_connection_latency()) {
      // Handle setting the connection latency.
      EXPECT_EQ(connection->sub_status(),
                SubStatus::WAITING_CONNECTION_LATENCY);
      EXPECT_EQ(connection->status(), Connection::Status::IN_PROGRESS);
      ASSERT_FALSE(connection_latency_callback_.is_null());
      ASSERT_FALSE(connection_latency_error_callback_.is_null());
      std::move(connection_latency_callback_).Run();
    }

    EXPECT_EQ(connection->sub_status(), SubStatus::WAITING_GATT_CONNECTION);
    EXPECT_EQ(connection->status(), Connection::Status::IN_PROGRESS);

    // Preparing |connection| to run |create_gatt_connection_callback_|.
    ASSERT_FALSE(create_gatt_connection_callback_.is_null());
    EXPECT_CALL(*connection, CreateCharacteristicsFinder_(_, _))
        .WillOnce(DoAll(
            MoveArg<0>(&characteristics_finder_success_callback_),
            MoveArg<1>(&characteristics_finder_error_callback_),
            Return(new NiceMock<MockBluetoothLowEnergyCharacteristicsFinder>(
                remote_device_))));

    std::move(create_gatt_connection_callback_)
        .Run(std::make_unique<NiceMock<device::MockBluetoothGattConnection>>(
                 adapter_, kTestRemoteDeviceBluetoothAddress),
             /*error_code=*/std::nullopt);

    EXPECT_EQ(connection->sub_status(), SubStatus::WAITING_CHARACTERISTICS);
    EXPECT_EQ(connection->status(), Connection::Status::IN_PROGRESS);
  }

  // Transitions |connection| from WAITING_CHARACTERISTICS to
  // WAITING_NOTIFY_SESSION state.
  void CharacteristicsFound(
      TestBluetoothLowEnergyWeaveClientConnection* connection) {
    EXPECT_CALL(*rx_characteristic_, StartNotifySession_(_, _))
        .WillOnce(DoAll(MoveArg<0>(&notify_session_success_callback_),
                        MoveArg<1>(&notify_session_error_callback_)));
    EXPECT_FALSE(characteristics_finder_error_callback_.is_null());
    ASSERT_FALSE(characteristics_finder_success_callback_.is_null());

    std::move(characteristics_finder_success_callback_)
        .Run({service_uuid_, kServiceID},
             {tx_characteristic_uuid_, kTXCharacteristicID},
             {rx_characteristic_uuid_, kRXCharacteristicID});

    EXPECT_EQ(connection->sub_status(), SubStatus::WAITING_NOTIFY_SESSION);
    EXPECT_EQ(connection->status(), Connection::Status::IN_PROGRESS);
  }

  // Transitions |connection| from WAITING_NOTIFY_SESSION to
  // WAITING_CONNECTION_RESPONSE state.
  void NotifySessionStarted(
      TestBluetoothLowEnergyWeaveClientConnection* connection) {
    EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _))
        .WillOnce(
            DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                  MoveArg<2>(&write_remote_characteristic_success_callback_),
                  MoveArg<3>(&write_remote_characteristic_error_callback_)));
    EXPECT_FALSE(notify_session_error_callback_.is_null());
    ASSERT_FALSE(notify_session_success_callback_.is_null());

    // Store an alias for the notify session passed |connection|.
    std::unique_ptr<device::MockBluetoothGattNotifySession> notify_session(
        new NiceMock<device::MockBluetoothGattNotifySession>(
            tx_characteristic_->GetWeakPtr()));

    std::move(notify_session_success_callback_).Run(std::move(notify_session));
    task_runner_->RunUntilIdle();

    VerifyGattNotifySessionResult(true);

    // Written value contains only the mock Connection Request.
    EXPECT_EQ(last_value_written_on_tx_characteristic_, kConnectionRequest);

    EXPECT_EQ(connection->sub_status(), SubStatus::WAITING_CONNECTION_RESPONSE);
    EXPECT_EQ(connection->status(), Connection::Status::IN_PROGRESS);
  }

  // Transitions |connection| from WAITING_CONNECTION_RESPONSE to CONNECTED.
  void ConnectionResponseReceived(
      TestBluetoothLowEnergyWeaveClientConnection* connection,
      uint16_t selected_packet_size) {
    // Written value contains only the mock Connection Request.
    EXPECT_EQ(last_value_written_on_tx_characteristic_, kConnectionRequest);

    // OnDidSendMessage is not called.
    EXPECT_EQ(0, connection_observer_->num_send_completed());

    RunWriteCharacteristicSuccessCallback();

    // Received Connection Response.
    if (selected_packet_size == kDefaultMaxPacketSize) {
      connection->GattCharacteristicValueChanged(
          adapter_.get(), rx_characteristic_.get(), kSmallConnectionResponse);
      EXPECT_EQ(receiver_->GetMaxPacketSize(), kDefaultMaxPacketSize);
      EXPECT_EQ(generator_->GetMaxPacketSize(), kDefaultMaxPacketSize);
    } else if (selected_packet_size == kLargeMaxPacketSize) {
      connection->GattCharacteristicValueChanged(
          adapter_.get(), rx_characteristic_.get(), kLargeConnectionResponse);
      EXPECT_EQ(receiver_->GetMaxPacketSize(), kLargeMaxPacketSize);
      EXPECT_EQ(generator_->GetMaxPacketSize(), kLargeMaxPacketSize);
    } else {
      NOTREACHED_IN_MIGRATION();
    }

    EXPECT_EQ(connection->sub_status(), SubStatus::CONNECTED_AND_IDLE);
    EXPECT_EQ(connection->status(), Connection::Status::CONNECTED);
  }

  // Transitions |connection| to a DISCONNECTED state regardless of its initial
  // state.
  void Disconnect(TestBluetoothLowEnergyWeaveClientConnection* connection) {
    if (connection->IsConnected()) {
      EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _))
          .WillOnce(
              DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                    MoveArg<2>(&write_remote_characteristic_success_callback_),
                    MoveArg<3>(&write_remote_characteristic_error_callback_)));
    }

    connection->Disconnect();

    if (connection->IsConnected()) {
      EXPECT_EQ(last_value_written_on_tx_characteristic_,
                kConnectionCloseSuccess);
      RunWriteCharacteristicSuccessCallback();
    }

    EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
    EXPECT_EQ(connection->status(), Connection::Status::DISCONNECTED);
  }

  void DeleteConnectionWithoutCallingDisconnect(
      std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection>*
          connection) {
    bool was_connected = (*connection)->IsConnected();
    if (was_connected) {
      EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _))
          .WillOnce(
              DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                    MoveArg<2>(&write_remote_characteristic_success_callback_),
                    MoveArg<3>(&write_remote_characteristic_error_callback_)));
    }

    connection->reset();
  }

  void InitializeConnection(
      TestBluetoothLowEnergyWeaveClientConnection* connection,
      uint32_t selected_packet_size) {
    ConnectGatt(connection);
    CharacteristicsFound(connection);
    NotifySessionStarted(connection);
    ConnectionResponseReceived(connection, selected_packet_size);
  }

  void RunWriteCharacteristicSuccessCallback() {
    EXPECT_FALSE(write_remote_characteristic_error_callback_.is_null());
    ASSERT_FALSE(write_remote_characteristic_success_callback_.is_null());
    std::move(write_remote_characteristic_success_callback_).Run();
    task_runner_->RunUntilIdle();
  }

  void VerifyGattConnectionResultSuccess() {
    histogram_tester_.ExpectUniqueSample(
        "ProximityAuth.BluetoothGattConnectionResult",
        BluetoothLowEnergyWeaveClientConnection::GattConnectionResult::
            GATT_CONNECTION_RESULT_SUCCESS,
        1);
  }

  void VerifyGattNotifySessionResult(bool success) {
    histogram_tester_.ExpectUniqueSample(
        "ProximityAuth.BluetoothGattNotifySessionResult",
        GattServiceOperationResultSuccessOrFailure(success), 1);
  }

  void VerifyGattWriteCharacteristicResult(bool success, int num_writes) {
    histogram_tester_.ExpectBucketCount(
        "ProximityAuth.BluetoothGattWriteCharacteristicResult",
        GattServiceOperationResultSuccessOrFailure(success), num_writes);
  }

  void VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult
          expected_result) {
    histogram_tester_.ExpectUniqueSample(
        "ProximityAuth.BleWeaveConnectionResult", expected_result, 1);
    has_verified_connection_result_ = true;
  }

  BluetoothLowEnergyWeaveClientConnection::GattServiceOperationResult
  GattServiceOperationResultSuccessOrFailure(bool success) {
    return success ? BluetoothLowEnergyWeaveClientConnection::
                         GattServiceOperationResult::
                             GATT_SERVICE_OPERATION_RESULT_SUCCESS
                   : BluetoothLowEnergyWeaveClientConnection::
                         GattServiceOperationResult::
                             GATT_SERVICE_OPERATION_RESULT_GATT_ERROR_UNKNOWN;
  }

  std::optional<int32_t> GetRssi(
      TestBluetoothLowEnergyWeaveClientConnection* connection) {
    connection->GetConnectionRssi(base::BindOnce(
        &SecureChannelBluetoothLowEnergyWeaveClientConnectionTest::
            OnConnectionRssi,
        base::Unretained(this)));

    std::optional<int32_t> rssi = rssi_;
    rssi_.reset();

    return rssi;
  }

 protected:
  const multidevice::RemoteDeviceRef remote_device_;
  const device::BluetoothUUID service_uuid_;
  const device::BluetoothUUID tx_characteristic_uuid_;
  const device::BluetoothUUID rx_characteristic_uuid_;
  const multidevice::ScopedDisableLoggingForTesting disable_logging_;

  scoped_refptr<device::MockBluetoothAdapter> adapter_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> test_timer_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  std::unique_ptr<device::MockBluetoothDevice> mock_bluetooth_device_;
  std::unique_ptr<device::MockBluetoothGattService> service_;
  std::unique_ptr<device::MockBluetoothGattCharacteristic> tx_characteristic_;
  std::unique_ptr<device::MockBluetoothGattCharacteristic> rx_characteristic_;
  std::vector<uint8_t> last_value_written_on_tx_characteristic_;
  base::test::TaskEnvironment task_environment_;
  int32_t rssi_for_channel_ = device::BluetoothDevice::kUnknownPower;
  bool last_wire_message_success_;
  bool has_verified_connection_result_;
  raw_ptr<NiceMock<MockBluetoothLowEnergyWeavePacketGenerator>,
          DanglingUntriaged>
      generator_;
  raw_ptr<NiceMock<MockBluetoothLowEnergyWeavePacketReceiver>,
          DanglingUntriaged>
      receiver_;
  std::unique_ptr<MockConnectionObserver> connection_observer_;

  // Callbacks
  base::OnceClosure connection_latency_callback_;
  device::BluetoothDevice::ErrorCallback connection_latency_error_callback_;
  device::BluetoothDevice::GattConnectionCallback
      create_gatt_connection_callback_;

  BluetoothLowEnergyCharacteristicsFinder::SuccessCallback
      characteristics_finder_success_callback_;
  base::OnceClosure characteristics_finder_error_callback_;

  device::BluetoothRemoteGattCharacteristic::NotifySessionCallback
      notify_session_success_callback_;
  device::BluetoothRemoteGattCharacteristic::ErrorCallback
      notify_session_error_callback_;

  base::OnceClosure write_remote_characteristic_success_callback_;
  device::BluetoothRemoteGattCharacteristic::ErrorCallback
      write_remote_characteristic_error_callback_;

  base::HistogramTester histogram_tester_;

 private:
  void MockGetConnectionInfo(
      device::BluetoothDevice::ConnectionInfoCallback callback) {
    std::move(callback).Run(device::BluetoothDevice::ConnectionInfo(
        rssi_for_channel_, 0 /* transmit_power */, 0 /* max_transmit_power */));
  }

  void OnConnectionRssi(std::optional<int32_t> rssi) { rssi_ = rssi; }

  std::optional<int32_t> rssi_;
};

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       CreateAndDestroyWithoutConnectCallDoesntCrash) {
  std::unique_ptr<BluetoothLowEnergyWeaveClientConnection> connection =
      std::make_unique<BluetoothLowEnergyWeaveClientConnection>(
          remote_device_, adapter_, service_uuid_,
          kTestRemoteDeviceBluetoothAddress,
          true /* should_set_low_connection_latency */);

  connection.reset();
  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       DisconnectWithoutConnectDoesntCrash) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  Disconnect(connection.get());

  DeleteConnectionWithoutCallingDisconnect(&connection);
  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectSuccess) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  ConnectGatt(connection.get());
  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());
  ConnectionResponseReceived(connection.get(), kDefaultMaxPacketSize);

  DeleteConnectionWithoutCallingDisconnect(&connection);
  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectSuccessDisconnect) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  InitializeConnection(connection.get(), kDefaultMaxPacketSize);
  EXPECT_EQ(connection->sub_status(), SubStatus::CONNECTED_AND_IDLE);
  Disconnect(connection.get());

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectThenBluetoothDisconnects) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  InitializeConnection(connection.get(), kDefaultMaxPacketSize);
  EXPECT_EQ(connection->sub_status(), SubStatus::CONNECTED_AND_IDLE);

  connection->DeviceConnectedStateChanged(adapter_.get(),
                                          mock_bluetooth_device_.get(),
                                          false /* is_now_connected */);

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_ERROR_CONNECTION_DROPPED);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       DisconnectCalledTwice) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  InitializeConnection(connection.get(), kDefaultMaxPacketSize);
  EXPECT_EQ(connection->sub_status(), SubStatus::CONNECTED_AND_IDLE);

  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                MoveArg<2>(&write_remote_characteristic_success_callback_),
                MoveArg<3>(&write_remote_characteristic_error_callback_)));

  // Call Disconnect() twice; this should only result in one "close connection"
  // message (verified via WillOnce() above).
  connection->Disconnect();
  connection->Disconnect();

  EXPECT_EQ(last_value_written_on_tx_characteristic_, kConnectionCloseSuccess);
  RunWriteCharacteristicSuccessCallback();

  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::Status::DISCONNECTED);
  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectSuccessDisconnect_DoNotSetLowLatency) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(false /* should_set_low_connection_latency */));
  InitializeConnection(connection.get(), kDefaultMaxPacketSize);
  EXPECT_EQ(connection->sub_status(), SubStatus::CONNECTED_AND_IDLE);
  Disconnect(connection.get());

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectIncompleteDisconnectFromWaitingCharacteristicsState) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  ConnectGatt(connection.get());
  Disconnect(connection.get());

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectIncompleteDisconnectFromWaitingNotifySessionState) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  ConnectGatt(connection.get());
  CharacteristicsFound(connection.get());
  Disconnect(connection.get());

  DeleteConnectionWithoutCallingDisconnect(&connection);
  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectIncompleteDisconnectFromWaitingConnectionResponseState) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  ConnectGatt(connection.get());
  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());
  Disconnect(connection.get());

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectFailsCharacteristicsNotFound) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  ConnectGatt(connection.get());

  EXPECT_CALL(*rx_characteristic_, StartNotifySession_(_, _)).Times(0);
  EXPECT_FALSE(characteristics_finder_success_callback_.is_null());
  ASSERT_FALSE(characteristics_finder_error_callback_.is_null());

  std::move(characteristics_finder_error_callback_).Run();

  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::Status::DISCONNECTED);

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_ERROR_FINDING_GATT_CHARACTERISTICS);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectFailsCharacteristicsFoundThenUnavailable) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  ConnectGatt(connection.get());

  // Simulate the inability to fetch the characteristic after it was received.
  // This would most likely be due to the Bluetooth device or service being
  // removed during a connection attempt. See crbug.com/756174.
  EXPECT_CALL(*service_, GetCharacteristic(_)).WillOnce(Return(nullptr));

  EXPECT_FALSE(characteristics_finder_error_callback_.is_null());
  ASSERT_FALSE(characteristics_finder_success_callback_.is_null());
  std::move(characteristics_finder_success_callback_)
      .Run({service_uuid_, kServiceID},
           {tx_characteristic_uuid_, kTXCharacteristicID},
           {rx_characteristic_uuid_, kRXCharacteristicID});

  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::Status::DISCONNECTED);

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_ERROR_GATT_CHARACTERISTIC_NOT_AVAILABLE);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectFailsNotifySessionError) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  ConnectGatt(connection.get());
  CharacteristicsFound(connection.get());

  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _))
      .Times(0);
  EXPECT_FALSE(notify_session_success_callback_.is_null());
  ASSERT_FALSE(notify_session_error_callback_.is_null());

  std::move(notify_session_error_callback_)
      .Run(device::BluetoothGattService::GattErrorCode::kUnknown);

  VerifyGattNotifySessionResult(false);

  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::Status::DISCONNECTED);

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_ERROR_STARTING_NOTIFY_SESSION);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectFailsErrorSendingConnectionRequest) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  ConnectGatt(connection.get());
  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());

  // |connection| will call WriteRemoteCharacteristics(_,_) to try to send the
  // message |kMaxNumberOfTries| times. There is already one EXPECT_CALL for
  // WriteRemoteCharacteristic(_,_,_) in NotifySessionStated, that's why we use
  // |kMaxNumberOfTries-1| in the EXPECT_CALL statement.
  EXPECT_EQ(0, connection_observer_->num_send_completed());
  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _))
      .Times(kMaxNumberOfTries - 1)
      .WillRepeatedly(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                MoveArg<2>(&write_remote_characteristic_success_callback_),
                MoveArg<3>(&write_remote_characteristic_error_callback_)));

  for (int i = 0; i < kMaxNumberOfTries; i++) {
    EXPECT_EQ(last_value_written_on_tx_characteristic_, kConnectionRequest);
    ASSERT_FALSE(write_remote_characteristic_error_callback_.is_null());
    EXPECT_FALSE(write_remote_characteristic_success_callback_.is_null());
    std::move(write_remote_characteristic_error_callback_)
        .Run(device::BluetoothGattService::GattErrorCode::kUnknown);
    task_runner_->RunUntilIdle();
    VerifyGattWriteCharacteristicResult(false /* success */,
                                        i + 1 /* num_writes */);
  }

  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::Status::DISCONNECTED);

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_ERROR_WRITING_GATT_CHARACTERISTIC);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ReceiveMessageSmallerThanCharacteristicSize) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  InitializeConnection(connection.get(), kDefaultMaxPacketSize);

  std::string received_bytes;
  EXPECT_CALL(*connection, OnBytesReceived(_))
      .WillOnce(SaveArg<0>(&received_bytes));

  connection->GattCharacteristicValueChanged(
      adapter_.get(), rx_characteristic_.get(), kSmallPackets0);

  EXPECT_EQ(received_bytes, kSmallMessage);

  DeleteConnectionWithoutCallingDisconnect(&connection);
  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ReceiveMessageLargerThanCharacteristicSize) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));

  InitializeConnection(connection.get(), kLargeMaxPacketSize);

  std::string received_bytes;
  EXPECT_CALL(*connection, OnBytesReceived(_))
      .WillOnce(SaveArg<0>(&received_bytes));

  std::vector<Packet> packets = kLargePackets;

  for (auto packet : packets) {
    connection->GattCharacteristicValueChanged(
        adapter_.get(), rx_characteristic_.get(), packet);
  }
  EXPECT_EQ(received_bytes, kLargeMessage);

  DeleteConnectionWithoutCallingDisconnect(&connection);
  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       SendMessageSmallerThanCharacteristicSize) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  InitializeConnection(connection.get(), kDefaultMaxPacketSize);

  // Expecting a first call of WriteRemoteCharacteristic, after SendMessage is
  // called.
  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                MoveArg<2>(&write_remote_characteristic_success_callback_),
                MoveArg<3>(&write_remote_characteristic_error_callback_)));

  connection->SendMessage(
      std::make_unique<FakeWireMessage>(kSmallMessage, kTestFeature));

  EXPECT_EQ(last_value_written_on_tx_characteristic_, kSmallPackets0);

  RunWriteCharacteristicSuccessCallback();
  VerifyGattWriteCharacteristicResult(true /* success */, 2 /* num_writes */);

  EXPECT_EQ(1, connection_observer_->num_send_completed());
  EXPECT_EQ(kSmallMessage, connection_observer_->GetLastDeserializedMessage());
  EXPECT_TRUE(connection_observer_->last_send_success());

  DeleteConnectionWithoutCallingDisconnect(&connection);
  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       SendMessageLargerThanCharacteristicSize) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));

  InitializeConnection(connection.get(), kLargeMaxPacketSize);

  // Expecting a first call of WriteRemoteCharacteristic, after SendMessage is
  // called.
  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                MoveArg<2>(&write_remote_characteristic_success_callback_),
                MoveArg<3>(&write_remote_characteristic_error_callback_)));

  connection->SendMessage(
      std::make_unique<FakeWireMessage>(kLargeMessage, kTestFeature));

  EXPECT_EQ(last_value_written_on_tx_characteristic_, kLargePackets0);
  std::vector<uint8_t> bytes_received(
      last_value_written_on_tx_characteristic_.begin() + 1,
      last_value_written_on_tx_characteristic_.end());

  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                MoveArg<2>(&write_remote_characteristic_success_callback_),
                MoveArg<3>(&write_remote_characteristic_error_callback_)));

  RunWriteCharacteristicSuccessCallback();
  VerifyGattWriteCharacteristicResult(true /* success */, 2 /* num_writes */);
  bytes_received.insert(bytes_received.end(),
                        last_value_written_on_tx_characteristic_.begin() + 1,
                        last_value_written_on_tx_characteristic_.end());

  std::vector<uint8_t> expected(std::begin(kLargeMessage),
                                std::end(kLargeMessage));
  expected.pop_back();  // Drop the null '\0' at the end.
  EXPECT_EQ(expected, bytes_received);

  RunWriteCharacteristicSuccessCallback();
  VerifyGattWriteCharacteristicResult(true /* success */, 3 /* num_writes */);

  EXPECT_EQ(1, connection_observer_->num_send_completed());
  EXPECT_EQ(kLargeMessage, connection_observer_->GetLastDeserializedMessage());
  EXPECT_TRUE(connection_observer_->last_send_success());

  DeleteConnectionWithoutCallingDisconnect(&connection);
  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       SendMessageKeepsFailing) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  InitializeConnection(connection.get(), kDefaultMaxPacketSize);

  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _))
      .Times(kMaxNumberOfTries)
      .WillRepeatedly(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                MoveArg<2>(&write_remote_characteristic_success_callback_),
                MoveArg<3>(&write_remote_characteristic_error_callback_)));

  connection->SendMessage(
      std::make_unique<FakeWireMessage>(kSmallMessage, kTestFeature));

  for (int i = 0; i < kMaxNumberOfTries; i++) {
    EXPECT_EQ(last_value_written_on_tx_characteristic_, kSmallPackets0);
    ASSERT_FALSE(write_remote_characteristic_error_callback_.is_null());
    EXPECT_FALSE(write_remote_characteristic_success_callback_.is_null());
    std::move(write_remote_characteristic_error_callback_)
        .Run(device::BluetoothGattService::GattErrorCode::kUnknown);
    task_runner_->RunUntilIdle();
    VerifyGattWriteCharacteristicResult(false /* success */,
                                        i + 1 /* num_writes */);
    if (i == kMaxNumberOfTries - 1) {
      EXPECT_EQ(1, connection_observer_->num_send_completed());
      EXPECT_EQ(kSmallMessage,
                connection_observer_->GetLastDeserializedMessage());
      EXPECT_FALSE(connection_observer_->last_send_success());
    } else {
      EXPECT_EQ(0, connection_observer_->num_send_completed());
    }
  }

  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::Status::DISCONNECTED);

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_ERROR_WRITING_GATT_CHARACTERISTIC);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ReceiveCloseConnectionTest) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  InitializeConnection(connection.get(), kDefaultMaxPacketSize);

  connection->GattCharacteristicValueChanged(
      adapter_.get(), rx_characteristic_.get(), kConnectionCloseUnknownError);

  EXPECT_EQ(receiver_->GetReasonForClose(), ReasonForClose::UNKNOWN_ERROR);
  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::Status::DISCONNECTED);

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ReceiverErrorTest) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));

  InitializeConnection(connection.get(), kDefaultMaxPacketSize);

  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                MoveArg<2>(&write_remote_characteristic_success_callback_),
                MoveArg<3>(&write_remote_characteristic_error_callback_)));

  connection->GattCharacteristicValueChanged(
      adapter_.get(), rx_characteristic_.get(), kErroneousPacket);

  EXPECT_EQ(last_value_written_on_tx_characteristic_,
            kConnectionCloseApplicationError);
  EXPECT_EQ(receiver_->GetReasonToClose(), ReasonForClose::APPLICATION_ERROR);

  RunWriteCharacteristicSuccessCallback();
  VerifyGattWriteCharacteristicResult(true /* success */, 2 /* num_writes */);
  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::Status::DISCONNECTED);

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ReceiverErrorWithPendingWritesTest) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));

  InitializeConnection(connection.get(), kLargeMaxPacketSize);

  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                MoveArg<2>(&write_remote_characteristic_success_callback_),
                MoveArg<3>(&write_remote_characteristic_error_callback_)));

  connection->SendMessage(
      std::make_unique<FakeWireMessage>(kLargeMessage, kTestFeature));

  connection->GattCharacteristicValueChanged(
      adapter_.get(), rx_characteristic_.get(), kErroneousPacket);

  EXPECT_EQ(last_value_written_on_tx_characteristic_, kLargePackets0);

  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                MoveArg<2>(&write_remote_characteristic_success_callback_),
                MoveArg<3>(&write_remote_characteristic_error_callback_)));

  RunWriteCharacteristicSuccessCallback();
  VerifyGattWriteCharacteristicResult(true /* success */, 2 /* num_writes */);

  EXPECT_EQ(last_value_written_on_tx_characteristic_,
            kConnectionCloseApplicationError);
  EXPECT_EQ(receiver_->GetReasonToClose(), ReasonForClose::APPLICATION_ERROR);

  RunWriteCharacteristicSuccessCallback();
  VerifyGattWriteCharacteristicResult(true /* success */, 3 /* num_writes */);
  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::Status::DISCONNECTED);

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

// Test for fix to crbug.com/708744. Without the fix, this test will crash.
TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ObserverDeletesConnectionOnDisconnect) {
  TestBluetoothLowEnergyWeaveClientConnection* connection =
      CreateConnection(true /* should_set_low_connection_latency */).release();
  connection_observer_->set_delete_on_disconnect(true);

  InitializeConnection(connection, kDefaultMaxPacketSize);

  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                MoveArg<2>(&write_remote_characteristic_success_callback_),
                MoveArg<3>(&write_remote_characteristic_error_callback_)));

  connection->GattCharacteristicValueChanged(
      adapter_.get(), rx_characteristic_.get(), kErroneousPacket);

  EXPECT_EQ(last_value_written_on_tx_characteristic_,
            kConnectionCloseApplicationError);
  EXPECT_EQ(receiver_->GetReasonToClose(), ReasonForClose::APPLICATION_ERROR);

  RunWriteCharacteristicSuccessCallback();
  VerifyGattWriteCharacteristicResult(true /* success */, 2 /* num_writes */);

  // We cannot check if connection's status and sub_status are DISCONNECTED
  // because it has been deleted.

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

// Test for fix to crbug.com/ 751884. Without the fix, this test will crash.
TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ObserverDeletesConnectionOnMessageSent) {
  TestBluetoothLowEnergyWeaveClientConnection* connection =
      CreateConnection(true /* should_set_low_connection_latency */).release();
  connection_observer_->set_delete_on_message_sent(true);

  InitializeConnection(connection, kDefaultMaxPacketSize);

  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _))
      .Times(2)
      .WillRepeatedly(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                MoveArg<2>(&write_remote_characteristic_success_callback_),
                MoveArg<3>(&write_remote_characteristic_error_callback_)));

  connection->SendMessage(
      std::make_unique<FakeWireMessage>(kSmallMessage, kTestFeature));
  EXPECT_EQ(last_value_written_on_tx_characteristic_, kSmallPackets0);

  RunWriteCharacteristicSuccessCallback();
  VerifyGattWriteCharacteristicResult(true /* success */, 2 /* num_writes */);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(1, connection_observer_->num_send_completed());
  EXPECT_EQ(kSmallMessage, connection_observer_->GetLastDeserializedMessage());
  EXPECT_TRUE(connection_observer_->last_send_success());

  // Connection close packet should have been sent when the object was deleted.
  EXPECT_EQ(last_value_written_on_tx_characteristic_, kConnectionCloseSuccess);

  // We cannot check if connection's status and sub_status are DISCONNECTED
  // because it has been deleted.

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       WriteConnectionCloseMaxNumberOfTimes) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));

  InitializeConnection(connection.get(), kDefaultMaxPacketSize);
  EXPECT_EQ(connection->sub_status(), SubStatus::CONNECTED_AND_IDLE);

  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                MoveArg<2>(&write_remote_characteristic_success_callback_),
                MoveArg<3>(&write_remote_characteristic_error_callback_)));
  connection->Disconnect();
  EXPECT_EQ(connection->sub_status(), SubStatus::CONNECTED_AND_SENDING_MESSAGE);

  for (int i = 0; i < kMaxNumberOfTries; i++) {
    EXPECT_EQ(last_value_written_on_tx_characteristic_,
              kConnectionCloseSuccess);
    ASSERT_FALSE(write_remote_characteristic_error_callback_.is_null());
    EXPECT_FALSE(write_remote_characteristic_success_callback_.is_null());

    if (i != kMaxNumberOfTries - 1) {
      EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _))
          .WillOnce(
              DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                    MoveArg<2>(&write_remote_characteristic_success_callback_),
                    MoveArg<3>(&write_remote_characteristic_error_callback_)));
    }

    std::move(write_remote_characteristic_error_callback_)
        .Run(device::BluetoothGattService::GattErrorCode::kUnknown);
    task_runner_->RunUntilIdle();
    VerifyGattWriteCharacteristicResult(false /* success */,
                                        i + 1 /* num_writes */);
  }

  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::Status::DISCONNECTED);

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_ERROR_WRITING_GATT_CHARACTERISTIC);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectAfterADelayWhenThrottled) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));

  EXPECT_CALL(*mock_bluetooth_device_,
              SetConnectionLatency(
                  device::BluetoothDevice::CONNECTION_LATENCY_LOW, _, _))
      .WillOnce(WithArgs<1, 2>(
          [&](base::OnceClosure callback,
              device::BluetoothDevice::ErrorCallback error_callback) {
            connection_latency_callback_ = std::move(callback);
            connection_latency_error_callback_ = std::move(error_callback);
          }));
  EXPECT_CALL(*mock_bluetooth_device_, CreateGattConnection(_, _))
      .WillOnce(DoAll(MoveArg<0>(&create_gatt_connection_callback_)));

  // No GATT connection should be created before the delay.
  connection->Connect();
  EXPECT_EQ(connection->sub_status(), SubStatus::WAITING_CONNECTION_LATENCY);
  EXPECT_EQ(connection->status(), Connection::Status::IN_PROGRESS);
  EXPECT_TRUE(create_gatt_connection_callback_.is_null());

  // A GATT connection should be created after the delay and after setting the
  // connection latency.
  task_runner_->RunUntilIdle();
  ASSERT_FALSE(connection_latency_callback_.is_null());
  std::move(connection_latency_callback_).Run();

  ASSERT_FALSE(create_gatt_connection_callback_.is_null());

  // Preparing |connection| to run |create_gatt_connection_callback_|.
  EXPECT_CALL(*connection, CreateCharacteristicsFinder_(_, _))
      .WillOnce(DoAll(
          MoveArg<0>(&characteristics_finder_success_callback_),
          MoveArg<1>(&characteristics_finder_error_callback_),
          Return(new NiceMock<MockBluetoothLowEnergyCharacteristicsFinder>(
              remote_device_))));

  std::move(create_gatt_connection_callback_)
      .Run(std::make_unique<NiceMock<device::MockBluetoothGattConnection>>(
               adapter_, kTestRemoteDeviceBluetoothAddress),
           /*error_code=*/std::nullopt);

  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());
  ConnectionResponseReceived(connection.get(), kDefaultMaxPacketSize);

  VerifyGattConnectionResultSuccess();

  DeleteConnectionWithoutCallingDisconnect(&connection);
  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       SetConnectionLatencyError) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));

  EXPECT_CALL(*mock_bluetooth_device_,
              SetConnectionLatency(
                  device::BluetoothDevice::CONNECTION_LATENCY_LOW, _, _))
      .WillOnce(WithArgs<1, 2>(
          [&](base::OnceClosure callback,
              device::BluetoothDevice::ErrorCallback error_callback) {
            connection_latency_callback_ = std::move(callback);
            connection_latency_error_callback_ = std::move(error_callback);
          }));

  // Even if setting the connection interval fails, we should still connect.
  connection->Connect();
  ASSERT_FALSE(connection_latency_error_callback_.is_null());

  EXPECT_CALL(*mock_bluetooth_device_, CreateGattConnection(_, _))
      .WillOnce(DoAll(MoveArg<0>(&create_gatt_connection_callback_)));
  std::move(connection_latency_error_callback_).Run();
  ASSERT_FALSE(create_gatt_connection_callback_.is_null());

  // Preparing |connection| to run |create_gatt_connection_callback_|.
  EXPECT_CALL(*connection, CreateCharacteristicsFinder_(_, _))
      .WillOnce(DoAll(
          MoveArg<0>(&characteristics_finder_success_callback_),
          MoveArg<1>(&characteristics_finder_error_callback_),
          Return(new NiceMock<MockBluetoothLowEnergyCharacteristicsFinder>(
              remote_device_))));

  std::move(create_gatt_connection_callback_)
      .Run(std::make_unique<NiceMock<device::MockBluetoothGattConnection>>(
               adapter_, kTestRemoteDeviceBluetoothAddress),
           /*error_code=*/std::nullopt);

  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());
  ConnectionResponseReceived(connection.get(), kDefaultMaxPacketSize);

  VerifyGattConnectionResultSuccess();

  DeleteConnectionWithoutCallingDisconnect(&connection);
  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       Timeout_ConnectionLatency) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));

  EXPECT_CALL(*mock_bluetooth_device_,
              SetConnectionLatency(
                  device::BluetoothDevice::CONNECTION_LATENCY_LOW, _, _))
      .WillOnce(WithArgs<1, 2>(
          [&](base::OnceClosure callback,
              device::BluetoothDevice::ErrorCallback error_callback) {
            connection_latency_callback_ = std::move(callback);
            connection_latency_error_callback_ = std::move(error_callback);
          }));

  // Call Connect(), which should set the connection latency.
  connection->Connect();
  EXPECT_EQ(connection->sub_status(), SubStatus::WAITING_CONNECTION_LATENCY);
  EXPECT_EQ(connection->status(), Connection::Status::IN_PROGRESS);
  ASSERT_FALSE(connection_latency_callback_.is_null());
  ASSERT_FALSE(connection_latency_error_callback_.is_null());

  EXPECT_CALL(*mock_bluetooth_device_, CreateGattConnection(_, _))
      .WillOnce(DoAll(MoveArg<0>(&create_gatt_connection_callback_)));

  // Simulate a timeout.
  test_timer_->Fire();

  ASSERT_FALSE(create_gatt_connection_callback_.is_null());

  // Robustness check: simulate the SetConnectionLatency success callback firing
  // while a GATT connection is in progress. It should recognize that a GATT
  // connection is in progress and not call CreateGattConnection a 2nd time.
  std::move(connection_latency_callback_).Run();

  // Preparing |connection| to run |create_gatt_connection_callback_|.
  EXPECT_CALL(*connection, CreateCharacteristicsFinder_(_, _))
      .WillOnce(DoAll(
          MoveArg<0>(&characteristics_finder_success_callback_),
          MoveArg<1>(&characteristics_finder_error_callback_),
          Return(new NiceMock<MockBluetoothLowEnergyCharacteristicsFinder>(
              remote_device_))));

  std::move(create_gatt_connection_callback_)
      .Run(std::make_unique<NiceMock<device::MockBluetoothGattConnection>>(
               adapter_, kTestRemoteDeviceBluetoothAddress),
           /*error_code=*/std::nullopt);

  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());
  ConnectionResponseReceived(connection.get(), kDefaultMaxPacketSize);

  VerifyGattConnectionResultSuccess();

  DeleteConnectionWithoutCallingDisconnect(&connection);
  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       Timeout_GattConnection) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));

  EXPECT_CALL(*mock_bluetooth_device_,
              SetConnectionLatency(
                  device::BluetoothDevice::CONNECTION_LATENCY_LOW, _, _))
      .WillOnce(WithArgs<1, 2>(
          [&](base::OnceClosure callback,
              device::BluetoothDevice::ErrorCallback error_callback) {
            connection_latency_callback_ = std::move(callback);
            connection_latency_error_callback_ = std::move(error_callback);
          }));

  // Preparing |connection| for a CreateGattConnection call.
  EXPECT_CALL(*mock_bluetooth_device_, CreateGattConnection(_, _))
      .WillOnce(DoAll(MoveArg<0>(&create_gatt_connection_callback_)));

  connection->Connect();

  // Handle setting the connection latency.
  EXPECT_EQ(connection->sub_status(), SubStatus::WAITING_CONNECTION_LATENCY);
  EXPECT_EQ(connection->status(), Connection::Status::IN_PROGRESS);
  ASSERT_FALSE(connection_latency_callback_.is_null());
  ASSERT_FALSE(connection_latency_error_callback_.is_null());
  std::move(connection_latency_callback_).Run();

  EXPECT_EQ(connection->sub_status(), SubStatus::WAITING_GATT_CONNECTION);
  EXPECT_EQ(connection->status(), Connection::Status::IN_PROGRESS);

  // Simulate a timeout.
  test_timer_->Fire();

  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::Status::DISCONNECTED);

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_TIMEOUT_CREATING_GATT_CONNECTION);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       Timeout_GattCharacteristics) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  ConnectGatt(connection.get());
  EXPECT_EQ(connection->sub_status(), SubStatus::WAITING_CHARACTERISTICS);
  EXPECT_EQ(connection->status(), Connection::Status::IN_PROGRESS);

  // Simulate a timeout.
  test_timer_->Fire();

  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::Status::DISCONNECTED);

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_TIMEOUT_FINDING_GATT_CHARACTERISTICS);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       Timeout_NotifySession) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  ConnectGatt(connection.get());
  CharacteristicsFound(connection.get());
  EXPECT_EQ(connection->sub_status(), SubStatus::WAITING_NOTIFY_SESSION);
  EXPECT_EQ(connection->status(), Connection::Status::IN_PROGRESS);

  // Simulate a timeout.
  test_timer_->Fire();

  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::Status::DISCONNECTED);

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_TIMEOUT_STARTING_NOTIFY_SESSION);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       Timeout_ConnectionResponse) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));
  ConnectGatt(connection.get());
  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());

  // Simulate a timeout.
  test_timer_->Fire();

  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::Status::DISCONNECTED);

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_TIMEOUT_WAITING_FOR_CONNECTION_RESPONSE);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest,
       Timeout_SendingMessage) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));

  InitializeConnection(connection.get(), kDefaultMaxPacketSize);
  EXPECT_EQ(connection->sub_status(), SubStatus::CONNECTED_AND_IDLE);
  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic_(_, _, _, _));

  connection->SendMessage(
      std::make_unique<FakeWireMessage>(kSmallMessage, kTestFeature));
  EXPECT_EQ(connection->sub_status(), SubStatus::CONNECTED_AND_SENDING_MESSAGE);

  // Simulate a timeout.
  test_timer_->Fire();

  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::Status::DISCONNECTED);

  VerifyBleWeaveConnectionResult(
      BluetoothLowEnergyWeaveClientConnection::BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_TIMEOUT_WAITING_FOR_MESSAGE_TO_SEND);
}

TEST_F(SecureChannelBluetoothLowEnergyWeaveClientConnectionTest, GetRssi) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection(true /* should_set_low_connection_latency */));

  EXPECT_FALSE(GetRssi(connection.get()));

  rssi_for_channel_ = -50;
  EXPECT_EQ(-50, GetRssi(connection.get()));

  rssi_for_channel_ = -40;
  EXPECT_EQ(-40, GetRssi(connection.get()));

  rssi_for_channel_ = -30;
  EXPECT_EQ(-30, GetRssi(connection.get()));
}

}  // namespace ash::secure_channel::weave
