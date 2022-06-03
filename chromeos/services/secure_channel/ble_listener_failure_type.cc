// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_listener_failure_type.h"

#include "base/check_op.h"

namespace chromeos {

namespace secure_channel {

std::ostream& operator<<(std::ostream& stream,
                         const BleListenerFailureType& failure_type) {
  DCHECK_EQ(BleListenerFailureType::kAuthenticationError, failure_type);
  stream << "[authentication error]";
  return stream;
}

}  // namespace secure_channel

}  // namespace chromeos
