// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAST_PAIR_DELEGATE_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAST_PAIR_DELEGATE_H_

namespace chromeos {
namespace bluetooth_config {

class DeviceNameManager;

// Delegate class used to connect the bluetooth_config and Fast Pair systems,
// which live in different parts of the dependency tree and cannot directly
// call each other.
class FastPairDelegate {
 public:
  virtual ~FastPairDelegate() = default;

  virtual void SetDeviceNameManager(DeviceNameManager* device_name_manager) = 0;
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAST_PAIR_DELEGATE_H_
