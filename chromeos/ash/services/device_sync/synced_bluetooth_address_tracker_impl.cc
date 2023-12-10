// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/synced_bluetooth_address_tracker_impl.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/cryptauth_scheduler.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash {

namespace device_sync {

// Value stored in the kCryptAuthBluetoothAddressProvidedDuringLastSync pref
// when a Bluetooth address has not yet been provided after a successful
// DeviceSync attempt.
const char kHasNotSyncedYetPrefValue[] = "hasNotSyncedYet";

// static
SyncedBluetoothAddressTrackerImpl::Factory*
    SyncedBluetoothAddressTrackerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<SyncedBluetoothAddressTracker>
SyncedBluetoothAddressTrackerImpl::Factory::Create(
    CryptAuthScheduler* cryptauth_scheduler,
    PrefService* pref_service) {
  if (test_factory_) {
    return test_factory_->CreateInstance(cryptauth_scheduler, pref_service);
  }

  return base::WrapUnique(
      new SyncedBluetoothAddressTrackerImpl(cryptauth_scheduler, pref_service));
}

// static
void SyncedBluetoothAddressTrackerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

SyncedBluetoothAddressTrackerImpl::Factory::~Factory() = default;

// static
void SyncedBluetoothAddressTrackerImpl::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(
      prefs::kCryptAuthBluetoothAddressProvidedDuringLastSync,
      kHasNotSyncedYetPrefValue);
}

SyncedBluetoothAddressTrackerImpl::SyncedBluetoothAddressTrackerImpl(
    CryptAuthScheduler* cryptauth_scheduler,
    PrefService* pref_service)
    : cryptauth_scheduler_(cryptauth_scheduler), pref_service_(pref_service) {
  // If the flag is disabled, set the pref to the "has not synced" state. This
  // ensures that when the flag is enabled, the device kicks off a DeviceSync
  // attempt with the address, which may not have happened in the case that the
  // flag was flipped on, off, then on again.
  if (!features::IsPhoneHubEnabled()) {
    pref_service_->SetString(
        prefs::kCryptAuthBluetoothAddressProvidedDuringLastSync,
        kHasNotSyncedYetPrefValue);
  }

  device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &SyncedBluetoothAddressTrackerImpl::OnBluetoothAdapterReceived,
      weak_ptr_factory_.GetWeakPtr()));
}

SyncedBluetoothAddressTrackerImpl::~SyncedBluetoothAddressTrackerImpl() {
  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
}

void SyncedBluetoothAddressTrackerImpl::GetBluetoothAddress(
    BluetoothAddressCallback callback) {
  if (!bluetooth_adapter_) {
    pending_callbacks_during_init_.push_back(std::move(callback));
    return;
  }

  std::move(callback).Run(GetAddress());
}

void SyncedBluetoothAddressTrackerImpl::SetLastSyncedBluetoothAddress(
    const std::string& last_synced_address) {
  // No pref should be stored with the flag disabled.
  if (!features::IsPhoneHubEnabled())
    return;

  if (last_synced_address.empty()) {
    PA_LOG(VERBOSE) << "Recording successful DeviceSync without a Bluetooth "
                    << "address.";
  } else {
    PA_LOG(INFO) << "Recording successful DeviceSync with Bluetooth address: "
                 << last_synced_address;
  }

  pref_service_->SetString(
      prefs::kCryptAuthBluetoothAddressProvidedDuringLastSync,
      last_synced_address);
}

void SyncedBluetoothAddressTrackerImpl::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  DCHECK_EQ(adapter, bluetooth_adapter_.get());

  if (present)
    ScheduleSyncIfAddressChanged();
}

void SyncedBluetoothAddressTrackerImpl::OnBluetoothAdapterReceived(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  bluetooth_adapter_ = std::move(bluetooth_adapter);
  bluetooth_adapter_->AddObserver(this);

  for (auto& callback : pending_callbacks_during_init_)
    std::move(callback).Run(GetAddress());
  pending_callbacks_during_init_.clear();

  ScheduleSyncIfAddressChanged();
}

void SyncedBluetoothAddressTrackerImpl::ScheduleSyncIfAddressChanged() {
  // No sync should be scheduled if the flag is off.
  if (!features::IsPhoneHubEnabled())
    return;

  std::string address_from_last_sync = pref_service_->GetString(
      prefs::kCryptAuthBluetoothAddressProvidedDuringLastSync);

  // If we've already synced and the Bluetooth address has changed (perhaps due
  // to the user inserting a USB Bluetooth adapter), we should sync this new
  // address. Return early if this is not the case.
  if (GetAddress().empty() ||
      address_from_last_sync == kHasNotSyncedYetPrefValue ||
      address_from_last_sync == GetAddress()) {
    return;
  }

  PA_LOG(INFO) << "Bluetooth address has changed since last DeviceSync. "
               << "Requesting new DeviceSync attempt.";
  cryptauth_scheduler_->RequestDeviceSync(
      cryptauthv2::ClientMetadata::InvocationReason::
          ClientMetadata_InvocationReason_ADDRESS_CHANGE,
      /*session_id=*/std::nullopt);
}

std::string SyncedBluetoothAddressTrackerImpl::GetAddress() {
  DCHECK(bluetooth_adapter_);
  if (features::IsPhoneHubEnabled())
    return bluetooth_adapter_->GetAddress();

  // Return an empty string if the flag is disabled.
  return std::string();
}

}  // namespace device_sync

}  // namespace ash
