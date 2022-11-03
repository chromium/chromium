// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FEATURE_STATUS_CHANGE_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FEATURE_STATUS_CHANGE_H_

namespace ash {

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

}  // namespace ash

#endif  //  CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FEATURE_STATUS_CHANGE_H_
