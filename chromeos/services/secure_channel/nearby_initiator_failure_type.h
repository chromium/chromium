// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_NEARBY_INITIATOR_FAILURE_TYPE_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_NEARBY_INITIATOR_FAILURE_TYPE_H_

#include <ostream>

namespace chromeos {

namespace secure_channel {

enum class NearbyInitiatorFailureType {
  // Could not detect a device nearby to initiate a connection within the
  // timeout period.
  kTimeoutDiscoveringDevice,

  // An API call to Nearby Connections failed.
  kNearbyApiError,

  // The remote device rejected the connection attempt.
  kConnectionRejected,

  // Bluetooth or WebRTC connection failed.
  kConnectivityError,

  // A connection was formed successfully, but there was an error
  // authenticating the connection.
  kAuthenticationError,
};

std::ostream& operator<<(std::ostream& stream,
                         const NearbyInitiatorFailureType& failure_type);

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_NEARBY_INITIATOR_FAILURE_TYPE_H_
