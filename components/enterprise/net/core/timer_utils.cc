// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/timer_utils.h"

#include <algorithm>

namespace enterprise_net {

base::TimeDelta CalculateProactiveRefreshDelay(base::Time expires,
                                               base::Time now) {
  base::Time target_expires = expires;
  if (target_expires.is_null()) {
    target_expires = now + kDefaultExpirationTtl;
  }

  base::TimeDelta ttl = target_expires - now;
  if (ttl <= base::TimeDelta()) {
    return kMinRefreshDelay;
  }

  base::TimeDelta raw_delay = ttl * kProactiveRefreshRatio;
  return std::max(raw_delay, kMinRefreshDelay);
}

base::TimeDelta CalculateTransientRetryDelay(int consecutive_failures) {
  if (consecutive_failures <= 1) {
    return kTransientInitialRetryDelay;
  }
  // Clamp exponent to prevent arithmetic overflow.
  int exponent = std::min(consecutive_failures - 1, 6);
  int64_t multiplier = 1;
  for (int i = 0; i < exponent; ++i) {
    multiplier *= kTransientBackoffFactor;
  }
  base::TimeDelta delay = kTransientInitialRetryDelay * multiplier;
  return std::min(delay, kTransientMaxRetryDelay);
}

}  // namespace enterprise_net
