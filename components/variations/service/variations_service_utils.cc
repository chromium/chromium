// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_service_utils.h"

#include "base/build_time.h"
#include "base/time/time.h"

namespace variations {
namespace {

// Maximum age permitted for a variations seed, in days.
const int kMaxVariationsSeedAgeDays = 30;

// Helper function for "HasSeedExpiredSinceTime" that exposes |build_time| and
// makes it overridable by tests.
bool HasSeedExpiredSinceTimeHelper(base::Time fetch_time,
                                   base::Time build_time) {
  const base::TimeDelta seed_age = base::Time::Now() - fetch_time;
  // base::TimeDelta::InDays() rounds down to the nearest integer, so the seed
  // would not be considered expired if it is less than
  // `kMaxVariationsSeedAgeDays + 1`.
  return seed_age.InDays() > kMaxVariationsSeedAgeDays &&
         build_time > fetch_time;
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
