// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SYNCED_BLUETOOTH_ADDRESS_TRACKER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SYNCED_BLUETOOTH_ADDRESS_TRACKER_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/device_sync/synced_bluetooth_address_tracker.h"
#include "device/bluetooth/bluetooth_adapter.h"

class PrefRegistrySimple;
class PrefService;

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace ash {

namespace device_sync {

class CryptAuthScheduler;

// SyncedBluetoothAddressTracker implementation which uses profile prefs to
// store the last synced Bluetooth address. If the address changes, it triggers
// a new DeviceSync attempt via CryptAuthScheduler.
class SyncedBluetoothAddressTrackerImpl
    : public SyncedBluetoothAddressTracker,
      public device::BluetoothAdapter::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<SyncedBluetoothAddressTracker> Create(
        CryptAuthScheduler* cryptauth_scheduler,
        PrefService* pref_service);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<SyncedBluetoothAddressTracker> CreateInstance(
        CryptAuthScheduler* cryptauth_scheduler,
        PrefService* pref_service) = 0;

   private:
    static Factory* test_factory_;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~SyncedBluetoothAddressTrackerImpl() override;

  // device::BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;

 private:
  SyncedBluetoothAddressTrackerImpl(CryptAuthScheduler* cryptauth_scheduler,
                                    PrefService* pref_service);

  // SyncedBluetoothAddressTracker:
  void GetBluetoothAddress(BluetoothAddressCallback callback) override;
  void SetLastSyncedBluetoothAddress(
      const std::string& last_synced_address) override;

  void OnBluetoothAdapterReceived(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
  void ScheduleSyncIfAddressChanged();
  std::string GetAddress();

  raw_ptr<CryptAuthScheduler> cryptauth_scheduler_;
  raw_ptr<PrefService> pref_service_;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  std::vector<BluetoothAddressCallback> pending_callbacks_during_init_;

  base::WeakPtrFactory<SyncedBluetoothAddressTrackerImpl> weak_ptr_factory_{
      this};
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SYNCED_BLUETOOTH_ADDRESS_TRACKER_IMPL_H_
