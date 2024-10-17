// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/limited_layer_entropy_cost_tracker.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/limited_entropy_mode_gate.h"
#include "components/variations/proto/layer.pb.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_layers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

inline constexpr uint32_t kTestLayerId = 1001;
inline constexpr int kTestLayerMemberId = 2001;
inline constexpr char kTestClientId[] = "test_client_id";

// The following value ensures slot 0 is selected  (among 100 slots) when the
// limited entropy provider is used.
inline constexpr char kTestLimitedEntropyRandomizationSource[] =
    "limited_entropy_randomization_source_964";

Study::Experiment CreateExperiment(int weight) {
  Study::Experiment experiment;
  experiment.set_probability_weight(weight);
  return experiment;
}

Study::Experiment CreateGoogleWebExperiment(int weight,
                                            int google_web_experiment_id) {
  auto experiment = CreateExperiment(weight);
  experiment.set_google_web_experiment_id(google_web_experiment_id);
  return experiment;
}

Study::Experiment CreateTriggerExperiment(
    int weight,
    int google_web_trigger_experiment_id) {
  auto experiment = CreateExperiment(weight);
  experiment.set_google_web_trigger_experiment_id(
      google_web_trigger_experiment_id);
  return experiment;
}

std::vector<Study::Experiment> CreateExperimentsWithTwoBitsOfEntropy() {
  // Create 3 experiments with a total probability weight of 100. Only the 25%
  // group has a google_web_experiment_id so the entropy used should be
  // -log2(1/4) = 2.
  std::vector<Study::Experiment> experiments = {
      CreateExperiment(10), CreateGoogleWebExperiment(25, 100001),
      CreateExperiment(65)};
  return experiments;
}

LayerMemberReference CreateLayerMemberReference(
    uint32_t layer_id,
    const std::vector<uint32_t>& layer_member_ids) {
  LayerMemberReference layer_member_reference;
  layer_member_reference.set_layer_id(layer_id);
  for (uint32_t layer_member_id : layer_member_ids) {
    layer_member_reference.add_layer_member_ids(layer_member_id);
  }
  return layer_member_reference;
}

// Creates a test study.
Study CreateTestStudy(const std::vector<Study::Experiment>& experiments) {
  Study study;
  study.set_name("test_study");

  std::vector<Study::Experiment> copied_experiments(experiments);
  for (size_t i = 0; i < copied_experiments.size(); ++i) {
    copied_experiments[i].set_name(
        base::StrCat({"test_experiment_", base::NumberToString(i)}));
    Study_Experiment* experiment_to_add = study.add_experiment();
    experiment_to_add->MergeFrom(copied_experiments[i]);
  }

  return study;
}

Study CreateTestStudy(const std::vector<Study::Experiment>& experiments,
                      const LayerMemberReference& layer_member_reference) {
  auto study = CreateTestStudy(experiments);
  auto* layer_member_reference_to_add = study.mutable_layer();
  layer_member_reference_to_add->MergeFrom(layer_member_reference);
  return study;
}

// Creates a test layer member with slot ranges given as <start, end> pairs.
Layer::LayerMember CreateLayerMember(
    int layer_member_id,
    const std::vector<std::pair<int, int>>& slot_ranges) {
  Layer::LayerMember layer_member;
  layer_member.set_id(layer_member_id);

  for (const auto& slot_range : slot_ranges) {
    auto* slot = layer_member.add_slots();
    slot->set_start(slot_range.first);
    slot->set_end(slot_range.second);
  }
  return layer_member;
}

Layer CreateLayer(int layer_id,
                  int num_slots,
                  Layer::EntropyMode entropy_mode,
                  const std::vector<Layer::LayerMember>& layer_members) {
  Layer layer;
  layer.set_id(layer_id);
  layer.set_num_slots(num_slots);
  layer.set_entropy_mode(entropy_mode);

  for (const auto& layer_member : layer_members) {
    auto* member_to_add = layer.add_members();
    member_to_add->MergeFrom(layer_member);
  }

  return layer;
}

VariationsSeed CreateTestSeed(const std::vector<Layer>& layers,
                              const std::vector<Study>& studies) {
  VariationsSeed seed;

  for (const auto& study : studies) {
    auto* study_to_add = seed.add_study();
    study_to_add->MergeFrom(study);
  }

  for (const auto& layer : layers) {
    auto* layer_to_add = seed.add_layers();
    layer_to_add->MergeFrom(layer);
  }

  return seed;
}

}  // namespace

class LimitedLayerEntropyCostTrackerTest : public ::testing::Test {
 public:
  LimitedLayerEntropyCostTrackerTest()
      : entropy_providers_(
            kTestClientId,
            // Using 100 as the test LES value (as opposed to the production
            // value of 8000) since this test suite is for using the limited
            // entropy randomization source.
            {0, 100},
            kTestLimitedEntropyRandomizationSource) {
    EnableLimitedEntropyModeForTesting();
  }

 protected:
  const EntropyProviders entropy_providers_;
};

TEST_F(LimitedLayerEntropyCostTrackerTest, TestConstructor_WithNoLimitedLayer) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(50, 100001),
      CreateExperiment(50),
  };
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 18);

  EXPECT_EQ(18, limited_entropy_tracker.entropy_limit_in_bits_);
  EXPECT_EQ(0u, limited_entropy_tracker.active_limited_layer_id_);
  EXPECT_TRUE(limited_entropy_tracker.limited_layers_ids_.empty());
  EXPECT_TRUE(limited_entropy_tracker.entropy_used_by_layer_members_.empty());
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestConstructor_WithNonLimitedEntropyMode) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(50, 100001),
      CreateExperiment(50),
  };
  auto test_layer =
      CreateLayer(kTestLayerId, 100, Layer::DEFAULT,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 5);

  EXPECT_EQ(5, limited_entropy_tracker.entropy_limit_in_bits_);
  EXPECT_EQ(0u, limited_entropy_tracker.active_limited_layer_id_);
  EXPECT_TRUE(limited_entropy_tracker.limited_layers_ids_.empty());
  EXPECT_TRUE(limited_entropy_tracker.entropy_used_by_layer_members_.empty());
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest, TestConstructor_WithNoLayerMembers) {
  auto test_layer = CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                                /*layer_members=*/{});
  auto test_study = CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study});

  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 5);

  EXPECT_EQ(5, limited_entropy_tracker.entropy_limit_in_bits_);
  EXPECT_EQ(0u, limited_entropy_tracker.active_limited_layer_id_);
  EXPECT_EQ(1u, limited_entropy_tracker.limited_layers_ids_.size());
  EXPECT_TRUE(
      limited_entropy_tracker.limited_layers_ids_.contains(kTestLayerId));
  EXPECT_TRUE(limited_entropy_tracker.entropy_used_by_layer_members_.empty());
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestConstructor_WithManyLimitedLayers) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(50, 100001),
      CreateExperiment(50),
  };
  auto test_layer_1 =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_layer_2 =
      CreateLayer(kTestLayerId + 1, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer_1, test_layer_2}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 5);

  EXPECT_EQ(5, limited_entropy_tracker.entropy_limit_in_bits_);
  EXPECT_EQ(0u, limited_entropy_tracker.active_limited_layer_id_);
  EXPECT_EQ(2u, limited_entropy_tracker.limited_layers_ids_.size());
  EXPECT_TRUE(
      limited_entropy_tracker.limited_layers_ids_.contains(kTestLayerId));
  EXPECT_TRUE(
      limited_entropy_tracker.limited_layers_ids_.contains(kTestLayerId + 1));
  EXPECT_TRUE(limited_entropy_tracker.entropy_used_by_layer_members_.empty());
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestConstructor_WithMisconfiguredLimitedLayer) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(50, 100001),
      CreateExperiment(50),
  };
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/0, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 5);

  EXPECT_EQ(5, limited_entropy_tracker.entropy_limit_in_bits_);
  EXPECT_EQ(0u, limited_entropy_tracker.active_limited_layer_id_);
  EXPECT_EQ(1u, limited_entropy_tracker.limited_layers_ids_.size());
  EXPECT_TRUE(
      limited_entropy_tracker.limited_layers_ids_.contains(kTestLayerId));
  EXPECT_TRUE(limited_entropy_tracker.entropy_used_by_layer_members_.empty());
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest, TestConstructor_WithLimitedLayer) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(50, 100001),
      CreateExperiment(50),
  };
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
      {CreateLayerMember(1, {{0, 49}}), CreateLayerMember(2, {{50, 99}})});
  ;
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 15);

  EXPECT_EQ(15, limited_entropy_tracker.entropy_limit_in_bits_);
  EXPECT_EQ(kTestLayerId, limited_entropy_tracker.active_limited_layer_id_);
  EXPECT_EQ(1u, limited_entropy_tracker.limited_layers_ids_.size());
  EXPECT_TRUE(
      limited_entropy_tracker.limited_layers_ids_.contains(kTestLayerId));
  EXPECT_DOUBLE_EQ(
      2, limited_entropy_tracker.entropy_used_by_layer_members_.size());
  EXPECT_EQ(1, limited_entropy_tracker.entropy_used_by_layer_members_[1]);
  EXPECT_EQ(1, limited_entropy_tracker.entropy_used_by_layer_members_[2]);
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestConstructor_LayerMembersUsingEntropyAboveLimit) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(25, 100001),
      CreateGoogleWebExperiment(25, 100002),
      CreateExperiment(50),
  };
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
      {CreateLayerMember(1, {{0, 24}}), CreateLayerMember(2, {{25, 49}}),
       CreateLayerMember(3, {{50, 99}})});
  ;
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 1);

  EXPECT_EQ(1, limited_entropy_tracker.entropy_limit_in_bits_);
  EXPECT_EQ(kTestLayerId, limited_entropy_tracker.active_limited_layer_id_);
  EXPECT_EQ(1u, limited_entropy_tracker.limited_layers_ids_.size());
  EXPECT_TRUE(
      limited_entropy_tracker.limited_layers_ids_.contains(kTestLayerId));
  EXPECT_DOUBLE_EQ(
      3, limited_entropy_tracker.entropy_used_by_layer_members_.size());
  EXPECT_EQ(2, limited_entropy_tracker.entropy_used_by_layer_members_[1]);
  EXPECT_EQ(2, limited_entropy_tracker.entropy_used_by_layer_members_[2]);
  EXPECT_EQ(1, limited_entropy_tracker.entropy_used_by_layer_members_[3]);

  // The total entropy used is zero because no study entropy was added to the
  // tracker at this stage.
  EXPECT_FALSE(limited_entropy_tracker.includes_entropy_used_by_studies_);
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
  EXPECT_FALSE(limited_entropy_tracker.entropy_limit_reached_);
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_StudyReferencingNonLimitedLayer) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(25, 100001),
      CreateGoogleWebExperiment(25, 200002),
      CreateExperiment(50),
  };
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::DEFAULT,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 0);

  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study));
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
  EXPECT_FALSE(limited_entropy_tracker.IsEntropyLimitReached());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_NoLayerMembers) {
  auto test_layer = CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                                /*layer_members=*/{});
  auto test_study = CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study});

  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 0);

  // Entropy used is zero since no study referencing the LIMITED layer will be
  // assigned.
  EXPECT_FALSE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study));
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_StudyNotReferencingLayer) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(25, 100001),
      CreateGoogleWebExperiment(25, 200002),
      CreateExperiment(50),
  };
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(experiments);
  auto test_seed = CreateTestSeed({test_layer}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 0);

  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study));
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
  EXPECT_FALSE(limited_entropy_tracker.IsEntropyLimitReached());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_StudyReferencingNoLayerMember) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(25, 100001),
      CreateGoogleWebExperiment(25, 200002),
      CreateExperiment(50),
  };
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments, CreateLayerMemberReference(kTestLayerId, {}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 0);

  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study));
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
  EXPECT_FALSE(limited_entropy_tracker.IsEntropyLimitReached());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_StudyReferencingLimitedLayer) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(25, 100001),
      CreateGoogleWebExperiment(25, 200002),
      CreateExperiment(50),
  };
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 2);

  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study));
  EXPECT_EQ(2, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
  EXPECT_FALSE(limited_entropy_tracker.IsEntropyLimitReached());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_StudyReferencingMisconfiguredLimitedLayer) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(25, 100001),
      CreateGoogleWebExperiment(25, 200002),
      CreateExperiment(50),
  };
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/0, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 2);

  EXPECT_FALSE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study));
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
  EXPECT_FALSE(limited_entropy_tracker.IsEntropyLimitReached());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_WithManyLimitedLayers) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(50, 100001),
      CreateExperiment(50),
  };
  auto test_layer_1 =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_layer_2 =
      CreateLayer(kTestLayerId + 1, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer_1, test_layer_2}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 5);

  EXPECT_FALSE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study));
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
  EXPECT_FALSE(limited_entropy_tracker.IsEntropyLimitReached());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_MultipleStudies) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(25, 100001),
      CreateGoogleWebExperiment(25, 200002),
      CreateExperiment(50),
  };
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 49}}),
                   CreateLayerMember(kTestLayerMemberId + 1, {{49, 99}})});
  auto test_study_1 = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_study_2 = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId,
                                 {kTestLayerMemberId, kTestLayerMemberId + 1}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study_1, test_study_2});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 5);

  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_1));
  EXPECT_EQ(3, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_2));
  EXPECT_EQ(5, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
  EXPECT_FALSE(limited_entropy_tracker.IsEntropyLimitReached());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_EntropyLimitReachedByLayerMember) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(25, 100001),
      CreateGoogleWebExperiment(25, 200002),
      CreateExperiment(50),
  };
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 24}}),
                   CreateLayerMember(kTestLayerMemberId + 1, {{25, 49}}),
                   CreateLayerMember(kTestLayerMemberId + 2, {{50, 99}})});
  auto test_study_1 = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_study_2 = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study_1, test_study_2});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 1);

  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
  EXPECT_FALSE(limited_entropy_tracker.IsEntropyLimitReached());
  EXPECT_FALSE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_1));
  EXPECT_EQ(4, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
  EXPECT_TRUE(limited_entropy_tracker.IsEntropyLimitReached());
  EXPECT_FALSE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_2));
  EXPECT_EQ(4, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
  EXPECT_TRUE(limited_entropy_tracker.IsEntropyLimitReached());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_EntropyLimitReachedAfterAddingStudy) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(25, 100001),
      CreateGoogleWebExperiment(25, 200002),
      CreateExperiment(50),
  };
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study_1 = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_study_2 = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study_1, test_study_2});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 3);

  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_1));
  EXPECT_EQ(2, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
  EXPECT_FALSE(limited_entropy_tracker.IsEntropyLimitReached());
  EXPECT_FALSE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_2));
  EXPECT_EQ(4, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
  EXPECT_TRUE(limited_entropy_tracker.IsEntropyLimitReached());
}

TEST_F(
    LimitedLayerEntropyCostTrackerTest,
    TestTryAddEntropyUsedByStudy_StudyNotReferencingLimitedLayerAndEntropyLimitIsReached) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(25, 100001),
      CreateGoogleWebExperiment(25, 200002),
      CreateExperiment(50),
  };
  auto test_layer_1 =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_layer_2 =
      CreateLayer(kTestLayerId + 1, /*num_slots=*/100, Layer::DEFAULT,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study_1 = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_study_2 = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId + 1, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer_1, test_layer_2},
                                  {test_study_1, test_study_2});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 1);

  EXPECT_FALSE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_1));
  EXPECT_EQ(2, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
  EXPECT_TRUE(limited_entropy_tracker.IsEntropyLimitReached());
  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_2));
  EXPECT_EQ(2, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
  EXPECT_TRUE(limited_entropy_tracker.IsEntropyLimitReached());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_MultipleExperimentsWithID) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(125, 100001),
      CreateGoogleWebExperiment(125, 100002),
      CreateGoogleWebExperiment(250, 200001),
      CreateGoogleWebExperiment(250, 200002),
      CreateExperiment(250),
  };
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 13);

  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study));
  EXPECT_EQ(3, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_NoGoogleWebExperimentID) {
  // Experiments without google_web_experiment_id are excluded from entropy
  // calculation.
  std::vector<Study::Experiment> experiments = {
      CreateExperiment(10), CreateExperiment(20), CreateExperiment(30),
      CreateExperiment(40)};
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 13);

  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study));
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_SkipZeroProbabilityWeightedStudies) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(0, 100001), CreateExperiment(100)};
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 13);

  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study));
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_NoExperiments) {
  std::vector<Study::Experiment> experiments;
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 13);

  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study));
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_WithGoogleWebTriggerExpID) {
  std::vector<Study::Experiment> experiments = {
      CreateTriggerExperiment(25, 100001), CreateTriggerExperiment(25, 100002),
      CreateExperiment(50)};
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 13);

  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study));
  EXPECT_EQ(2, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_TestEntropyUsedByLimitedLayer) {
  auto experiments = CreateExperimentsWithTwoBitsOfEntropy();
  // Creates a layer with LIMITED entropy mode that takes 1 bit of entropy from
  // the layer member.
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 13);

  // Expecting 3 bits of total usage with 2 bits from the study, and 1 bit from
  // the layer member.
  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study));
  EXPECT_EQ(3, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_StudiesOfDifferentEntropyInLayerMember) {
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_study_1 = CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_study_2 = CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study_1, test_study_2});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 5);

  // Expecting 5 bits of total usage with 4 bits from the two 2-bit studies, and
  // 1 bit from the layer member.
  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_1));
  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_2));
  EXPECT_EQ(5, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_MultipleLayerMembers) {
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
      {CreateLayerMember(1, {{0, 49}}), CreateLayerMember(2, {{50, 74}}),
       CreateLayerMember(3, {{75, 99}})});
  auto test_seed = CreateTestSeed(
      {test_layer},
      {CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                       CreateLayerMemberReference(kTestLayerId, {0})),
       CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                       CreateLayerMemberReference(kTestLayerId, {1})),
       CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                       CreateLayerMemberReference(kTestLayerId, {2})),
       CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                       CreateLayerMemberReference(kTestLayerId, {2}))});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 6);

  // Expecting 6 bits of total usage with 4 bits from the two 2-bit studies in
  // layer member #2, and 2 bit from the 25% layer member itself.
  for (const Study& study : test_seed.study()) {
    EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(study));
  }
  EXPECT_EQ(6, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_ReferencingMultipleLayers) {
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
      {CreateLayerMember(1, {{0, 49}}), CreateLayerMember(2, {{50, 74}}),
       CreateLayerMember(3, {{75, 99}})});
  auto test_study_1 =
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                      CreateLayerMemberReference(kTestLayerId,
                                                 /*layer_member_ids=*/{0, 1}));
  auto test_study_2 =
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                      CreateLayerMemberReference(kTestLayerId,
                                                 /*layer_member_ids=*/{2}));
  auto test_seed = CreateTestSeed({test_layer}, {test_study_1, test_study_2});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 4);

  // Entropy usages:
  // - layer member 1:
  //   1 bit from the layer member + 2 bits from the first study = 3 bits.
  // - layer member 2:
  //   2 bit from the layer member + 2 bits from the first study = 4 bits.
  // - layer member 3:
  //   2 bit from the layer member + 2 bits from the second study = 4 bits.
  // - Therefore the layer uses a maximum of 4 bits.
  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_1));
  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_2));
  EXPECT_EQ(4, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_SomeStudiesDoNotReferenceLayer) {
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_study_1 = CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_study_2 = CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy());
  auto test_seed = CreateTestSeed({test_layer}, {test_study_1, test_study_2});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 3);

  // 1 bit from the layer member and 2 bits from the studies assigned.
  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_1));
  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_2));
  EXPECT_EQ(3, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_SomeStudiesReferenceOtherLayers) {
  int limited_layer_id = kTestLayerId;
  int default_layer_id = kTestLayerId + 1;
  auto test_limited_layer =
      CreateLayer(limited_layer_id, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_default_layer =
      CreateLayer(default_layer_id, /*num_slots=*/100, Layer::DEFAULT,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_study_1 = CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(limited_layer_id, {kTestLayerMemberId}));
  auto test_study_2 = CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(default_layer_id, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_limited_layer, test_default_layer},
                                  {test_study_1, test_study_2});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 3);

  // 1 bit from the layer member and 2 bits from the studies assigned to the
  // layer with LIMITED entropy mode.
  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_1));
  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_2));
  EXPECT_EQ(3, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestTryAddEntropyUsedByStudy_NoStudiesReferencingLimitedLayer) {
  int limited_layer_id = kTestLayerId;
  int default_layer_id = kTestLayerId + 1;
  auto test_limited_layer =
      CreateLayer(limited_layer_id, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId + 2, {{0, 49}}),
                   CreateLayerMember(kTestLayerMemberId + 1, {{50, 74}}),
                   CreateLayerMember(kTestLayerMemberId, {{75, 99}})});
  auto test_default_layer =
      CreateLayer(default_layer_id, /*num_slots=*/100, Layer::DEFAULT,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_study_1 = CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(default_layer_id, {kTestLayerMemberId}));
  auto test_study_2 = CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(default_layer_id, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_limited_layer, test_default_layer},
                                  {test_study_1, test_study_2});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 2);

  // Entropy used is zero since the layer members in the limited layer is
  // unused (the studies in `test_seed` is not constrained to a limited layer).
  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_1));
  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study_2));
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

TEST_F(
    LimitedLayerEntropyCostTrackerTest,
    TestTryAddEntropyUsedByStudy_NoReferencingStudiesWithGoogleExperimentID) {
  auto test_limited_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId + 2, {{0, 49}}),
                   CreateLayerMember(kTestLayerMemberId + 1, {{50, 74}}),
                   CreateLayerMember(kTestLayerMemberId, {{75, 99}})});
  auto test_study = CreateTestStudy(
      {CreateExperiment(1), CreateExperiment(1)},
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_seed = CreateTestSeed({test_limited_layer}, {test_study});
  VariationsLayers layers(test_seed, entropy_providers_);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(layers, test_seed, 2);

  // Without Google experiment IDs, a study that is constrained to a limited
  // layer does not use entropy. Entropy usage is zero if none of the studies in
  // any layer members use entropy.
  EXPECT_TRUE(limited_entropy_tracker.TryAddEntropyUsedByStudy(test_study));
  EXPECT_EQ(0, limited_entropy_tracker.GetTotalEntropyUsedForTesting());
}

}  // namespace variations
