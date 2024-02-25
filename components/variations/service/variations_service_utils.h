// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_UTILS_H_
#define COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_UTILS_H_

#include <string>

namespace base {
class Time;
}  // namespace base

namespace variations {
class VariationsService;

// Returns whether a seed fetched at |fetch_time| has expired.
bool HasSeedExpiredSinceTime(base::Time fetch_time);

bool HasSeedExpiredSinceTimeHelperForTesting(base::Time fetch_time,
                                             base::Time build_time);

// Gets the user's current country code. If access through variations fails, the
// country_codes component is used. `variations` can be null in test.
std::string GetCurrentCountryCode(const VariationsService* variations);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_UTILS_H_
