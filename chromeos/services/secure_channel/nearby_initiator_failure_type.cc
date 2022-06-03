// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/nearby_initiator_failure_type.h"

namespace chromeos {

namespace secure_channel {

std::ostream& operator<<(std::ostream& stream,
                         const NearbyInitiatorFailureType& failure_type) {
  switch (failure_type) {
    case NearbyInitiatorFailureType::kTimeoutDiscoveringDevice:
      stream << "[Timeout discovering device]";
      break;
    case NearbyInitiatorFailureType::kNearbyApiError:
      stream << "[Nearby API error]";
      break;
    case NearbyInitiatorFailureType::kConnectionRejected:
      stream << "[Connection rejected]";
      break;
    case NearbyInitiatorFailureType::kConnectivityError:
      stream << "[Connectivity error]";
      break;
    case NearbyInitiatorFailureType::kAuthenticationError:
      stream << "[Authentication error]";
      break;
  }
  return stream;
}

}  // namespace secure_channel

}  // namespace chromeos
