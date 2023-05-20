// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_RATE_LIMITER_INTERFACE_H_
#define COMPONENTS_REPORTING_UTIL_RATE_LIMITER_INTERFACE_H_

#include <cstddef>

namespace reporting {

// Rate limiter expects the subclass to implement actual state and use it by
// calling `Acquire` to decide whether the event of `event_size` bytes can be
// posted, and if it can, update the state accordingly.
// Rate limiter instance is owned by the caller.
// Thread-unsafe, `Acquire` method needs to be called on sequence.
class RateLimiterInterface {
 public:
  RateLimiterInterface(const RateLimiterInterface&) = delete;
  RateLimiterInterface& operator=(const RateLimiterInterface&) = delete;

  virtual ~RateLimiterInterface() = default;

  // If the event is allowed, the method returns `true` and updates state to
  // prepare for the next call. Otherwise returns false.
  virtual bool Acquire(size_t event_size) = 0;

 protected:
  RateLimiterInterface() = default;
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_RATE_LIMITER_INTERFACE_H_
