// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_DEVICE_TIMESTAMP_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_DEVICE_TIMESTAMP_MANAGER_H_

#include <optional>

#include "base/time/time.h"

namespace ash {

namespace multidevice_setup {

// Records time at which the logged in user completed the MultiDevice setup flow
// on this device.
class HostDeviceTimestampManager {
 public:
  HostDeviceTimestampManager(const HostDeviceTimestampManager&) = delete;
  HostDeviceTimestampManager& operator=(const HostDeviceTimestampManager&) =
      delete;

  virtual ~HostDeviceTimestampManager() = default;

  // Returns true when there is a host set (not necessarily verified) for the
  // logged in GAIA account and that host was set from this Chromebook.
  virtual bool WasHostSetFromThisChromebook() = 0;

  // If the logged in GAIA account has completed the MultiDevice setup flow on
  // this device, this returns the time at which the flow was completed. If the
  // flow was completed more than once, it records the most recent time of
  // completion. Otherwise it returns std::nullopt.
  virtual std::optional<base::Time> GetLatestSetupFlowCompletionTimestamp() = 0;

  // If the logged in GAIA account has ever received a host status update that
  // a host was verified, this returns the time at which the last such update
  // was received. Otherwise it returns std::nullopt.
  virtual std::optional<base::Time> GetLatestVerificationTimestamp() = 0;

 protected:
  HostDeviceTimestampManager() = default;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_DEVICE_TIMESTAMP_MANAGER_H_
