// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FEATURE_STATUS_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FEATURE_STATUS_PROVIDER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/phonehub/feature_status.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

namespace ash::phonehub {

// Provides the current status of Phone Hub and notifies observers when the
// status changes.
class FeatureStatusProvider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when the status has changed; use GetStatus() for the new status.
    virtual void OnFeatureStatusChanged() = 0;

    // Called when there are eligible Phone Hub Hosts
    virtual void OnEligiblePhoneHubHostFound(
        multidevice::RemoteDeviceRefList device) {}
  };

  FeatureStatusProvider(const FeatureStatusProvider&) = delete;
  FeatureStatusProvider& operator=(const FeatureStatusProvider&) = delete;
  virtual ~FeatureStatusProvider();

  virtual FeatureStatus GetStatus() const = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  FeatureStatusProvider();

  void NotifyStatusChanged();

  void NotifyEligibleDevicesFound(
      const multidevice::RemoteDeviceRefList device);

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FEATURE_STATUS_PROVIDER_H_
