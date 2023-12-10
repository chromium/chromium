// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_LIMITED_ENTROPY_SYNTHETIC_TRIAL_H_
#define COMPONENTS_VARIATIONS_SERVICE_LIMITED_ENTROPY_SYNTHETIC_TRIAL_H_

#include <string>

#include "components/prefs/pref_registry_simple.h"

class PrefService;

namespace variations {

inline constexpr char kLimitedEntropySyntheticTrialName[] =
    "LimitedEntropySyntheticTrial";
inline constexpr char kLimitedEntropySyntheticTrialEnabled[] = "Enabled";
inline constexpr char kLimitedEntropySyntheticTrialControl[] = "Control";
inline constexpr char kLimitedEntropySyntheticTrialDefault[] = "Default";

class LimitedEntropySyntheticTrial {
 public:
  explicit LimitedEntropySyntheticTrial(PrefService* local_state);

  LimitedEntropySyntheticTrial(const LimitedEntropySyntheticTrial&) = delete;
  LimitedEntropySyntheticTrial& operator=(const LimitedEntropySyntheticTrial&) =
      delete;
  ~LimitedEntropySyntheticTrial();

  // Registers the prefs needed for this trial.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns whether the client is in the enabled group for this trial.
  bool IsEnabled();

  // Returns the name of the group that the client belongs to for this trial.
  std::string_view GetGroupName();

 private:
  const std::string_view group_name_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_LIMITED_ENTROPY_SYNTHETIC_TRIAL_H_
