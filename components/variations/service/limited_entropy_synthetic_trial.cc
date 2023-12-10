// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/variations/service/limited_entropy_synthetic_trial.h"

#include <cstdint>

#include "base/rand_util.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"

namespace variations {
namespace {

// The percentage of population that is enabled in this trial. It can be either
// 100 or an integer within [0, 50].
constexpr uint64_t kEnabledPercentage = 50;

std::string_view SelectGroup(PrefService* local_state) {
  static_assert((kEnabledPercentage >= 0 && kEnabledPercentage <= 50) ||
                kEnabledPercentage == 100);
  auto* seed_pref_name = prefs::kVariationsLimitedEntropySyntheticTrialSeed;
  if (!local_state->HasPrefPath(seed_pref_name)) {
    // base::RandGenerator(100) will return a number within [0, 100).
    local_state->SetUint64(seed_pref_name, base::RandGenerator(100));
  }
  auto rand_val = local_state->GetUint64(seed_pref_name);

  if (rand_val < kEnabledPercentage) {
    return kLimitedEntropySyntheticTrialEnabled;
  } else if (rand_val < 2 * kEnabledPercentage) {
    return kLimitedEntropySyntheticTrialControl;
  } else {
    return kLimitedEntropySyntheticTrialDefault;
  }
}

}  // namespace

LimitedEntropySyntheticTrial::LimitedEntropySyntheticTrial(
    PrefService* local_state)
    : group_name_(SelectGroup(local_state)) {}

LimitedEntropySyntheticTrial::~LimitedEntropySyntheticTrial() = default;

// static
void LimitedEntropySyntheticTrial::RegisterPrefs(PrefRegistrySimple* registry) {
  // The default value of 0 is a placeholder and will not be used.
  registry->RegisterUint64Pref(
      variations::prefs::kVariationsLimitedEntropySyntheticTrialSeed, 0);
}

bool LimitedEntropySyntheticTrial::IsEnabled() {
  return group_name_ == kLimitedEntropySyntheticTrialEnabled;
}

std::string_view LimitedEntropySyntheticTrial::GetGroupName() {
  return group_name_;
}

}  // namespace variations
