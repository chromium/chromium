// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_ELIGIBLE_HOST_DEVICES_PROVIDER_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_ELIGIBLE_HOST_DEVICES_PROVIDER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/multidevice_setup/device_with_connectivity_status.h"

namespace ash {

namespace multidevice_setup {

// Provides all remote devices which are eligible to be MultiDevice hosts.
class EligibleHostDevicesProvider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Call when new devices synced from
    // `DeviceSyncClient::OnNewDevicesSynced()` have been processed by the
    // `EligibleHostDevicesProvider`, which  entails filtering out devices
    // that do not support BETTER_TOGETHER, and that are inactive. Observers
    // should wait on`EligibleHostDevicesProvider::OnEligibleDevicesSynced()`
    // rather than `DeviceSyncClient::OnNewDevicesSynced()` to calculate the
    // host status to prevent calculating host status from an outdated
    // eligible device cache.
    virtual void OnEligibleDevicesSynced() = 0;
  };

  EligibleHostDevicesProvider(const EligibleHostDevicesProvider&) = delete;
  EligibleHostDevicesProvider& operator=(const EligibleHostDevicesProvider&) =
      delete;

  virtual ~EligibleHostDevicesProvider();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void NotifyObserversEligibleDevicesSynced();

  // Returns all eligible host devices sorted by the last time they were updated
  // on (i.e. enrolled with) the server. In this context, this means that the
  // devices have a SoftwareFeatureState of kSupported or kEnabled for the
  // BETTER_TOGETHER_HOST feature.
  virtual multidevice::RemoteDeviceRefList GetEligibleHostDevices() const = 0;

  // Returns all eligible host devices sorted by the last time they were used
  // as determined by the server (based on GCM connectivity status and last
  // contact time with the server). In this context, this means that the
  // devices have a SoftwareFeatureState of kSupported or kEnabled for the
  // BETTER_TOGETHER_HOST feature.
  virtual multidevice::DeviceWithConnectivityStatusList
  GetEligibleActiveHostDevices() const = 0;

 protected:
  EligibleHostDevicesProvider();

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_ELIGIBLE_HOST_DEVICES_PROVIDER_H_
