// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_SOFTWARE_FEATURE_STATE_H_
#define CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_SOFTWARE_FEATURE_STATE_H_

#include <ostream>

namespace ash::multidevice {

// State of a multi-device feature (see SoftwareFeature).
//
// Note that numerical enum values must not be changed as these values are
// serialized to numbers and stored persistently.
enum class SoftwareFeatureState {
  // Not supported by the device (e.g., hardware does not support feature).
  kNotSupported = 0,

  // Supported by device, but device has not enabled the feature.
  kSupported = 1,

  // Supported by device, and device has enabled the feature.
  kEnabled = 2
};

std::ostream& operator<<(std::ostream& stream,
                         const SoftwareFeatureState& state);

}  // namespace ash::multidevice

#endif  // CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_SOFTWARE_FEATURE_STATE_H_
