// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SEED_SIMULATOR_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SEED_SIMULATOR_H_

#include "base/component_export.h"

namespace variations {

class EntropyProviders;
struct ClientFilterableState;
class VariationsSeed;

// The result of variations seed simulation, counting the number of experiment
// group changes of each type that are expected to occur on a restart with the
// seed.
struct COMPONENT_EXPORT(VARIATIONS) SeedSimulationResult {
  // The number of expected group changes that do not fall into any special
  // category. This is a lower bound due to session randomized studies.
  int normal_group_change_count = 0;

  // The number of expected group changes that fall in the category of killed
  // experiments that should trigger the "best effort" restart mechanism.
  int kill_best_effort_group_change_count = 0;

  // The number of expected group changes that fall in the category of killed
  // experiments that should trigger the "critical" restart mechanism.
  int kill_critical_group_change_count = 0;
};

// Computes differences between the current process' field trial state and
// the result of evaluating |seed| with the given parameters.
COMPONENT_EXPORT(VARIATIONS)
SeedSimulationResult SimulateSeedStudies(
    const VariationsSeed& seed,
    const ClientFilterableState& client_state,
    const EntropyProviders& entropy_providers);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SEED_SIMULATOR_H_
