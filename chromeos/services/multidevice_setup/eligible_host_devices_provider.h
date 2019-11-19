// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_ELIGIBLE_HOST_DEVICES_PROVIDER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_ELIGIBLE_HOST_DEVICES_PROVIDER_H_

#include "base/macros.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/multidevice_setup/device_with_connectivity_status.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace chromeos {

namespace multidevice_setup {

// Provides all remote devices which are eligible to be MultiDevice hosts.
class EligibleHostDevicesProvider {
 public:
  virtual ~EligibleHostDevicesProvider() = default;

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
  EligibleHostDevicesProvider() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(EligibleHostDevicesProvider);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_ELIGIBLE_HOST_DEVICES_PROVIDER_H_
