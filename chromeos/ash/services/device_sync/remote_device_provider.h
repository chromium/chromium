// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_PROVIDER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_PROVIDER_H_

#include "base/observer_list.h"
#include "chromeos/ash/components/multidevice/remote_device.h"

namespace ash {

namespace device_sync {

// This class generates and caches RemoteDevice objects when associated metadata
// has been synced, and updates this cache when a new sync occurs.
class RemoteDeviceProvider {
 public:
  class Observer {
   public:
    virtual void OnSyncDeviceListChanged() {}

   protected:
    virtual ~Observer() = default;
  };

  RemoteDeviceProvider();
  virtual ~RemoteDeviceProvider();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns a list of all RemoteDevices that have been synced.
  virtual const multidevice::RemoteDeviceList& GetSyncedDevices() const = 0;

 protected:
  void NotifyObserversDeviceListChanged();

 private:
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_PROVIDER_H_
