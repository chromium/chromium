// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/ash/components/multidevice/remote_device_cache.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/timer_factory/fake_timer_factory.h"
#include "chromeos/ash/components/timer_factory/timer_factory_impl.h"
#include "chromeos/ash/services/secure_channel/active_connection_manager_impl.h"
#include "chromeos/ash/services/secure_channel/ble_connection_manager_impl.h"
#include "chromeos/ash/services/secure_channel/ble_scanner_impl.h"
#include "chromeos/ash/services/secure_channel/ble_synchronizer.h"
#include "chromeos/ash/services/secure_channel/bluetooth_helper_impl.h"
#include "chromeos/ash/services/secure_channel/client_connection_parameters_impl.h"
#include "chromeos/ash/services/secure_channel/fake_active_connection_manager.h"
#include "chromeos/ash/services/secure_channel/fake_authenticated_channel.h"
#include "chromeos/ash/services/secure_channel/fake_ble_connection_manager.h"
#include "chromeos/ash/services/secure_channel/fake_ble_scanner.h"
#include "chromeos/ash/services/secure_channel/fake_ble_synchronizer.h"
#include "chromeos/ash/services/secure_channel/fake_bluetooth_helper.h"
#include "chromeos/ash/services/secure_channel/fake_client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/fake_connection_delegate.h"
#include "chromeos/ash/services/secure_channel/fake_nearby_connection_manager.h"
#include "chromeos/ash/services/secure_channel/fake_pending_connection_manager.h"
#include "chromeos/ash/services/secure_channel/fake_secure_channel_disconnector.h"
#include "chromeos/ash/services/secure_channel/nearby_connection_manager_impl.h"
#include "chromeos/ash/services/secure_channel/pending_connection_manager_impl.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_nearby_connector.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_secure_channel_structured_metrics_logger.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/ash/services/secure_channel/secure_channel_disconnector_impl.h"
#include "chromeos/ash/services/secure_channel/secure_channel_initializer.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

const size_t kNumTestDevices = 6;

class TestRemoteDeviceCacheFactory
    : public multidevice::RemoteDeviceCache::Factory {
 public:
  TestRemoteDeviceCacheFactory() = default;

  TestRemoteDeviceCacheFactory(const TestRemoteDeviceCacheFactory&) = delete;
  TestRemoteDeviceCacheFactory& operator=(const TestRemoteDeviceCacheFactory&) =
      delete;

  ~TestRemoteDeviceCacheFactory() override = default;

  multidevice::RemoteDeviceCache* instance() { return instance_; }

 private:
  // multidevice::RemoteDeviceCache::Factory:
  std::unique_ptr<multidevice::RemoteDeviceCache> CreateInstance() override {
    EXPECT_FALSE(instance_);
    // Silly hack to avoid infinite recursion: this factory really just wants to
    // save a pointer to the created object.
    multidevice::RemoteDeviceCache::Factory::SetFactoryForTesting(nullptr);
    auto instance = multidevice::RemoteDeviceCache::Factory::Create();
    multidevice::RemoteDeviceCache::Factory::SetFactoryForTesting(this);
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<multidevice::RemoteDeviceCache, DanglingUntriaged> instance_ =
      nullptr;
};

class FakeBluetoothHelperFactory : public BluetoothHelperImpl::Factory {
 public:
  FakeBluetoothHelperFactory(
      TestRemoteDeviceCacheFactory* test_remote_device_cache_factory)
      : test_remote_device_cache_factory_(test_remote_device_cache_factory) {}

  FakeBluetoothHelperFactory(const FakeBluetoothHelperFactory&) = delete;
  FakeBluetoothHelperFactory& operator=(const FakeBluetoothHelperFactory&) =
      delete;

  ~FakeBluetoothHelperFactory() override = default;

  FakeBluetoothHelper* instance() { return instance_; }

 private:
  // BluetoothHelperImpl::Factory:
  std::unique_ptr<BluetoothHelper> CreateInstance(
      multidevice::RemoteDeviceCache* remote_device_cache) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(test_remote_device_cache_factory_->instance(),
              remote_device_cache);

    auto instance = std::make_unique<FakeBluetoothHelper>();
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<TestRemoteDeviceCacheFactory> test_remote_device_cache_factory_;

  raw_ptr<FakeBluetoothHelper, DanglingUntriaged> instance_ = nullptr;
};

class FakeBleSynchronizerFactory : public BleSynchronizer::Factory {
 public:
  FakeBleSynchronizerFactory() = default;

  FakeBleSynchronizerFactory(const FakeBleSynchronizerFactory&) = delete;
  FakeBleSynchronizerFactory& operator=(const FakeBleSynchronizerFactory&) =
      delete;

  ~FakeBleSynchronizerFactory() override = default;

  FakeBleSynchronizer* instance() { return instance_; }

 private:
  // BleSynchronizer::Factory:
  std::unique_ptr<BleSynchronizerBase> CreateInstance(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) override {
    EXPECT_FALSE(instance_);

    auto instance = std::make_unique<FakeBleSynchronizer>();
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<FakeBleSynchronizer, DanglingUntriaged> instance_ = nullptr;
};

class FakeBleScannerFactory : public BleScannerImpl::Factory {
 public:
  FakeBleScannerFactory(
      FakeBluetoothHelperFactory* fake_bluetooth_helper_factory,
      FakeBleSynchronizerFactory* fake_ble_synchronizer_factory)
      : fake_bluetooth_helper_factory_(fake_bluetooth_helper_factory),
        fake_ble_synchronizer_factory_(fake_ble_synchronizer_factory) {}

  FakeBleScannerFactory(const FakeBleScannerFactory&) = delete;
  FakeBleScannerFactory& operator=(const FakeBleScannerFactory&) = delete;

  ~FakeBleScannerFactory() override = default;

  FakeBleScanner* instance() { return instance_; }

 private:
  // BleScannerImpl::Factory:
  std::unique_ptr<BleScanner> CreateInstance(
      BluetoothHelper* bluetooth_helper,
      BleSynchronizerBase* ble_synchronizer_base,
      scoped_refptr<device::BluetoothAdapter> adapter) override {
    EXPECT_EQ(fake_bluetooth_helper_factory_->instance(), bluetooth_helper);
    EXPECT_EQ(fake_ble_synchronizer_factory_->instance(),
              ble_synchronizer_base);
    EXPECT_FALSE(instance_);

    auto instance = std::make_unique<FakeBleScanner>();
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<FakeBleScanner, DanglingUntriaged> instance_ = nullptr;

  raw_ptr<FakeBluetoothHelperFactory> fake_bluetooth_helper_factory_;
  raw_ptr<FakeBleSynchronizerFactory> fake_ble_synchronizer_factory_;
};

class FakeSecureChannelDisconnectorFactory
    : public SecureChannelDisconnectorImpl::Factory {
 public:
  FakeSecureChannelDisconnectorFactory() = default;

  FakeSecureChannelDisconnectorFactory(
      const FakeSecureChannelDisconnectorFactory&) = delete;
  FakeSecureChannelDisconnectorFactory& operator=(
      const FakeSecureChannelDisconnectorFactory&) = delete;

  ~FakeSecureChannelDisconnectorFactory() override = default;

  FakeSecureChannelDisconnector* instance() { return instance_; }

 private:
  // SecureChannelDisconnectorImpl::Factory:
  std::unique_ptr<SecureChannelDisconnector> CreateInstance() override {
    auto instance = std::make_unique<FakeSecureChannelDisconnector>();
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<FakeSecureChannelDisconnector, DanglingUntriaged> instance_ = nullptr;
};

class FakeBleConnectionManagerFactory
    : public BleConnectionManagerImpl::Factory {
 public:
  FakeBleConnectionManagerFactory(
      device::BluetoothAdapter* expected_bluetooth_adapter,
      FakeBluetoothHelperFactory* fake_bluetooth_helper_factory,
      FakeBleSynchronizerFactory* fake_ble_synchronizer_factory,
      FakeBleScannerFactory* fake_ble_scanner_factory,
      FakeSecureChannelDisconnectorFactory*
          fake_secure_channel_disconnector_factory)
      : expected_bluetooth_adapter_(expected_bluetooth_adapter),
        fake_bluetooth_helper_factory_(fake_bluetooth_helper_factory),
        fake_ble_synchronizer_factory_(fake_ble_synchronizer_factory),
        fake_ble_scanner_factory_(fake_ble_scanner_factory),
        fake_secure_channel_disconnector_factory_(
            fake_secure_channel_disconnector_factory) {}

  FakeBleConnectionManagerFactory(const FakeBleConnectionManagerFactory&) =
      delete;
  FakeBleConnectionManagerFactory& operator=(
      const FakeBleConnectionManagerFactory&) = delete;

  ~FakeBleConnectionManagerFactory() override = default;

  FakeBleConnectionManager* instance() { return instance_; }

 private:
  // BleConnectionManagerImpl::Factory:
  std::unique_ptr<BleConnectionManager> CreateInstance(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      BluetoothHelper* bluetooth_helper,
      BleSynchronizerBase* ble_synchronizer,
      BleScanner* ble_scanner,
      SecureChannelDisconnector* secure_channel_disconnector,
      ash::timer_factory::TimerFactory* timer_factory,
      base::Clock* clock) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(expected_bluetooth_adapter_, bluetooth_adapter.get());
    EXPECT_EQ(fake_bluetooth_helper_factory_->instance(), bluetooth_helper);
    EXPECT_EQ(fake_ble_synchronizer_factory_->instance(), ble_synchronizer);
    EXPECT_EQ(fake_ble_scanner_factory_->instance(), ble_scanner);
    EXPECT_EQ(fake_secure_channel_disconnector_factory_->instance(),
              secure_channel_disconnector);

    auto instance = std::make_unique<FakeBleConnectionManager>();
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<device::BluetoothAdapter> expected_bluetooth_adapter_;
  raw_ptr<FakeBluetoothHelperFactory> fake_bluetooth_helper_factory_;
  raw_ptr<FakeBleSynchronizerFactory> fake_ble_synchronizer_factory_;
  raw_ptr<FakeBleScannerFactory> fake_ble_scanner_factory_;
  raw_ptr<FakeSecureChannelDisconnectorFactory>
      fake_secure_channel_disconnector_factory_;

  raw_ptr<FakeBleConnectionManager, DanglingUntriaged> instance_ = nullptr;
};

class FakeNearbyConnectionManagerFactory
    : public NearbyConnectionManagerImpl::Factory {
 public:
  FakeNearbyConnectionManagerFactory(
      FakeBleScannerFactory* fake_ble_scanner_factory,
      FakeSecureChannelDisconnectorFactory*
          fake_secure_channel_disconnector_factory)
      : fake_ble_scanner_factory_(fake_ble_scanner_factory),
        fake_secure_channel_disconnector_factory_(
            fake_secure_channel_disconnector_factory) {}

  FakeNearbyConnectionManagerFactory(
      const FakeNearbyConnectionManagerFactory&) = delete;
  FakeNearbyConnectionManagerFactory& operator=(
      const FakeNearbyConnectionManagerFactory&) = delete;

  ~FakeNearbyConnectionManagerFactory() override = default;

  FakeNearbyConnectionManager* instance() { return instance_; }

 private:
  // NearbyConnectionManagerImpl::Factory:
  std::unique_ptr<NearbyConnectionManager> CreateInstance(
      BleScanner* ble_scanner,
      SecureChannelDisconnector* secure_channel_disconnector) override {
    EXPECT_EQ(fake_ble_scanner_factory_->instance(), ble_scanner);
    EXPECT_EQ(fake_secure_channel_disconnector_factory_->instance(),
              secure_channel_disconnector);

    EXPECT_FALSE(instance_);
    auto instance = std::make_unique<FakeNearbyConnectionManager>();
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<FakeBleScannerFactory> fake_ble_scanner_factory_;
  raw_ptr<FakeSecureChannelDisconnectorFactory>
      fake_secure_channel_disconnector_factory_;

  raw_ptr<FakeNearbyConnectionManager, DanglingUntriaged> instance_ = nullptr;
};

class FakePendingConnectionManagerFactory
    : public PendingConnectionManagerImpl::Factory {
 public:
  FakePendingConnectionManagerFactory(
      FakeBleConnectionManagerFactory* fake_ble_connection_manager_factory,
      FakeNearbyConnectionManagerFactory*
          fake_nearby_connection_manager_factory)
      : fake_ble_connection_manager_factory_(
            fake_ble_connection_manager_factory),
        fake_nearby_connection_manager_factory_(
            fake_nearby_connection_manager_factory) {}

  FakePendingConnectionManagerFactory(
      const FakePendingConnectionManagerFactory&) = delete;
  FakePendingConnectionManagerFactory& operator=(
      const FakePendingConnectionManagerFactory&) = delete;

  ~FakePendingConnectionManagerFactory() override = default;

  FakePendingConnectionManager* instance() { return instance_; }

 private:
  // PendingConnectionManagerImpl::Factory:
  std::unique_ptr<PendingConnectionManager> CreateInstance(
      PendingConnectionManager::Delegate* delegate,
      BleConnectionManager* ble_connection_manager,
      NearbyConnectionManager* nearby_connection_manager,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(fake_ble_connection_manager_factory_->instance(),
              ble_connection_manager);
    EXPECT_EQ(fake_nearby_connection_manager_factory_->instance(),
              nearby_connection_manager);

    auto instance = std::make_unique<FakePendingConnectionManager>(delegate);
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<FakeBleConnectionManagerFactory> fake_ble_connection_manager_factory_;
  raw_ptr<FakeNearbyConnectionManagerFactory>
      fake_nearby_connection_manager_factory_;

  raw_ptr<FakePendingConnectionManager, DanglingUntriaged> instance_ = nullptr;
};

class FakeActiveConnectionManagerFactory
    : public ActiveConnectionManagerImpl::Factory {
 public:
  FakeActiveConnectionManagerFactory() = default;

  FakeActiveConnectionManagerFactory(
      const FakeActiveConnectionManagerFactory&) = delete;
  FakeActiveConnectionManagerFactory& operator=(
      const FakeActiveConnectionManagerFactory&) = delete;

  ~FakeActiveConnectionManagerFactory() override = default;

  FakeActiveConnectionManager* instance() { return instance_; }

 private:
  // ActiveConnectionManagerImpl::Factory:
  std::unique_ptr<ActiveConnectionManager> CreateInstance(
      ActiveConnectionManager::Delegate* delegate) override {
    EXPECT_FALSE(instance_);
    auto instance = std::make_unique<FakeActiveConnectionManager>(delegate);
    instance_ = instance.get();
    return instance;
  }

  raw_ptr<FakeActiveConnectionManager, DanglingUntriaged> instance_ = nullptr;
};

class TestSecureChannelInitializerFactory
    : public SecureChannelInitializer::Factory {
 public:
  TestSecureChannelInitializerFactory(
      scoped_refptr<base::TestSimpleTaskRunner> test_task_runner)
      : test_task_runner_(test_task_runner) {}

  TestSecureChannelInitializerFactory(
      const TestSecureChannelInitializerFactory&) = delete;
  TestSecureChannelInitializerFactory& operator=(
      const TestSecureChannelInitializerFactory&) = delete;

  ~TestSecureChannelInitializerFactory() override = default;

 private:
  // SecureChannelInitializer::Factory:
  std::unique_ptr<SecureChannelBase> CreateInstance(
      scoped_refptr<base::TaskRunner> task_runner) override {
    EXPECT_FALSE(instance_);
    // Silly hack to avoid infinite recursion: this factory really just wants to
    // save a pointer to the created object.
    SecureChannelInitializer::Factory::SetFactoryForTesting(nullptr);
    auto instance =
        SecureChannelInitializer::Factory::Create(test_task_runner_);
    SecureChannelInitializer::Factory::SetFactoryForTesting(this);
    instance_ = instance.get();
    return instance;
  }

  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_;

  raw_ptr<SecureChannelBase, DanglingUntriaged> instance_ = nullptr;
};

class FakeClientConnectionParametersFactory
    : public ClientConnectionParametersImpl::Factory {
 public:
  FakeClientConnectionParametersFactory() = default;

  FakeClientConnectionParametersFactory(
      const FakeClientConnectionParametersFactory&) = delete;
  FakeClientConnectionParametersFactory& operator=(
      const FakeClientConnectionParametersFactory&) = delete;

  ~FakeClientConnectionParametersFactory() override = default;

  const base::UnguessableToken& last_created_instance_id() {
    return last_created_instance_id_;
  }

  std::unordered_map<base::UnguessableToken,
                     raw_ptr<FakeClientConnectionParameters, CtnExperimental>,
                     base::UnguessableTokenHash>&
  id_to_active_client_parameters_map() {
    return id_to_active_client_parameters_map_;
  }

  const std::unordered_map<base::UnguessableToken,
                           std::optional<mojom::ConnectionAttemptFailureReason>,
                           base::UnguessableTokenHash>&
  id_to_failure_reason_when_deleted_map() {
    return id_to_failure_reason_when_deleted_map_;
  }

 private:
  // ClientConnectionParametersImpl::Factory:
  std::unique_ptr<ClientConnectionParameters> CreateInstance(
      const std::string& feature,
      mojo::PendingRemote<mojom::ConnectionDelegate> connection_delegate_remote,
      mojo::PendingRemote<mojom::SecureChannelStructuredMetricsLogger>
          secure_channel_structured_metrics_logger) override {
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
                     raw_ptr<FakeClientConnectionParameters, CtnExperimental>,
                     base::UnguessableTokenHash>
      id_to_active_client_parameters_map_;

  std::unordered_map<base::UnguessableToken,
                     std::optional<mojom::ConnectionAttemptFailureReason>,
                     base::UnguessableTokenHash>
      id_to_failure_reason_when_deleted_map_;
};

}  // namespace

class SecureChannelServiceTest : public testing::Test {
 public:
  SecureChannelServiceTest(const SecureChannelServiceTest&) = delete;
  SecureChannelServiceTest& operator=(const SecureChannelServiceTest&) = delete;

 protected:
  SecureChannelServiceTest()
      : test_devices_(
            multidevice::CreateRemoteDeviceListForTest(kNumTestDevices)) {}
  ~SecureChannelServiceTest() override = default;

  // testing::Test:
  void SetUp() override {
    bluez::BluezDBusManager::GetSetterForTesting();
    bluez::BluezDBusManager::InitializeFake();
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

    fake_nearby_connector_ = std::make_unique<FakeNearbyConnector>();

    ash::timer_factory::TimerFactoryImpl::Factory::SetFactoryForTesting(
        std::make_unique<ash::timer_factory::FakeTimerFactory::Factory>());

    test_remote_device_cache_factory_ =
        std::make_unique<TestRemoteDeviceCacheFactory>();
    multidevice::RemoteDeviceCache::Factory::SetFactoryForTesting(
        test_remote_device_cache_factory_.get());

    fake_bluetooth_helper_factory_ =
        std::make_unique<FakeBluetoothHelperFactory>(
            test_remote_device_cache_factory_.get());
    BluetoothHelperImpl::Factory::SetFactoryForTesting(
        fake_bluetooth_helper_factory_.get());

    fake_ble_synchronizer_factory_ =
        std::make_unique<FakeBleSynchronizerFactory>();
    BleSynchronizer::Factory::SetFactoryForTesting(
        fake_ble_synchronizer_factory_.get());

    fake_ble_scanner_factory_ = std::make_unique<FakeBleScannerFactory>(
        fake_bluetooth_helper_factory_.get(),
        fake_ble_synchronizer_factory_.get());
    BleScannerImpl::Factory::SetFactoryForTesting(
        fake_ble_scanner_factory_.get());

    fake_secure_channel_disconnector_factory_ =
        std::make_unique<FakeSecureChannelDisconnectorFactory>();
    SecureChannelDisconnectorImpl::Factory::SetFactoryForTesting(
        fake_secure_channel_disconnector_factory_.get());

    fake_ble_connection_manager_factory_ =
        std::make_unique<FakeBleConnectionManagerFactory>(
            mock_adapter_.get(), fake_bluetooth_helper_factory_.get(),
            fake_ble_synchronizer_factory_.get(),
            fake_ble_scanner_factory_.get(),
            fake_secure_channel_disconnector_factory_.get());
    BleConnectionManagerImpl::Factory::SetFactoryForTesting(
        fake_ble_connection_manager_factory_.get());

    fake_nearby_connection_manager_factory_ =
        std::make_unique<FakeNearbyConnectionManagerFactory>(
            fake_ble_scanner_factory_.get(),
            fake_secure_channel_disconnector_factory_.get());
    NearbyConnectionManagerImpl::Factory::SetFactoryForTesting(
        fake_nearby_connection_manager_factory_.get());

    fake_pending_connection_manager_factory_ =
        std::make_unique<FakePendingConnectionManagerFactory>(
            fake_ble_connection_manager_factory_.get(),
            fake_nearby_connection_manager_factory_.get());
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

    service_ = SecureChannelInitializer::Factory::Create();
    service_->BindReceiver(secure_channel_remote_.BindNewPipeAndPassReceiver());
    secure_channel_remote_.FlushForTesting();
  }

  void TearDown() override {
    ash::timer_factory::TimerFactoryImpl::Factory::SetFactoryForTesting(
        nullptr);
    multidevice::RemoteDeviceCache::Factory::SetFactoryForTesting(nullptr);
    BluetoothHelperImpl::Factory::SetFactoryForTesting(nullptr);
    BleSynchronizer::Factory::SetFactoryForTesting(nullptr);
    BleScannerImpl::Factory::SetFactoryForTesting(nullptr);
    SecureChannelDisconnectorImpl::Factory::SetFactoryForTesting(nullptr);
    BleConnectionManagerImpl::Factory::SetFactoryForTesting(nullptr);
    NearbyConnectionManagerImpl::Factory::SetFactoryForTesting(nullptr);
    PendingConnectionManagerImpl::Factory::SetFactoryForTesting(nullptr);
    ActiveConnectionManagerImpl::Factory::SetFactoryForTesting(nullptr);
    SecureChannelInitializer::Factory::SetFactoryForTesting(nullptr);
    ClientConnectionParametersImpl::Factory::SetFactoryForTesting(nullptr);
  }

  void CallListenForConnectionFromDeviceAndVerifyInitializationNotComplete(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority) {
    AttemptConnectionPreInitialization(device_to_connect, local_device, feature,
                                       connection_medium, connection_priority,
                                       true /* is_listener */);
  }

  void CallInitiateConnectionToDeviceAndVerifyInitializationNotComplete(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority) {
    AttemptConnectionPreInitialization(device_to_connect, local_device, feature,
                                       connection_medium, connection_priority,
                                       false /* is_listener */);
  }

  void CallListenForConnectionFromDeviceAndVerifyRejection(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      mojom::ConnectionAttemptFailureReason expected_failure_reason) {
    AttemptConnectionAndVerifyRejection(
        device_to_connect, local_device, feature, connection_medium,
        connection_priority, expected_failure_reason, true /* is_listener */);
  }

  void CallInitiateConnectionToDeviceAndVerifyRejection(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      mojom::ConnectionAttemptFailureReason expected_failure_reason) {
    AttemptConnectionAndVerifyRejection(
        device_to_connect, local_device, feature, connection_medium,
        connection_priority, expected_failure_reason, false /* is_listener */);
  }

  void CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority) {
    AttemptConnectionAndVerifyPendingConnection(
        device_to_connect, local_device, feature, connection_medium,
        connection_priority, true /* is_listener */);
  }

  void CallInitiateConnectionToDeviceAndVerifyPendingConnection(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority) {
    AttemptConnectionAndVerifyPendingConnection(
        device_to_connect, local_device, feature, connection_medium,
        connection_priority, false /* is_listener */);
  }

  void CallListenForConnectionFromDeviceAndVerifyActiveConnection(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority) {
    AttemptConnectionAndVerifyActiveConnection(
        device_to_connect, local_device, feature, connection_medium,
        connection_priority, true /* is_listener */);
  }

  void CallInitiateConnectionToDeviceAndVerifyActiveConnection(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority) {
    AttemptConnectionAndVerifyActiveConnection(
        device_to_connect, local_device, feature, connection_medium,
        connection_priority, false /* is_listener */);
  }

  base::UnguessableToken
  CallListenForConnectionFromDeviceAndVerifyStillDisconnecting(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority) {
    return AttemptConnectionAndVerifyStillDisconnecting(
        device_to_connect, local_device, feature, connection_medium,
        connection_priority, true /* is_listener */);
  }

  base::UnguessableToken
  CallInitiateConnectionToDeviceAndVerifyStillDisconnecting(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority) {
    return AttemptConnectionAndVerifyStillDisconnecting(
        device_to_connect, local_device, feature, connection_medium,
        connection_priority, false /* is_listener */);
  }

  void SimulateSuccessfulConnection(const std::string& device_id,
                                    ConnectionMedium connection_medium) {
    ConnectionDetails connection_details(device_id, connection_medium);

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
    for (size_t i = 0; i < moved_client_list.size(); ++i) {
      EXPECT_EQ(moved_client_list[i], std::get<2>(metadata)[i].get());
    }
  }

  void SimulateConnectionStartingDisconnecting(
      const std::string& device_id,
      ConnectionMedium connection_medium) {
    fake_active_connection_manager()->SetDisconnecting(
        ConnectionDetails(device_id, connection_medium));
  }

  void SimulateConnectionBecomingDisconnected(
      const std::string& device_id,
      ConnectionMedium connection_medium) {
    ConnectionDetails connection_details(device_id, connection_medium);

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
        ConnectionDetails(device_id, connection_medium));

    // If there were no pending metadata, there is no need to make additional
    // verifications.
    if (pending_metadata_list.empty()) {
      return;
    }

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

  void FinishInitialization(bool set_nearby_connector = true) {
    // The PendingConnectionManager should not have yet been created.
    EXPECT_FALSE(fake_pending_connection_manager());

    if (set_nearby_connector) {
      secure_channel_remote_->SetNearbyConnector(
          fake_nearby_connector_->GeneratePendingRemote());
      secure_channel_remote_.FlushForTesting();
    }

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
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      bool is_listener) {
    // If this is the first time the Mojo service will be accessed,
    // |fake_pending_connection_manager_factory_| will not yet have created an
    // instance, so fake_pending_connection_manager() will be null.
    size_t num_handled_requests_before_call =
        fake_pending_connection_manager()
            ? fake_pending_connection_manager()->handled_requests().size()
            : 0;

    auto id = AttemptConnectionWithoutRejection(
        device_to_connect, local_device, feature, connection_medium,
        connection_priority, is_listener);

    FakePendingConnectionManager::HandledRequestsList& handled_requests =
        fake_pending_connection_manager()->handled_requests();
    EXPECT_EQ(num_handled_requests_before_call + 1u, handled_requests.size());

    VerifyHandledRequest(DeviceIdPair(device_to_connect.GetDeviceId(),
                                      local_device.GetDeviceId()),
                         id,
                         is_listener ? ConnectionRole::kListenerRole
                                     : ConnectionRole::kInitiatorRole,
                         connection_priority, connection_medium,
                         handled_requests.size() - 1);
  }

  void AttemptConnectionAndVerifyActiveConnection(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      bool is_listener) {
    ConnectionDetails connection_details(device_to_connect.GetDeviceId(),
                                         connection_medium);

    const std::vector<std::unique_ptr<ClientConnectionParameters>>&
        clients_for_active_connection =
            std::get<2>(fake_active_connection_manager()
                            ->connection_details_to_active_metadata_map()
                            .find(connection_details)
                            ->second);
    size_t num_clients_before_call = clients_for_active_connection.size();

    auto id = AttemptConnectionWithoutRejection(
        device_to_connect, local_device, feature, connection_medium,
        connection_priority, true /* is_listener */);

    EXPECT_EQ(num_clients_before_call + 1u,
              clients_for_active_connection.size());
    EXPECT_EQ(id, clients_for_active_connection.back()->id());
  }

  base::UnguessableToken AttemptConnectionAndVerifyStillDisconnecting(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      bool is_listener) {
    FakePendingConnectionManager::HandledRequestsList& handled_requests =
        fake_pending_connection_manager()->handled_requests();
    size_t num_handled_requests_before_call = handled_requests.size();

    auto id = AttemptConnectionWithoutRejection(
        device_to_connect, local_device, feature, connection_medium,
        connection_priority, is_listener);

    // Since the channel is expected to be disconnecting, no additional
    // pending request should have been sent.
    EXPECT_EQ(num_handled_requests_before_call, handled_requests.size());

    // Store the metadata associated with this attempt in
    // |disconnecting_details_to_requests_map_|. When the connection becomes
    // fully disconnected, this entry will be verified in
    // SimulateConnectionBecomingDisconnected().
    ConnectionDetails connection_details(device_to_connect.GetDeviceId(),
                                         connection_medium);
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
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      mojom::ConnectionAttemptFailureReason expected_failure_reason,
      bool is_listener) {
    auto id = AttemptConnectionPostInitialization(
        device_to_connect, local_device, feature, connection_medium,
        connection_priority, is_listener);
    EXPECT_EQ(expected_failure_reason, GetFailureReasonForRequest(id));
  }

  const std::optional<mojom::ConnectionAttemptFailureReason>&
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
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      bool is_listener) {
    auto id = AttemptConnectionPostInitialization(
        device_to_connect, local_device, feature, connection_medium,
        connection_priority, is_listener);

    // |device_to_connect| should be in the cache.
    EXPECT_TRUE(multidevice::IsSameDevice(
        device_to_connect,
        *remote_device_cache()->GetRemoteDevice(
            device_to_connect.instance_id, device_to_connect.GetDeviceId())));

    // |local_device| should also be in the cache.
    EXPECT_TRUE(multidevice::IsSameDevice(
        local_device,
        *remote_device_cache()->GetRemoteDevice(local_device.instance_id,
                                                local_device.GetDeviceId())));

    return id;
  }

  base::UnguessableToken AttemptConnectionPostInitialization(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      bool is_listener) {
    base::UnguessableToken last_id_before_call =
        fake_client_connection_parameters_factory_->last_created_instance_id();

    AttemptConnection(device_to_connect, local_device, feature,
                      connection_medium, connection_priority, is_listener);

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
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      bool is_listener) {
    // Should not have been any ClientConnectionParameters before the attempt.
    EXPECT_TRUE(
        fake_client_connection_parameters_factory_->last_created_instance_id()
            .is_empty());

    AttemptConnection(device_to_connect, local_device, feature,
                      connection_medium, connection_priority, is_listener);

    // Should still not have been any after the attempt.
    EXPECT_TRUE(
        fake_client_connection_parameters_factory_->last_created_instance_id()
            .is_empty());

    ++num_queued_requests_before_initialization_;
  }

  void AttemptConnection(const multidevice::RemoteDevice& device_to_connect,
                         const multidevice::RemoteDevice& local_device,
                         const std::string& feature,
                         ConnectionMedium connection_medium,
                         ConnectionPriority connection_priority,
                         bool is_listener) {
    FakeConnectionDelegate fake_connection_delegate;
    FakeSecureChannelStructuredMetricsLogger
        fake_secure_channel_structured_metrics_logger;

    if (is_listener) {
      secure_channel_remote_->ListenForConnectionFromDevice(
          device_to_connect, local_device, feature, connection_medium,
          connection_priority, fake_connection_delegate.GenerateRemote());
    } else {
      secure_channel_remote_->InitiateConnectionToDevice(
          device_to_connect, local_device, feature, connection_medium,
          connection_priority, fake_connection_delegate.GenerateRemote(),
          fake_secure_channel_structured_metrics_logger.GenerateRemote());
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
  std::unique_ptr<FakeNearbyConnector> fake_nearby_connector_;

  std::unique_ptr<TestRemoteDeviceCacheFactory>
      test_remote_device_cache_factory_;
  std::unique_ptr<FakeBluetoothHelperFactory> fake_bluetooth_helper_factory_;
  std::unique_ptr<FakeBleSynchronizerFactory> fake_ble_synchronizer_factory_;
  std::unique_ptr<FakeBleScannerFactory> fake_ble_scanner_factory_;
  std::unique_ptr<FakeSecureChannelDisconnectorFactory>
      fake_secure_channel_disconnector_factory_;
  std::unique_ptr<FakeBleConnectionManagerFactory>
      fake_ble_connection_manager_factory_;
  std::unique_ptr<FakeNearbyConnectionManagerFactory>
      fake_nearby_connection_manager_factory_;
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
};

TEST_F(SecureChannelServiceTest, ListenForConnection_MissingPublicKey) {
  FinishInitialization();

  multidevice::RemoteDevice device_to_connect = test_devices()[0];
  device_to_connect.public_key.clear();

  CallListenForConnectionFromDeviceAndVerifyRejection(
      device_to_connect, test_devices()[1], "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::REMOTE_DEVICE_INVALID_PUBLIC_KEY);
}

TEST_F(SecureChannelServiceTest, InitiateConnection_MissingPublicKey) {
  FinishInitialization();

  multidevice::RemoteDevice device_to_connect = test_devices()[0];
  device_to_connect.public_key.clear();

  CallInitiateConnectionToDeviceAndVerifyRejection(
      device_to_connect, test_devices()[1], "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::REMOTE_DEVICE_INVALID_PUBLIC_KEY);
}

TEST_F(SecureChannelServiceTest, ListenForConnection_MissingPsk) {
  FinishInitialization();

  multidevice::RemoteDevice device_to_connect = test_devices()[0];
  device_to_connect.persistent_symmetric_key.clear();

  CallListenForConnectionFromDeviceAndVerifyRejection(
      device_to_connect, test_devices()[1], "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::REMOTE_DEVICE_INVALID_PSK);
}

TEST_F(SecureChannelServiceTest, InitiateConnection_MissingPsk) {
  FinishInitialization();

  multidevice::RemoteDevice device_to_connect = test_devices()[0];
  device_to_connect.persistent_symmetric_key.clear();

  CallInitiateConnectionToDeviceAndVerifyRejection(
      device_to_connect, test_devices()[1], "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::REMOTE_DEVICE_INVALID_PSK);
}

TEST_F(SecureChannelServiceTest,
       ListenForConnection_MissingLocalDevicePublicKey) {
  FinishInitialization();

  multidevice::RemoteDevice local_device = test_devices()[1];
  local_device.public_key.clear();

  CallListenForConnectionFromDeviceAndVerifyRejection(
      test_devices()[0], local_device, "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::LOCAL_DEVICE_INVALID_PUBLIC_KEY);
}

TEST_F(SecureChannelServiceTest,
       InitiateConnection_MissingLocalDevicePublicKey) {
  FinishInitialization();

  multidevice::RemoteDevice local_device = test_devices()[1];
  local_device.public_key.clear();

  CallInitiateConnectionToDeviceAndVerifyRejection(
      test_devices()[0], local_device, "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::LOCAL_DEVICE_INVALID_PUBLIC_KEY);
}

TEST_F(SecureChannelServiceTest, ListenForConnection_MissingLocalDevicePsk) {
  FinishInitialization();

  multidevice::RemoteDevice local_device = test_devices()[1];
  local_device.persistent_symmetric_key.clear();

  CallListenForConnectionFromDeviceAndVerifyRejection(
      test_devices()[0], local_device, "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::LOCAL_DEVICE_INVALID_PSK);
}

TEST_F(SecureChannelServiceTest, InitiateConnection_MissingLocalDevicePsk) {
  FinishInitialization();

  multidevice::RemoteDevice local_device = test_devices()[1];
  local_device.persistent_symmetric_key.clear();

  CallInitiateConnectionToDeviceAndVerifyRejection(
      test_devices()[0], local_device, "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::LOCAL_DEVICE_INVALID_PSK);
}

TEST_F(SecureChannelServiceTest,
       ListenForConnection_BluetoothAdapterNotPresent) {
  FinishInitialization();

  set_is_adapter_present(false);

  CallListenForConnectionFromDeviceAndVerifyRejection(
      test_devices()[0], test_devices()[1], "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::ADAPTER_NOT_PRESENT);
}

TEST_F(SecureChannelServiceTest,
       InitiateConnection_BluetoothAdapterNotPresent) {
  FinishInitialization();

  set_is_adapter_present(false);

  CallInitiateConnectionToDeviceAndVerifyRejection(
      test_devices()[0], test_devices()[1], "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::ADAPTER_NOT_PRESENT);
}

TEST_F(SecureChannelServiceTest, ListenForConnection_BluetoothAdapterDisabled) {
  FinishInitialization();

  set_is_adapter_powered(false);

  CallListenForConnectionFromDeviceAndVerifyRejection(
      test_devices()[0], test_devices()[1], "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::ADAPTER_DISABLED);
}

TEST_F(SecureChannelServiceTest, InitiateConnection_BluetoothAdapterDisabled) {
  FinishInitialization();

  set_is_adapter_powered(false);

  CallInitiateConnectionToDeviceAndVerifyRejection(
      test_devices()[0], test_devices()[1], "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::ADAPTER_DISABLED);
}

TEST_F(SecureChannelServiceTest,
       InitiateConnection_Nearby_RemoteDeviceMissingBluetoothAddress) {
  FinishInitialization();

  multidevice::RemoteDevice device_to_connect = test_devices()[0];
  device_to_connect.bluetooth_public_address.clear();

  CallInitiateConnectionToDeviceAndVerifyRejection(
      device_to_connect, test_devices()[1], "feature",
      ConnectionMedium::kNearbyConnections, ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::
          REMOTE_DEVICE_INVALID_BLUETOOTH_ADDRESS);
}

TEST_F(SecureChannelServiceTest,
       InitiateConnection_Nearby_MissingNearbyConnector) {
  FinishInitialization(/*set_nearby_connector=*/false);

  CallInitiateConnectionToDeviceAndVerifyRejection(
      test_devices()[0], test_devices()[1], "feature",
      ConnectionMedium::kNearbyConnections, ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::MISSING_NEARBY_CONNECTOR);
}

TEST_F(SecureChannelServiceTest, ListenForConnection_Nearby) {
  FinishInitialization();

  CallListenForConnectionFromDeviceAndVerifyRejection(
      test_devices()[0], test_devices()[1], "feature",
      ConnectionMedium::kNearbyConnections, ConnectionPriority::kLow,
      mojom::ConnectionAttemptFailureReason::UNSUPPORTED_ROLE_FOR_MEDIUM);
}

TEST_F(SecureChannelServiceTest, CallsQueuedBeforeInitializationComplete) {
  CallInitiateConnectionToDeviceAndVerifyInitializationNotComplete(
      test_devices()[4], test_devices()[5], "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow);
  CallListenForConnectionFromDeviceAndVerifyInitializationNotComplete(
      test_devices()[4], test_devices()[5], "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow);
  FinishInitialization();
}

TEST_F(SecureChannelServiceTest, ListenForConnection_OneDevice) {
  FinishInitialization();

  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow);
  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId(),
                               ConnectionMedium::kBluetoothLowEnergy);
  SimulateConnectionStartingDisconnecting(
      test_devices()[0].GetDeviceId(), ConnectionMedium::kBluetoothLowEnergy);
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId(),
                                         ConnectionMedium::kBluetoothLowEnergy);
}

TEST_F(SecureChannelServiceTest, InitiateConnection_OneDevice) {
  FinishInitialization();

  CallInitiateConnectionToDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow);
  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId(),
                               ConnectionMedium::kBluetoothLowEnergy);
  SimulateConnectionStartingDisconnecting(
      test_devices()[0].GetDeviceId(), ConnectionMedium::kBluetoothLowEnergy);
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId(),
                                         ConnectionMedium::kBluetoothLowEnergy);
}

TEST_F(SecureChannelServiceTest, InitiateConnection_OneDevice_Nearby) {
  FinishInitialization();

  CallInitiateConnectionToDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature",
      ConnectionMedium::kNearbyConnections, ConnectionPriority::kLow);
  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId(),
                               ConnectionMedium::kNearbyConnections);
  SimulateConnectionStartingDisconnecting(test_devices()[0].GetDeviceId(),
                                          ConnectionMedium::kNearbyConnections);
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId(),
                                         ConnectionMedium::kNearbyConnections);
}

TEST_F(SecureChannelServiceTest,
       ListenForConnection_OneDevice_RequestSpecificLocalDevice) {
  FinishInitialization();

  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow);
  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId(),
                               ConnectionMedium::kBluetoothLowEnergy);
  SimulateConnectionStartingDisconnecting(
      test_devices()[0].GetDeviceId(), ConnectionMedium::kBluetoothLowEnergy);
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId(),
                                         ConnectionMedium::kBluetoothLowEnergy);
}

TEST_F(SecureChannelServiceTest,
       InitiateConnection_OneDevice_RequestSpecificLocalDevice) {
  FinishInitialization();

  CallInitiateConnectionToDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow);
  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId(),
                               ConnectionMedium::kBluetoothLowEnergy);
  SimulateConnectionStartingDisconnecting(
      test_devices()[0].GetDeviceId(), ConnectionMedium::kBluetoothLowEnergy);
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId(),
                                         ConnectionMedium::kBluetoothLowEnergy);
}

TEST_F(SecureChannelServiceTest, OneDevice_TwoConnectionRequests) {
  FinishInitialization();

  // Two pending connection requests for the same device.
  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature1",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow);
  CallInitiateConnectionToDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature2",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kMedium);

  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId(),
                               ConnectionMedium::kBluetoothLowEnergy);
  SimulateConnectionStartingDisconnecting(
      test_devices()[0].GetDeviceId(), ConnectionMedium::kBluetoothLowEnergy);
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId(),
                                         ConnectionMedium::kBluetoothLowEnergy);
}

TEST_F(SecureChannelServiceTest,
       OneDevice_TwoConnectionRequests_OneAfterConnection) {
  FinishInitialization();

  // First request is successful.
  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature1",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow);
  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId(),
                               ConnectionMedium::kBluetoothLowEnergy);

  // Second request is added to the existing channel.
  CallInitiateConnectionToDeviceAndVerifyActiveConnection(
      test_devices()[0], test_devices()[1], "feature2",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kMedium);

  SimulateConnectionStartingDisconnecting(
      test_devices()[0].GetDeviceId(), ConnectionMedium::kBluetoothLowEnergy);
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId(),
                                         ConnectionMedium::kBluetoothLowEnergy);
}

TEST_F(SecureChannelServiceTest,
       OneDevice_TwoConnectionRequests_OneWhileDisconnecting) {
  FinishInitialization();

  // First request is successful.
  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature1",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow);
  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId(),
                               ConnectionMedium::kBluetoothLowEnergy);

  // Connection starts disconnecting.
  SimulateConnectionStartingDisconnecting(
      test_devices()[0].GetDeviceId(), ConnectionMedium::kBluetoothLowEnergy);

  // Second request is added before disconnecting is complete.
  CallInitiateConnectionToDeviceAndVerifyStillDisconnecting(
      test_devices()[0], test_devices()[1], "feature2",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kMedium);

  // Complete the disconnection; this should cause the second request to be
  // delivered to PendingConnectionManager.
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId(),
                                         ConnectionMedium::kBluetoothLowEnergy);

  // The second attempt succeeds.
  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId(),
                               ConnectionMedium::kBluetoothLowEnergy);

  SimulateConnectionStartingDisconnecting(
      test_devices()[0].GetDeviceId(), ConnectionMedium::kBluetoothLowEnergy);
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId(),
                                         ConnectionMedium::kBluetoothLowEnergy);
}

TEST_F(SecureChannelServiceTest,
       OneDevice_TwoConnectionRequests_OneWhileDisconnecting_Canceled) {
  FinishInitialization();

  // First request is successful.
  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature1",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow);
  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId(),
                               ConnectionMedium::kBluetoothLowEnergy);

  // Connection starts disconnecting.
  SimulateConnectionStartingDisconnecting(
      test_devices()[0].GetDeviceId(), ConnectionMedium::kBluetoothLowEnergy);

  // Second request is added before disconnecting is complete, but the request
  // is canceled before the disconnection completes.
  auto id = CallInitiateConnectionToDeviceAndVerifyStillDisconnecting(
      test_devices()[0], test_devices()[1], "feature2",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kMedium);
  CancelPendingRequest(id);

  // Complete the disconnection; even though the request was canceled, it should
  // still have been added to PendingConnectionManager.
  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId(),
                                         ConnectionMedium::kBluetoothLowEnergy);
}

TEST_F(SecureChannelServiceTest, ThreeDevices) {
  FinishInitialization();

  // Two requests for each device.
  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature1",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow);
  CallInitiateConnectionToDeviceAndVerifyPendingConnection(
      test_devices()[0], test_devices()[1], "feature2",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kMedium);
  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[2], test_devices()[1], "feature3",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kHigh);
  CallInitiateConnectionToDeviceAndVerifyPendingConnection(
      test_devices()[2], test_devices()[1], "feature4",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kLow);
  CallListenForConnectionFromDeviceAndVerifyPendingConnection(
      test_devices()[3], test_devices()[1], "feature5",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kMedium);
  CallInitiateConnectionToDeviceAndVerifyPendingConnection(
      test_devices()[3], test_devices()[1], "feature6",
      ConnectionMedium::kBluetoothLowEnergy, ConnectionPriority::kHigh);

  SimulateSuccessfulConnection(test_devices()[0].GetDeviceId(),
                               ConnectionMedium::kBluetoothLowEnergy);
  SimulateSuccessfulConnection(test_devices()[2].GetDeviceId(),
                               ConnectionMedium::kBluetoothLowEnergy);
  SimulateSuccessfulConnection(test_devices()[3].GetDeviceId(),
                               ConnectionMedium::kBluetoothLowEnergy);

  SimulateConnectionStartingDisconnecting(
      test_devices()[0].GetDeviceId(), ConnectionMedium::kBluetoothLowEnergy);
  SimulateConnectionStartingDisconnecting(
      test_devices()[2].GetDeviceId(), ConnectionMedium::kBluetoothLowEnergy);
  SimulateConnectionStartingDisconnecting(
      test_devices()[3].GetDeviceId(), ConnectionMedium::kBluetoothLowEnergy);

  SimulateConnectionBecomingDisconnected(test_devices()[0].GetDeviceId(),
                                         ConnectionMedium::kBluetoothLowEnergy);
  SimulateConnectionBecomingDisconnected(test_devices()[2].GetDeviceId(),
                                         ConnectionMedium::kBluetoothLowEnergy);
  SimulateConnectionBecomingDisconnected(test_devices()[3].GetDeviceId(),
                                         ConnectionMedium::kBluetoothLowEnergy);
}

}  // namespace ash::secure_channel
