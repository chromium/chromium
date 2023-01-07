// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/multidevice/software_feature_state.h"

namespace ash::multidevice {

std::ostream& operator<<(std::ostream& stream,
                         const SoftwareFeatureState& state) {
  switch (state) {
    case SoftwareFeatureState::kNotSupported:
      stream << "[not supported]";
      break;
    case SoftwareFeatureState::kSupported:
      stream << "[supported]";
      break;
    case SoftwareFeatureState::kEnabled:
      stream << "[enabled]";
      break;
  }
  return stream;
}

}  // namespace ash::multidevice
