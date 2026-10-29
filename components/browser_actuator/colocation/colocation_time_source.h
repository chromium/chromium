// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_COLOCATION_COLOCATION_TIME_SOURCE_H_
#define COMPONENTS_BROWSER_ACTUATOR_COLOCATION_COLOCATION_TIME_SOURCE_H_

#include "base/time/time.h"

namespace base {
class Clock;
}  // namespace base

namespace network_time {
class NetworkTimeTracker;
}  // namespace network_time

namespace browser_actuator {

// Timestamp source for the co-location proof; see colocation_proof_builder.h
// for what the timestamp ends up in.
//
// Returns the current network time when `tracker` is non-null and has a valid
// network time sync (`NETWORK_TIME_AVAILABLE`), or `clock->Now()` otherwise.
// `clock` must be non-null. Must be called on the sequence that owns `tracker`.
base::Time GetColocationProofTime(network_time::NetworkTimeTracker* tracker,
                                  const base::Clock* clock);

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_COLOCATION_COLOCATION_TIME_SOURCE_H_
