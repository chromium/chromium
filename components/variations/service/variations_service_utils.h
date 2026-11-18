// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_UTILS_H_
#define COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_UTILS_H_

#include <string>

#include "components/variations/variations_ids_provider.h"

namespace base {
class Time;
}  // namespace base

namespace network_time {
class NetworkTimeTracker;
}  // namespace network_time

namespace variations {
class VariationsService;

// Returns whether a seed fetched at |fetch_time| has expired.
bool HasSeedExpiredSinceTime(base::Time fetch_time);

bool HasSeedExpiredSinceTimeHelperForTesting(base::Time fetch_time,
                                             base::Time build_time);

// Gets the user's current country code. If access through variations fails, the
// country_codes component is used. `variations` can be null in test.
std::string GetCurrentCountryCode(const VariationsService* variations);

// Creates the global `VariationsIdsProvider` with a `VariationsNetworkClock`.
// `network_time_tracker` must be non-null.
VariationsIdsProvider* CreateVariationsIdsProvider(
    VariationsIdsProvider::Mode mode,
    network_time::NetworkTimeTracker* network_time_tracker);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_UTILS_H_
