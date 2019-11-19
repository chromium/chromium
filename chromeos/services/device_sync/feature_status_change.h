// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FEATURE_STATUS_CHANGE_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FEATURE_STATUS_CHANGE_H_

namespace chromeos {

namespace device_sync {

// The target status change for a device's SoftwareFeature.
enum class FeatureStatusChange {
  // Enables a feature on a device and disables the feature on all other devices
  // associated with the account.
  kEnableExclusively = 0,

  // Enables a feature on a device; other devices on the account are unaffected.
  kEnableNonExclusively = 1,

  // Disables a feature on a device; other devices on the account are
  // unaffected.
  kDisable = 2
};

}  // namespace device_sync

}  // namespace chromeos

#endif  //  CHROMEOS_SERVICES_DEVICE_SYNC_FEATURE_STATUS_CHANGE_H_
