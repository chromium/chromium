// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_service_utils.h"

#include <string>

#include "base/build_time.h"
#include "base/time/time.h"
#include "components/country_codes/country_codes.h"
#include "components/variations/service/variations_service.h"

namespace variations {
namespace {

// Maximum age permitted for a variations seed, in days.
const int kMaxSeedAgeDays = 30;

// Helper function for "HasSeedExpiredSinceTime" that exposes |build_time| and
// makes it overridable by tests.
bool HasSeedExpiredSinceTimeHelper(base::Time fetch_time,
                                   base::Time build_time) {
  // TODO(crbug.com/40274989): Consider comparing the server-provided fetch time
  // with the network time.
  const base::TimeDelta seed_age = base::Time::Now() - fetch_time;

  // base::TimeDelta::InDays() rounds down to the nearest integer, so the seed
  // would not be considered expired if it is less than `kMaxSeedAgeDays + 1`.
  return seed_age.InDays() > kMaxSeedAgeDays && build_time > fetch_time;
}

}  // namespace

bool HasSeedExpiredSinceTime(base::Time fetch_time) {
  return HasSeedExpiredSinceTimeHelper(fetch_time, base::GetBuildTime());
}

bool HasSeedExpiredSinceTimeHelperForTesting(base::Time fetch_time,
                                             base::Time build_time) {
  return HasSeedExpiredSinceTimeHelper(fetch_time, build_time);
}

std::string GetCurrentCountryCode(const VariationsService* variations) {
  std::string country;

  if (variations) {
    country = variations->GetStoredPermanentCountry();
  }

  // Since variations doesn't provide a permanent country by default on things
  // like local builds, we try to fall back to the country_codes component which
  // should always have one.
  return country.empty() ? country_codes::GetCurrentCountryCode() : country;
}

}  // namespace variations
