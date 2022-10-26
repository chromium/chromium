// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/nearby_initiator_failure_type.h"

namespace ash::secure_channel {

std::ostream& operator<<(std::ostream& stream,
                         const NearbyInitiatorFailureType& failure_type) {
  switch (failure_type) {
    case NearbyInitiatorFailureType::kConnectivityError:
      stream << "[Connectivity error]";
      break;
    case NearbyInitiatorFailureType::kAuthenticationError:
      stream << "[Authentication error]";
      break;
  }
  return stream;
}

}  // namespace ash::secure_channel
