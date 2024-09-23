// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/ble_scanner_impl.h"

#include <iostream>
#include <sstream>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/ash/services/secure_channel/ble_synchronizer_base.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/ble_constants.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "device/bluetooth/floss/floss_features.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace ash::secure_channel {

namespace {

// TODO(hansberry): Share this constant with BluetoothHelper.
const size_t kMinNumBytesInServiceData = 2;

constexpr base::TimeDelta kScanningDeviceFoundTimeout = base::Seconds(1);
constexpr base::TimeDelta kScanningDeviceLostTimeout = base::Seconds(7);
constexpr base::TimeDelta kScanningRssiSamplingPeriod = base::Seconds(1);

}  // namespace

// static
BleScannerImpl::Factory* BleScannerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<BleScanner> BleScannerImpl::Factory::Create(
    BluetoothHelper* bluetooth_helper,
    BleSynchronizerBase* ble_synchronizer,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (test_factory_) {
    return test_factory_->CreateInstance(bluetooth_helper, ble_synchronizer,
                                         adapter);
  }

  return base::WrapUnique(
      new BleScannerImpl(bluetooth_helper, ble_synchronizer, adapter));
}

// static
void BleScannerImpl::Factory::SetFactoryForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

BleScannerImpl::Factory::~Factory() = default;

BleScannerImpl::ServiceDataProvider::~ServiceDataProvider() = default;

const std::vector<uint8_t>*
BleScannerImpl::ServiceDataProvider::ExtractProximityAuthServiceData(
    device::BluetoothDevice* bluetooth_device) {
  return bluetooth_device->GetServiceDataForUUID(
      device::BluetoothUUID(kAdvertisingServiceUuid));
}

BleScannerImpl::BleScannerImpl(BluetoothHelper* bluetooth_helper,
                               BleSynchronizerBase* ble_synchronizer,
                               scoped_refptr<device::BluetoothAdapter> adapter)
    : bluetooth_helper_(bluetooth_helper),
      ble_synchronizer_(ble_synchronizer),
      adapter_(adapter),
      service_data_provider_(std::make_unique<ServiceDataProvider>()) {
  adapter_->AddObserver(this);
}

BleScannerImpl::~BleScannerImpl() {
  adapter_->RemoveObserver(this);
}

void BleScannerImpl::HandleScanRequestChange() {
  if (GetAllDeviceIdPairs().empty()) {
    PA_LOG(INFO) << "No devices to scan for";
  } else {
    PA_LOG(INFO) << "Scanning for: "
                 << bluetooth_helper_->ExpectedServiceDataToString(
                        GetAllDeviceIdPairs());
  }

  UpdateDiscoveryStatus();
}

void BleScannerImpl::AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                           bool powered) {
  DCHECK_EQ(adapter_.get(), adapter);
  if (!floss::features::IsFlossEnabled()) {
    // No-op for BlueZ.
    return;
  }

  if (powered) {
    PA_LOG(INFO) << "Update LE scan session due to power on.";
    UpdateDiscoveryStatus();
  } else {
    // The BluetoothLowEnergyScanSession callbacks may never be called due to
    // Floss being powered off. Reset the session anyway.
    PA_LOG(INFO) << "Reset LE scan session due to power off.";
    SetDiscoverySeissionFailed(mojom::DiscoveryErrorCode::kBluetoothTurnedOff);
    is_initializing_discovery_session_ = false;
    le_scan_session_.reset();
  }
}

void BleScannerImpl::DeviceAdvertisementReceived(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* bluetooth_device,
    int16_t rssi,
    const std::vector<uint8_t>& eir) {
  DCHECK_EQ(adapter_.get(), adapter);
  HandleDeviceUpdated(bluetooth_device);
}

void BleScannerImpl::UpdateDiscoveryStatus() {
  if (should_discovery_session_be_active())
    EnsureDiscoverySessionActive();
  else
    EnsureDiscoverySessionNotActive();
}

bool BleScannerImpl::IsDiscoverySessionActive() {
  if (floss::features::IsFlossEnabled()) {
    return le_scan_session_.get() != nullptr;
  }

  ResetDiscoverySessionIfNotActive();
  return discovery_session_.get() != nullptr;
}

void BleScannerImpl::ResetDiscoverySessionIfNotActive() {
  if (!discovery_session_ || discovery_session_->IsActive())
    return;

  PA_LOG(ERROR) << "BluetoothDiscoverySession became out of sync. Session is "
                << "no longer active, but it was never stopped successfully. "
                << "Resetting session.";

  // |discovery_session_| should be deleted as part of
  // OnDiscoverySessionStopped() whenever the session is no longer active.
  // However, a Bluetooth issue (https://crbug.com/768521) sometimes causes the
  // session to become inactive without Stop() ever succeeding. If this
  // occurs, reset state accordingly.
  discovery_session_.reset();
  discovery_session_weak_ptr_factory_.reset();
  is_initializing_discovery_session_ = false;
  is_stopping_discovery_session_ = false;
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void BleScannerImpl::EnsureDiscoverySessionActive() {
  if (IsDiscoverySessionActive() || is_initializing_discovery_session_)
    return;

  is_initializing_discovery_session_ = true;

  if (floss::features::IsFlossEnabled()) {
    device::BluetoothLowEnergyScanFilter::Pattern pattern(
        /*start_position=*/0,
        device::BluetoothLowEnergyScanFilter::AdvertisementDataType::
            kServiceData,
        kAdvertisingServiceUuidAsBytes);
    auto filter = device::BluetoothLowEnergyScanFilter::Create(
        device::BluetoothLowEnergyScanFilter::Range::kFar,
        kScanningDeviceFoundTimeout, kScanningDeviceLostTimeout, {pattern},
        kScanningRssiSamplingPeriod);
    if (!filter) {
      PA_LOG(ERROR)
          << "Failed to start LE scanning due to failure to create filter.";
      SetDiscoverySeissionFailed(
          mojom::DiscoveryErrorCode::kFilterCreationFailed);
      return;
    }

    le_scan_session_ = adapter_->StartLowEnergyScanSession(
        std::move(filter), weak_ptr_factory_.GetWeakPtr());
    return;
  }

  ble_synchronizer_->StartDiscoverySession(
      base::BindOnce(&BleScannerImpl::OnDiscoverySessionStarted,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&BleScannerImpl::OnStartDiscoverySessionError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BleScannerImpl::OnDiscoverySessionStarted(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  PA_LOG(INFO) << "Started discovery session successfully.";
  is_initializing_discovery_session_ = false;

  discovery_session_ = std::move(discovery_session);
  discovery_session_weak_ptr_factory_ =
      std::make_unique<base::WeakPtrFactory<device::BluetoothDiscoverySession>>(
          discovery_session_.get());

  UpdateDiscoveryStatus();
}

void BleScannerImpl::OnStartDiscoverySessionError() {
  is_initializing_discovery_session_ = false;
  PA_LOG(ERROR) << "Error starting discovery session.";
  SetDiscoverySeissionFailed(
      mojom::DiscoveryErrorCode::kErrorStartingDiscovery);
  UpdateDiscoveryStatus();
}

void BleScannerImpl::EnsureDiscoverySessionNotActive() {
  if (floss::features::IsFlossEnabled() && is_initializing_discovery_session_) {
    // We won't be able to receive any updates from Floss after
    // |le_scan_session_| is reset. Since this function aims to ensure the
    // session is down, |is_initializing_discovery_session_| must be reset as
    // well otherwise we would get stuck in the initializing state forever.
    PA_LOG(WARNING) << "LE scan is reset while still initializing.";
    is_initializing_discovery_session_ = false;
  }

  if (!IsDiscoverySessionActive() || is_stopping_discovery_session_)
    return;

  if (floss::features::IsFlossEnabled()) {
    if (le_scan_session_)
      le_scan_session_.reset();
    return;
  }

  is_stopping_discovery_session_ = true;

  ble_synchronizer_->StopDiscoverySession(
      discovery_session_weak_ptr_factory_->GetWeakPtr(),
      base::BindOnce(&BleScannerImpl::OnDiscoverySessionStopped,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&BleScannerImpl::OnStopDiscoverySessionError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BleScannerImpl::OnDiscoverySessionStopped() {
  is_stopping_discovery_session_ = false;
  PA_LOG(INFO) << "Stopped discovery session successfully.";

  discovery_session_.reset();
  discovery_session_weak_ptr_factory_.reset();

  UpdateDiscoveryStatus();
}

void BleScannerImpl::OnStopDiscoverySessionError() {
  is_stopping_discovery_session_ = false;
  PA_LOG(ERROR) << "Error stopping discovery session.";
  UpdateDiscoveryStatus();
}

void BleScannerImpl::OnDeviceFound(
    device::BluetoothLowEnergyScanSession* scan_session,
    device::BluetoothDevice* device) {
  // This can be left empty since Floss will also invoke
  // DeviceAdvertisementReceived when it receives a scan result.
}

void BleScannerImpl::OnDeviceLost(
    device::BluetoothLowEnergyScanSession* scan_session,
    device::BluetoothDevice* device) {
  // This can be left empty since there is nothing to do when
  // a device is no longer in range.
}

void BleScannerImpl::OnSessionStarted(
    device::BluetoothLowEnergyScanSession* scan_session,
    std::optional<device::BluetoothLowEnergyScanSession::ErrorCode>
        error_code) {
  is_initializing_discovery_session_ = false;

  if (error_code) {
    PA_LOG(ERROR) << "LE scan session failed to start, error_code = "
                  << static_cast<int>(error_code.value());
    SetDiscoverySeissionFailed(
        mojom::DiscoveryErrorCode::kErrorStartingDiscovery);
    if (le_scan_session_)
      le_scan_session_.reset();
  } else {
    PA_LOG(INFO) << "Started LE scan session successfully.";
  }

  UpdateDiscoveryStatus();
}

void BleScannerImpl::OnSessionInvalidated(
    device::BluetoothLowEnergyScanSession* scan_session) {
  PA_LOG(INFO) << "LE scan session was invalidated";
  SetDiscoverySeissionFailed(mojom::DiscoveryErrorCode::kBleSessionInvalidated);
  if (le_scan_session_)
    le_scan_session_.reset();
  UpdateDiscoveryStatus();
}

void BleScannerImpl::HandleDeviceUpdated(
    device::BluetoothDevice* bluetooth_device) {
  DCHECK(bluetooth_device);

  const std::vector<uint8_t>* service_data =
      service_data_provider_->ExtractProximityAuthServiceData(bluetooth_device);
  if (!service_data || service_data->size() < kMinNumBytesInServiceData) {
    // If there is no service data or the service data is of insufficient
    // length, there is not enough information to create a connection.
    return;
  }

  std::string service_data_str(service_data->begin(), service_data->end());
  auto potential_result = bluetooth_helper_->IdentifyRemoteDevice(
      service_data_str, GetAllDeviceIdPairs());

  // There was service data for the ProximityAuth UUID, but it did not apply to
  // any active scan requests. The advertisement was likely from a nearby device
  // attempting a ProximityAuth connection for another account.
  if (!potential_result)
    return;

  HandlePotentialScanResult(service_data_str, *potential_result,
                            bluetooth_device);
}

void BleScannerImpl::HandlePotentialScanResult(
    const std::string& service_data,
    const BluetoothHelper::DeviceWithBackgroundBool& potential_result,
    device::BluetoothDevice* bluetooth_device) {
  std::vector<std::pair<ConnectionMedium, ConnectionRole>> results;

  // Check to see if a corresponding scan request exists. At this point, it is
  // possible that a scan result was received for the correct DeviceIdPair but
  // incorrect ConnectionMedium and/or ConnectionRole.
  for (const auto& scan_request : scan_requests()) {
    if (scan_request.remote_device_id() !=
        potential_result.first.GetDeviceId()) {
      continue;
    }

    switch (scan_request.connection_medium()) {
      // BLE scans for background advertisements in the listener role and for
      // foreground advertisements in the initiator role.
      case ConnectionMedium::kBluetoothLowEnergy:
        if (potential_result.second &&
            scan_request.connection_role() == ConnectionRole::kListenerRole) {
          results.emplace_back(ConnectionMedium::kBluetoothLowEnergy,
                               ConnectionRole::kListenerRole);
        } else if (!potential_result.second &&
                   scan_request.connection_role() ==
                       ConnectionRole::kInitiatorRole) {
          results.emplace_back(ConnectionMedium::kBluetoothLowEnergy,
                               ConnectionRole::kInitiatorRole);
        }
        break;

      // Nearby Connections scans for background advertisements in the initiator
      // role and does not have support for the listener role.
      case ConnectionMedium::kNearbyConnections:
        DCHECK_EQ(ConnectionRole::kInitiatorRole,
                  scan_request.connection_role());
        results.emplace_back(ConnectionMedium::kNearbyConnections,
                             ConnectionRole::kInitiatorRole);
        break;
    }
  }

  // Prepare a hex string of |service_data|.
  std::string hex_service_data =
      base::StrCat({"0x", base::HexEncode(service_data)});

  if (results.empty()) {
    PA_LOG(WARNING) << "BleScannerImpl::HandleDeviceUpdated(): Received scan "
                    << "result from device with ID \""
                    << potential_result.first.GetTruncatedDeviceIdForLogs()
                    << "\", but it did not correspond to an active scan "
                    << "request. Service data: " << hex_service_data
                    << ", Background advertisement: "
                    << (potential_result.second ? "true" : "false");
    SetDiscoverySeissionFailed(
        mojom::DiscoveryErrorCode::kDeviceNotInScanRequest);
    return;
  }

  PA_LOG(INFO) << "BleScannerImpl::HandleDeviceUpdated(): Received scan result "
               << "from device with ID \""
               << potential_result.first.GetTruncatedDeviceIdForLogs() << "\""
               << ". Service data: " << hex_service_data
               << ", Background advertisement: "
               << (potential_result.second ? "true" : "false");

  std::vector<uint8_t> eid(service_data.begin(),
                           service_data.begin() + kMinNumBytesInServiceData);

  for (const std::pair<ConnectionMedium, ConnectionRole>& result : results) {
    NotifyReceivedAdvertisementFromDevice(potential_result.first,
                                          bluetooth_device, result.first,
                                          result.second, eid);
  }
}

void BleScannerImpl::SetDiscoverySeissionFailed(
    mojom::DiscoveryErrorCode error_code) {
  for (const auto& device_id_pair : GetAllDeviceIdPairs()) {
    NotifyBleDiscoverySessionFailed(
        device_id_pair, mojom::DiscoveryResult::kFailure, error_code);
  }
}

void BleScannerImpl::SetServiceDataProviderForTesting(
    std::unique_ptr<ServiceDataProvider> service_data_provider) {
  service_data_provider_ = std::move(service_data_provider);
}

}  // namespace ash::secure_channel
