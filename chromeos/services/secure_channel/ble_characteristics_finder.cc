// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_characteristics_finder.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/secure_channel/background_eid_generator.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothRemoteGattCharacteristic;
using device::BluetoothRemoteGattService;
using device::BluetoothUUID;

namespace {
// The UUID of the characteristic for eid verification.
const char kEidCharacteristicUuid[] = "f21843b0-9411-434b-b85f-a9b92bd69f77";
}  // namespace

namespace chromeos {

namespace secure_channel {

BluetoothLowEnergyCharacteristicsFinder::
    BluetoothLowEnergyCharacteristicsFinder(
        scoped_refptr<BluetoothAdapter> adapter,
        BluetoothDevice* device,
        const RemoteAttribute& remote_service,
        const RemoteAttribute& to_peripheral_char,
        const RemoteAttribute& from_peripheral_char,
        const SuccessCallback& success_callback,
        const ErrorCallback& error_callback,
        const multidevice::RemoteDeviceRef& remote_device,
        std::unique_ptr<BackgroundEidGenerator> background_eid_generator)
    : adapter_(adapter),
      bluetooth_device_(device),
      remote_service_(remote_service),
      to_peripheral_char_(to_peripheral_char),
      from_peripheral_char_(from_peripheral_char),
      success_callback_(success_callback),
      error_callback_(error_callback),
      remote_device_(remote_device),
      background_eid_generator_(std::move(background_eid_generator)) {
  adapter_->AddObserver(this);
  if (device->IsGattServicesDiscoveryComplete())
    ScanRemoteCharacteristics();
}

BluetoothLowEnergyCharacteristicsFinder::
    BluetoothLowEnergyCharacteristicsFinder(
        const multidevice::RemoteDeviceRef& remote_device)
    : remote_device_(remote_device) {}

BluetoothLowEnergyCharacteristicsFinder::
    ~BluetoothLowEnergyCharacteristicsFinder() {
  if (adapter_) {
    adapter_->RemoveObserver(this);
  }
}

void BluetoothLowEnergyCharacteristicsFinder::GattServicesDiscovered(
    BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  // Ignore events about other devices.
  if (device != bluetooth_device_)
    return;
  PA_LOG(VERBOSE) << "All services discovered.";

  ScanRemoteCharacteristics();
}

void BluetoothLowEnergyCharacteristicsFinder::ScanRemoteCharacteristics() {
  if (have_services_been_parsed_)
    return;

  have_services_been_parsed_ = true;
  base::flat_set<BluetoothRemoteGattCharacteristic*>
      eid_characteristics_to_check;
  for (const BluetoothRemoteGattService* service :
       bluetooth_device_->GetGattServices()) {
    if (service->GetUUID() != remote_service_.uuid)
      continue;

    std::vector<BluetoothRemoteGattCharacteristic*> tx_chars =
        service->GetCharacteristicsByUUID(to_peripheral_char_.uuid);
    std::vector<BluetoothRemoteGattCharacteristic*> rx_chars =
        service->GetCharacteristicsByUUID(from_peripheral_char_.uuid);
    std::vector<BluetoothRemoteGattCharacteristic*> eid_chars =
        service->GetCharacteristicsByUUID(
            device::BluetoothUUID(kEidCharacteristicUuid));

    if (tx_chars.empty()) {
      PA_LOG(WARNING) << "Service missing TX char.";
      continue;
    }
    if (rx_chars.empty()) {
      PA_LOG(WARNING) << "Service missing RX char.";
      continue;
    }

    // If the GATT service has a TX and RX characteristic, but no EID
    // characteristic, the phone does not require an EID check. Either this is
    // a phone running an old version of GmsCore, or it does not have a work
    // profile. This is the right GATT service to use already.
    if (eid_chars.empty()) {
      NotifySuccess(service->GetIdentifier(), tx_chars.front()->GetIdentifier(),
                    rx_chars.front()->GetIdentifier());
      return;
    }

    eid_characteristics_to_check.insert(eid_chars.front());
    service_ids_pending_eid_read_.insert(service->GetIdentifier());
  }

  // If there were eid characteristics found when parsing services, read their
  // values and compare them to expected device EIDs to determine whether each
  // is the right GATT service.
  if (!eid_characteristics_to_check.empty()) {
    for (BluetoothRemoteGattCharacteristic* eid_char :
         eid_characteristics_to_check)
      TryToVerifyEid(eid_char);
    return;
  }

  // If all GATT services have been discovered and we haven't found the
  // characteristics we are looking for, call the error callback.
  NotifyFailureIfNoPendingEidCharReads();
}

void BluetoothLowEnergyCharacteristicsFinder::NotifySuccess(
    std::string service_id,
    std::string tx_id,
    std::string rx_id) {
  DCHECK(!has_callback_been_invoked_);
  has_callback_been_invoked_ = true;
  from_peripheral_char_.id = rx_id;
  to_peripheral_char_.id = tx_id;
  remote_service_.id = service_id;
  success_callback_.Run(remote_service_, to_peripheral_char_,
                        from_peripheral_char_);
}

void BluetoothLowEnergyCharacteristicsFinder::
    NotifyFailureIfNoPendingEidCharReads() {
  if (!service_ids_pending_eid_read_.empty())
    return;
  DCHECK(!has_callback_been_invoked_);
  has_callback_been_invoked_ = true;
  error_callback_.Run();
}

void BluetoothLowEnergyCharacteristicsFinder::TryToVerifyEid(
    device::BluetoothRemoteGattCharacteristic* eid_char) {
  eid_char->ReadRemoteCharacteristic(
      base::BindOnce(
          &BluetoothLowEnergyCharacteristicsFinder::OnRemoteCharacteristicRead,
          weak_ptr_factory_.GetWeakPtr(),
          eid_char->GetService()->GetIdentifier()),
      base::BindOnce(&BluetoothLowEnergyCharacteristicsFinder::
                         OnReadRemoteCharacteristicError,
                     weak_ptr_factory_.GetWeakPtr(),
                     eid_char->GetService()->GetIdentifier()));
}

void BluetoothLowEnergyCharacteristicsFinder::OnRemoteCharacteristicRead(
    const std::string& service_id,
    const std::vector<uint8_t>& value) {
  auto it = service_ids_pending_eid_read_.find(service_id);
  if (it == service_ids_pending_eid_read_.end()) {
    PA_LOG(WARNING) << "No request entry for " << service_id;
    return;
  }

  service_ids_pending_eid_read_.erase(it);

  if (has_callback_been_invoked_) {
    PA_LOG(VERBOSE) << "Characteristic read after callback was invoked.";
    return;
  }

  if (!DoesEidMatchExpectedDevice(value)) {
    if (!has_callback_been_invoked_)
      NotifyFailureIfNoPendingEidCharReads();
    return;
  }

  // Found the right GATT service! Grab identifiers and trigger success.
  const BluetoothRemoteGattService* service =
      bluetooth_device_->GetGattService(service_id);

  if (!service) {
    if (!has_callback_been_invoked_)
      NotifyFailureIfNoPendingEidCharReads();
    return;
  }

  std::vector<BluetoothRemoteGattCharacteristic*> tx_chars =
      service->GetCharacteristicsByUUID(to_peripheral_char_.uuid);
  std::vector<BluetoothRemoteGattCharacteristic*> rx_chars =
      service->GetCharacteristicsByUUID(from_peripheral_char_.uuid);
  NotifySuccess(service_id, tx_chars.front()->GetIdentifier(),
                rx_chars.front()->GetIdentifier());
}

void BluetoothLowEnergyCharacteristicsFinder::OnReadRemoteCharacteristicError(
    const std::string& service_id,
    device::BluetoothRemoteGattService::GattErrorCode error) {
  auto it = service_ids_pending_eid_read_.find(service_id);
  if (it == service_ids_pending_eid_read_.end()) {
    PA_LOG(WARNING) << "No request entry for " << service_id;
    return;
  }

  service_ids_pending_eid_read_.erase(it);

  PA_LOG(ERROR) << "OnWriteRemoteCharacteristicError() Error code: " << error;
  service_ids_pending_eid_read_.erase(service_id);
  if (!has_callback_been_invoked_)
    NotifyFailureIfNoPendingEidCharReads();
}

bool BluetoothLowEnergyCharacteristicsFinder::DoesEidMatchExpectedDevice(
    const std::vector<uint8_t>& eid_value_read) {
  // Convert the char data from a std::vector<uint8_t> to a std::string.
  std::string eid_char_data_str;
  char* string_contents_ptr =
      base::WriteInto(&eid_char_data_str, eid_value_read.size() + 1);
  memcpy(string_contents_ptr, eid_value_read.data(), eid_value_read.size());

  multidevice::RemoteDeviceRefList remote_device_list{remote_device_};
  std::string identified_device_id =
      background_eid_generator_->IdentifyRemoteDeviceByAdvertisement(
          eid_char_data_str, remote_device_list);
  return !identified_device_id.empty();
}

}  // namespace secure_channel

}  // namespace chromeos
