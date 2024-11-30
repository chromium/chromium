// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/variations/service/limited_entropy_synthetic_trial.h"

#include <cstdint>

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/version_info/channel.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/synthetic_trials.h"

namespace variations {
namespace {

// The percentage of population that is enabled in this trial. It can be either
// 100 or an integer within [0, 50]. `kStableEnabledPercentage` specifies this
// percentage in the stable channel, and `kNonStableEnabledPercentage` is for
// other channels including `Channel::UNKNOWN`.
constexpr uint64_t kStableEnabledPercentage = 100;
constexpr uint64_t kNonStableEnabledPercentage = 100;

uint64_t SelectEnabledPercentage(version_info::Channel channel) {
  return channel == version_info::Channel::STABLE ? kStableEnabledPercentage
                                                  : kNonStableEnabledPercentage;
}

bool IsValidTrialSeed(uint64_t seed) {
  return seed > 0 && seed <= 100;
}

uint64_t GenerateTrialSeed() {
  // base::RandGenerator(100) will return a number within [0, 100). Adding one
  // to avoid 0 being a valid value since 0 might be a default uint64 value.
  auto seed = base::RandGenerator(100) + 1;
  CHECK(IsValidTrialSeed(seed));
  return seed;
}

constexpr bool IsValidEnabledPercentage(uint64_t percentage) {
  return (percentage >= 0 && percentage <= 50) || percentage == 100;
}

std::string_view SelectGroup(PrefService* local_state,
                             version_info::Channel channel) {
  static_assert(IsValidEnabledPercentage(kStableEnabledPercentage) &&
                IsValidEnabledPercentage(kNonStableEnabledPercentage));
  uint64_t enabled_percentage = SelectEnabledPercentage(channel);

  auto* seed_pref_name = prefs::kVariationsLimitedEntropySyntheticTrialSeed;
  if (!local_state->HasPrefPath(seed_pref_name)) {
    local_state->SetUint64(seed_pref_name, GenerateTrialSeed());
  }
  auto rand_val = local_state->GetUint64(seed_pref_name);

  if (rand_val <= enabled_percentage) {
    return kLimitedEntropySyntheticTrialEnabled;
  } else if (rand_val <= 2 * enabled_percentage) {
    return kLimitedEntropySyntheticTrialControl;
  } else {
    return kLimitedEntropySyntheticTrialDefault;
  }
}

}  // namespace

LimitedEntropySyntheticTrial::LimitedEntropySyntheticTrial(
    PrefService* local_state,
    version_info::Channel channel)
    : group_name_(SelectGroup(local_state, channel)) {}

LimitedEntropySyntheticTrial::LimitedEntropySyntheticTrial(
    std::string_view group_name)
    : group_name_(group_name) {}

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

void LimitedEntropySyntheticTrial::Register(
    SyntheticTrialRegistry& synthetic_trial_registry) {
  SyntheticTrialGroup trial_group(kLimitedEntropySyntheticTrialName,
                                  GetGroupName(),
                                  SyntheticTrialAnnotationMode::kCurrentLog);
  synthetic_trial_registry.RegisterSyntheticFieldTrial(trial_group);
}

}  // namespace variations
