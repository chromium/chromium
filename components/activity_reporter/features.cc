// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/activity_reporter/features.h"

#include <algorithm>

#include "base/feature.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/update_client/persisted_data.h"

namespace activity_reporter::features {

namespace {

// Rotating the ping freshness more than once a day would inflate active counts
// further, since the server may count a client again after each rotation.
constexpr base::TimeDelta kMinPingFreshnessRotationMaxAge = base::Hours(24);

// Keeps the reporter from sending one ping freshness value for more than four
// weeks.
constexpr base::TimeDelta kMaxPingFreshnessRotationMaxAge = base::Days(28);

}  // namespace

// Enables replacing the ping freshness before each report's update check if it
// is `GetPingFreshnessRotationMaxAge()` old or older, or if its write time is
// unknown or in the future.
// TODO(b/577188262): Remove the feature and its param, and their mentions in
// README.md and histograms.xml, once the rotation launches.
BASE_FEATURE(kActivityReporterPingFreshnessRotation,
             base::FEATURE_DISABLED_BY_DEFAULT);

// The age at which the reporter replaces the ping freshness. Read it through
// `GetPingFreshnessRotationMaxAge()`, which bounds it.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kPingFreshnessRotationMaxAge,
                   &kActivityReporterPingFreshnessRotation,
                   update_client::kPingFreshnessMaxAge);

base::TimeDelta GetPingFreshnessRotationMaxAge() {
  const base::TimeDelta max_age = kPingFreshnessRotationMaxAge.Get();
  if (!max_age.is_positive()) {
    return update_client::kPingFreshnessMaxAge;
  }
  return std::clamp(max_age, kMinPingFreshnessRotationMaxAge,
                    kMaxPingFreshnessRotationMaxAge);
}

}  // namespace activity_reporter::features
