// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_LIMITED_ENTROPY_SYNTHETIC_TRIAL_H_
#define COMPONENTS_VARIATIONS_SERVICE_LIMITED_ENTROPY_SYNTHETIC_TRIAL_H_

#include <string>

#include "base/version_info/channel.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/variations/synthetic_trial_registry.h"

class PrefService;

namespace variations {

inline constexpr char kLimitedEntropySyntheticTrialName[] =
    "LimitedEntropySyntheticTrial";
inline constexpr char kLimitedEntropySyntheticTrialEnabled[] = "Enabled";
inline constexpr char kLimitedEntropySyntheticTrialControl[] = "Control";
inline constexpr char kLimitedEntropySyntheticTrialDefault[] = "Default";

inline constexpr char kIsLimitedEntropySyntheticTrialSeedValidHistogram[] =
    "Variations.LimitedEntropyTrial.AshSeedIsValid.OnSyncToLacros";

class LimitedEntropySyntheticTrial {
 public:
  explicit LimitedEntropySyntheticTrial(PrefService* local_state,
                                        version_info::Channel channel);

  LimitedEntropySyntheticTrial(const LimitedEntropySyntheticTrial&) = delete;
  LimitedEntropySyntheticTrial& operator=(const LimitedEntropySyntheticTrial&) =
      delete;
  ~LimitedEntropySyntheticTrial();

  // Registers the prefs needed for this trial.
  static void RegisterPrefs(PrefRegistrySimple* registry);

#if BUILDFLAG(IS_CHROMEOS)
  // Overrides the seed of this trial with the value used in Ash chrome. Note
  // this method needs to be called before instantiation of the trial for the
  // seed to take effect. This should only be used by the Lacros client.
  static void SetSeedFromAsh(PrefService* local_state, uint64_t seed);

  // Returns the randomization seed of this trial. This should only be used by
  // the Ash Chrome client when sending the seed to Lacros, or in tests.
  uint64_t GetRandomizationSeed(PrefService* local_state);
#endif

  // Returns whether the client is in the enabled group for this trial.
  bool IsEnabled();

  // Returns the name of the group that the client belongs to for this trial.
  std::string_view GetGroupName();

  // Registers the group membership of this trial to the given
  // `synthetic_trial_registry`.
  void Register(SyntheticTrialRegistry& synthetic_trial_registry);

 protected:
  // Testing only. Provides a convenient way to instantiate a trial with the
  // given group assignment.
  LimitedEntropySyntheticTrial(std::string_view group_name);

 private:
  const std::string_view group_name_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_LIMITED_ENTROPY_SYNTHETIC_TRIAL_H_
