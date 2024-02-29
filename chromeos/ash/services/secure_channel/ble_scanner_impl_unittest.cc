// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/ble_scanner_impl.h"

#include <iterator>
#include <memory>
#include <utility>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/services/secure_channel/connection_role.h"
#include "chromeos/ash/services/secure_channel/fake_ble_scanner.h"
#include "chromeos/ash/services/secure_channel/fake_ble_synchronizer.h"
#include "chromeos/ash/services/secure_channel/fake_bluetooth_helper.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/ble_constants.h"
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "device/bluetooth/floss/floss_features.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_low_energy_scan_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

// Extends device::MockBluetoothDevice, adding the ability to set service data
// to be returned.
class FakeBluetoothDevice : public device::MockBluetoothDevice {
 public:
  FakeBluetoothDevice(const std::string& service_data,
                      device::MockBluetoothAdapter* adapter)
      : device::MockBluetoothDevice(adapter,
                                    0u /* bluetooth_class */,
                                    "name",
                                    "address",
                                    false /* paired */,
                                    false /* connected */) {
    // Convert |service_data| from a std::string to a std::vector<uint8_t>.
    service_data_vector_ = base::ToVector(service_data, [](char character) {
      return static_cast<uint8_t>(character);
    });
  }

  FakeBluetoothDevice(const FakeBluetoothDevice&) = delete;
  FakeBluetoothDevice& operator=(const FakeBluetoothDevice&) = delete;

  const std::vector<uint8_t>* service_data() { return &service_data_vector_; }

 private:
  std::vector<uint8_t> service_data_vector_;
};

std::vector<std::pair<ConnectionMedium, ConnectionRole>>
CreateSingleBleScanResult(bool is_background_advertisement) {
  return std::vector<std::pair<ConnectionMedium, ConnectionRole>>{
      {ConnectionMedium::kBluetoothLowEnergy,
       is_background_advertisement ? ConnectionRole::kListenerRole
                                   : ConnectionRole::kInitiatorRole}};
}

}  // namespace

class SecureChannelBleScannerImplTest : public testing::Test {
 protected:
  class FakeServiceDataProvider : public BleScannerImpl::ServiceDataProvider {
   public:
    FakeServiceDataProvider() = default;
    ~FakeServiceDataProvider() override = default;

    // ServiceDataProvider:
    const std::vector<uint8_t>* ExtractProximityAuthServiceData(
        device::BluetoothDevice* bluetooth_device) override {
      FakeBluetoothDevice* mock_device =
          static_cast<FakeBluetoothDevice*>(bluetooth_device);
      return mock_device->service_data();
    }
  };

  SecureChannelBleScannerImplTest()
      : test_devices_(multidevice::CreateRemoteDeviceRefListForTest(3)) {}

  SecureChannelBleScannerImplTest(const SecureChannelBleScannerImplTest&) =
      delete;
  SecureChannelBleScannerImplTest& operator=(
      const SecureChannelBleScannerImplTest&) = delete;

  ~SecureChannelBleScannerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_delegate_ = std::make_unique<FakeBleScannerObserver>();
    fake_bluetooth_helper_ = std::make_unique<FakeBluetoothHelper>();
    fake_ble_synchronizer_ = std::make_unique<FakeBleSynchronizer>();

    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();

    ble_scanner_ = BleScannerImpl::Factory::Create(fake_bluetooth_helper_.get(),
                                                   fake_ble_synchronizer_.get(),
                                                   mock_adapter_);
    ble_scanner_->AddObserver(fake_delegate_.get());

    auto fake_service_data_provider =
        std::make_unique<FakeServiceDataProvider>();
    fake_service_data_provider_ = fake_service_data_provider.get();

    BleScannerImpl* ble_scanner_derived =
        static_cast<BleScannerImpl*>(ble_scanner_.get());
    ble_scanner_derived->SetServiceDataProviderForTesting(
        std::move(fake_service_data_provider));

    ON_CALL(*mock_adapter_, StartScanWithFilter_(testing::_, testing::_))
        .WillByDefault(testing::Invoke(
            [](const device::BluetoothDiscoveryFilter* discovery_filter,
               device::BluetoothAdapter::DiscoverySessionResultCallback&
                   callback) {
              std::move(callback).Run(
                  /*is_error=*/false,
                  device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
            }));
    ON_CALL(*mock_adapter_, StopScan(testing::_))
        .WillByDefault(testing::Invoke(
            [](device::BluetoothAdapter::DiscoverySessionResultCallback
                   callback) {
              std::move(callback).Run(
                  /*is_error=*/false,
                  device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
            }));
    ON_CALL(*mock_adapter_, StartLowEnergyScanSession(testing::_, testing::_))
        .WillByDefault(Invoke(
            this, &SecureChannelBleScannerImplTest::StartLowEnergyScanSession));
  }

  void TearDown() override {
    ble_scanner_->RemoveObserver(fake_delegate_.get());
  }

  void AddScanRequest(const ConnectionAttemptDetails& scan_filter) {
    EXPECT_FALSE(ble_scanner_->HasScanRequest(scan_filter));
    ble_scanner_->AddScanRequest(scan_filter);
    EXPECT_TRUE(ble_scanner_->HasScanRequest(scan_filter));
  }

  // StartDiscoverySession in the mock adapter mostly for the purpose of
  // creating a DiscoverySession.
  void StartDiscoverySession() {
    mock_adapter_->StartDiscoverySession(
        /*client_name=*/std::string(),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<device::BluetoothDiscoverySession>
                    discovery_session) {
              discovery_session_ = std::move(discovery_session);
              discovery_session_weak_ptr_ = discovery_session_->GetWeakPtr();
            }),
        base::RepeatingClosure());
  }

  void RemoveScanRequest(const ConnectionAttemptDetails& scan_filter) {
    EXPECT_TRUE(ble_scanner_->HasScanRequest(scan_filter));
    ble_scanner_->RemoveScanRequest(scan_filter);
    EXPECT_FALSE(ble_scanner_->HasScanRequest(scan_filter));
  }

  void ProcessScanResultAndVerifyNoDeviceIdentified(
      const std::string& service_data) {
    const std::vector<FakeBleScannerObserver::Result>& results =
        fake_delegate_->handled_scan_results();

    size_t num_results_before_call = results.size();
    SimulateScanResult(service_data);
    EXPECT_EQ(num_results_before_call, results.size());
  }

  // |expected_scan_results| contains the data expected to be provided to
  // scan observers; if null, we default to a single BLE scan result.
  void ProcessScanResultAndVerifyDevice(
      const std::string& service_data,
      multidevice::RemoteDeviceRef expected_remote_device,
      bool is_background_advertisement,
      const std::optional<
          std::vector<std::pair<ConnectionMedium, ConnectionRole>>>&
          expected_scan_results = std::nullopt) {
    std::vector<std::pair<ConnectionMedium, ConnectionRole>>
        new_expected_results =
            expected_scan_results.has_value()
                ? *expected_scan_results
                : CreateSingleBleScanResult(is_background_advertisement);

    const std::vector<FakeBleScannerObserver::Result>& results =
        fake_delegate_->handled_scan_results();

    fake_bluetooth_helper_->SetIdentifiedDevice(
        service_data, expected_remote_device, is_background_advertisement);

    size_t num_results_before_call = results.size();
    std::unique_ptr<FakeBluetoothDevice> fake_bluetooth_device =
        SimulateScanResult(service_data);
    EXPECT_EQ(num_results_before_call + new_expected_results.size(),
              results.size());

    for (size_t i = 0; i < new_expected_results.size(); ++i) {
      const auto& result =
          results[results.size() - new_expected_results.size() + i];
      EXPECT_EQ(expected_remote_device, result.remote_device);
      EXPECT_EQ(fake_bluetooth_device.get(), result.bluetooth_device);
      EXPECT_EQ(new_expected_results[i].first, result.connection_medium);
      EXPECT_EQ(new_expected_results[i].second, result.connection_role);
    }
  }

  void InvokeStartDiscoveryCallback(bool success, size_t command_index) {
    if (!success) {
      fake_ble_synchronizer_->TakeStartDiscoveryErrorCallback(command_index)
          .Run();
      return;
    }

    StartDiscoverySession();
    fake_ble_synchronizer_->TakeStartDiscoveryCallback(command_index)
        .Run(std::move(discovery_session_));
  }

  void InvokeStopDiscoveryCallback(bool success, size_t command_index) {
    if (success) {
      fake_ble_synchronizer_->GetStopDiscoveryCallback(command_index).Run();
    } else {
      fake_ble_synchronizer_->GetStopDiscoveryErrorCallback(command_index)
          .Run();
    }
  }

  std::unique_ptr<device::BluetoothLowEnergyScanSession>
  StartLowEnergyScanSession(
      std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
      base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate) {
    EXPECT_FALSE(scan_session_ptr_);
    EXPECT_TRUE(filter);
    auto scan_session =
        std::make_unique<device::MockBluetoothLowEnergyScanSession>(
            base::BindOnce(
                &SecureChannelBleScannerImplTest::OnScanSessionDestroyed,
                base::Unretained(this)));
    scan_session_ptr_ = scan_session.get();
    le_scan_delegate_ = delegate;
    return scan_session;
  }

  void InvokeStartLEScanSessionCallback(bool success) {
    ASSERT_TRUE(le_scan_delegate_);

    if (success) {
      le_scan_delegate_->OnSessionStarted(scan_session_ptr_,
                                          /*error_code=*/std::nullopt);
    } else {
      le_scan_delegate_->OnSessionStarted(
          scan_session_ptr_,
          device::BluetoothLowEnergyScanSession::ErrorCode::kFailed);
    }
  }

  void OnScanSessionDestroyed() {
    EXPECT_TRUE(scan_session_ptr_);
    scan_session_ptr_ = nullptr;
    le_scan_delegate_ = nullptr;
  }

  void InvokeLEScanSessionInvalidated() {
    ASSERT_TRUE(le_scan_delegate_);

    le_scan_delegate_->OnSessionInvalidated(scan_session_ptr_);
  }

  size_t GetNumBleCommands() {
    return fake_ble_synchronizer_->GetNumCommands();
  }

  bool discovery_session_is_active() {
    if (floss::features::IsFlossEnabled()) {
      return scan_session_ptr_ != nullptr;
    } else {
      return discovery_session_weak_ptr_.get();
    }
  }

  FakeBluetoothHelper* fake_bluetooth_helper() {
    return fake_bluetooth_helper_.get();
  }

  const multidevice::RemoteDeviceRefList& test_devices() {
    return test_devices_;
  }

  void ExpectHostSeenTimestamp(bool present) {
    EXPECT_EQ(
        present,
        ble_scanner_->GetLastSeenTimestamp(test_devices()[0].GetDeviceId())
            .has_value());
  }

  void SimulateAdapterPoweredChanged(bool powered) {
    // Note: MockBluetoothAdapter provides no way to notify observers, so the
    // observer callback must be invoked directly.
    for (auto& observer : mock_adapter_->GetObservers()) {
      observer.AdapterPoweredChanged(mock_adapter_.get(), powered);
    }
  }

 private:
  std::unique_ptr<FakeBluetoothDevice> SimulateScanResult(
      const std::string& service_data) {
    static const int16_t kFakeRssi = -70;
    static const std::vector<uint8_t> kFakeEir;

    // Scan result should not be received if there is no active discovery
    // session.
    EXPECT_TRUE(discovery_session_is_active());

    auto fake_bluetooth_device = std::make_unique<FakeBluetoothDevice>(
        service_data, mock_adapter_.get());

    // Note: MockBluetoothAdapter provides no way to notify observers, so the
    // observer callback must be invoked directly.
    for (auto& observer : mock_adapter_->GetObservers()) {
      observer.DeviceAdvertisementReceived(mock_adapter_.get(),
                                           fake_bluetooth_device.get(),
                                           kFakeRssi, kFakeEir);
    }

    return fake_bluetooth_device;
  }

  const multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<FakeBleScannerObserver> fake_delegate_;
  std::unique_ptr<FakeBluetoothHelper> fake_bluetooth_helper_;
  std::unique_ptr<FakeBleSynchronizer> fake_ble_synchronizer_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;

  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;
  raw_ptr<FakeServiceDataProvider, DanglingUntriaged>
      fake_service_data_provider_ = nullptr;
  base::WeakPtr<device::BluetoothDiscoverySession> discovery_session_weak_ptr_;
  raw_ptr<device::BluetoothLowEnergyScanSession> scan_session_ptr_ = nullptr;
  base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate>
      le_scan_delegate_;

  std::unique_ptr<BleScanner> ble_scanner_;
};

TEST_F(SecureChannelBleScannerImplTest, UnrelatedScanResults) {
  ConnectionAttemptDetails filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                               test_devices()[1].GetDeviceId()),
                                  ConnectionMedium::kBluetoothLowEnergy,
                                  ConnectionRole::kListenerRole);

  AddScanRequest(filter);
  InvokeStartDiscoveryCallback(true /* success */, 0u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  ProcessScanResultAndVerifyNoDeviceIdentified("unrelatedServiceData");

  RemoveScanRequest(filter);
  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());
  ExpectHostSeenTimestamp(false);
}

TEST_F(SecureChannelBleScannerImplTest, IncorrectRole) {
  ConnectionAttemptDetails filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                               test_devices()[1].GetDeviceId()),
                                  ConnectionMedium::kBluetoothLowEnergy,
                                  ConnectionRole::kListenerRole);

  AddScanRequest(filter);
  InvokeStartDiscoveryCallback(true /* success */, 0u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  // Set the device to be a foreground advertisement, even though the registered
  // role is listener.
  fake_bluetooth_helper()->SetIdentifiedDevice(
      "wrongRoleServiceData", test_devices()[0],
      false /* is_background_advertisement */);

  ProcessScanResultAndVerifyNoDeviceIdentified("wrongRoleServiceData");

  RemoveScanRequest(filter);
  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());
  ExpectHostSeenTimestamp(false);
}

TEST_F(SecureChannelBleScannerImplTest, IdentifyDevice_Background) {
  ConnectionAttemptDetails filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                               test_devices()[1].GetDeviceId()),
                                  ConnectionMedium::kBluetoothLowEnergy,
                                  ConnectionRole::kListenerRole);

  AddScanRequest(filter);
  InvokeStartDiscoveryCallback(true /* success */, 0u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  ProcessScanResultAndVerifyDevice("device0ServiceData", test_devices()[0],
                                   true /* is_background_advertisement */);

  RemoveScanRequest(filter);
  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());
  ExpectHostSeenTimestamp(true);
}

TEST_F(SecureChannelBleScannerImplTest, IdentifyDevice_BleAndNearby) {
  ConnectionAttemptDetails ble_filter(
      DeviceIdPair(test_devices()[0].GetDeviceId(),
                   test_devices()[1].GetDeviceId()),
      ConnectionMedium::kBluetoothLowEnergy, ConnectionRole::kListenerRole);
  ConnectionAttemptDetails nearby_filter(
      DeviceIdPair(test_devices()[0].GetDeviceId(),
                   test_devices()[1].GetDeviceId()),
      ConnectionMedium::kNearbyConnections, ConnectionRole::kInitiatorRole);

  AddScanRequest(ble_filter);
  InvokeStartDiscoveryCallback(true /* success */, 0u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  AddScanRequest(nearby_filter);
  EXPECT_TRUE(discovery_session_is_active());

  std::vector<std::pair<ConnectionMedium, ConnectionRole>> expected_results{
      {ConnectionMedium::kBluetoothLowEnergy, ConnectionRole::kListenerRole},
      {ConnectionMedium::kNearbyConnections, ConnectionRole::kInitiatorRole}};

  ProcessScanResultAndVerifyDevice("device0ServiceData", test_devices()[0],
                                   true /* is_background_advertisement */,
                                   expected_results);

  RemoveScanRequest(ble_filter);
  RemoveScanRequest(nearby_filter);
  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());
  ExpectHostSeenTimestamp(true);
}

TEST_F(SecureChannelBleScannerImplTest, IdentifyDevice_Foreground) {
  ConnectionAttemptDetails filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                               test_devices()[1].GetDeviceId()),
                                  ConnectionMedium::kBluetoothLowEnergy,
                                  ConnectionRole::kInitiatorRole);

  AddScanRequest(filter);
  InvokeStartDiscoveryCallback(true /* success */, 0u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  ProcessScanResultAndVerifyDevice("device0ServiceData", test_devices()[0],
                                   false /* is_background_advertisement */);

  RemoveScanRequest(filter);
  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());
  ExpectHostSeenTimestamp(true);
}

TEST_F(SecureChannelBleScannerImplTest, IdentifyDevice_MultipleScans) {
  ConnectionAttemptDetails filter_1(
      DeviceIdPair(test_devices()[0].GetDeviceId(),
                   test_devices()[1].GetDeviceId()),
      ConnectionMedium::kBluetoothLowEnergy, ConnectionRole::kInitiatorRole);
  ConnectionAttemptDetails filter_2(
      DeviceIdPair(test_devices()[2].GetDeviceId(),
                   test_devices()[1].GetDeviceId()),
      ConnectionMedium::kBluetoothLowEnergy, ConnectionRole::kInitiatorRole);

  AddScanRequest(filter_1);
  AddScanRequest(filter_2);
  InvokeStartDiscoveryCallback(true /* success */, 0u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  // Identify device 0.
  ProcessScanResultAndVerifyDevice("device0ServiceData", test_devices()[0],
                                   false /* is_background_advertisement */);

  // Remove the identified device from the list of scan filters.
  RemoveScanRequest(filter_1);

  // No additional BLE command should have been posted, since the existing scan
  // should not have been stopped.
  EXPECT_EQ(1u, GetNumBleCommands());
  EXPECT_TRUE(discovery_session_is_active());

  // Remove the scan filter, and verify that the scan stopped.
  RemoveScanRequest(filter_2);
  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());

  // Add the scan filter back again; this should start the discovery session
  // back up again.
  AddScanRequest(filter_2);
  InvokeStartDiscoveryCallback(true /* success */, 2u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  // Identify device 2.
  ProcessScanResultAndVerifyDevice("device2ServiceData", test_devices()[2],
                                   false /* is_background_advertisement */);

  // Remove the scan filter, and verify that the scan stopped.
  RemoveScanRequest(filter_2);
  InvokeStopDiscoveryCallback(true /* success */, 3u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());
  ExpectHostSeenTimestamp(true);
}

TEST_F(SecureChannelBleScannerImplTest, StartAndStopFailures) {
  ConnectionAttemptDetails filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                               test_devices()[1].GetDeviceId()),
                                  ConnectionMedium::kBluetoothLowEnergy,
                                  ConnectionRole::kListenerRole);
  AddScanRequest(filter);

  // A request was made to start discovery; simulate this request failing.
  InvokeStartDiscoveryCallback(false /* success */, 0u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());

  // BleScanner should have retried this attempt; simulate another failure.
  InvokeStartDiscoveryCallback(false /* success */, 1u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());

  // Succeed this time.
  InvokeStartDiscoveryCallback(true /* success */, 2u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  // Remove scan filters, which should trigger BleScanner to stop the
  // discovery session.
  RemoveScanRequest(filter);

  // Simulate a failure to stop.
  InvokeStopDiscoveryCallback(false /* success */, 3u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  // Simulate another failure.
  InvokeStopDiscoveryCallback(false /* success */, 4u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  // Succeed this time.
  InvokeStopDiscoveryCallback(true /* success */, 5u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());
}

TEST_F(SecureChannelBleScannerImplTest, StartAndStop_EdgeCases) {
  ConnectionAttemptDetails filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                               test_devices()[1].GetDeviceId()),
                                  ConnectionMedium::kBluetoothLowEnergy,
                                  ConnectionRole::kListenerRole);
  AddScanRequest(filter);

  // Remove scan filters before the start discovery callback succeeds.
  RemoveScanRequest(filter);

  // Complete starting the discovery session.
  InvokeStartDiscoveryCallback(true /* success */, 0u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  // BleScanner should have realized that it should now stop the discovery
  // session. Invoke the pending stop discovery callback.
  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());
}

TEST_F(SecureChannelBleScannerImplTest, StartAndStopFailures_EdgeCases) {
  ConnectionAttemptDetails filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                               test_devices()[1].GetDeviceId()),
                                  ConnectionMedium::kBluetoothLowEnergy,
                                  ConnectionRole::kListenerRole);
  AddScanRequest(filter);

  // Remove scan filters before the start discovery callback succeeds.
  RemoveScanRequest(filter);

  // Fail the pending call to start a discovery session.
  InvokeStartDiscoveryCallback(false /* success */, 0u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());

  // No additional BLE command should have been posted.
  EXPECT_EQ(1u, GetNumBleCommands());
}

TEST_F(SecureChannelBleScannerImplTest, StartAndStopFloss) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(floss::features::kFlossEnabled);

  ConnectionAttemptDetails filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                               test_devices()[1].GetDeviceId()),
                                  ConnectionMedium::kBluetoothLowEnergy,
                                  ConnectionRole::kListenerRole);
  AddScanRequest(filter);

  // A request was made to start scanning; simulate this request succeeding.
  InvokeStartLEScanSessionCallback(/*success=*/true);
  EXPECT_TRUE(discovery_session_is_active());

  // Remove scan filters, which should trigger BleScanner to stop the
  // scan session.
  RemoveScanRequest(filter);
  EXPECT_FALSE(discovery_session_is_active());
}

TEST_F(SecureChannelBleScannerImplTest, StartAndStop_EdgeCaseFloss) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(floss::features::kFlossEnabled);

  ConnectionAttemptDetails filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                               test_devices()[1].GetDeviceId()),
                                  ConnectionMedium::kBluetoothLowEnergy,
                                  ConnectionRole::kListenerRole);
  AddScanRequest(filter);

  // A request was made to start scanning; simulate this request failing.
  InvokeStartLEScanSessionCallback(/*success=*/false);

  // BleScanner should have realized that it didn't start a scan session
  // successfully and try again.
  EXPECT_TRUE(discovery_session_is_active());
}

TEST_F(SecureChannelBleScannerImplTest, StartAndInvalidateSessionFloss) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(floss::features::kFlossEnabled);

  ConnectionAttemptDetails filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                               test_devices()[1].GetDeviceId()),
                                  ConnectionMedium::kBluetoothLowEnergy,
                                  ConnectionRole::kListenerRole);
  AddScanRequest(filter);

  // Complete starting the scan session.
  InvokeStartLEScanSessionCallback(/*success=*/true);
  EXPECT_TRUE(discovery_session_is_active());

  // Simulate the session being invalidated.
  InvokeLEScanSessionInvalidated();

  // BleScanner should have realized that it was invalidated and start another
  // session.
  EXPECT_TRUE(discovery_session_is_active());
}

TEST_F(SecureChannelBleScannerImplTest, StartAndPowerOffAndPowerOnFloss) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(floss::features::kFlossEnabled);

  ConnectionAttemptDetails filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                               test_devices()[1].GetDeviceId()),
                                  ConnectionMedium::kBluetoothLowEnergy,
                                  ConnectionRole::kListenerRole);

  AddScanRequest(filter);
  SimulateAdapterPoweredChanged(false);

  // BleScanner should have realized that Floss is powered off and give up
  // starting another session.
  EXPECT_FALSE(discovery_session_is_active());

  SimulateAdapterPoweredChanged(true);
  EXPECT_TRUE(discovery_session_is_active());
}

}  // namespace ash::secure_channel
