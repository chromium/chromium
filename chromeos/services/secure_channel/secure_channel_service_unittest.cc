// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/components/multidevice/remote_device_cache.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/services/secure_channel/active_connection_manager_impl.h"
#include "chromeos/services/secure_channel/ble_connection_manager_impl.h"
#include "chromeos/services/secure_channel/ble_service_data_helper_impl.h"
#include "chromeos/services/secure_channel/client_connection_parameters_impl.h"
#include "chromeos/services/secure_channel/fake_active_connection_manager.h"
#include "chromeos/services/secure_channel/fake_authenticated_channel.h"
#include "chromeos/services/secure_channel/fake_ble_connection_manager.h"
#include "chromeos/services/secure_channel/fake_ble_service_data_helper.h"
#include "chromeos/services/secure_channel/fake_client_connection_parameters.h"
#include "chromeos/services/secure_channel/fake_connection_delegate.h"
#include "chromeos/services/secure_channel/fake_pending_connection_manager.h"
#include "chromeos/services/secure_channel/fake_timer_factory.h"
#include "chromeos/services/secure_channel/pending_connection_manager_impl.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/services/secure_channel/secure_channel_initializer.h"
#include "chromeos/services/secure_channel/timer_factory_impl.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

namespace {

const size_t kNumTestDevices = 6;

class FakeTimerFactoryFactory : public TimerFactoryImpl::Factory {
 public:
  FakeTimerFactoryFactory() = default;
  ~FakeTimerFactoryFactory() override = default;

  FakeTimerFactory* instance() { return instance_; }

 private:
  // TimerFactoryImpl::Factory:
  std::unique_ptr<TimerFactory> BuildInstance() override {
    EXPECT_FALSE(instance_);
    auto instance = std::make_unique<FakeTimerFactory>();
    instance_ = instance.get();
    return instance;
  }

  FakeTimerFactory* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeTimerFactoryFactory);
};

class TestRemoteDeviceCacheFactory
    : public multidevice::RemoteDeviceCache::Factory {
 public:
  TestRemoteDeviceCacheFactory() = default;
  ~TestRemoteDeviceCacheFactory() override = default;

  multidevice::RemoteDeviceCache* instance() { return instance_; }

 private:
  // multidevice::RemoteDeviceCache::Factory:
  std::unique_ptr<multidevice::RemoteDeviceCache> BuildInstance() override {
    EXPECT_FALSE(instance_);
    auto instance = multidevice::RemoteDeviceCache::Factory::BuildInstance();
    instance_ = instance.get();
    return instance;
  }

  multidevice::RemoteDeviceCache* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestRemoteDeviceCacheFactory);
};

class FakeBleServiceDataHelperFactory
    : public BleServiceDataHelperImpl::Factory {
 public:
  FakeBleServiceDataHelperFactory(
      TestRemoteDeviceCacheFactory* test_remote_device_cache_factory)
      : test_remote_device_cache_factory_(test_remote_device_cache_factory) {}

  ~FakeBleServiceDataHelperFactory() override = default;

  FakeBleServiceDataHelper* instance() { return instance_; }

 private:
  // BleServiceDataHelperImpl::Factory:
  std::unique_ptr<BleServiceDataHelper> BuildInstance(
      multidevice::RemoteDeviceCache* remote_device_cache) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(test_remote_device_cache_factory_->instance(),
              remote_device_cache);

    auto instance = std::make_unique<FakeBleServiceDataHelper>();
    instance_ = instance.get();
    return instance;
  }

  TestRemoteDeviceCacheFactory* test_remote_device_cache_factory_;

  FakeBleServiceDataHelper* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeBleServiceDataHelperFactory);
};

class FakeBleConnectionManagerFactory
    : public BleConnectionManagerImpl::Factory {
 public:
  FakeBleConnectionManagerFactory(
      device::BluetoothAdapter* expected_bluetooth_adapter,
      FakeBleServiceDataHelperFactory* fake_ble_service_data_helper_factory,
      FakeTimerFactoryFactory* fake_timer_factory_factory)
      : expected_bluetooth_adapter_(expected_bluetooth_adapter),
        fake_ble_service_data_helper_factory_(
            fake_ble_service_data_helper_factory),
        fake_timer_factory_factory_(fake_timer_factory_factory) {}

  ~FakeBleConnectionManagerFactory() override = default;

  FakeBleConnectionManager* instance() { return instance_; }

 private:
  // BleConnectionManagerImpl::Factory:
  std::unique_ptr<BleConnectionManager> BuildInstance(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      BleServiceDataHelper* ble_service_data_helper,
      TimerFactory* timer_factory,
      base::Clock* clock) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(expected_bluetooth_adapter_, bluetooth_adapter.get());
    EXPECT_EQ(fake_ble_service_data_helper_factory_->instance(),
              ble_service_data_helper);
    EXPECT_EQ(fake_timer_factory_factory_->instance(), timer_factory);

    auto instance = std::make_unique<FakeBleConnectionManager>();
    instance_ = instance.get();
    return instance;
  }

  device::BluetoothAdapter* expected_bluetooth_adapter_;
  FakeBleServiceDataHelperFactory* fake_ble_service_data_helper_factory_;
  FakeTimerFactoryFactory* fake_timer_factory_factory_;

  FakeBleConnectionManager* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeBleConnectionManagerFactory);
};

class FakePendingConnectionManagerFactory
    : public PendingConnectionManagerImpl::Factory {
 public:
  FakePendingConnectionManagerFactory(
      FakeBleConnectionManagerFactory* fake_ble_connection_manager_factory)
      : fake_ble_connection_manager_factory_(
            fake_ble_connection_manager_factory) {}

  ~FakePendingConnectionManagerFactory() override = default;

  FakePendingConnectionManager* instance() { return instance_; }

 private:
  // PendingConnectionManagerImpl::Factory:
  std::unique_ptr<PendingConnectionManager> BuildInstance(
      PendingConnectionManager::Delegate* delegate,
      BleConnectionManager* ble_connection_manager,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(fake_ble_connection_manager_factory_->instance(),
              ble_connection_manager);

    auto instance = std::make_unique<FakePendingConnectionManager>(delegate);
    instance_ = instance.get();
    return instance;
  }

  FakeBleConnectionManagerFactory* fake_ble_connection_manager_factory_;

  FakePendingConnectionManager* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakePendingConnectionManagerFactory);
};

class FakeActiveConnectionManagerFactory
    : public ActiveConnectionManagerImpl::Factory {
 public:
  FakeActiveConnectionManagerFactory() = default;
  ~FakeActiveConnectionManagerFactory() override = default;

  FakeActiveConnectionManager* instance() { return instance_; }

 private:
  // ActiveConnectionManagerImpl::Factory:
  std::unique_ptr<ActiveConnectionManager> BuildInstance(
      ActiveConnectionManager::Delegate* delegate) override {
    EXPECT_FALSE(instance_);
    auto instance = std::make_unique<FakeActiveConnectionManager>(delegate);
    instance_ = instance.get();
    return instance;
  }

  FakeActiveConnectionManager* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeActiveConnectionManagerFactory);
};

class TestSecureChannelInitializerFactory
    : public SecureChannelInitializer::Factory {
 public:
  TestSecureChannelInitializerFactory(
      scoped_refptr<base::TestSimpleTaskRunner> test_task_runner)
      : test_task_runner_(test_task_runner) {}

  ~TestSecureChannelInitializerFactory() override = default;

 private:
  // SecureChannelInitializer::Factory:
  std::unique_ptr<SecureChannelBase> BuildInstance(
      scoped_refptr<base::TaskRunner> task_runner) override {
    EXPECT_FALSE(instance_);
    auto instance =
        SecureChannelInitializer::Factory::BuildInstance(test_task_runner_);
    instance_ = instance.get();
    return instance;
  }

  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_;

  SecureChannelBase* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestSecureChannelInitializerFactory);
};

class FakeClientConnectionParametersFactory
    : public ClientConnectionParametersImpl::Factory {
 public:
  FakeClientConnectionParametersFactory() = default;
  ~FakeClientConnectionParametersFactory() override = default;

  const base::UnguessableToken& last_created_instance_id() {
    return last_created_instance_id_;
  }

  std::unordered_map<base::UnguessableToken,
                     FakeClientConnectionParameters*,
                     base::UnguessableTokenHash>&
  id_to_active_client_parameters_map() {
    return id_to_active_client_parameters_map_;
  }

  const std::unordered_map<
      base::UnguessableToken,
      base::Optional<mojom::ConnectionAttemptFailureReason>,
      base::UnguessableTokenHash>&
  id_to_failure_reason_when_deleted_map() {
    return id_to_failure_reason_when_deleted_map_;
  }

 private:
  // ClientConnectionParametersImpl::Factory:
  std::unique_ptr<ClientConnectionParameters> BuildInstance(
      const std::string& feature,
      mojo::PendingRemote<mojom::ConnectionDelegate> connection_delegate_remote)
      override {
    auto instance = std::make_unique<FakeClientConnectionParameters>(
        feature, base::BindOnce(
                     &FakeClientConnectionParametersFactory::OnInstanceDeleted,
                     base::Unretained(this)));
    last_created_instance_id_ = instance->id();
    id_to_active_client_parameters_map_[instance->id()] = instance.get();
    return instance;
  }

  void OnInstanceDeleted(const base::UnguessableToken& instance_id) {
    // Store failure reason before deleting.
    id_to_failure_reason_when_deleted_map_[instance_id] =
        id_to_active_client_parameters_map_[instance_id]->failure_reason();

    size_t num_deleted = id_to_active_client_parameters_map_.erase(instance_id);
    EXPECT_EQ(1u, num_deleted);
  }

  base::UnguessableToken last_created_instance_id_;

  std::unordered_map<base::UnguessableToken,
                     FakeClientConnectionParameters*,
                     base::UnguessableTokenHash>
      id_to_active_client_parameters_map_;

  std::unordered_map<base::UnguessableToken,
                     base::Optional<mojom::ConnectionAttemptFailureReason>,
                     base::UnguessableTokenHash>
      id_to_failure_reason_when_deleted_map_;

  DISALLOW_COPY_AND_ASSIGN(FakeClientConnectionParametersFactory);
};

}  // namespace

class SecureChannelServiceTest : public testing::Test {
 protected:
  SecureChannelServiceTest()
      : test_devices_(
            multidevice::CreateRemoteDeviceListForTest(kNumTestDevices)) {}
  ~SecureChannelServiceTest() override = default;

  // testing::Test:
  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    is_adapter_powered_ = true;
    is_adapter_present_ = true;
    ON_CALL(*mock_adapter_, IsPresent())
        .WillByDefault(
            Invoke(this, &SecureChannelServiceTest::is_adapter_present));
    ON_CALL(*mock_adapter_, IsPowered())
        .WillByDefault(
            Invoke(this, &SecureChannelServiceTest::is_adapter_powered));
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);

    test_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();

    fake_timer_factory_factory_ = std::make_unique<FakeTimerFactoryFactory>();
    TimerFactoryImpl::Factory::SetFactoryForTesting(
        fake_timer_factory_factory_.get());

    test_remote_device_cache_factory_ =
        std::make_unique<TestRemoteDeviceCacheFactory>();
    multidevice::RemoteDeviceCache::Factory::SetFactoryForTesting(
        test_remote_device_cache_factory_.get());

    fake_ble_service_data_helper_factory_ =
        std::make_unique<FakeBleServiceDataHelperFactory>(
            test_remote_device_cache_factory_.get());
    BleServiceDataHelperImpl::Factory::SetFactoryForTesting(
        fake_ble_service_data_helper_factory_.get());

    fake_ble_connection_manager_factory_ =
        std::make_unique<FakeBleConnectionManagerFactory>(
            mock_adapter_.get(), fake_ble_service_data_helper_factory_.get(),
            fake_timer_factory_factory_.get());
    BleConnectionManagerImpl::Factory::SetFactoryForTesting(
        fake_ble_connection_manager_factory_.get());

    fake_pending_connection_manager_factory_ =
        std::make_unique<FakePendingConnectionManagerFactory>(
            fake_ble_connection_manager_factory_.get());
    PendingConnectionManagerImpl::Factory::SetFactoryForTesting(
        fake_pending_connection_manager_factory_.get());

    fake_active_connection_manager_factory_ =
        std::make_unique<FakeActiveConnectionManagerFactory>();
    ActiveConnectionManagerImpl::Factory::SetFactoryForTesting(
        fake_active_connection_manager_factory_.get());

    test_secure_channel_initializer_factory_ =
        std::make_unique<TestSecureChannelInitializerFactory>(
            test_task_runner_);
    SecureChannelInitializer::Factory::SetFactoryForTesting(
        test_secure_channel_initializer_factory_.get());

    fake_client_connection_parameters_factory_ =
        std::make_unique<FakeClientConnectionParametersFactory>();
    ClientConnectionParametersImpl::Factory::SetFactoryForTesting(
        fake_client_connection_parameters_factory_.get());

    service_ = SecureChannelInitializer::Factory::Get()->BuildInstance();
    service_->BindReceiver(secure_channel_remote_.BindNewPipeAndPassReceiver());
    secure_channel_remote_.FlushForTesting();
  }

  void TearDown() override {
    TimerFactoryImpl::Factory::SetFactoryForTesting(nullptr);
    multidevice::RemoteDeviceCache::Factory::SetFactoryForTesting(nullptr);
    BleServiceDataHelperImpl::Factory::SetFactoryForTesting(nullptr);
    BleConnectionManagerImpl::Factory::SetFactoryForTesting(nullptr);
    PendingConnectionManagerImpl::Factory::SetFactoryForTesting(nullptr);
    ActiveConnectionManagerImpl::Factory::SetFactoryForTesting(nullptr);
    SecureChannelInitializer::Factory::SetFactoryForTesting(nullptr);
    ClientConnectionParametersImpl::Factory::SetFactoryForTesting(nullptr);
  }

  void CallListenForConnectionFromDeviceAndVerifyInitializationNotComplete(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority) {
    AttemptConnectionPreInitialization(device_to_connect, local_device, feature,
                                       connection_priority,
                                       true /* is_listener */);
  }

  void CallInitiateConnectionToDeviceAndVerifyInitializationNotComplete(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority) {
    AttemptConnectionPreInitialization(device_to_connect, local_device, feature,
                                       connection_priority,
                                       false /* is_listener */);
  }

  void CallListenForConnectionFromDeviceAndVerifyRejection(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority,
      mojom::ConnectionAttemptFailureReason expected_failure_reason) {
    AttemptConnectionAndVerifyRejection(
        device_to_connect, local_device, feature, connection_priority,
        expected_failure_reason, true /* is_listener */);
  }

  void CallInitiateConnectionToDeviceAndVerifyRejection(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority,
      mojom::ConnectionAttemptFailureReason expected_failure_reason) {
    AttemptConnectionAndVerifyRejection(
        device_to_connect, local_device, feature, connection_priority,
        expected_failure_reason, false /* is_listener */);
  }

  void CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority) {
    AttemptConnectionAndVerifyPendingConnection(device_to_connect, local_device,
                                                feature, connection_priority,
                                                true /* is_listener */);
  }

  void CallInitiateConnectionToDeviceAndVerifyPendingConnection(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority) {
    AttemptConnectionAndVerifyPendingConnection(device_to_connect, local_device,
                                                feature, connection_priority,
                                                false /* is_listener */);
  }

  void CallListenForConnectionFromDeviceAndVerifyActiveConnection(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority) {
    AttemptConnectionAndVerifyActiveConnection(device_to_connect, local_device,
                                               feature, connection_priority,
                                               true /* is_listener */);
  }

  void CallInitiateConnectionToDeviceAndVerifyActiveConnection(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority) {
    AttemptConnectionAndVerifyActiveConnection(device_to_connect, local_device,
                                               feature, connection_priority,
                                               false /* is_listener */);
  }

  base::UnguessableToken
  CallListenForConnectionFromDeviceAndVerifyStillDisconnecting(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority) {
    return AttemptConnectionAndVerifyStillDisconnecting(
        device_to_connect, local_device, feature, connection_priority,
        true /* is_listener */);
  }

  base::UnguessableToken
  CallInitiateConnectionToDeviceAndVerifyStillDisconnecting(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority) {
    return AttemptConnectionAndVerifyStillDisconnecting(
        device_to_connect, local_device, feature, connection_priority,
        false /* is_listener */);
  }

  void SimulateSuccessfulConnection(const std::string& device_id) {
    ConnectionDetails connection_details(device_id,
                                         ConnectionMedium::kBluetoothLowEnergy);

    auto fake_authenticated_channel =
        std::make_unique<FakeAuthenticatedChannel>();
    auto* fake_authenticated_channel_raw = fake_authenticated_channel.get();

    std::vector<ClientConnectionParameters*> moved_client_list =
        fake_pending_connection_manager()->NotifyConnectionForHandledRequests(
            std::move(fake_authenticated_channel), connection_details);

    // Now, verify that ActiveConnectionManager has received the moved data.
    const auto& metadata = fake_active_connection_manager()
                               ->connection_details_to_active_metadata_map()
                               .find(connection_details)
                               ->second;
    EXPECT_EQ(ActiveConnectionManager::ConnectionState::kActiveConnectionExists,
              std::get<0>(metadata));
    EXPECT_EQ(fake_authenticated_channel_raw, std::get<1>(metadata).get());
    for (size_t i = 0; i < moved_client_list.size(); ++i)
      EXPECT_EQ(moved_client_list[i], std::get<2>(metadata)[i].get());
  }

  void SimulateConnectionStartingDisconnecting(const std::string& device_id) {
    fake_active_connection_manager()->SetDisconnecting(
        ConnectionDetails(device_id, ConnectionMedium::kBluetoothLowEnergy));
  }

  void SimulateConnectionBecomingDisconnected(const std::string& device_id) {
    ConnectionDetails connection_details(device_id,
                                         ConnectionMedium::kBluetoothLowEnergy);

    // If the connection was previously disconnected, there may have been
    // pending metadata corresponding to any connection attempts which were
    // triggered while the previous connection was in the disconnecting state.
    std::vector<std::tuple<base::UnguessableToken, std::string, ConnectionRole,
                           ConnectionPriority>>
        pending_metadata_list;

    auto it = disconnecting_details_to_requests_map_.find(connection_details);
    if (it != disconnecting_details_to_requests_map_.end()) {
      pending_metadata_list = it->second;
      disconnecting_details_to_requests_map_.erase(it);
    }

    fake_active_connection_manager()->SetDisconnected(
        ConnectionDetails(device_id, ConnectionMedium::kBluetoothLowEnergy));

    // If there were no pending metadata, there is no need to make additional
    // verifications.
    if (pending_metadata_list.empty())
      return;

    size_t num_handled_requests_start_index =
        fake_pending_connection_manager()->handled_requests().size() -
        pending_metadata_list.size();

    for (size_t i = 0; i < pending_metadata_list.size(); ++i) {
      VerifyHandledRequest(
          DeviceIdPair(device_id, std::get<1>(pending_metadata_list[i])),
          std::get<0>(pending_metadata_list[i]),
          std::get<2>(pending_metadata_list[i]),
          std::get<3>(pending_metadata_list[i]),
          connection_details.connection_medium(),
          num_handled_requests_start_index + i);
    }
  }

  void CancelPendingRequest(const base::UnguessableToken& request_id) {
    fake_client_connection_parameters_factory_
        ->id_to_active_client_parameters_map()
        .at(request_id)
        ->CancelClientRequest();
  }

  void FinishInitialization() {
    // The PendingConnectionManager should not have yet been created.
    EXPECT_FALSE(fake_pending_connection_manager());

    EXPECT_TRUE(test_task_runner_->HasPendingTask());
    test_task_runner_->RunUntilIdle();

    // The PendingConnectionManager should have been created, and all pending
    // requests should have been passed to it.
    EXPECT_EQ(num_queued_requests_before_initialization_,
              fake_pending_connection_manager()->handled_requests().size());
  }

  const multidevice::RemoteDeviceList& test_devices() { return test_devices_; }

  bool is_adapter_present() { return is_adapter_present_; }
  void set_is_adapter_present(bool present) { is_adapter_present_ = present; }

  bool is_adapter_powered() { return is_adapter_powered_; }
  void set_is_adapter_powered(bool powered) { is_adapter_powered_ = powered; }

 private:
  void AttemptConnectionAndVerifyPendingConnection(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority,
      bool is_listener) {
    // If this is the first time the Mojo service will be accessed,
    // |fake_pending_connection_manager_factory_| will not yet have created an
    // instance, so fake_pending_connection_manager() will be null.
    size_t num_handled_requests_before_call =
        fake_pending_connection_manager()
            ? fake_pending_connection_manager()->handled_requests().size()
            : 0;

    auto id = AttemptConnectionWithoutRejection(device_to_connect, local_device,
                                                feature, connection_priority,
                                                is_listener);

    FakePendingConnectionManager::HandledRequestsList& handled_requests =
        fake_pending_connection_manager()->handled_requests();
    EXPECT_EQ(num_handled_requests_before_call + 1u, handled_requests.size());

    VerifyHandledRequest(DeviceIdPair(device_to_connect.GetDeviceId(),
                                      local_device.GetDeviceId()),
                         id,
                         is_listener ? ConnectionRole::kListenerRole
                                     : ConnectionRole::kInitiatorRole,
                         connection_priority,
                         ConnectionMedium::kBluetoothLowEnergy,
                         handled_requests.size() - 1);
  }

  void AttemptConnectionAndVerifyActiveConnection(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority,
      bool is_listener) {
    ConnectionDetails connection_details(device_to_connect.GetDeviceId(),
                                         ConnectionMedium::kBluetoothLowEnergy);

    const std::vector<std::unique_ptr<ClientConnectionParameters>>&
        clients_for_active_connection =
            std::get<2>(fake_active_connection_manager()
                            ->connection_details_to_active_metadata_map()
                            .find(connection_details)
                            ->second);
    size_t num_clients_before_call = clients_for_active_connection.size();

    auto id = AttemptConnectionWithoutRejection(device_to_connect, local_device,
                                                feature, connection_priority,
                                                true /* is_listener */);

    EXPECT_EQ(num_clients_before_call + 1u,
              clients_for_active_connection.size());
    EXPECT_EQ(id, clients_for_active_connection.back()->id());
  }

  base::UnguessableToken AttemptConnectionAndVerifyStillDisconnecting(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority,
      bool is_listener) {
    FakePendingConnectionManager::HandledRequestsList& handled_requests =
        fake_pending_connection_manager()->handled_requests();
    size_t num_handled_requests_before_call = handled_requests.size();

    auto id = AttemptConnectionWithoutRejection(device_to_connect, local_device,
                                                feature, connection_priority,
                                                is_listener);

    // Since the channel is expected to be disconnecting, no additional
    // pending request should have been sent.
    EXPECT_EQ(num_handled_requests_before_call, handled_requests.size());

    // Store the metadata associated with this attempt in
    // |disconnecting_details_to_requests_map_|. When the connection becomes
    // fully disconnected, this entry will be verified in
    // SimulateConnectionBecomingDisconnected().
    ConnectionDetails connection_details(device_to_connect.GetDeviceId(),
                                         ConnectionMedium::kBluetoothLowEnergy);
    disconnecting_details_to_requests_map_[connection_details].push_back(
        std::make_tuple(id, local_device.GetDeviceId(),
                        is_listener ? ConnectionRole::kListenerRole
                                    : ConnectionRole::kInitiatorRole,
                        connection_priority));

    return id;
  }

  void VerifyHandledRequest(
      const DeviceIdPair& expected_device_id_pair,
      const base::UnguessableToken& expected_client_parameters_id,
      ConnectionRole expected_connection_role,
      ConnectionPriority expected_connection_priority,
      ConnectionMedium expected_connection_medium,
      size_t expected_pending_connection_manager_index) {
    const auto& request =
        fake_pending_connection_manager()->handled_requests().at(
            expected_pending_connection_manager_index);

    EXPECT_EQ(ConnectionAttemptDetails(expected_device_id_pair,
                                       expected_connection_medium,
                                       expected_connection_role),
              std::get<0>(request));
    EXPECT_EQ(expected_client_parameters_id, std::get<1>(request)->id());
    EXPECT_EQ(expected_connection_priority, std::get<2>(request));
  }

  void AttemptConnectionAndVerifyRejection(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority,
      mojom::ConnectionAttemptFailureReason expected_failure_reason,
      bool is_listener) {
    auto id = AttemptConnectionPostInitialization(
        device_to_connect, local_device, feature, connection_priority,
        is_listener);
    EXPECT_EQ(expected_failure_reason, GetFailureReasonForRequest(id));
  }

  const base::Optional<mojom::ConnectionAttemptFailureReason>&
  GetFailureReasonForRequest(const base::UnguessableToken& id) {
    return fake_client_connection_parameters_factory_
        ->id_to_failure_reason_when_deleted_map()
        .at(id);
  }

  // Attempt a connection that is not expected to be rejected. This function
  // verifies that devices were correctly set in the RemoteDeviceCache after the
  // request completed.
  base::UnguessableToken AttemptConnectionWithoutRejection(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority,
      bool is_listener) {
    auto id = AttemptConnectionPostInitialization(
        device_to_connect, local_device, feature, connection_priority,
        is_listener);

    // |device_to_connect| should be in the cache.
    EXPECT_TRUE(multidevice::IsSameDevice(
        device_to_connect, *remote_device_cache()->GetRemoteDevice(
                               device_to_connect.GetDeviceId())));

    // |local_device| should also be in the cache.
    EXPECT_TRUE(multidevice::IsSameDevice(
        local_device,
        *remote_device_cache()->GetRemoteDevice(local_device.GetDeviceId())));

    return id;
  }

  base::UnguessableToken AttemptConnectionPostInitialization(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority,
      bool is_listener) {
    base::UnguessableToken last_id_before_call =
        fake_client_connection_parameters_factory_->last_created_instance_id();

    AttemptConnection(device_to_connect, local_device, feature,
                      connection_priority, is_listener);

    base::UnguessableToken id_generated_by_call =
        fake_client_connection_parameters_factory_->last_created_instance_id();

    // The request should have caused a FakeClientConnectionParameters to be
    // created.
    EXPECT_NE(last_id_before_call, id_generated_by_call);

    return id_generated_by_call;
  }

  void AttemptConnectionPreInitialization(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority,
      bool is_listener) {
    // Should not have been any ClientConnectionParameters before the attempt.
    EXPECT_TRUE(
        fake_client_connection_parameters_factory_->last_created_instance_id()
            .is_empty());

    AttemptConnection(device_to_connect, local_device, feature,
                      connection_priority, is_listener);

    // Should still not have been any after the attempt.
    EXPECT_TRUE(
        fake_client_connection_parameters_factory_->last_created_instance_id()
            .is_empty());

    ++num_queued_requests_before_initialization_;
  }

  void AttemptConnection(const multidevice::RemoteDevice& device_to_connect,
                         const multidevice::RemoteDevice& local_device,
                         const std::string& feature,
                         ConnectionPriority connection_priority,
                         bool is_listener) {
    FakeConnectionDelegate fake_connection_delegate;

    if (is_listener) {
      secure_channel_remote_->ListenForConnectionFromDevice(
          device_to_connect, local_device, feature, connection_priority,
          fake_connection_delegate.GenerateRemote());
    } else {
      secure_channel_remote_->InitiateConnectionToDevice(
          device_to_connect, local_device, feature, connection_priority,
          fake_connection_delegate.GenerateRemote());
    }

    secure_channel_remote_.FlushForTesting();
  }

  FakeActiveConnectionManager* fake_active_connection_manager() {
    return fake_active_connection_manager_factory_->instance();
  }

  FakePendingConnectionManager* fake_pending_connection_manager() {
    return fake_pending_connection_manager_factory_->instance();
  }

  multidevice::RemoteDeviceCache* remote_device_cache() {
    return test_remote_device_cache_factory_->instance();
  }

  base::test::TaskEnvironment task_environment_;
  const multidevice::RemoteDeviceList test_devices_;

  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_;

  std::unique_ptr<FakeTimerFactoryFactory> fake_timer_factory_factory_;
  std::unique_ptr<TestRemoteDeviceCacheFactory>
      test_remote_device_cache_factory_;
  std::unique_ptr<FakeBleServiceDataHelperFactory>
      fake_ble_service_data_helper_factory_;
  std::unique_ptr<FakeBleConnectionManagerFactory>
      fake_ble_connection_manager_factory_;
  std::unique_ptr<FakePendingConnectionManagerFactory>
      fake_pending_connection_manager_factory_;
  std::unique_ptr<FakeActiveConnectionManagerFactory>
      fake_active_connection_manager_factory_;
  std::unique_ptr<TestSecureChannelInitializerFactory>
      test_secure_channel_initializer_factory_;
  std::unique_ptr<FakeClientConnectionParametersFactory>
      fake_client_connection_parameters_factory_;

  // Stores metadata which is expected to be pending when a connection attempt
  // is made while an ongoing connection is in the process of disconnecting.
  base::flat_map<ConnectionDetails,
                 std::vector<std::tuple<base::UnguessableToken,
                                        std::string,  // Local device ID.
                                        ConnectionRole,
                                        ConnectionPriority>>>
      disconnecting_details_to_requests_map_;

  size_t num_queued_requests_before_initialization_ = 0u;

  std::unique_ptr<SecureChannelBase> service_;

  bool is_adapter_powered_;
  bool is_adapter_present_;

  mojo::Remote<mojom::SecureChannel> secure_channel_remote_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelServiceTest);
};

TEST_F(SecureChannelServiceTest, ListenForConnection_MissingPublicKey) {
  FinishInitialization();

  multidevice::RemoteDevice device_to_connect = test_devices()[0];
  device_to_connect.public_key.clear();

  CallListenForConnectionFromDeviceAndVerifyRejection(
      device_to_connect, test_devices()[1], "feature", ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::REMOTE_DEVICE_INVALID_PUBLIC_KEY);
}

TEST_F(SecureChannelServiceTest, InitiateConnection_MissingPublicKey) {
  FinishInitialization();

  multidevice::RemoteDevice device_to_connect = test_devices()[0];
  device_to_connect.public_key.clear();

  CallInitiateConnectionToDeviceAndVerifyRejection(
      device_to_connect, test_devices()[1], "feature", ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::REMOTE_DEVICE_INVALID_PUBLIC_KEY);
}

TEST_F(SecureChannelServiceTest, ListenForConnection_MissingPsk) {
  FinishInitialization();

  multidevice::RemoteDevice device_to_connect = test_devices()[0];
  device_to_connect.persistent_symmetric_key.clear();

  CallListenForConnectionFromDeviceAndVerifyRejection(
      device_to_connect, test_devices()[1], "feature", ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::REMOTE_DEVICE_INVALID_PSK);
}

TEST_F(SecureChannelServiceTest, InitiateConnection_MissingPsk) {
  FinishInitialization();

  multidevice::RemoteDevice device_to_connect = test_devices()[0];
  device_to_connect.persistent_symmetric_key.clear();

  CallInitiateConnectionToDeviceAndVerifyRejection(
      device_to_connect, test_devices()[1], "feature", ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::REMOTE_DEVICE_INVALID_PSK);
}

TEST_F(SecureChannelServiceTest,
       ListenForConnection_MissingLocalDevicePublicKey) {
  FinishInitialization();

  multidevice::RemoteDevice local_device = test_devices()[1];
  local_device.public_key.clear();

  CallListenForConnectionFromDeviceAndVerifyRejection(
      test_devices()[0], local_device, "feature", ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::LOCAL_DEVICE_INVALID_PUBLIC_KEY);
}

TEST_F(SecureChannelServiceTest,
       InitiateConnection_MissingLocalDevicePublicKey) {
  FinishInitialization();

  multidevice::RemoteDevice local_device = test_devices()[1];
  local_device.public_key.clear();

  CallInitiateConnectionToDeviceAndVerifyRejection(
      test_devices()[0], local_device, "feature", ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::LOCAL_DEVICE_INVALID_PUBLIC_KEY);
}

TEST_F(SecureChannelServiceTest, ListenForConnection_MissingLocalDevicePsk) {
  FinishInitialization();

  multidevice::RemoteDevice local_device = test_devices()[1];
  local_device.persistent_symmetric_key.clear();

  CallListenForConnectionFromDeviceAndVerifyRejection(
      test_devices()[0], local_device, "feature", ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::LOCAL_DEVICE_INVALID_PSK);
}

TEST_F(SecureChannelServiceTest, InitiateConnection_MissingLocalDevicePsk) {
  FinishInitialization();

  multidevice::RemoteDevice local_device = test_devices()[1];
  local_device.persistent_symmetric_key.clear();

  CallInitiateConnectionToDeviceAndVerifyRejection(
      test_devices()[0], local_device, "feature", ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::LOCAL_DEVICE_INVALID_PSK);
}

TEST_F(SecureChannelServiceTest,
       ListenForConnection_BluetoothAdapterNotPresent) {
  FinishInitialization();

  set_is_adapter_present(false);

  CallListenForConnectionFromDeviceAndVerifyRejection(
      test_devices()[0], test_devices()[1], "feature", ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::ADAPTER_NOT_PRESENT);
}

TEST_F(SecureChannelServiceTest,
       InitiateConnection_BluetoothAdapterNotPresent) {
  FinishInitialization();

  set_is_adapter_present(false);

  CallInitiateConnectionToDeviceAndVerifyRejection(
      test_devices()[0], test_devices()[1], "feature", ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::ADAPTER_NOT_PRESENT);
}

TEST_F(SecureChannelServiceTest, ListenForConnection_BluetoothAdapterDisabled) {
  FinishInitialization();

  set_is_adapter_powered(false);

  CallListenForConnectionFromDeviceAndVerifyRejection(
      test_devices()[0], test_devices()[1], "feature", ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::ADAPTER_DISABLED);
}

TEST_F(SecureChannelServiceTest, InitiateConnection_BluetoothAdapterDisabled) {
  FinishInitialization();

  set_is_adapter_powered(false);

  CallInitiateConnectionToDeviceAndVerifyRejection(
      test_devices()[0], test_devices()[1], "feature", ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::ADAPTER_DISABLED);
}

TEST_F(SecureChannelServiceTest, CallsQueuedBeforeInitializationComplete) {
  CallInitiateConnectionToDeviceAndVerifyInitializationNotComplete(
      test_devices()[4], test_devices()[5], "feature",
      ConnectionPriority::kLow);
  CallListenForConnectionFromDeviceAndVerifyInitializationNotComplete(
      test_devices()[4], test_devices()[5], "feature",
      ConnectionPriority::kLow);
  FinishInitialization();
}

TEST_F(SecureChannelServiceTest, ListenForConnection_OneDevice) {
  FinishInitialization();

  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature",
      ConnectionPriority::kLow);
  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId());
  SimulateConnectionStartingDisconnecting(test_devices()[0].GetDeviceId());
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId());
}

TEST_F(SecureChannelServiceTest, InitiateConnection_OneDevice) {
  FinishInitialization();

  CallInitiateConnectionToDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature",
      ConnectionPriority::kLow);
  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId());
  SimulateConnectionStartingDisconnecting(test_devices()[0].GetDeviceId());
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId());
}

TEST_F(SecureChannelServiceTest,
       ListenForConnection_OneDevice_RequestSpecificLocalDevice) {
  FinishInitialization();

  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature",
      ConnectionPriority::kLow);
  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId());
  SimulateConnectionStartingDisconnecting(test_devices()[0].GetDeviceId());
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId());
}

TEST_F(SecureChannelServiceTest,
       InitiateConnection_OneDevice_RequestSpecificLocalDevice) {
  FinishInitialization();

  CallInitiateConnectionToDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature",
      ConnectionPriority::kLow);
  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId());
  SimulateConnectionStartingDisconnecting(test_devices()[0].GetDeviceId());
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId());
}

TEST_F(SecureChannelServiceTest, OneDevice_TwoConnectionRequests) {
  FinishInitialization();

  // Two pending connection requests for the same device.
  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature1",
      ConnectionPriority::kLow);
  CallInitiateConnectionToDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature2",
      ConnectionPriority::kMedium);

  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId());
  SimulateConnectionStartingDisconnecting(test_devices()[0].GetDeviceId());
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId());
}

TEST_F(SecureChannelServiceTest,
       OneDevice_TwoConnectionRequests_OneAfterConnection) {
  FinishInitialization();

  // First request is successful.
  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature1",
      ConnectionPriority::kLow);
  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId());

  // Second request is added to the existing channel.
  CallInitiateConnectionToDeviceAndVerifyActiveConnection(
      test_devices()[0], test_devices()[1], "feature2",
      ConnectionPriority::kMedium);

  SimulateConnectionStartingDisconnecting(test_devices()[0].GetDeviceId());
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId());
}

TEST_F(SecureChannelServiceTest,
       OneDevice_TwoConnectionRequests_OneWhileDisconnecting) {
  FinishInitialization();

  // First request is successful.
  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature1",
      ConnectionPriority::kLow);
  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId());

  // Connection starts disconnecting.
  SimulateConnectionStartingDisconnecting(test_devices()[0].GetDeviceId());

  // Second request is added before disconnecting is complete.
  CallInitiateConnectionToDeviceAndVerifyStillDisconnecting(
      test_devices()[0], test_devices()[1], "feature2",
      ConnectionPriority::kMedium);

  // Complete the disconnection; this should cause the second request to be
  // delivered to PendingConnectionManager.
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId());

  // The second attempt succeeds.
  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId());

  SimulateConnectionStartingDisconnecting(test_devices()[0].GetDeviceId());
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId());
}

TEST_F(SecureChannelServiceTest,
       OneDevice_TwoConnectionRequests_OneWhileDisconnecting_Canceled) {
  FinishInitialization();

  // First request is successful.
  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature1",
      ConnectionPriority::kLow);
  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId());

  // Connection starts disconnecting.
  SimulateConnectionStartingDisconnecting(test_devices()[0].GetDeviceId());

  // Second request is added before disconnecting is complete, but the request
  // is canceled before the disconnection completes.
  auto id = CallInitiateConnectionToDeviceAndVerifyStillDisconnecting(
      test_devices()[0], test_devices()[1], "feature2",
      ConnectionPriority::kMedium);
  CancelPendingRequest(id);

  // Complete the disconnection; even though the request was canceled, it should
  // still have been added to PendingConnectionManager.
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId());
}

TEST_F(SecureChannelServiceTest, ThreeDevices) {
  FinishInitialization();

  // Two requests for each device.
  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature1",
      ConnectionPriority::kLow);
  CallInitiateConnectionToDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature2",
      ConnectionPriority::kMedium);
  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[2], test_devices()[1], "feature3",
      ConnectionPriority::kHigh);
  CallInitiateConnectionToDeviceAndVerifyPendingConnection(
      test_devices()[2], test_devices()[1], "feature4",
      ConnectionPriority::kLow);
  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[3], test_devices()[1], "feature5",
      ConnectionPriority::kMedium);
  CallInitiateConnectionToDeviceAndVerifyPendingConnection(
      test_devices()[3], test_devices()[1], "feature6",
      ConnectionPriority::kHigh);

  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId());
  SimulateSuccessfulConnection(test_devices()[2].GetDeviceId());
  SimulateSuccessfulConnection(test_devices()[3].GetDeviceId());

  SimulateConnectionStartingDisconnecting(test_devices()[0].GetDeviceId());
  SimulateConnectionStartingDisconnecting(test_devices()[2].GetDeviceId());
  SimulateConnectionStartingDisconnecting(test_devices()[3].GetDeviceId());

  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId());
  SimulateConnectionBecomingDisconnected(test_devices()[2].GetDeviceId());
  SimulateConnectionBecomingDisconnected(test_devices()[3].GetDeviceId());
}

}  // namespace secure_channel

}  // namespace chromeos
