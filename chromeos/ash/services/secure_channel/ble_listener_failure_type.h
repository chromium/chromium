// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_LISTENER_FAILURE_TYPE_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_LISTENER_FAILURE_TYPE_H_

#include <ostream>

namespace ash::secure_channel {

enum class BleListenerFailureType {
  // A connection was formed successfully, but there was an error
  // authenticating the connection.
  kAuthenticationError,
};

std::ostream& operator<<(std::ostream& stream,
                         const BleListenerFailureType& failure_type);

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_LISTENER_FAILURE_TYPE_H_
