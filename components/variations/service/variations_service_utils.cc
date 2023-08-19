// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_service_utils.h"

#include "base/build_time.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace variations {
namespace {

// Maximum age permitted for a variations seed, in days.
const int kMaxVariationsSeedAgeDays = 30;

// Emits to future-seed-related histograms.
void RecordFutureSeedMetrics(int seed_age_days) {
  // The seed age is the difference between base::Time::Now() and the
  // client-provided timestamp of when the seed was fetched. If the seed age is
  // negative, then client timestamp must be in the future.
  bool seed_has_future_age = seed_age_days < 0;
  base::UmaHistogramBoolean("Variations.HasFutureSeed", seed_has_future_age);
  if (seed_has_future_age) {
    base::UmaHistogramCounts1000("Variations.SeedFreshness.Future",
                                 -seed_age_days);
  }
}

// Helper function for "HasSeedExpiredSinceTime" that exposes |build_time| and
// makes it overridable by tests.
bool HasSeedExpiredSinceTimeHelper(base::Time fetch_time,
                                   base::Time build_time) {
  // TODO(crbug/1462588): Consider comparing the server-provided fetch time with
  // the network time.
  const base::TimeDelta seed_age = base::Time::Now() - fetch_time;
  const int seed_age_days = seed_age.InDays();
  RecordFutureSeedMetrics(seed_age_days);

  // base::TimeDelta::InDays() rounds down to the nearest integer, so the seed
  // would not be considered expired if it is less than
  // `kMaxVariationsSeedAgeDays + 1`.
  return seed_age_days > kMaxVariationsSeedAgeDays && build_time > fetch_time;
}

}  // namespace

bool HasSeedExpiredSinceTime(base::Time fetch_time) {
  return HasSeedExpiredSinceTimeHelper(fetch_time, base::GetBuildTime());
}

bool HasSeedExpiredSinceTimeHelperForTesting(base::Time fetch_time,
                                             base::Time build_time) {
  return HasSeedExpiredSinceTimeHelper(fetch_time, build_time);
}
}  // namespace variations
