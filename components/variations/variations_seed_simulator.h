// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SEED_SIMULATOR_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SEED_SIMULATOR_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/version.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"

namespace variations {

class ProcessedStudy;
struct ClientFilterableState;
class VariationsSeed;

// VariationsSeedSimulator simulates the result of creating a set of studies
// and detecting which studies would result in group changes.
class VariationsSeedSimulator {
 public:
  // The result of variations seed simulation, counting the number of experiment
  // group changes of each type that are expected to occur on a restart with the
  // seed.
  struct Result {
    // The number of expected group changes that do not fall into any special
    // category. This is a lower bound due to session randomized studies.
    int normal_group_change_count;

    // The number of expected group changes that fall in the category of killed
    // experiments that should trigger the "best effort" restart mechanism.
    int kill_best_effort_group_change_count;

    // The number of expected group changes that fall in the category of killed
    // experiments that should trigger the "critical" restart mechanism.
    int kill_critical_group_change_count;

    Result();
    ~Result();
  };

  // Creates the simulator with the given default and low entropy providers. The
  // |low_entropy_provider| will be used for studies that should only use a low
  // entropy source. This is defined by
  // VariationsSeedProcessor::ShouldStudyUseLowEntropy, in
  // variations_seed_processor.h.
  VariationsSeedSimulator(
      const base::FieldTrial::EntropyProvider& default_entropy_provider,
      const base::FieldTrial::EntropyProvider& low_entropy_provider);
  virtual ~VariationsSeedSimulator();

  // Computes differences between the current process' field trial state and
  // the result of evaluating |seed| with the given parameters. Returns the
  // results of the simulation as a set of expected group change counts of each
  // type.
  Result SimulateSeedStudies(const VariationsSeed& seed,
                             const ClientFilterableState& client_state);

 private:
  friend class VariationsSeedSimulatorTest;

  enum ChangeType {
    NO_CHANGE,
    CHANGED,
    CHANGED_KILL_BEST_EFFORT,
    CHANGED_KILL_CRITICAL,
  };

  // Computes differences between the current process' field trial state and
  // the result of evaluating the |processed_studies| list. It is expected that
  // |processed_studies| have already been filtered and only contain studies
  // that apply to the configuration being simulated. Returns the results of the
  // simulation as a set of expected group change counts of each type.
  Result ComputeDifferences(
      const std::vector<ProcessedStudy>& processed_studies);

  // Maps proto enum |type| to a ChangeType.
  ChangeType ConvertExperimentTypeToChangeType(Study_Experiment_Type type);

  // For the given |processed_study| with PERMANENT consistency, simulates group
  // assignment and returns a corresponding ChangeType if the result differs
  // from that study's group in the current process.
  ChangeType PermanentStudyGroupChanged(const ProcessedStudy& processed_study,
                                        const std::string& selected_group);

  // For the given |processed_study| with SESSION consistency, determines if
  // there are enough changes in the study config that restarting will result
  // in a guaranteed different group assignment (or different params) and
  // returns the corresponding ChangeType.
  ChangeType SessionStudyGroupChanged(const ProcessedStudy& filtered_study,
                                      const std::string& selected_group);

  const base::FieldTrial::EntropyProvider& default_entropy_provider_;
  const base::FieldTrial::EntropyProvider& low_entropy_provider_;

  DISALLOW_COPY_AND_ASSIGN(VariationsSeedSimulator);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SEED_SIMULATOR_H_
