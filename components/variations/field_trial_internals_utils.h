// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_FIELD_TRIAL_INTERNALS_UTILS_H_
#define COMPONENTS_VARIATIONS_FIELD_TRIAL_INTERNALS_UTILS_H_

#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/metrics/field_trial.h"
#include "base/time/time.h"

class PrefService;
class PrefRegistrySimple;

namespace variations {

struct ClientFilterableState;
class Study;
class VariationsSeed;
class EntropyProviders;

// This file supports temporary forcing of field trials through the
// chrome://field-trial-internals page.

// Forced trial expiration:
// When field trials are forced with field-trial-internals, they remain active
// for a limited time. After 3 days, or 3 Chrome restarts, forced field trials
// are disabled. Note however that forced trials have their expirations
// refreshed if the field-trial-internals page is visited again before
// they are expired.
// In summary, all field trial overrides share the same expiration, and that
// expiration is always 3 days / restarts when visiting the
// field-trial-internals page.

// Upon restart, we check whether the forced trials should
// be honored.
// How long before a temporary field trial expires.
inline constexpr base::TimeDelta kManualForceFieldTrialDuration = base::Days(3);
// How many Chrome restarts before the temporary field trial override expires.
inline constexpr int kChromeStartCountBeforeResetForcedFieldTrials = 3;

// The name of a study and its groups.
struct COMPONENT_EXPORT(VARIATIONS) StudyGroupNames {
  StudyGroupNames();
  explicit StudyGroupNames(const Study& study);
  ~StudyGroupNames();
  StudyGroupNames(const StudyGroupNames&);
  StudyGroupNames& operator=(const StudyGroupNames&);

  // Name of the study.
  std::string name;
  // Name of the groups in the study.
  std::vector<std::string> groups;
};

COMPONENT_EXPORT(VARIATIONS)
void RegisterFieldTrialInternalsPrefs(PrefRegistrySimple& registry);

// Called at startup to override field trials which were overridden with
// `SetTemporaryTrialOverrides`.
COMPONENT_EXPORT(VARIATIONS)
void ForceTrialsAtStartup(PrefService& prefs);

// Sets the list of field trials which will be enabled by override upon the next
// restart. Returns false if the current set of trial overrides already in
// effect match `override_groups`, and therefore no restart is necessary.
COMPONENT_EXPORT(VARIATIONS)
bool SetTemporaryTrialOverrides(
    PrefService& local_state,
    base::span<std::pair<std::string, std::string>> override_groups);

// Returns the set of unexpired field trial overrides. If any exist, their
// expiration is reset. `requires_restart` is set to whether overrides have
// changed since the last restart, and therefore are not yet in effect.
// This is used when populating chrome://field-trial-internals.
COMPONENT_EXPORT(VARIATIONS)
base::flat_map<std::string, std::string> RefreshAndGetFieldTrialOverrides(
    const std::vector<variations::StudyGroupNames>& available_studies,
    PrefService& local_state,
    bool& requires_restart);

// Returns the study and groups that could potentially be forced.
// Due to a deficiency in how studies are forced, studies which use layers
// may not be forcable, and therefore not returned here. If the layer member
// associated with the study is active, it will be available to force.
COMPONENT_EXPORT(VARIATIONS)
std::vector<StudyGroupNames> GetStudiesAvailableToForce(
    VariationsSeed seed,
    const EntropyProviders& entropy_providers,
    const ClientFilterableState& client_filterable_state);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_FIELD_TRIAL_INTERNALS_UTILS_H_
