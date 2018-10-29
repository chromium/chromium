// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_DEVICE_TIMESTAMP_MANAGER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_DEVICE_TIMESTAMP_MANAGER_H_

#include "base/optional.h"
#include "base/time/time.h"

namespace chromeos {

namespace multidevice_setup {

// Records time at which the logged in user completed the MultiDevice setup flow
// on this device.
class HostDeviceTimestampManager {
 public:
  virtual ~HostDeviceTimestampManager() = default;

  // Returns true when there is a host set (not necessarily verified) for the
  // logged in GAIA account and that host was set from this Chromebook.
  virtual bool WasHostSetFromThisChromebook() = 0;

  // If the logged in GAIA account has completed the MultiDevice setup flow on
  // this device, this returns the time at which the flow was completed. If the
  // flow was completed more than once, it records the most recent time of
  // completion. Otherwise it returns base::nullopt.
  virtual base::Optional<base::Time>
  GetLatestSetupFlowCompletionTimestamp() = 0;

  // If the logged in GAIA account has ever received a host status update that
  // a host was verified, this returns the time at which the last such update
  // was received. Otherwise it returns base::nullopt.
  virtual base::Optional<base::Time> GetLatestVerificationTimestamp() = 0;

 protected:
  HostDeviceTimestampManager() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostDeviceTimestampManager);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_DEVICE_TIMESTAMP_MANAGER_H_
