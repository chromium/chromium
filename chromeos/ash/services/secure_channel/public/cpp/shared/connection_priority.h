// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_SHARED_CONNECTION_PRIORITY_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_SHARED_CONNECTION_PRIORITY_H_

#include <ostream>

namespace ash::secure_channel {

// Determines the order in which connections are attempted when system resources
// must be shared. For example, a device can only register a limited number of
// BLE advertisements at a given time due to hardware constraints; in this
// situation, a connection attempt with a higher priority will be allowed to
// register an advertisement before an attempt with a lower priority.
//
// For connection mediums which do not require use of limited system resources,
// ConnectionPriority is ignored.
enum class ConnectionPriority {
  // Should be used for connection attempts which do not have latency
  // requirements (e.g., background scans for nearby devices).
  kLow = 1,

  // Should be used when the connection attempt should complete in a reasonable
  // amount of time but is not urgent (e.g., heartbeat/keep-alive messages).
  kMedium = 2,

  // Should be used when the user is directly waiting on the result of the
  // connection (e.g., the user clicks a button and sees a spinner in the UI
  // until the connection succeeds).
  kHigh = 3
};

std::ostream& operator<<(std::ostream& stream,
                         const ConnectionPriority& connection_priority);

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_SHARED_CONNECTION_PRIORITY_H_
