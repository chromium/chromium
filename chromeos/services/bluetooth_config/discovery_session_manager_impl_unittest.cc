// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/discovery_session_manager_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/token_type.h"
#include "chromeos/services/bluetooth_config/device_conversion_util.h"
#include "chromeos/services/bluetooth_config/device_pairing_handler_impl.h"
#include "chromeos/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/fake_bluetooth_discovery_delegate.h"
#include "chromeos/services/bluetooth_config/fake_device_pairing_delegate.h"
#include "chromeos/services/bluetooth_config/fake_device_pairing_handler.h"
#include "chromeos/services/bluetooth_config/fake_discovered_devices_provider.h"
#include "chromeos/services/bluetooth_config/fake_discovery_session_status_observer.h"
#include "device/bluetooth/floss/floss_features.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace bluetooth_config {
namespace {

using NiceMockDevice =
    std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>;

using StartScanCallback = base::OnceCallback<void(
    /*is_error=*/bool,
    device::UMABluetoothDiscoverySessionOutcome)>;
using StopScanCallback =
    device::BluetoothAdapter::DiscoverySessionResultCallback;

using HandlerId = base::TokenType<class HandlerIdTag>;

const uint32_t kTestBluetoothClass = 1337u;
const char kTestBluetoothName[] = "testName";

}  // namespace

class DiscoverySessionManagerImplTest : public testing::Test {
 protected:
  DiscoverySessionManagerImplTest() = default;
  DiscoverySessionManagerImplTest(const DiscoverySessionManagerImplTest&) =
      delete;
  DiscoverySessionManagerImplTest& operator=(
      const DiscoverySessionManagerImplTest&) = delete;
  ~DiscoverySessionManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    DevicePairingHandlerImpl::Factory::SetFactoryForTesting(
        &fake_device_pairing_handler_factory_);

    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_adapter_, StartScanWithFilter_(testing::_, testing::_))
        .WillByDefault(testing::Invoke(
            [this](const device::BluetoothDiscoveryFilter* filter,
                   StartScanCallback& callback) {
              EXPECT_FALSE(start_scan_callback_);
              start_scan_callback_ = std::move(callback);

              if (should_synchronously_invoke_start_scan_callback_) {
                InvokePendingStartScanCallback(/*success=*/true);
              }
            }));
    ON_CALL(*mock_adapter_, StopScan(testing::_))
        .WillByDefault(testing::Invoke([this](StopScanCallback callback) {
          EXPECT_FALSE(stop_scan_callback_);
          stop_scan_callback_ = std::move(callback);
        }));

    discovery_session_manager_ = std::make_unique<DiscoverySessionManagerImpl>(
        &fake_adapter_state_controller_, mock_adapter_,
        &fake_discovered_devices_provider_, /*fast_pair_delegate=*/nullptr);
  }

  void TearDown() override {
    DevicePairingHandlerImpl::Factory::SetFactoryForTesting(nullptr);
  }

  std::unique_ptr<FakeBluetoothDiscoveryDelegate> StartDiscovery() {
    auto delegate = std::make_unique<FakeBluetoothDiscoveryDelegate>();
    discovery_session_manager_->StartDiscovery(
        delegate->GeneratePendingRemote());

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(delegate->IsMojoPipeConnected());

    return delegate;
  }

  bool HasPendingStartScanCallback() const {
    return !start_scan_callback_.is_null();
  }

  void InvokePendingStartScanCallback(bool success) {
    std::move(start_scan_callback_)
        .Run(!success,
             success ? device::UMABluetoothDiscoverySessionOutcome::SUCCESS
                     : device::UMABluetoothDiscoverySessionOutcome::FAILED);
    base::RunLoop().RunUntilIdle();
  }

  bool HasPendingStopScanCallback() const {
    return !stop_scan_callback_.is_null();
  }

  void InvokePendingStopScanCallback(bool success) {
    std::move(stop_scan_callback_)
        .Run(!success,
             success ? device::UMABluetoothDiscoverySessionOutcome::SUCCESS
                     : device::UMABluetoothDiscoverySessionOutcome::FAILED);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateDiscoverySessionStopping() {
    DiscoverySessionManagerImpl* impl =
        static_cast<DiscoverySessionManagerImpl*>(
            discovery_session_manager_.get());
    impl->AdapterDiscoveringChanged(mock_adapter_.get(), /*discovering=*/false);
    base::RunLoop().RunUntilIdle();
  }

  void SetBluetoothSystemState(mojom::BluetoothSystemState system_state) {
    fake_adapter_state_controller_.SetSystemState(system_state);
  }

  void AddDevice(std::string* id_out) {
    // We use the number of devices created in this test as the address.
    std::string address = base::NumberToString(num_devices_created_);
    ++num_devices_created_;

    // Mock devices have their ID set to "${address}-Identifier".
    *id_out = base::StrCat({address, "-Identifier"});

    auto mock_device =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            mock_adapter_.get(), kTestBluetoothClass, kTestBluetoothName,
            address, /*paired=*/false,
            /*connected=*/false);
    ON_CALL(*mock_device, Connect_(testing::_, testing::_))
        .WillByDefault(testing::Invoke(
            [this](device::BluetoothDevice::PairingDelegate* pairing_delegate,
                   device::BluetoothDevice::ConnectCallback& callback) {
              connect_callback_ = std::move(callback);
            }));
    mock_devices_.push_back(std::move(mock_device));

    // Add the device to the discovered devices provider's discovered devices.
    std::vector<mojom::BluetoothDevicePropertiesPtr> discovered_devices;
    for (auto& device : mock_devices_) {
      discovered_devices.push_back(GenerateBluetoothDeviceMojoProperties(
          device.get(), /*fast_pair_delegate=*/nullptr));
    }
    fake_discovered_devices_provider_.SetDiscoveredDevices(
        std::move(discovered_devices));
    fake_device_pairing_handler_factory_.UpdateActiveHandlerDeviceLists();
    discovery_session_manager_->FlushForTesting();
  }

  bool HasPendingConnectCallback() const {
    return !connect_callback_.is_null();
  }

  void InvokePendingConnectCallback() {
    std::move(connect_callback_).Run(absl::nullopt);
    base::RunLoop().RunUntilIdle();
  }

  void SetShouldSynchronouslyInvokeStartScanCallback(bool should) {
    should_synchronously_invoke_start_scan_callback_ = should;
  }

  std::unique_ptr<FakeDiscoverySessionStatusObserver> Observe() {
    auto observer = std::make_unique<FakeDiscoverySessionStatusObserver>();
    discovery_session_manager_->ObserveDiscoverySessionStatusChanges(
        observer->GeneratePendingRemote());
    discovery_session_manager_->FlushForTesting();
    return observer;
  }

 private:
  class FakeDevicePairingHandlerFactory
      : public DevicePairingHandlerImpl::Factory {
   public:
    explicit FakeDevicePairingHandlerFactory(
        const DiscoverySessionManagerImplTest& test)
        : test_(test) {}

    ~FakeDevicePairingHandlerFactory() override = default;

    // Sets the device list of each handler to |test_.mock_devices_|.
    void UpdateActiveHandlerDeviceLists() {
      for (auto entry : id_to_fake_handler_map_) {
        std::vector<device::BluetoothDevice*> unpaired_devices;
        for (auto& device : test_.mock_devices_)
          unpaired_devices.push_back(device.get());
        entry.second->SetDeviceList(std::move(unpaired_devices));
      }
    }

   private:
    std::unique_ptr<DevicePairingHandler> CreateInstance(
        mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
        AdapterStateController* adapter_state_controller,
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
        FastPairDelegate* fast_pair_delegate,
        base::OnceClosure finished_pairing_callback) override {
      EXPECT_TRUE(pending_receiver);
      EXPECT_TRUE(adapter_state_controller);
      EXPECT_TRUE(bluetooth_adapter);
      EXPECT_TRUE(finished_pairing_callback);

      HandlerId id;
      auto fake_device_pairing_handler = std::make_unique<
          FakeDevicePairingHandler>(
          std::move(pending_receiver), adapter_state_controller,
          base::BindOnce(
              &FakeDevicePairingHandlerFactory::OnFakeHandlerFinishedPairing,
              base::Unretained(this), id,
              std::move(finished_pairing_callback)));
      id_to_fake_handler_map_[id] = fake_device_pairing_handler.get();

      // Initialize the handler with mock_devices_. This will redundantly set
      // the device lists for all handlers.
      UpdateActiveHandlerDeviceLists();
      return fake_device_pairing_handler;
    }

    void OnFakeHandlerFinishedPairing(
        const HandlerId& id,
        base::OnceClosure finished_pairing_callback) {
      id_to_fake_handler_map_.erase(id);
      std::move(finished_pairing_callback).Run();
    }

    std::map<HandlerId, FakeDevicePairingHandler*> id_to_fake_handler_map_;
    const DiscoverySessionManagerImplTest& test_;
  };

  base::test::TaskEnvironment task_environment_;

  std::vector<NiceMockDevice> mock_devices_;
  size_t num_devices_created_ = 0u;

  bool should_synchronously_invoke_start_scan_callback_ = false;
  StartScanCallback start_scan_callback_;
  StopScanCallback stop_scan_callback_;
  device::BluetoothDevice::ConnectCallback connect_callback_;

  FakeAdapterStateController fake_adapter_state_controller_;
  FakeDiscoveredDevicesProvider fake_discovered_devices_provider_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  FakeDevicePairingHandlerFactory fake_device_pairing_handler_factory_{*this};

  std::unique_ptr<DiscoverySessionManager> discovery_session_manager_;
};

TEST_F(DiscoverySessionManagerImplTest, StartDiscoveryThenDisconnectToStop) {
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate = StartDiscovery();

  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->num_start_callbacks());
  EXPECT_TRUE(delegate->discovered_devices_list().empty());

  // Add an unpaired device and verify that the delegate was notified.
  std::string device_id1;
  AddDevice(&device_id1);
  EXPECT_EQ(1u, delegate->discovered_devices_list().size());
  EXPECT_EQ(device_id1, delegate->discovered_devices_list()[0]->id);

  // Add another unpaired device and verify that the delegate was notified.
  std::string device_id2;
  AddDevice(&device_id2);
  EXPECT_EQ(2u, delegate->discovered_devices_list().size());
  EXPECT_EQ(device_id2, delegate->discovered_devices_list()[1]->id);

  // Disconnect the Mojo pipe; this should trigger a StopScan() call.
  delegate->DisconnectMojoPipe();
  EXPECT_FALSE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->num_start_callbacks());

  // Invoke the StopScan() callback. Since the delegate was already
  // disconnected, it should not have received a callback.
  InvokePendingStopScanCallback(/*success=*/true);
  EXPECT_EQ(0u, delegate->num_stop_callbacks());

  // Add another unpaired device; the delegate should not be notified.
  std::string device_id3;
  AddDevice(&device_id3);
  EXPECT_EQ(2u, delegate->discovered_devices_list().size());
  EXPECT_EQ(device_id1, delegate->discovered_devices_list()[0]->id);
}

TEST_F(DiscoverySessionManagerImplTest, FailToStart) {
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate = StartDiscovery();

  // Fail to start scanning.
  InvokePendingStartScanCallback(/*success=*/false);
  EXPECT_EQ(0u, delegate->num_start_callbacks());

  // We expect that another request was made to retry scanning; succeed this
  // time.
  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_EQ(1u, delegate->num_start_callbacks());
}

TEST_F(DiscoverySessionManagerImplTest, DisconnectBeforeFailureToStart) {
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate = StartDiscovery();

  // Disconnect the Mojo pipe, before the discovery session starts.
  delegate->DisconnectMojoPipe();
  EXPECT_FALSE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(0u, delegate->num_start_callbacks());

  // Invoke the pending callback to start the scan. Since the delegate was
  // already disconnected, it should not have receivd a callback.
  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_EQ(0u, delegate->num_start_callbacks());

  // Since there is no longer a client, we should have attempted to stop the
  // discovery session. Invoke the StopScan() callback; since the delegate was
  // already disconnected, it should not have received a callback.
  InvokePendingStopScanCallback(/*success=*/true);
  EXPECT_EQ(0u, delegate->num_stop_callbacks());
}

TEST_F(DiscoverySessionManagerImplTest, UnexpectedlyStoppedDiscovering) {
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate = StartDiscovery();

  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->num_start_callbacks());

  // Simulate the discovery session stopping unexpectedly. The delegate should
  // become disconnected.
  SimulateDiscoverySessionStopping();
  EXPECT_FALSE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->num_stop_callbacks());
}

TEST_F(DiscoverySessionManagerImplTest, BluetoothTurnsOff) {
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate = StartDiscovery();

  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->num_start_callbacks());

  // Start disabling Bluetooth; the delegate should become disconnected.
  SetBluetoothSystemState(mojom::BluetoothSystemState::kDisabling);
  EXPECT_FALSE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->num_stop_callbacks());
}

TEST_F(DiscoverySessionManagerImplTest, MultipleClients) {
  // Add the first client and start discovery.
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate1 = StartDiscovery();
  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_TRUE(delegate1->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate1->num_start_callbacks());
  EXPECT_TRUE(delegate1->discovered_devices_list().empty());

  // Add an unpaired device and verify that the first client was notified.
  std::string device_id1;
  AddDevice(&device_id1);
  EXPECT_EQ(1u, delegate1->discovered_devices_list().size());
  EXPECT_EQ(device_id1, delegate1->discovered_devices_list()[0]->id);

  // Add a second client; it should reuse the existing discovery session, and no
  // new pending request should have been created. It should immediately be
  // notified of the current discovered devices list.
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate2 = StartDiscovery();
  EXPECT_TRUE(delegate2->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate2->num_start_callbacks());
  EXPECT_FALSE(HasPendingStartScanCallback());
  EXPECT_EQ(1u, delegate2->discovered_devices_list().size());
  EXPECT_EQ(device_id1, delegate2->discovered_devices_list()[0]->id);

  // Disconnect the first client; since the second client is still active, there
  // should be no pending StopScan() call.
  delegate1->DisconnectMojoPipe();
  EXPECT_FALSE(HasPendingStopScanCallback());

  // Add another unpaired device; the second client should be notified but the
  // first client should not.
  std::string device_id2;
  AddDevice(&device_id2);
  EXPECT_EQ(1u, delegate1->discovered_devices_list().size());
  EXPECT_EQ(device_id1, delegate1->discovered_devices_list()[0]->id);
  EXPECT_EQ(2u, delegate2->discovered_devices_list().size());
  EXPECT_EQ(device_id2, delegate2->discovered_devices_list()[1]->id);

  // Disconnect the second client; now that there are no remaining clients,
  // StopScan() should have been called.
  delegate2->DisconnectMojoPipe();
  InvokePendingStopScanCallback(/*success=*/true);
}

TEST_F(DiscoverySessionManagerImplTest, DiscoverDeviceBeforeStart) {
  // Add an unpaired device.
  std::string device_id;
  AddDevice(&device_id);

  // Add a client and start discovery. The client should be notified of the
  // current discovered devices list once discovery has started.
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate = StartDiscovery();
  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->discovered_devices_list().size());
  EXPECT_EQ(device_id, delegate->discovered_devices_list()[0]->id);

  delegate->DisconnectMojoPipe();
  InvokePendingStopScanCallback(/*success=*/true);
}

TEST_F(DiscoverySessionManagerImplTest, MultipleClientsAttemptPairing) {
  std::unique_ptr<FakeDiscoverySessionStatusObserver> observer = Observe();

  // Initially, observer would see the default state, which is 0 sessions.
  EXPECT_FALSE(observer->has_at_least_one_discovery_session());
  EXPECT_EQ(0, observer->num_discovery_session_changed_calls());

  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate1 = StartDiscovery();
  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_TRUE(delegate1->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate1->num_start_callbacks());
  EXPECT_TRUE(delegate1->pairing_handler().is_connected());
  // Going from 0 to 1 discovery sessions should notify observers.
  EXPECT_TRUE(observer->has_at_least_one_discovery_session());
  EXPECT_EQ(1, observer->num_discovery_session_changed_calls());

  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate2 = StartDiscovery();
  EXPECT_TRUE(delegate2->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate2->num_start_callbacks());
  EXPECT_TRUE(delegate2->pairing_handler().is_connected());
  // Going from 1 to 1+ discovery sessions should not notify observers.
  EXPECT_TRUE(observer->has_at_least_one_discovery_session());
  EXPECT_EQ(1, observer->num_discovery_session_changed_calls());

  // Simulate first client attempting to pair with an unknown device_id.
  absl::optional<mojom::PairingResult> result;
  auto pairing_delegate1 = std::make_unique<FakeDevicePairingDelegate>();
  delegate1->pairing_handler()->PairDevice(
      "device_id", pairing_delegate1->GeneratePendingRemote(),
      base::BindLambdaForTesting(
          [&result](mojom::PairingResult pairing_result) {
            result = pairing_result;
          }));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(HasPendingConnectCallback());
  EXPECT_EQ(result, mojom::PairingResult::kNonAuthFailure);

  // First client's pairing handler should still be alive because we can retry
  // pairing.
  EXPECT_TRUE(delegate1->IsMojoPipeConnected());
  EXPECT_TRUE(delegate1->pairing_handler().is_connected());
  EXPECT_TRUE(pairing_delegate1->IsMojoPipeConnected());

  // First client pairs with a known device_id.
  std::string device_id1;
  AddDevice(&device_id1);
  auto pairing_delegate2 = std::make_unique<FakeDevicePairingDelegate>();
  delegate1->pairing_handler()->PairDevice(
      device_id1, pairing_delegate2->GeneratePendingRemote(),
      base::BindLambdaForTesting(
          [&result](mojom::PairingResult pairing_result) {
            result = pairing_result;
          }));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasPendingConnectCallback());
  InvokePendingConnectCallback();
  EXPECT_EQ(result, mojom::PairingResult::kSuccess);

  // First client's Mojo pipes should be disconnected.
  EXPECT_FALSE(delegate1->IsMojoPipeConnected());
  EXPECT_FALSE(delegate1->pairing_handler().is_connected());
  EXPECT_FALSE(pairing_delegate2->IsMojoPipeConnected());
  // Going from 1+ to 1 discovery sessions should not notify observers.
  EXPECT_TRUE(observer->has_at_least_one_discovery_session());
  EXPECT_EQ(1, observer->num_discovery_session_changed_calls());

  // Simulate second client pairing with a known device_id.
  std::string device_id2;
  AddDevice(&device_id2);
  auto pairing_delegate3 = std::make_unique<FakeDevicePairingDelegate>();
  delegate2->pairing_handler()->PairDevice(
      device_id2, pairing_delegate3->GeneratePendingRemote(),
      base::BindLambdaForTesting(
          [&result](mojom::PairingResult pairing_result) {
            result = pairing_result;
          }));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasPendingConnectCallback());
  InvokePendingConnectCallback();
  EXPECT_EQ(result, mojom::PairingResult::kSuccess);

  // Second client's Mojo pipes should be disconnected and discovery stopped.
  EXPECT_FALSE(delegate2->IsMojoPipeConnected());
  EXPECT_FALSE(delegate2->pairing_handler().is_connected());
  EXPECT_FALSE(pairing_delegate3->IsMojoPipeConnected());
  InvokePendingStopScanCallback(/*success=*/true);
  // Going from 1 to 0 discovery sessions should notify observers.
  EXPECT_FALSE(observer->has_at_least_one_discovery_session());
  EXPECT_EQ(2, observer->num_discovery_session_changed_calls());
}

TEST_F(DiscoverySessionManagerImplTest, StartDiscoverySynchronous) {
  // Simulate adapter finishing starting scanning immediately. This should cause
  // |delegate.OnBluetoothDiscoveryStarted()| still to only be called once.
  SetShouldSynchronouslyInvokeStartScanCallback(true);
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate = StartDiscovery();
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->num_start_callbacks());
  EXPECT_TRUE(delegate->discovered_devices_list().empty());

  // Disconnect the Mojo pipe; this should trigger a StopScan() call.
  delegate->DisconnectMojoPipe();
  EXPECT_FALSE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->num_start_callbacks());

  // Invoke the StopScan() callback. Since the delegate was already
  // disconnected, it should not have received a callback.
  InvokePendingStopScanCallback(/*success=*/true);
  EXPECT_EQ(0u, delegate->num_stop_callbacks());
}

TEST_F(DiscoverySessionManagerImplTest, DisconnectToStopObserving) {
  std::unique_ptr<FakeDiscoverySessionStatusObserver> observer = Observe();

  // Initially, observer would see the default state, which is 0 sessions.
  EXPECT_FALSE(observer->has_at_least_one_discovery_session());
  EXPECT_EQ(0, observer->num_discovery_session_changed_calls());

  // Disconnect the Mojo pipe; this should stop observing.
  observer->DisconnectMojoPipe();

  // Add the first client and start discovery. The observer should not be
  // notified since it is disconnected.
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate1 = StartDiscovery();
  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_FALSE(observer->has_at_least_one_discovery_session());
  EXPECT_EQ(0, observer->num_discovery_session_changed_calls());
}

TEST_F(DiscoverySessionManagerImplTest, AdapterDiscoveringStopsDuringPairing) {
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate = StartDiscovery();

  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->num_start_callbacks());
  EXPECT_TRUE(delegate->pairing_handler().is_connected());

  // Simulate client pairing with a device.
  std::string device_id;
  AddDevice(&device_id);
  absl::optional<mojom::PairingResult> result;
  auto pairing_delegate = std::make_unique<FakeDevicePairingDelegate>();
  delegate->pairing_handler()->PairDevice(
      device_id, pairing_delegate->GeneratePendingRemote(),
      base::BindLambdaForTesting(
          [&result](mojom::PairingResult pairing_result) {
            result = pairing_result;
          }));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasPendingConnectCallback());

  // Simulate the discovery session stopping unexpectedly before pairing
  // completes. Discovery should stop and the discovery delegate, pairing
  // handler, and pairing delegate disconnected.
  SimulateDiscoverySessionStopping();
  EXPECT_FALSE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->num_stop_callbacks());
  EXPECT_FALSE(delegate->pairing_handler().is_connected());
  EXPECT_FALSE(pairing_delegate->IsMojoPipeConnected());
  InvokePendingStopScanCallback(/*success=*/true);
}

TEST_F(DiscoverySessionManagerImplTest,
       AdapterDiscoveringStopsDuringPairing_Floss) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(floss::features::kFlossEnabled);
  EXPECT_TRUE(base::FeatureList::IsEnabled(floss::features::kFlossEnabled));

  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate = StartDiscovery();

  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->num_start_callbacks());
  EXPECT_TRUE(delegate->pairing_handler().is_connected());

  // Simulate client pairing with a device.
  std::string device_id;
  AddDevice(&device_id);
  absl::optional<mojom::PairingResult> result;
  auto pairing_delegate = std::make_unique<FakeDevicePairingDelegate>();
  delegate->pairing_handler()->PairDevice(
      device_id, pairing_delegate->GeneratePendingRemote(),
      base::BindLambdaForTesting(
          [&result](mojom::PairingResult pairing_result) {
            result = pairing_result;
          }));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasPendingConnectCallback());

  // Simulate the discovery session stopping before pairing
  // completes. This can happen with Floss enabled, but the device pairing
  // handler should remain alive.
  // TODO(b/222230887): Remove this test when DiscoverySessionManager and
  // DevicePairingHandler lifecycles are decoupled.
  SimulateDiscoverySessionStopping();
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(0u, delegate->num_stop_callbacks());
  EXPECT_TRUE(delegate->pairing_handler().is_connected());
  EXPECT_TRUE(pairing_delegate->IsMojoPipeConnected());

  InvokePendingConnectCallback();
  EXPECT_EQ(result, mojom::PairingResult::kSuccess);

  // The client's Mojo pipes should now be disconnected and discovery stopped.
  EXPECT_FALSE(delegate->IsMojoPipeConnected());
  EXPECT_FALSE(delegate->pairing_handler().is_connected());
  EXPECT_FALSE(pairing_delegate->IsMojoPipeConnected());
  InvokePendingStopScanCallback(/*success=*/true);
}

}  // namespace bluetooth_config
}  // namespace chromeos
