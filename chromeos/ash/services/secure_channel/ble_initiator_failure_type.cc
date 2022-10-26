// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/ble_initiator_failure_type.h"

namespace ash::secure_channel {

std::ostream& operator<<(std::ostream& stream,
                         const BleInitiatorFailureType& failure_type) {
  switch (failure_type) {
    case BleInitiatorFailureType::kAuthenticationError:
      stream << "[authentication error]";
      break;
    case BleInitiatorFailureType::kGattConnectionError:
      stream << "[GATT connection error]";
      break;
    case BleInitiatorFailureType::kInterruptedByHigherPriorityConnectionAttempt:
      stream << "[interrupted by higher priority attempt]";
      break;
    case BleInitiatorFailureType::kTimeoutContactingRemoteDevice:
      stream << "[timeout contacting remote device]";
      break;
    case BleInitiatorFailureType::kCouldNotGenerateAdvertisement:
      stream << "[could not generate advertisement]";
      break;
  }
  return stream;
}

}  // namespace ash::secure_channel
