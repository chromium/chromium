// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_H_

namespace ash::quick_start {

class TargetDeviceBootstrapController {
 public:
  enum class FeatureSupportStatus {
    kUndetermined = 0,
    kNotSupported,
    kSupported
  };

  TargetDeviceBootstrapController() = default;
  virtual ~TargetDeviceBootstrapController() = default;

  // Checks to see whether the feature can be supported on the device's
  // hardware. The feature is supported if Bluetooth is supported and an adapter
  // is present.
  virtual FeatureSupportStatus GetFeatureSupportStatus() const = 0;
};

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_H_
