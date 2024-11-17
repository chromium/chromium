// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_simulator.h"

#include <stdint.h>

#include <map>

#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/processed_study.h"
#include "components/variations/proto/layer.pb.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

constexpr int kLimitedLayerId = 101;
constexpr int kLimitedLayerMemberId = 1001;

// Creates and activates a single-group field trial with name |trial_name| and
// group |group_name| and variations |params| (if not null).
void CreateTrial(const std::string& trial_name,
                 const std::string& group_name,
                 const std::map<std::string, std::string>* params) {
  base::FieldTrialList::CreateFieldTrial(trial_name, group_name);
  if (params != nullptr)
    base::AssociateFieldTrialParams(trial_name, group_name, *params);
  base::FieldTrialList::FindFullName(trial_name);
}

// Creates and activates a single-group field trial with name |trial_name| and
// group |group_name|.
void CreateTrial(const std::string& trial_name, const std::string& group_name) {
  return CreateTrial(trial_name, group_name, nullptr);
}

// Creates a study with the given |study_name| and |consistency|.
Study CreateStudy(const std::string& study_name,
                  Study_Consistency consistency) {
  Study study;
  study.set_name(study_name);
  study.set_consistency(consistency);
  return study;
}

// Creates a layer with the given |layer_id| with a single layer member using
// the given |layer_member_id|.
Layer CreateLimitedLayer(int layer_id, int layer_member_id) {
  Layer layer;
  layer.set_id(kLimitedLayerId);
  layer.set_num_slots(100);
  layer.set_entropy_mode(Layer_EntropyMode_LIMITED);

  Layer_LayerMember* layer_member = layer.add_members();
  layer_member->set_id(kLimitedLayerMemberId);
  Layer_LayerMember_SlotRange* slot = layer_member->add_slots();
  slot->set_start(0);
  slot->set_end(99);

  return layer;
}

// Adds an experiment to |study| with the specified |experiment_name| and
// |probability| values and sets it as the study's default experiment.
Study_Experiment* AddExperiment(const std::string& experiment_name,
                                int probability,
                                Study* study) {
  Study_Experiment* experiment = study->add_experiment();
  experiment->set_name(experiment_name);
  experiment->set_probability_weight(probability);
  study->set_default_experiment_name(experiment_name);
  return experiment;
}

// Add an experiment param with |param_name| and |param_value| to |experiment|.
Study_Experiment_Param* AddExperimentParam(const std::string& param_name,
                                           const std::string& param_value,
                                           Study_Experiment* experiment) {
  Study_Experiment_Param* param = experiment->add_param();
  param->set_name(param_name);
  param->set_value(param_value);
  return param;
}

// Adds a LayerMemberReference with the given IDs to |study|.
void ConstrainToLayer(Study& study, int layer_id, int layer_member_id) {
  LayerMemberReference layer_member_reference;
  layer_member_reference.set_layer_id(layer_id);
  layer_member_reference.add_layer_member_ids(layer_member_id);
  *study.mutable_layer() = layer_member_reference;
}

}  // namespace

class VariationsSeedSimulatorTest : public ::testing::Test {
 public:
  VariationsSeedSimulatorTest() = default;

  VariationsSeedSimulatorTest(const VariationsSeedSimulatorTest&) = delete;
  VariationsSeedSimulatorTest& operator=(const VariationsSeedSimulatorTest&) =
      delete;

  ~VariationsSeedSimulatorTest() override {
    // Ensure that the maps are cleared between tests, since they are stored as
    // process singletons.
    testing::ClearAllVariationIDs();
    testing::ClearAllVariationParams();
  }

  // Simulates the differences between |seed|'s studies and the current field
  // trial state, returning a string like "1 2 3", where 1 is the number of
  // regular group changes, 2 is the number of "kill best effort" group changes
  // and 3 is the number of "kill critical" group changes.
  std::string SimulateStudyDifferences(const VariationsSeed& seed) {
    auto client_state = CreateDummyClientFilterableState();
    MockEntropyProviders entropy_providers({
        .low_entropy = kAlwaysUseLastGroup,
        .high_entropy = kAlwaysUseFirstGroup,
        .limited_entropy = kAlwaysUseLastGroup,
    });
    return ConvertSimulationResultToString(
        SimulateSeedStudies(seed, *client_state, entropy_providers));
  }

  // Formats |result| as a string with format "1 2 3", where 1 is the number of
  // regular group changes, 2 is the number of "kill best effort" group changes
  // and 3 is the number of "kill critical" group changes.
  std::string ConvertSimulationResultToString(
      const SeedSimulationResult& result) {
    return base::StringPrintf("%d %d %d",
                              result.normal_group_change_count,
                              result.kill_best_effort_group_change_count,
                              result.kill_critical_group_change_count);
  }
};

TEST_F(VariationsSeedSimulatorTest, PermanentNoChanges) {
  CreateTrial("A", "B");

  VariationsSeed seed;
  *seed.add_study() = CreateStudy("A", Study_Consistency_PERMANENT);
  Study* study = seed.mutable_study(0);
  Study_Experiment* experiment = AddExperiment("B", 100, study);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));

  experiment->set_type(Study_Experiment_Type_NORMAL);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_IGNORE_CHANGE);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_KILL_BEST_EFFORT);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_KILL_CRITICAL);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));
}

TEST_F(VariationsSeedSimulatorTest, PermanentGroupChange) {
  CreateTrial("A", "B");

  VariationsSeed seed;
  *seed.add_study() = CreateStudy("A", Study_Consistency_PERMANENT);
  Study* study = seed.mutable_study(0);
  Study_Experiment* experiment = AddExperiment("C", 100, study);

  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));

  // Changing "C" group type should not affect the type of change. (Since the
  // type is evaluated for the "old" group.)
  //
  // Note: The current (i.e. old) group is checked for the type since that group
  // is the one that should be annotated with the type when killing it.
  experiment->set_type(Study_Experiment_Type_NORMAL);
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_IGNORE_CHANGE);
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_KILL_BEST_EFFORT);
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_KILL_CRITICAL);
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));
}

// The seed simulator should be able to detect group changes when a permanently
// consistent study constrained to a limited layer is configured with a
// different experiment.
TEST_F(VariationsSeedSimulatorTest,
       PermanentGroupChange_StudyConstrainedToLimitedLayer) {
  CreateTrial("A", "B");

  VariationsSeed seed;
  *seed.add_study() = CreateStudy("A", Study_Consistency_PERMANENT);
  Study* study = seed.mutable_study(0);

  // Adding an experiment with |google_web_experiment_id| so that the limited
  // entropy provider can be used.
  Study_Experiment* experiment = AddExperiment("C", 100, study);
  experiment->set_google_web_experiment_id(1234);

  // When a study with an experiment that's associated with a
  // |google_web_experiment_id| is constrained to a layer with
  // |EntropyMode.LIMITED|, it will be randomized with the limited entropy
  // provider.
  Layer limited_layer =
      CreateLimitedLayer(kLimitedLayerId, kLimitedLayerMemberId);
  ConstrainToLayer(*study, kLimitedLayerId, kLimitedLayerMemberId);
  *seed.add_layers() = limited_layer;

  // There is precisely one change since for trial "A", group "C" instead of
  // group "B" is now selected, and this change is a normal since it's a
  // group-only change.
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));

  // Changing "C" group type should not affect the type of change. This is the
  // same behavior as randomizing with other entropy providers.
  experiment->set_type(Study_Experiment_Type_NORMAL);
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));

  experiment->set_type(Study_Experiment_Type_IGNORE_CHANGE);
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));

  experiment->set_type(Study_Experiment_Type_KILL_BEST_EFFORT);
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));

  experiment->set_type(Study_Experiment_Type_KILL_CRITICAL);
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));
}

TEST_F(VariationsSeedSimulatorTest, PermanentGroupChangeDueToExperimentID) {
  CreateTrial("A", "B");

  VariationsSeed seed;
  *seed.add_study() = CreateStudy("A", Study_Consistency_PERMANENT);
  Study* study = seed.mutable_study(0);
  Study_Experiment* experiment_b = AddExperiment("B", 50, study);
  AddExperiment("Default", 50, study);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));

  // Adding a google_web_experiment_id will cause the low entropy provider to be
  // used, causing a group change.
  experiment_b->set_google_web_experiment_id(1234);
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));
}

// The seed simulator should be able to detect group changes when a permanently
// consistent study constrained to a limited layer is given a
// |google_web_experiment_id|.
TEST_F(VariationsSeedSimulatorTest,
       PermanentGroupChangeDueToExperimentID_StudyConstrainedToLimitedLayer) {
  CreateTrial("A", "B");

  VariationsSeed seed;
  *seed.add_study() = CreateStudy("A", Study_Consistency_PERMANENT);
  Study* study = seed.mutable_study(0);

  Study_Experiment* experiment_b = AddExperiment("B", 50, study);
  AddExperiment("Default", 50, study);
  Layer limited_layer =
      CreateLimitedLayer(kLimitedLayerId, kLimitedLayerMemberId);
  ConstrainToLayer(*study, kLimitedLayerId, kLimitedLayerMemberId);
  *seed.add_layers() = limited_layer;

  // Upon receiving the seed, the client should use the limited entropy provider
  // to randomize this study since it is constrained to the |limited_layer|. The
  // entropy provider will select "Default" (from |kAlwaysUseLastGroup|). Thus
  // the group change.
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));

  // The limited entropy provider should still be used if the study is
  // associated with a Google web experiment ID. Therefore the group assignment
  // is the same as above.
  experiment_b->set_google_web_experiment_id(1234);
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));
}

TEST_F(VariationsSeedSimulatorTest,
       PermanentGroupChangeDueToExperimentID_NoLimitedSource) {
  CreateTrial("A", "B");

  VariationsSeed seed;
  *seed.add_study() = CreateStudy("A", Study_Consistency_PERMANENT);
  Study* study = seed.mutable_study(0);
  AddExperiment("B", 50, study);
  AddExperiment("Default", 50, study);
  Layer limited_layer =
      CreateLimitedLayer(kLimitedLayerId, kLimitedLayerMemberId);
  ConstrainToLayer(*study, kLimitedLayerId, kLimitedLayerMemberId);
  *seed.add_layers() = limited_layer;

  auto client_state = CreateDummyClientFilterableState();
  EntropyProviders entropy_providers(
      "high_entropy_value", {0, 1},
      /*limited_entropy_value=*/std::string_view());

  auto result = ConvertSimulationResultToString(
      SimulateSeedStudies(seed, *client_state, entropy_providers));

  // Without a limited entropy randomization source, the study that requires the
  // limited entropy provider is not assigned and therefore is skipped in the
  // seed simulator.
  EXPECT_EQ("0 0 0", result);
}

TEST_F(VariationsSeedSimulatorTest, SessionRandomized) {
  CreateTrial("A", "B");

  VariationsSeed seed;
  *seed.add_study() = CreateStudy("A", Study_Consistency_SESSION);
  Study* study = seed.mutable_study(0);
  Study_Experiment* experiment = AddExperiment("B", 1, study);
  AddExperiment("C", 1, study);
  AddExperiment("D", 1, study);

  // There should be no differences, since a session randomized study can result
  // in any of the groups being chosen on startup.
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));

  experiment->set_type(Study_Experiment_Type_NORMAL);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_IGNORE_CHANGE);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_KILL_BEST_EFFORT);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_KILL_CRITICAL);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));
}

TEST_F(VariationsSeedSimulatorTest, SessionRandomizedGroupRemoved) {
  CreateTrial("A", "B");

  VariationsSeed seed;
  *seed.add_study() = CreateStudy("A", Study_Consistency_SESSION);
  Study* study = seed.mutable_study(0);
  AddExperiment("C", 1, study);
  AddExperiment("D", 1, study);

  // There should be a difference since there is no group "B" in the new config.
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));
}

TEST_F(VariationsSeedSimulatorTest, SessionRandomizedGroupProbabilityZero) {
  CreateTrial("A", "B");

  VariationsSeed seed;
  *seed.add_study() = CreateStudy("A", Study_Consistency_SESSION);
  Study* study = seed.mutable_study(0);
  Study_Experiment* experiment = AddExperiment("B", 0, study);
  AddExperiment("C", 1, study);
  AddExperiment("D", 1, study);

  // There should be a difference since group "B" has probability 0.
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));

  experiment->set_type(Study_Experiment_Type_NORMAL);
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_IGNORE_CHANGE);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_KILL_BEST_EFFORT);
  EXPECT_EQ("0 1 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_KILL_CRITICAL);
  EXPECT_EQ("0 0 1", SimulateStudyDifferences(seed));
}

TEST_F(VariationsSeedSimulatorTest, ParamsUnchanged) {
  std::map<std::string, std::string> params;
  params["p1"] = "x";
  params["p2"] = "y";
  params["p3"] = "z";
  CreateTrial("A", "B", &params);

  VariationsSeed seed;
  *seed.add_study() = CreateStudy("A", Study_Consistency_PERMANENT);
  Study* study = seed.mutable_study(0);
  Study_Experiment* experiment = AddExperiment("B", 100, study);
  AddExperimentParam("p2", "y", experiment);
  AddExperimentParam("p1", "x", experiment);
  AddExperimentParam("p3", "z", experiment);

  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));

  experiment->set_type(Study_Experiment_Type_NORMAL);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_IGNORE_CHANGE);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_KILL_BEST_EFFORT);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_KILL_CRITICAL);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));
}

TEST_F(VariationsSeedSimulatorTest, ParamsChanged) {
  std::map<std::string, std::string> params;
  params["p1"] = "x";
  params["p2"] = "y";
  params["p3"] = "z";
  CreateTrial("A", "B", &params);

  VariationsSeed seed;
  *seed.add_study() = CreateStudy("A", Study_Consistency_PERMANENT);
  Study* study = seed.mutable_study(0);
  Study_Experiment* experiment = AddExperiment("B", 100, study);
  AddExperimentParam("p2", "test", experiment);
  AddExperimentParam("p1", "x", experiment);
  AddExperimentParam("p3", "z", experiment);

  // The param lists differ.
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));

  experiment->set_type(Study_Experiment_Type_NORMAL);
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_IGNORE_CHANGE);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_KILL_BEST_EFFORT);
  EXPECT_EQ("0 1 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_KILL_CRITICAL);
  EXPECT_EQ("0 0 1", SimulateStudyDifferences(seed));
}

TEST_F(VariationsSeedSimulatorTest, ParamsRemoved) {
  std::map<std::string, std::string> params;
  params["p1"] = "x";
  params["p2"] = "y";
  params["p3"] = "z";
  CreateTrial("A", "B", &params);

  VariationsSeed seed;
  *seed.add_study() = CreateStudy("A", Study_Consistency_PERMANENT);
  Study* study = seed.mutable_study(0);
  Study_Experiment* experiment = AddExperiment("B", 100, study);

  // The current group has params, but the new config doesn't have any.
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));

  experiment->set_type(Study_Experiment_Type_NORMAL);
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_IGNORE_CHANGE);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_KILL_BEST_EFFORT);
  EXPECT_EQ("0 1 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_KILL_CRITICAL);
  EXPECT_EQ("0 0 1", SimulateStudyDifferences(seed));
}

TEST_F(VariationsSeedSimulatorTest, ParamsAdded) {
  CreateTrial("A", "B");

  VariationsSeed seed;
  *seed.add_study() = CreateStudy("A", Study_Consistency_PERMANENT);
  Study* study = seed.mutable_study(0);
  Study_Experiment* experiment = AddExperiment("B", 100, study);
  AddExperimentParam("p2", "y", experiment);
  AddExperimentParam("p1", "x", experiment);
  AddExperimentParam("p3", "z", experiment);

  // The current group has no params, but the config has added some.
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));

  experiment->set_type(Study_Experiment_Type_NORMAL);
  EXPECT_EQ("1 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_IGNORE_CHANGE);
  EXPECT_EQ("0 0 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_KILL_BEST_EFFORT);
  EXPECT_EQ("0 1 0", SimulateStudyDifferences(seed));
  experiment->set_type(Study_Experiment_Type_KILL_CRITICAL);
  EXPECT_EQ("0 0 1", SimulateStudyDifferences(seed));
}

}  // namespace variations
