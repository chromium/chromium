// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_TIMER_UTILS_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_TIMER_UTILS_H_

#include "base/time/time.h"

namespace enterprise_net {

// Default TTL applied when a Provisioning Domain configuration response lacks
// an explicit expiration timestamp.
inline constexpr base::TimeDelta kDefaultExpirationTtl = base::Hours(1);

// Minimum floor for proactive refresh delays to prevent tight refresh loops.
inline constexpr base::TimeDelta kMinRefreshDelay = base::Minutes(1);

// Ratio of TTL (80%) at which a proactive refresh is scheduled before
// expiration.
inline constexpr double kProactiveRefreshRatio = 0.8;

// Initial retry delay for transient network/HTTP failures (e.g. device wake or
// momentary server restart).
inline constexpr base::TimeDelta kTransientInitialRetryDelay =
    base::Seconds(15);

// Multiplier applied to transient retry delays on consecutive failures.
inline constexpr int kTransientBackoffFactor = 4;

// Maximum retry delay cap for transient failures (16 minutes).
inline constexpr base::TimeDelta kTransientMaxRetryDelay = base::Minutes(16);

// Maximum number of consecutive transient retry attempts before marking the
// domain as blocked (kFailedBlocked) and stopping automatic timer retries.
inline constexpr int kMaxTransientRetries = 5;

// Calculates the proactive refresh delay for a Provisioning Domain
// configuration based on its expiration timestamp and the current time.
// - If `expires` is null, uses `now + kDefaultExpirationTtl`.
// - Clamps the calculated delay to `kMinRefreshDelay`.
base::TimeDelta CalculateProactiveRefreshDelay(base::Time expires,
                                               base::Time now);

// Calculates the retry delay for transient failures with exponential backoff
// based on the consecutive failure count:
// - 1st failure  (consecutive_failures <= 1): 15s
// - 2nd failure  (consecutive_failures == 2): 60s (1m)
// - 3rd failure  (consecutive_failures == 3): 240s (4m)
// - 4th+ failure (consecutive_failures >= 4): 960s (16m cap)
base::TimeDelta CalculateTransientRetryDelay(int consecutive_failures);

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_TIMER_UTILS_H_
