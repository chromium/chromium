// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/ble_connection_manager_impl.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/timer_factory/fake_timer_factory.h"
#include "chromeos/ash/components/timer_factory/timer_factory.h"
#include "chromeos/ash/services/secure_channel/authenticated_channel_impl.h"
#include "chromeos/ash/services/secure_channel/ble_advertiser_impl.h"
#include "chromeos/ash/services/secure_channel/ble_initiator_failure_type.h"
#include "chromeos/ash/services/secure_channel/ble_listener_failure_type.h"
#include "chromeos/ash/services/secure_channel/ble_weave_client_connection.h"
#include "chromeos/ash/services/secure_channel/fake_authenticated_channel.h"
#include "chromeos/ash/services/secure_channel/fake_ble_advertiser.h"
#include "chromeos/ash/services/secure_channel/fake_ble_scanner.h"
#include "chromeos/ash/services/secure_channel/fake_ble_synchronizer.h"
#include "chromeos/ash/services/secure_channel/fake_bluetooth_helper.h"
#include "chromeos/ash/services/secure_channel/fake_connection.h"
#include "chromeos/ash/services/secure_channel/fake_secure_channel_connection.h"
#include "chromeos/ash/services/secure_channel/fake_secure_channel_disconnector.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/ble_constants.h"
#include "chromeos/ash/services/secure_channel/secure_channel.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

const size_t kNumTestDevices = 5;

constexpr base::TimeDelta kScanToAdvertisementTime = base::Seconds(3);
constexpr base::TimeDelta kAdvertisementToConnectionTime = base::Seconds(2);
constexpr base::TimeDelta kConnectionToAuthenticationTime = base::Seconds(1);

class FakeBleAdvertiserFactory : public BleAdvertiserImpl::Factory {
 public:
  FakeBleAdvertiserFactory(
      FakeBluetoothHelper* expected_fake_bluetooth_helper,
      FakeBleSynchronizer* expected_fake_ble_synchronizer,
      ash::timer_factory::FakeTimerFactory* expected_fake_timer_factory)
      : expected_fake_bluetooth_helper_(expected_fake_bluetooth_helper),
        expected_fake_ble_synchronizer_(expected_fake_ble_synchronizer),
        expected_fake_timer_factory_(expected_fake_timer_factory) {}

  FakeBleAdvertiserFactory(const FakeBleAdvertiserFactory&) = delete;
  FakeBleAdvertiserFactory& operator=(const FakeBleAdvertiserFactory&) = delete;

  ~FakeBleAdvertiserFactory() override = default;

  FakeBleAdvertiser* instance() { return instance_; }

 private:
  // BleAdvertiserImpl::Factory:
  std::unique_ptr<BleAdvertiser> CreateInstance(
      BleAdvertiser::Delegate* delegate,
      BluetoothHelper* bluetooth_helper,
      BleSynchronizerBase* ble_synchronizer_base,
      ash::timer_factory::TimerFactory* timer_factory,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner) override {
    EXPECT_EQ(expected_fake_bluetooth_helper_, bluetooth_helper);
    EXPECT_EQ(expected_fake_ble_synchronizer_, ble_synchronizer_base);
    EXPECT_EQ(expected_fake_timer_factory_, timer_factory);
    EXPECT_FALSE(instance_);

    auto instance = std::make_unique<FakeBleAdvertiser>(delegate);
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<FakeBleAdvertiser, DanglingUntriaged> instance_ = nullptr;

  raw_ptr<FakeBluetoothHelper, DanglingUntriaged>
      expected_fake_bluetooth_helper_;
  raw_ptr<FakeBleSynchronizer, DanglingUntriaged>
      expected_fake_ble_synchronizer_;
  raw_ptr<ash::timer_factory::FakeTimerFactory, DanglingUntriaged>
      expected_fake_timer_factory_;
};

class FakeWeaveClientConnectionFactory
    : public weave::BluetoothLowEnergyWeaveClientConnection::Factory {
 public:
  FakeWeaveClientConnectionFactory(
      scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
          expected_mock_adapter)
      : expected_mock_adapter_(expected_mock_adapter) {}

  FakeWeaveClientConnectionFactory(const FakeWeaveClientConnectionFactory&) =
      delete;
  FakeWeaveClientConnectionFactory& operator=(
      const FakeWeaveClientConnectionFactory&) = delete;

  virtual ~FakeWeaveClientConnectionFactory() = default;

  void set_expected_bluetooth_device(
      device::MockBluetoothDevice* expected_bluetooth_device) {
    expected_bluetooth_device_ = expected_bluetooth_device;
  }

  FakeConnection* last_created_instance() { return last_created_instance_; }

 private:
  // cryptauth::BluetoothLowEnergyWeaveClientConnection::Factory:
  std::unique_ptr<Connection> CreateInstance(
      multidevice::RemoteDeviceRef remote_device,
      scoped_refptr<device::BluetoothAdapter> adapter,
      const device::BluetoothUUID remote_service_uuid,
      const std::string& device_address,
      bool should_set_low_connection_latency) override {
    EXPECT_EQ(expected_mock_adapter_, adapter);
    EXPECT_EQ(device::BluetoothUUID(kGattServerUuid), remote_service_uuid);
    EXPECT_TRUE(should_set_low_connection_latency);

    auto instance = std::make_unique<FakeConnection>(remote_device);
    last_created_instance_ = instance.get();
    return instance;
  }

  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
      expected_mock_adapter_;
  raw_ptr<device::MockBluetoothDevice, DanglingUntriaged>
      expected_bluetooth_device_;

  raw_ptr<FakeConnection, DanglingUntriaged> last_created_instance_ = nullptr;
};

class FakeSecureChannelFactory : public SecureChannel::Factory {
 public:
  FakeSecureChannelFactory(
      FakeWeaveClientConnectionFactory* fake_weave_client_connection_factory)
      : fake_weave_client_connection_factory_(
            fake_weave_client_connection_factory) {}

  FakeSecureChannelFactory(const FakeSecureChannelFactory&) = delete;
  FakeSecureChannelFactory& operator=(const FakeSecureChannelFactory&) = delete;

  virtual ~FakeSecureChannelFactory() = default;

  FakeSecureChannelConnection* last_created_instance() {
    return last_created_instance_;
  }

 private:
  // SecureChannel::Factory:
  std::unique_ptr<SecureChannel> CreateInstance(
      std::unique_ptr<Connection> connection) override {
    EXPECT_EQ(fake_weave_client_connection_factory_->last_created_instance(),
              connection.get());

    auto instance =
        std::make_unique<FakeSecureChannelConnection>(std::move(connection));
    last_created_instance_ = instance.get();
    return instance;
  }

  raw_ptr<FakeWeaveClientConnectionFactory>
      fake_weave_client_connection_factory_;

  raw_ptr<FakeSecureChannelConnection, DanglingUntriaged>
      last_created_instance_ = nullptr;
};

class FakeAuthenticatedChannelFactory
    : public AuthenticatedChannelImpl::Factory {
 public:
  FakeAuthenticatedChannelFactory() = default;

  FakeAuthenticatedChannelFactory(const FakeAuthenticatedChannelFactory&) =
      delete;
  FakeAuthenticatedChannelFactory& operator=(
      const FakeAuthenticatedChannelFactory&) = delete;

  ~FakeAuthenticatedChannelFactory() override = default;

  void SetExpectationsForNextCall(
      FakeSecureChannelConnection* expected_fake_secure_channel,
      bool expected_to_be_background_advertisement) {
    expected_fake_secure_channel_ = expected_fake_secure_channel;
    expected_to_be_background_advertisement_ =
        expected_to_be_background_advertisement;
  }

  FakeAuthenticatedChannel* last_created_instance() {
    return last_created_instance_;
  }

 private:
  // AuthenticatedChannelImpl::Factory:
  std::unique_ptr<AuthenticatedChannel> CreateInstance(
      const std::vector<mojom::ConnectionCreationDetail>&
          connection_creation_details,
      std::unique_ptr<SecureChannel> secure_channel) override {
    EXPECT_EQ(expected_fake_secure_channel_, secure_channel.get());
    EXPECT_EQ(1u, connection_creation_details.size());
    if (expected_to_be_background_advertisement_) {
      EXPECT_EQ(mojom::ConnectionCreationDetail::
                    REMOTE_DEVICE_USED_BACKGROUND_BLE_ADVERTISING,
                connection_creation_details[0]);
    } else {
      EXPECT_EQ(mojom::ConnectionCreationDetail::
                    REMOTE_DEVICE_USED_FOREGROUND_BLE_ADVERTISING,
                connection_creation_details[0]);
    }

    auto instance = std::make_unique<FakeAuthenticatedChannel>();
    last_created_instance_ = instance.get();
    return instance;
  }

  raw_ptr<FakeSecureChannelConnection, DanglingUntriaged>
      expected_fake_secure_channel_ = nullptr;
  bool expected_to_be_background_advertisement_ = false;

  raw_ptr<FakeAuthenticatedChannel, DanglingUntriaged> last_created_instance_ =
      nullptr;
};

}  // namespace

class SecureChannelBleConnectionManagerImplTest : public testing::Test {
 public:
  SecureChannelBleConnectionManagerImplTest(
      const SecureChannelBleConnectionManagerImplTest&) = delete;
  SecureChannelBleConnectionManagerImplTest& operator=(
      const SecureChannelBleConnectionManagerImplTest&) = delete;

 protected:
  SecureChannelBleConnectionManagerImplTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        test_devices_(
            multidevice::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  ~SecureChannelBleConnectionManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();

    fake_bluetooth_helper_ = std::make_unique<FakeBluetoothHelper>();
    fake_ble_synchronizer_ = std::make_unique<FakeBleSynchronizer>();
    fake_ble_scanner_ = std::make_unique<FakeBleScanner>();
    fake_secure_channel_disconnector_ =
        std::make_unique<FakeSecureChannelDisconnector>();

    fake_timer_factory_ =
        std::make_unique<ash::timer_factory::FakeTimerFactory>();

    test_clock_ = std::make_unique<base::SimpleTestClock>();
    test_clock_->SetNow(base::Time::UnixEpoch());

    fake_ble_advertiser_factory_ = std::make_unique<FakeBleAdvertiserFactory>(
        fake_bluetooth_helper_.get(), fake_ble_synchronizer_.get(),
        fake_timer_factory_.get());
    BleAdvertiserImpl::Factory::SetFactoryForTesting(
        fake_ble_advertiser_factory_.get());

    fake_weave_client_connection_factory_ =
        std::make_unique<FakeWeaveClientConnectionFactory>(mock_adapter_);
    weave::BluetoothLowEnergyWeaveClientConnection::Factory::
        SetFactoryForTesting(fake_weave_client_connection_factory_.get());

    fake_secure_channel_factory_ = std::make_unique<FakeSecureChannelFactory>(
        fake_weave_client_connection_factory_.get());
    SecureChannel::Factory::SetFactoryForTesting(
        fake_secure_channel_factory_.get());

    fake_authenticated_channel_factory_ =
        std::make_unique<FakeAuthenticatedChannelFactory>();
    AuthenticatedChannelImpl::Factory::SetFactoryForTesting(
        fake_authenticated_channel_factory_.get());

    manager_ = BleConnectionManagerImpl::Factory::Create(
        mock_adapter_, fake_bluetooth_helper_.get(),
        fake_ble_synchronizer_.get(), fake_ble_scanner_.get(),
        fake_secure_channel_disconnector_.get(), fake_timer_factory_.get(),
        test_clock_.get());
  }

  void TearDown() override {
    BleAdvertiserImpl::Factory::SetFactoryForTesting(nullptr);
    weave::BluetoothLowEnergyWeaveClientConnection::Factory::
        SetFactoryForTesting(nullptr);
    SecureChannel::Factory::SetFactoryForTesting(nullptr);
    AuthenticatedChannelImpl::Factory::SetFactoryForTesting(nullptr);
  }

  void AttemptBleInitiatorConnection(const DeviceIdPair& device_id_pair,
                                     ConnectionPriority connection_priority,
                                     bool expected_to_add_request,
                                     bool should_cancel_attempt_on_failure) {
    SetInRemoteDeviceIdToMetadataMap(
        device_id_pair, ConnectionRole::kInitiatorRole, connection_priority);

    EXPECT_FALSE(fake_ble_advertiser()->GetPriorityForRequest(device_id_pair));

    manager_->AttemptBleInitiatorConnection(
        device_id_pair, connection_priority,
        base::BindOnce(
            &SecureChannelBleConnectionManagerImplTest::OnConnectionSuccess,
            base::Unretained(this), device_id_pair,
            false /* created_via_background_advertisement */),
        base::BindRepeating(
            &SecureChannelBleConnectionManagerImplTest::OnBleInitiatorFailure,
            base::Unretained(this), device_id_pair,
            should_cancel_attempt_on_failure));

    if (expected_to_add_request) {
      EXPECT_EQ(connection_priority,
                *fake_ble_advertiser()->GetPriorityForRequest(device_id_pair));
      EXPECT_TRUE(fake_ble_scanner_->HasScanRequest(ConnectionAttemptDetails(
          device_id_pair, ConnectionMedium::kBluetoothLowEnergy,
          ConnectionRole::kInitiatorRole)));
    } else {
      EXPECT_FALSE(
          fake_ble_advertiser()->GetPriorityForRequest(device_id_pair));
      EXPECT_FALSE(fake_ble_scanner_->HasScanRequest(ConnectionAttemptDetails(
          device_id_pair, ConnectionMedium::kBluetoothLowEnergy,
          ConnectionRole::kInitiatorRole)));
    }
  }

  void UpdateBleInitiatorConnectionPriority(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority,
      bool expected_to_update_priority) {
    SetInRemoteDeviceIdToMetadataMap(
        device_id_pair, ConnectionRole::kInitiatorRole, connection_priority);

    manager_->UpdateBleInitiatorConnectionPriority(device_id_pair,
                                                   connection_priority);

    if (expected_to_update_priority) {
      EXPECT_EQ(connection_priority,
                *fake_ble_advertiser()->GetPriorityForRequest(device_id_pair));
      EXPECT_TRUE(fake_ble_scanner_->HasScanRequest(ConnectionAttemptDetails(
          device_id_pair, ConnectionMedium::kBluetoothLowEnergy,
          ConnectionRole::kInitiatorRole)));
    } else {
      EXPECT_FALSE(
          fake_ble_advertiser()->GetPriorityForRequest(device_id_pair));
      EXPECT_FALSE(fake_ble_scanner_->HasScanRequest(ConnectionAttemptDetails(
          device_id_pair, ConnectionMedium::kBluetoothLowEnergy,
          ConnectionRole::kInitiatorRole)));
    }
  }

  void CancelBleInitiatorConnectionAttempt(const DeviceIdPair& device_id_pair) {
    RemoveFromRemoteDeviceIdToMetadataMap(device_id_pair,
                                          ConnectionRole::kInitiatorRole);
    manager_->CancelBleInitiatorConnectionAttempt(device_id_pair);
    EXPECT_FALSE(fake_ble_advertiser()->GetPriorityForRequest(device_id_pair));
    EXPECT_FALSE(fake_ble_scanner_->HasScanRequest(ConnectionAttemptDetails(
        device_id_pair, ConnectionMedium::kBluetoothLowEnergy,
        ConnectionRole::kInitiatorRole)));
  }

  void AttemptBleListenerConnection(const DeviceIdPair& device_id_pair,
                                    ConnectionPriority connection_priority,
                                    bool expected_to_add_request,
                                    bool should_cancel_attempt_on_failure) {
    SetInRemoteDeviceIdToMetadataMap(
        device_id_pair, ConnectionRole::kListenerRole, connection_priority);

    manager_->AttemptBleListenerConnection(
        device_id_pair, connection_priority,
        base::BindOnce(
            &SecureChannelBleConnectionManagerImplTest::OnConnectionSuccess,
            base::Unretained(this), device_id_pair,
            true /* created_via_background_advertisement */),
        base::BindRepeating(
            &SecureChannelBleConnectionManagerImplTest::OnBleListenerFailure,
            base::Unretained(this), device_id_pair,
            should_cancel_attempt_on_failure));

    if (expected_to_add_request) {
      EXPECT_TRUE(fake_ble_scanner_->HasScanRequest(ConnectionAttemptDetails(
          device_id_pair, ConnectionMedium::kBluetoothLowEnergy,
          ConnectionRole::kListenerRole)));
    } else {
      EXPECT_FALSE(fake_ble_scanner_->HasScanRequest(ConnectionAttemptDetails(
          device_id_pair, ConnectionMedium::kBluetoothLowEnergy,
          ConnectionRole::kListenerRole)));
    }
  }

  void UpdateBleListenerConnectionPriority(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority,
      bool expected_to_update_priority) {
    SetInRemoteDeviceIdToMetadataMap(
        device_id_pair, ConnectionRole::kListenerRole, connection_priority);

    manager_->UpdateBleListenerConnectionPriority(device_id_pair,
                                                  connection_priority);

    if (expected_to_update_priority) {
      EXPECT_TRUE(fake_ble_scanner_->HasScanRequest(ConnectionAttemptDetails(
          device_id_pair, ConnectionMedium::kBluetoothLowEnergy,
          ConnectionRole::kListenerRole)));
    } else {
      EXPECT_FALSE(fake_ble_scanner_->HasScanRequest(ConnectionAttemptDetails(
          device_id_pair, ConnectionMedium::kBluetoothLowEnergy,
          ConnectionRole::kListenerRole)));
    }
  }

  void CancelBleListenerConnectionAttempt(const DeviceIdPair& device_id_pair) {
    RemoveFromRemoteDeviceIdToMetadataMap(device_id_pair,
                                          ConnectionRole::kListenerRole);
    manager_->CancelBleListenerConnectionAttempt(device_id_pair);
    EXPECT_FALSE(fake_ble_scanner_->HasScanRequest(ConnectionAttemptDetails(
        device_id_pair, ConnectionMedium::kBluetoothLowEnergy,
        ConnectionRole::kListenerRole)));
  }

  void SimulateBleSlotEnding(const DeviceIdPair& device_id_pair,
                             bool replaced_by_higher_priority_advertisement) {
    size_t num_failures_before_call = ble_initiator_failures_.size();

    fake_ble_advertiser()->NotifyAdvertisingSlotEnded(
        device_id_pair, replaced_by_higher_priority_advertisement);
    EXPECT_EQ(num_failures_before_call + 1u, ble_initiator_failures_.size());
    EXPECT_EQ(device_id_pair, ble_initiator_failures_.back().first);
    EXPECT_EQ(replaced_by_higher_priority_advertisement
                  ? BleInitiatorFailureType::
                        kInterruptedByHigherPriorityConnectionAttempt
                  : BleInitiatorFailureType::kTimeoutContactingRemoteDevice,
              ble_initiator_failures_.back().second);
  }

  void SimulateBleFailureToGenerateAdvertisement(
      const DeviceIdPair& device_id_pair) {
    size_t num_failures_before_call = ble_initiator_failures_.size();

    fake_ble_advertiser()->NotifyFailureToGenerateAdvertisement(device_id_pair);
    EXPECT_EQ(num_failures_before_call + 1u, ble_initiator_failures_.size());
    EXPECT_EQ(device_id_pair, ble_initiator_failures_.back().first);
    EXPECT_EQ(BleInitiatorFailureType::kCouldNotGenerateAdvertisement,
              ble_initiator_failures_.back().second);
  }

  // Returns the SecureChannel created by this call.
  FakeSecureChannelConnection* SimulateConnectionEstablished(
      multidevice::RemoteDeviceRef remote_device,
      ConnectionRole connection_role) {
    auto mock_bluetooth_device = std::make_unique<device::MockBluetoothDevice>(
        mock_adapter_.get(), 0u /* bluetooth_class */, "name", "address",
        false /* paired */, false /* connected */);
    fake_weave_client_connection_factory_->set_expected_bluetooth_device(
        mock_bluetooth_device.get());

    fake_ble_scanner_->NotifyReceivedAdvertisementFromDevice(
        remote_device, mock_bluetooth_device.get(),
        ConnectionMedium::kBluetoothLowEnergy, connection_role,
        {0, 0} /* eid */);

    // As a result of the connection, all ongoing connection attmepts should
    // have been canceled, since a connection is in progress.
    EXPECT_TRUE(fake_ble_advertiser()
                    ->GetAllRequestsForRemoteDevice(remote_device.GetDeviceId())
                    .empty());
    EXPECT_TRUE(
        fake_ble_scanner_
            ->GetAllScanRequestsForRemoteDevice(remote_device.GetDeviceId())
            .empty());

    FakeSecureChannelConnection* last_created_secure_channel =
        fake_secure_channel_factory_->last_created_instance();
    EXPECT_TRUE(last_created_secure_channel->was_initialized());
    return last_created_secure_channel;
  }

  void SimulateSecureChannelDisconnection(
      const std::string& remote_device_id,
      bool fail_during_authentication,
      FakeSecureChannelConnection* fake_secure_channel,
      size_t num_initiator_attempts_canceled_from_disconnection = 0u,
      size_t num_listener_attempts_canceled_from_disconnection = 0u) {
    size_t num_ble_initiator_failures_before_call =
        ble_initiator_failures_.size();
    size_t num_ble_listener_failures_before_call =
        ble_listener_failures_.size();

    // Connect, then disconnect. If needed, start authenticating before
    // disconnecting.
    fake_secure_channel->ChangeStatus(SecureChannel::Status::CONNECTED);
    if (fail_during_authentication) {
      fake_secure_channel->ChangeStatus(SecureChannel::Status::AUTHENTICATING);
    }
    fake_secure_channel->ChangeStatus(SecureChannel::Status::DISCONNECTED);

    // Iterate through all pending requests to |remote_device_id|, ensuring that
    // all expected failures have been communicated back to the client.
    size_t initiator_failures_index =
        num_ble_initiator_failures_before_call +
        num_initiator_attempts_canceled_from_disconnection;
    size_t listener_failure_index =
        num_ble_listener_failures_before_call +
        num_listener_attempts_canceled_from_disconnection;
    for (const auto& tuple :
         remote_device_id_to_metadata_map_[remote_device_id]) {
      switch (std::get<1>(tuple)) {
        case ConnectionRole::kInitiatorRole: {
          EXPECT_EQ(std::get<0>(tuple),
                    ble_initiator_failures_[initiator_failures_index].first);
          EXPECT_EQ(fail_during_authentication
                        ? BleInitiatorFailureType::kAuthenticationError
                        : BleInitiatorFailureType::kGattConnectionError,
                    ble_initiator_failures_[initiator_failures_index].second);
          ++initiator_failures_index;
          break;
        }

        case ConnectionRole::kListenerRole: {
          if (!fail_during_authentication)
            continue;

          EXPECT_EQ(std::get<0>(tuple),
                    ble_listener_failures_[listener_failure_index].first);
          EXPECT_EQ(BleListenerFailureType::kAuthenticationError,
                    ble_listener_failures_[listener_failure_index].second);
          ++listener_failure_index;
          break;
        }
      }
    }
    EXPECT_EQ(initiator_failures_index, ble_initiator_failures_.size());
    EXPECT_EQ(listener_failure_index, ble_listener_failures_.size());

    // All requests which were paused during the connection should have started
    // back up again, since the connection became disconnected.
    for (const auto& tuple :
         remote_device_id_to_metadata_map_[remote_device_id]) {
      switch (std::get<1>(tuple)) {
        case ConnectionRole::kInitiatorRole: {
          EXPECT_EQ(std::get<2>(tuple),
                    *fake_ble_advertiser()->GetPriorityForRequest(
                        std::get<0>(tuple)));
          EXPECT_TRUE(
              fake_ble_scanner_->HasScanRequest(ConnectionAttemptDetails(
                  std::get<0>(tuple), ConnectionMedium::kBluetoothLowEnergy,
                  ConnectionRole::kInitiatorRole)));
          break;
        }

        case ConnectionRole::kListenerRole: {
          EXPECT_TRUE(
              fake_ble_scanner_->HasScanRequest(ConnectionAttemptDetails(
                  std::get<0>(tuple), ConnectionMedium::kBluetoothLowEnergy,
                  ConnectionRole::kListenerRole)));
          break;
        }
      }
    }
  }

  void SimulateSecureChannelAuthentication(
      const std::string& remote_device_id,
      FakeSecureChannelConnection* fake_secure_channel,
      bool created_via_background_advertisement) {
    fake_authenticated_channel_factory_->SetExpectationsForNextCall(
        fake_secure_channel, created_via_background_advertisement);

    size_t num_success_callbacks_before_call = successful_connections_.size();

    test_clock()->Advance(kAdvertisementToConnectionTime);
    fake_secure_channel->ChangeStatus(SecureChannel::Status::CONNECTED);
    fake_secure_channel->ChangeStatus(SecureChannel::Status::AUTHENTICATING);

    test_clock()->Advance(kConnectionToAuthenticationTime);
    fake_secure_channel->ChangeStatus(SecureChannel::Status::AUTHENTICATED);

    // Verify that the callback was made. Verification that the provided
    // DeviceIdPair was correct occurs in OnConnectionSuccess().
    EXPECT_EQ(num_success_callbacks_before_call + 1u,
              successful_connections_.size());

    // For all remaining requests, verify that they were added back.
    for (const auto& tuple :
         remote_device_id_to_metadata_map_[remote_device_id]) {
      switch (std::get<1>(tuple)) {
        case ConnectionRole::kInitiatorRole: {
          EXPECT_EQ(std::get<2>(tuple),
                    *fake_ble_advertiser()->GetPriorityForRequest(
                        std::get<0>(tuple)));
          EXPECT_TRUE(
              fake_ble_scanner_->HasScanRequest(ConnectionAttemptDetails(
                  std::get<0>(tuple), ConnectionMedium::kBluetoothLowEnergy,
                  ConnectionRole::kInitiatorRole)));
          break;
        }

        case ConnectionRole::kListenerRole: {
          EXPECT_TRUE(
              fake_ble_scanner_->HasScanRequest(ConnectionAttemptDetails(
                  std::get<0>(tuple), ConnectionMedium::kBluetoothLowEnergy,
                  ConnectionRole::kListenerRole)));
          break;
        }
      }
    }
  }

  bool WasChannelHandledByDisconnector(
      FakeSecureChannelConnection* fake_secure_channel) {
    return fake_secure_channel_disconnector_->WasChannelHandled(
        fake_secure_channel);
  }

  base::test::TaskEnvironment task_environment_;
  const multidevice::RemoteDeviceRefList& test_devices() {
    return test_devices_;
  }

  base::SimpleTestClock* test_clock() { return test_clock_.get(); }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  void OnConnectionSuccess(
      const DeviceIdPair& device_id_pair,
      bool created_via_background_advertisement,
      std::unique_ptr<AuthenticatedChannel> authenticated_channel) {
    successful_connections_.push_back(
        std::make_pair(device_id_pair, std::move(authenticated_channel)));

    std::optional<std::tuple<DeviceIdPair, ConnectionRole, ConnectionPriority>>
        tuple_which_received_callback;
    for (const auto& tuple :
         remote_device_id_to_metadata_map_[device_id_pair.remote_device_id()]) {
      if (std::get<1>(tuple) == ConnectionRole::kInitiatorRole &&
          !created_via_background_advertisement) {
        EXPECT_EQ(std::get<0>(tuple), device_id_pair);
        tuple_which_received_callback = tuple;
        break;
      }

      if (std::get<1>(tuple) == ConnectionRole::kListenerRole &&
          created_via_background_advertisement) {
        EXPECT_EQ(std::get<0>(tuple), device_id_pair);
        tuple_which_received_callback = tuple;
        break;
      }
    }
    EXPECT_TRUE(tuple_which_received_callback);

    // The request which recieved the success callback is automatically removed
    // by BleConnectionManager, so it no longer needs to be tracked.
    remote_device_id_to_metadata_map_[device_id_pair.remote_device_id()].erase(
        *tuple_which_received_callback);

    // Make a copy of the entries which should be canceled. This is required
    // because the Cancel*() calls below end up removing entries from
    // |remote_device_id_to_metadata_map_|, which can cause access to deleted
    // memory.
    base::flat_set<std::tuple<DeviceIdPair, ConnectionRole, ConnectionPriority>>
        to_cancel = remote_device_id_to_metadata_map_[device_id_pair
                                                          .remote_device_id()];

    for (const auto& tuple : to_cancel) {
      switch (std::get<1>(tuple)) {
        case ConnectionRole::kInitiatorRole:
          CancelBleInitiatorConnectionAttempt(std::get<0>(tuple));
          break;

        case ConnectionRole::kListenerRole:
          CancelBleListenerConnectionAttempt(std::get<0>(tuple));
          break;
      }
    }
  }

  void OnBleInitiatorFailure(const DeviceIdPair& device_id_pair,
                             bool should_cancel_attempt_on_failure,
                             BleInitiatorFailureType failure_type) {
    ble_initiator_failures_.push_back(
        std::make_pair(device_id_pair, failure_type));
    if (!should_cancel_attempt_on_failure)
      return;

    // Make a copy of the pair before canceling the attempt, since the reference
    // points to memory owned by |manager_| which will be deleted.
    DeviceIdPair device_id_pair_copy = device_id_pair;
    CancelBleInitiatorConnectionAttempt(device_id_pair_copy);
  }

  void OnBleListenerFailure(const DeviceIdPair& device_id_pair,
                            bool should_cancel_attempt_on_failure,
                            BleListenerFailureType failure_type) {
    ble_listener_failures_.push_back(
        std::make_pair(device_id_pair, failure_type));
    if (!should_cancel_attempt_on_failure)
      return;

    // Make a copy of the pair before canceling the attempt, since the reference
    // points to memory owned by |manager_| which will be deleted.
    DeviceIdPair device_id_pair_copy = device_id_pair;
    CancelBleListenerConnectionAttempt(device_id_pair_copy);
  }

  void SetInRemoteDeviceIdToMetadataMap(
      const DeviceIdPair& device_id_pair,
      ConnectionRole connection_role,
      ConnectionPriority connection_priority) {
    // If the entry already exists, update its priority.
    for (auto& tuple :
         remote_device_id_to_metadata_map_[device_id_pair.remote_device_id()]) {
      if (std::get<0>(tuple) == device_id_pair &&
          std::get<1>(tuple) == connection_role) {
        std::get<2>(tuple) = connection_priority;
        return;
      }
    }

    // Otherwise, add a new entry.
    remote_device_id_to_metadata_map_[device_id_pair.remote_device_id()].insert(
        std::make_tuple(device_id_pair, connection_role, connection_priority));
  }

  void RemoveFromRemoteDeviceIdToMetadataMap(const DeviceIdPair& device_id_pair,
                                             ConnectionRole connection_role) {
    base::flat_set<std::tuple<DeviceIdPair, ConnectionRole,
                              ConnectionPriority>>& set_for_remote_device =
        remote_device_id_to_metadata_map_[device_id_pair.remote_device_id()];

    for (auto it = set_for_remote_device.begin();
         it != set_for_remote_device.end(); ++it) {
      if (std::get<0>(*it) == device_id_pair &&
          std::get<1>(*it) == connection_role) {
        set_for_remote_device.erase(it);
        return;
      }
    }

    NOTREACHED_IN_MIGRATION();
  }

  FakeBleAdvertiser* fake_ble_advertiser() {
    return fake_ble_advertiser_factory_->instance();
  }

  const multidevice::RemoteDeviceRefList test_devices_;

  base::HistogramTester histogram_tester_;

  base::flat_map<
      std::string,
      base::flat_set<
          std::tuple<DeviceIdPair, ConnectionRole, ConnectionPriority>>>
      remote_device_id_to_metadata_map_;

  std::vector<std::pair<DeviceIdPair, std::unique_ptr<AuthenticatedChannel>>>
      successful_connections_;
  std::vector<std::pair<DeviceIdPair, BleInitiatorFailureType>>
      ble_initiator_failures_;
  std::vector<std::pair<DeviceIdPair, BleListenerFailureType>>
      ble_listener_failures_;

  std::unique_ptr<FakeBleAdvertiserFactory> fake_ble_advertiser_factory_;
  std::unique_ptr<FakeWeaveClientConnectionFactory>
      fake_weave_client_connection_factory_;
  std::unique_ptr<FakeSecureChannelFactory> fake_secure_channel_factory_;
  std::unique_ptr<FakeAuthenticatedChannelFactory>
      fake_authenticated_channel_factory_;

  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<FakeBluetoothHelper> fake_bluetooth_helper_;
  std::unique_ptr<FakeBleSynchronizer> fake_ble_synchronizer_;
  std::unique_ptr<FakeBleScanner> fake_ble_scanner_;
  std::unique_ptr<FakeSecureChannelDisconnector>
      fake_secure_channel_disconnector_;
  std::unique_ptr<ash::timer_factory::FakeTimerFactory> fake_timer_factory_;
  std::unique_ptr<base::SimpleTestClock> test_clock_;

  std::unique_ptr<BleConnectionManager> manager_;
};

TEST_F(SecureChannelBleConnectionManagerImplTest,
       OneRequest_Initiator_UpdatePriority) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptBleInitiatorConnection(pair, ConnectionPriority::kLow,
                                true /* expected_to_add_request */,
                                false /* should_cancel_attempt_on_failure */);
  UpdateBleInitiatorConnectionPriority(pair, ConnectionPriority::kMedium,
                                       true /* expected_to_update_priority */);
  UpdateBleInitiatorConnectionPriority(pair, ConnectionPriority::kHigh,
                                       true /* expected_to_update_priority */);
  UpdateBleInitiatorConnectionPriority(pair, ConnectionPriority::kLow,
                                       true /* expected_to_update_priority */);
  UpdateBleInitiatorConnectionPriority(pair, ConnectionPriority::kMedium,
                                       true /* expected_to_update_priority */);
  UpdateBleInitiatorConnectionPriority(pair, ConnectionPriority::kHigh,
                                       true /* expected_to_update_priority */);

  CancelBleInitiatorConnectionAttempt(pair);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       OneRequest_Initiator_AdvertisementsUnansweredThenCanceled) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptBleInitiatorConnection(pair, ConnectionPriority::kLow,
                                true /* expected_to_add_request */,
                                false /* should_cancel_attempt_on_failure */);
  UpdateBleInitiatorConnectionPriority(pair, ConnectionPriority::kMedium,
                                       true /* expected_to_update_priority */);

  // Fail a few times due to timeouts.
  SimulateBleSlotEnding(pair,
                        false /* replaced_by_higher_priority_advertisement */);
  SimulateBleSlotEnding(pair,
                        false /* replaced_by_higher_priority_advertisement */);
  SimulateBleSlotEnding(pair,
                        false /* replaced_by_higher_priority_advertisement */);

  CancelBleInitiatorConnectionAttempt(pair);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       OneRequest_Initiator_FailToGenerateAdvertisement_ManualCleanup) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptBleInitiatorConnection(pair, ConnectionPriority::kLow,
                                true /* expected_to_add_request */,
                                false /* should_cancel_attempt_on_failure */);
  SimulateBleFailureToGenerateAdvertisement(pair);
  CancelBleInitiatorConnectionAttempt(pair);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       OneRequest_Initiator_FailToGenerateAdvertisement_CleanupOnCallback) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptBleInitiatorConnection(pair, ConnectionPriority::kLow,
                                true /* expected_to_add_request */,
                                true /* should_cancel_attempt_on_failure */);
  SimulateBleFailureToGenerateAdvertisement(pair);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       OneRequest_Initiator_FailsAuthenticationThenCanceled) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptBleInitiatorConnection(pair, ConnectionPriority::kLow,
                                true /* expected_to_add_request */,
                                false /* should_cancel_attempt_on_failure */);

  FakeSecureChannelConnection* fake_secure_channel =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kInitiatorRole);
  SimulateSecureChannelDisconnection(pair.remote_device_id(),
                                     true /* fail_during_authentication */,
                                     fake_secure_channel);

  CancelBleInitiatorConnectionAttempt(pair);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       OneRequest_Initiator_FailsAuthenticationThenCanceled_CleanupOnCallback) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptBleInitiatorConnection(pair, ConnectionPriority::kLow,
                                true /* expected_to_add_request */,
                                true /* should_cancel_attempt_on_failure */);

  FakeSecureChannelConnection* fake_secure_channel =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kInitiatorRole);
  SimulateSecureChannelDisconnection(
      pair.remote_device_id(), true /* fail_during_authentication */,
      fake_secure_channel,
      1u /* num_initiator_attempts_canceled_from_disconnection */,
      0u /* num_listener_attempts_canceled_from_disconnection */);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       OneRequest_Initiator_GattFailureThenCanceled) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptBleInitiatorConnection(pair, ConnectionPriority::kLow,
                                true /* expected_to_add_request */,
                                false /* should_cancel_attempt_on_failure */);

  FakeSecureChannelConnection* fake_secure_channel =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kInitiatorRole);
  SimulateSecureChannelDisconnection(pair.remote_device_id(),
                                     false /* fail_during_authentication */,
                                     fake_secure_channel);

  CancelBleInitiatorConnectionAttempt(pair);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       OneRequest_Initiator_SuccessfulConnection) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptBleInitiatorConnection(pair, ConnectionPriority::kLow,
                                true /* expected_to_add_request */,
                                false /* should_cancel_attempt_on_failure */);

  FakeSecureChannelConnection* fake_secure_channel =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kInitiatorRole);
  SimulateSecureChannelAuthentication(
      pair.remote_device_id(), fake_secure_channel,
      false /* created_via_background_advertisement */);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       OneRequest_Listener_UpdatePriority) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptBleListenerConnection(pair, ConnectionPriority::kLow,
                               true /* expected_to_add_request */,
                               false /* should_cancel_attempt_on_failure */);
  UpdateBleListenerConnectionPriority(pair, ConnectionPriority::kMedium,
                                      true /* expected_to_update_priority */);
  UpdateBleListenerConnectionPriority(pair, ConnectionPriority::kHigh,
                                      true /* expected_to_update_priority */);
  UpdateBleListenerConnectionPriority(pair, ConnectionPriority::kLow,
                                      true /* expected_to_update_priority */);
  UpdateBleListenerConnectionPriority(pair, ConnectionPriority::kMedium,
                                      true /* expected_to_update_priority */);
  UpdateBleListenerConnectionPriority(pair, ConnectionPriority::kHigh,
                                      true /* expected_to_update_priority */);

  CancelBleListenerConnectionAttempt(pair);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       OneRequest_Listener_FailsAuthenticationThenCanceled) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptBleListenerConnection(pair, ConnectionPriority::kLow,
                               true /* expected_to_add_request */,
                               false /* should_cancel_attempt_on_failure */);

  FakeSecureChannelConnection* fake_secure_channel =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kListenerRole);
  SimulateSecureChannelDisconnection(pair.remote_device_id(),
                                     true /* fail_during_authentication */,
                                     fake_secure_channel);

  CancelBleListenerConnectionAttempt(pair);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       OneRequest_Listener_FailsAuthenticationThenCanceled_CleanupOnCallback) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptBleListenerConnection(pair, ConnectionPriority::kLow,
                               true /* expected_to_add_request */,
                               true /* should_cancel_attempt_on_failure */);

  FakeSecureChannelConnection* fake_secure_channel =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kListenerRole);
  SimulateSecureChannelDisconnection(
      pair.remote_device_id(), true /* fail_during_authentication */,
      fake_secure_channel,
      0u /* num_initiator_attempts_canceled_from_disconnection */,
      1u /* num_listener_attempts_canceled_from_disconnection */);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       OneRequest_Listener_GattFailureThenCanceled) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptBleListenerConnection(pair, ConnectionPriority::kLow,
                               true /* expected_to_add_request */,
                               false /* should_cancel_attempt_on_failure */);

  FakeSecureChannelConnection* fake_secure_channel =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kListenerRole);
  SimulateSecureChannelDisconnection(pair.remote_device_id(),
                                     false /* fail_during_authentication */,
                                     fake_secure_channel);

  CancelBleListenerConnectionAttempt(pair);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       OneRequest_Listener_SuccessfulConnection) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptBleListenerConnection(pair, ConnectionPriority::kLow,
                               true /* expected_to_add_request */,
                               false /* should_cancel_attempt_on_failure */);

  FakeSecureChannelConnection* fake_secure_channel =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kListenerRole);
  SimulateSecureChannelAuthentication(
      pair.remote_device_id(), fake_secure_channel,
      true /* created_via_background_advertisement */);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       TwoRequests_BothInitiator_Failures) {
  DeviceIdPair pair_1(test_devices()[1].GetDeviceId(),
                      test_devices()[0].GetDeviceId());
  DeviceIdPair pair_2(test_devices()[2].GetDeviceId(),
                      test_devices()[0].GetDeviceId());

  AttemptBleInitiatorConnection(pair_1, ConnectionPriority::kLow,
                                true /* expected_to_add_request */,
                                false /* should_cancel_attempt_on_failure */);
  AttemptBleInitiatorConnection(pair_2, ConnectionPriority::kMedium,
                                true /* expected_to_add_request */,
                                false /* should_cancel_attempt_on_failure */);

  // One advertisement slot failure each.
  SimulateBleSlotEnding(pair_1,
                        false /* replaced_by_higher_priority_advertisement */);
  SimulateBleSlotEnding(pair_2,
                        false /* replaced_by_higher_priority_advertisement */);

  // For pair_1, establish a connection then fail due to GATT errors.
  FakeSecureChannelConnection* fake_secure_channel_1 =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kInitiatorRole);
  SimulateSecureChannelDisconnection(pair_1.remote_device_id(),
                                     false /* fail_during_authentication */,
                                     fake_secure_channel_1);

  // For pair_2, establish a connection then fail due to authentication errors.
  FakeSecureChannelConnection* fake_secure_channel_2 =
      SimulateConnectionEstablished(test_devices()[2],
                                    ConnectionRole::kInitiatorRole);
  SimulateSecureChannelDisconnection(pair_2.remote_device_id(),
                                     true /* fail_during_authentication */,
                                     fake_secure_channel_2);

  // Cancel both attempts.
  CancelBleInitiatorConnectionAttempt(pair_1);
  CancelBleInitiatorConnectionAttempt(pair_2);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       TwoRequests_BothInitiator_Success) {
  DeviceIdPair pair_1(test_devices()[1].GetDeviceId(),
                      test_devices()[0].GetDeviceId());
  DeviceIdPair pair_2(test_devices()[2].GetDeviceId(),
                      test_devices()[0].GetDeviceId());

  AttemptBleInitiatorConnection(pair_1, ConnectionPriority::kLow,
                                true /* expected_to_add_request */,
                                false /* should_cancel_attempt_on_failure */);
  AttemptBleInitiatorConnection(pair_2, ConnectionPriority::kMedium,
                                true /* expected_to_add_request */,
                                false /* should_cancel_attempt_on_failure */);

  FakeSecureChannelConnection* fake_secure_channel_1 =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kInitiatorRole);
  SimulateSecureChannelAuthentication(
      pair_1.remote_device_id(), fake_secure_channel_1,
      false /* created_via_background_advertisement */);

  FakeSecureChannelConnection* fake_secure_channel_2 =
      SimulateConnectionEstablished(test_devices()[2],
                                    ConnectionRole::kInitiatorRole);
  SimulateSecureChannelAuthentication(
      pair_2.remote_device_id(), fake_secure_channel_2,
      false /* created_via_background_advertisement */);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       TwoRequests_BothListener_Failures) {
  DeviceIdPair pair_1(test_devices()[1].GetDeviceId(),
                      test_devices()[0].GetDeviceId());
  DeviceIdPair pair_2(test_devices()[2].GetDeviceId(),
                      test_devices()[0].GetDeviceId());

  AttemptBleListenerConnection(pair_1, ConnectionPriority::kLow,
                               true /* expected_to_add_request */,
                               false /* should_cancel_attempt_on_failure */);
  AttemptBleListenerConnection(pair_2, ConnectionPriority::kMedium,
                               true /* expected_to_add_request */,
                               false /* should_cancel_attempt_on_failure */);

  FakeSecureChannelConnection* fake_secure_channel_1 =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kListenerRole);
  SimulateSecureChannelDisconnection(pair_1.remote_device_id(),
                                     true /* fail_during_authentication */,
                                     fake_secure_channel_1);

  FakeSecureChannelConnection* fake_secure_channel_2 =
      SimulateConnectionEstablished(test_devices()[2],
                                    ConnectionRole::kListenerRole);
  SimulateSecureChannelDisconnection(pair_2.remote_device_id(),
                                     true /* fail_during_authentication */,
                                     fake_secure_channel_2);

  CancelBleListenerConnectionAttempt(pair_1);
  CancelBleListenerConnectionAttempt(pair_2);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       TwoRequests_BothListener_Success) {
  DeviceIdPair pair_1(test_devices()[1].GetDeviceId(),
                      test_devices()[0].GetDeviceId());
  DeviceIdPair pair_2(test_devices()[2].GetDeviceId(),
                      test_devices()[0].GetDeviceId());

  AttemptBleListenerConnection(pair_1, ConnectionPriority::kLow,
                               true /* expected_to_add_request */,
                               false /* should_cancel_attempt_on_failure */);
  AttemptBleListenerConnection(pair_2, ConnectionPriority::kMedium,
                               true /* expected_to_add_request */,
                               false /* should_cancel_attempt_on_failure */);

  FakeSecureChannelConnection* fake_secure_channel_1 =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kListenerRole);
  SimulateSecureChannelAuthentication(
      pair_1.remote_device_id(), fake_secure_channel_1,
      true /* created_via_background_advertisement */);

  FakeSecureChannelConnection* fake_secure_channel_2 =
      SimulateConnectionEstablished(test_devices()[2],
                                    ConnectionRole::kListenerRole);
  SimulateSecureChannelAuthentication(
      pair_2.remote_device_id(), fake_secure_channel_2,
      true /* created_via_background_advertisement */);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       TwoRequests_SamePairDifferentRole_Failure) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptBleListenerConnection(pair, ConnectionPriority::kLow,
                               true /* expected_to_add_request */,
                               false /* should_cancel_attempt_on_failure */);
  AttemptBleInitiatorConnection(pair, ConnectionPriority::kMedium,
                                true /* expected_to_add_request */,
                                false /* should_cancel_attempt_on_failure */);

  // GATT failure.
  FakeSecureChannelConnection* fake_secure_channel_1 =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kInitiatorRole);
  SimulateSecureChannelDisconnection(pair.remote_device_id(),
                                     false /* fail_during_authentication */,
                                     fake_secure_channel_1);

  // Authentication failure.
  FakeSecureChannelConnection* fake_secure_channel_2 =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kListenerRole);
  SimulateSecureChannelDisconnection(pair.remote_device_id(),
                                     true /* fail_during_authentication */,
                                     fake_secure_channel_2);

  CancelBleListenerConnectionAttempt(pair);
  CancelBleInitiatorConnectionAttempt(pair);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       TwoRequests_SamePairDifferentRole_Success) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptBleListenerConnection(pair, ConnectionPriority::kLow,
                               true /* expected_to_add_request */,
                               false /* should_cancel_attempt_on_failure */);
  AttemptBleInitiatorConnection(pair, ConnectionPriority::kMedium,
                                true /* expected_to_add_request */,
                                false /* should_cancel_attempt_on_failure */);

  FakeSecureChannelConnection* fake_secure_channel =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kInitiatorRole);
  SimulateSecureChannelAuthentication(
      pair.remote_device_id(), fake_secure_channel,
      false /* created_via_background_advertisement */);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       TwoRequests_SamePairDifferentRole_NewAttemptWhileConnectionInProgress) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptBleListenerConnection(pair, ConnectionPriority::kLow,
                               true /* expected_to_add_request */,
                               false /* should_cancel_attempt_on_failure */);

  FakeSecureChannelConnection* fake_secure_channel =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kInitiatorRole);

  // There is already a connection in progress, so this is not expected to add
  // a request to BleAdvertiser/BleScanner.
  AttemptBleInitiatorConnection(pair, ConnectionPriority::kMedium,
                                false /* expected_to_add_request */,
                                false /* should_cancel_attempt_on_failure */);

  // Update the priority; this also should not cause an update in BleScanner.
  UpdateBleListenerConnectionPriority(pair, ConnectionPriority::kMedium,
                                      false /* expected_to_add_request */);

  SimulateSecureChannelAuthentication(
      pair.remote_device_id(), fake_secure_channel,
      false /* created_via_background_advertisement */);
}

TEST_F(SecureChannelBleConnectionManagerImplTest,
       TwoRequests_RemoveRequestWhileAuthenticating) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptBleListenerConnection(pair, ConnectionPriority::kLow,
                               true /* expected_to_add_request */,
                               false /* should_cancel_attempt_on_failure */);
  AttemptBleInitiatorConnection(pair, ConnectionPriority::kMedium,
                                true /* expected_to_add_request */,
                                false /* should_cancel_attempt_on_failure */);

  FakeSecureChannelConnection* fake_secure_channel =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kInitiatorRole);

  // Before the channel authenticates, remove both ongoing attempts. This should
  // cause the ongoing connection to be passed off to the
  // SecureChannelDisconnector.
  CancelBleListenerConnectionAttempt(pair);
  CancelBleInitiatorConnectionAttempt(pair);

  EXPECT_TRUE(WasChannelHandledByDisconnector(fake_secure_channel));
}

TEST_F(SecureChannelBleConnectionManagerImplTest, ConnectionMetrics) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  // Start a connection attempt and advance the clock.
  AttemptBleListenerConnection(pair, ConnectionPriority::kLow,
                               true /* expected_to_add_request */,
                               false /* should_cancel_attempt_on_failure */);

  test_clock()->Advance(kScanToAdvertisementTime);

  // Simulate a connection being established, then disconnected.
  FakeSecureChannelConnection* fake_secure_channel =
      SimulateConnectionEstablished(test_devices()[1],
                                    ConnectionRole::kListenerRole);
  test_clock()->Advance(kAdvertisementToConnectionTime);
  SimulateSecureChannelDisconnection(pair.remote_device_id(),
                                     false /* fail_during_authentication */,
                                     fake_secure_channel);
  histogram_tester()->ExpectTimeBucketCount(
      "MultiDevice.SecureChannel.BLE.Performance."
      "StartScanToReceiveAdvertisementDuration.Background",
      kScanToAdvertisementTime, 1);
  histogram_tester()->ExpectTimeBucketCount(
      "MultiDevice.SecureChannel.BLE.Performance."
      "ReceiveAdvertisementToConnectionDuration.Background",
      kAdvertisementToConnectionTime, 1);
  histogram_tester()->ExpectTimeBucketCount(
      "MultiDevice.SecureChannel.BLE.Performance."
      "StartScanToConnectionDuration.Background",
      kScanToAdvertisementTime + kAdvertisementToConnectionTime, 1);
  histogram_tester()->ExpectBucketCount(
      "MultiDevice.SecureChannel.BLE.ReceiveAdvertisementToGattConnection."
      "EffectiveSuccessRateWithRetries",
      true, 1u /* expected_count */);

  // Advance the clock again, then simulate a successful authentication.
  test_clock()->Advance(kScanToAdvertisementTime);
  fake_secure_channel = SimulateConnectionEstablished(
      test_devices()[1], ConnectionRole::kListenerRole);
  SimulateSecureChannelAuthentication(
      pair.remote_device_id(), fake_secure_channel,
      true /* created_via_background_advertisement */);

  histogram_tester()->ExpectTimeBucketCount(
      "MultiDevice.SecureChannel.BLE.Performance."
      "StartScanToReceiveAdvertisementDuration.Background",
      kScanToAdvertisementTime, 2);
  histogram_tester()->ExpectTimeBucketCount(
      "MultiDevice.SecureChannel.BLE.Performance."
      "ReceiveAdvertisementToConnectionDuration.Background",
      kAdvertisementToConnectionTime, 2);
  histogram_tester()->ExpectTimeBucketCount(
      "MultiDevice.SecureChannel.BLE.Performance."
      "StartScanToConnectionDuration.Background",
      kScanToAdvertisementTime + kAdvertisementToConnectionTime, 2);
  histogram_tester()->ExpectTimeBucketCount(
      "MultiDevice.SecureChannel.BLE.Performance."
      "ConnectionToAuthenticationDuration.Background",
      kConnectionToAuthenticationTime, 1);
  histogram_tester()->ExpectBucketCount(
      "MultiDevice.SecureChannel.BLE.ReceiveAdvertisementToGattConnection."
      "EffectiveSuccessRateWithRetries",
      true, 2u /* expected_count */);
  histogram_tester()->ExpectBucketCount(
      "MultiDevice.SecureChannel.BLE.ReceiveAdvertisementToAuthentication."
      "EffectiveSuccessRateWithRetries",
      true, 1u /* expected_count */);
  histogram_tester()->ExpectBucketCount(
      "MultiDevice.SecureChannel.BLE.GattConnectionToAuthentication."
      "EffectiveSuccessRateWithRetries",
      true, 1u /* expected_count */);
}

}  // namespace ash::secure_channel
