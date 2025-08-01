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
#include "components/variations/proto/layer.pb.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_layers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

inline constexpr uint32_t kTestLayerId = 1001;
inline constexpr int kTestLayerMemberId = 2001;
inline constexpr std::string_view kTestClientId = "test_client_id";

// The following value ensures slot 0 is selected  (among 100 slots) when the
// limited entropy provider is used.
inline constexpr std::string_view kTestLimitedEntropyRandomizationSource =
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
Study CreateTestStudy(const std::vector<Study::Experiment>& experiments,
                      const std::optional<const LayerMemberReference>&
                          layer_member_reference = std::nullopt) {
  Study study;
  study.set_name("test_study");
  study.set_consistency(Study::PERMANENT);

  for (size_t i = 0; i < experiments.size(); ++i) {
    Study_Experiment* experiment_to_add = study.add_experiment();
    experiment_to_add->MergeFrom(experiments[i]);
    experiment_to_add->set_name(
        base::StrCat({"test_experiment_", base::NumberToString(i)}));
  }

  if (layer_member_reference.has_value()) {
    auto* layer_member_reference_to_add = study.mutable_layer();
    layer_member_reference_to_add->MergeFrom(*layer_member_reference);
  }
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
            kTestLimitedEntropyRandomizationSource) {}

 protected:
  const EntropyProviders entropy_providers_;
};

TEST_F(LimitedLayerEntropyCostTrackerTest, TestConstructor_WithLimitedLayer) {
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(1, {{0, 49}}), CreateLayerMember(2, {{50, 99}})});
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 15);

  EXPECT_EQ(15, limited_entropy_tracker.entropy_limit_in_bits_);
  EXPECT_EQ(kTestLayerId, limited_entropy_tracker.limited_layer_id_);
  EXPECT_DOUBLE_EQ(2,
                   limited_entropy_tracker.entropy_used_by_member_id_.size());
  EXPECT_EQ(1, limited_entropy_tracker.entropy_used_by_member_id_[1]);
  EXPECT_EQ(1, limited_entropy_tracker.entropy_used_by_member_id_[2]);
  EXPECT_EQ(0, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestConstructor_LayerMembersUsingEntropyAboveLimit) {
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(1, {{0, 24}}), CreateLayerMember(2, {{25, 49}}),
       CreateLayerMember(3, {{50, 99}})});
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 1);

  EXPECT_EQ(1, limited_entropy_tracker.entropy_limit_in_bits_);
  EXPECT_EQ(kTestLayerId, limited_entropy_tracker.limited_layer_id_);
  EXPECT_DOUBLE_EQ(3,
                   limited_entropy_tracker.entropy_used_by_member_id_.size());
  // Note that the entropy used by layer members 1 and 2 is 2 bits, which is
  // above the total entropy limit of 1 bit.
  EXPECT_EQ(2, limited_entropy_tracker.entropy_used_by_member_id_[1]);
  EXPECT_EQ(2, limited_entropy_tracker.entropy_used_by_member_id_[2]);
  EXPECT_EQ(1, limited_entropy_tracker.entropy_used_by_member_id_[3]);

  // The total entropy used is zero because no study entropy has been added to
  // the limited_entropy_tracker at this stage.
  EXPECT_FALSE(limited_entropy_tracker.includes_study_entropy_);
  EXPECT_EQ(0, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
  EXPECT_FALSE(limited_entropy_tracker.IsEntropyLimitExceeded());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_StudyReferencingNoLayerMember) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(25, 100001),
      CreateGoogleWebExperiment(25, 200002),
      CreateExperiment(50),
  };
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments, CreateLayerMemberReference(kTestLayerId, {}));
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 4);

  EXPECT_FALSE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study));
  EXPECT_EQ(0, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
  EXPECT_FALSE(limited_entropy_tracker.IsEntropyLimitExceeded());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_StudyReferencingLimitedLayer) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(25, 100001),
      CreateGoogleWebExperiment(25, 200002),
      CreateExperiment(50),
  };
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 2);

  EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study));
  EXPECT_EQ(2, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
  EXPECT_FALSE(limited_entropy_tracker.IsEntropyLimitExceeded());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_StudyReferencingLimitedLayerUsingFallbackField) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(25, 100001),
      CreateGoogleWebExperiment(25, 200002),
      CreateExperiment(50),
  };
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  LayerMemberReference fallback_layer_member_reference;
  fallback_layer_member_reference.set_layer_id(kTestLayerId);
  fallback_layer_member_reference.set_layer_member_id(kTestLayerMemberId);
  auto test_study = CreateTestStudy(
      experiments,
      fallback_layer_member_reference);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 2);

  EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study));
  EXPECT_EQ(2, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
  EXPECT_FALSE(limited_entropy_tracker.IsEntropyLimitExceeded());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_MultipleStudies) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(25, 100001),
      CreateGoogleWebExperiment(25, 200002),
      CreateExperiment(50),
  };
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(kTestLayerMemberId, {{0, 49}}),
       CreateLayerMember(kTestLayerMemberId + 1, {{50, 99}})});
  auto test_study_1 = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_study_2 = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId,
                                 {kTestLayerMemberId, kTestLayerMemberId + 1}));
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 5);

  EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study_1));
  EXPECT_EQ(3, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
  EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study_2));
  EXPECT_EQ(5, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
  EXPECT_FALSE(limited_entropy_tracker.IsEntropyLimitExceeded());
  EXPECT_EQ(
      5,
      limited_entropy_tracker.entropy_used_by_member_id_[kTestLayerMemberId]);
  EXPECT_EQ(3, limited_entropy_tracker
                   .entropy_used_by_member_id_[kTestLayerMemberId + 1]);
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_EntropyLimitReachedByLayerMember) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(25, 100001),
      CreateGoogleWebExperiment(25, 200002),
      CreateExperiment(50),
  };
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(kTestLayerMemberId, {{0, 24}}),
       CreateLayerMember(kTestLayerMemberId + 1, {{25, 49}}),
       CreateLayerMember(kTestLayerMemberId + 2, {{50, 99}})});
  auto test_study_1 = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_study_2 = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 1);

  EXPECT_EQ(0, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
  EXPECT_FALSE(limited_entropy_tracker.IsEntropyLimitExceeded());
  EXPECT_FALSE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study_1));
  EXPECT_EQ(4, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
  EXPECT_TRUE(limited_entropy_tracker.IsEntropyLimitExceeded());
  EXPECT_FALSE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study_2));
  EXPECT_EQ(4, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
  EXPECT_TRUE(limited_entropy_tracker.IsEntropyLimitExceeded());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_EntropyLimitReachedAfterAddingStudy) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(25, 100001),
      CreateGoogleWebExperiment(25, 200002),
      CreateExperiment(50),
  };
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study_1 = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_study_2 = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));

  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 3);

  EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study_1));
  EXPECT_EQ(2, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
  EXPECT_FALSE(limited_entropy_tracker.IsEntropyLimitExceeded());
  EXPECT_FALSE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study_2));
  EXPECT_EQ(4, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
  EXPECT_TRUE(limited_entropy_tracker.IsEntropyLimitExceeded());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_NoGoogleWebExperimentID) {
  // Experiments without google_web_experiment_id are excluded from entropy
  // calculation.
  std::vector<Study::Experiment> experiments = {
      CreateExperiment(10), CreateExperiment(20), CreateExperiment(30),
      CreateExperiment(40)};
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 13);

  EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study));
  EXPECT_EQ(0, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_LaunchedStudy) {
  // Experiments without google_web_experiment_id are excluded from entropy
  // calculation.
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {
          CreateLayerMember(kTestLayerMemberId,
                            {{0, 49}}),  // 50% of the population.
          CreateLayerMember(kTestLayerMemberId + 1,
                            {{50, 99}}),  // 05% of the population.
      });
  auto launched_study = CreateTestStudy(
      {CreateGoogleWebExperiment(100, 100001)},  // 100% launched arm
      CreateLayerMemberReference(
          kTestLayerId,
          {kTestLayerMemberId, kTestLayerMemberId + 1}));  // 100% population.
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 13);

  EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(launched_study));
  EXPECT_EQ(0, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_LaunchedAndActiveStudies) {
  // Experiments without google_web_experiment_id are excluded from entropy
  // calculation.
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {
          CreateLayerMember(kTestLayerMemberId,
                            {{0, 49}}),  // 50% of the population.
          CreateLayerMember(kTestLayerMemberId + 1,
                            {{50, 99}}),  // 50% of the population.
      });
  auto launched_study = CreateTestStudy(
      {CreateGoogleWebExperiment(100, 100001)},  // 100% launched arm
      CreateLayerMemberReference(
          kTestLayerId,
          {kTestLayerMemberId, kTestLayerMemberId + 1}));  // 100% population.
  auto active_study = CreateTestStudy(
      {
          CreateGoogleWebExperiment(50, 100001),  // 50% active arm 1
          CreateGoogleWebExperiment(50, 200002),  // 50% active arm 2
      },
      CreateLayerMemberReference(kTestLayerId,
                                 {kTestLayerMemberId}));  // 25% population.
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 13);

  EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(launched_study));
  EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(active_study));
  EXPECT_EQ(2,  // member: 1 bit; active study: 1 bit, launched study: 0 bits
            limited_entropy_tracker.GetMaxEntropyUsedForTesting());
  EXPECT_EQ(
      2,
      limited_entropy_tracker.entropy_used_by_member_id_[kTestLayerMemberId]);
  EXPECT_EQ(1, limited_entropy_tracker
                   .entropy_used_by_member_id_[kTestLayerMemberId + 1]);
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_SkipZeroProbabilityWeightedStudies) {
  std::vector<Study::Experiment> experiments = {
      CreateGoogleWebExperiment(0, 100001), CreateExperiment(100)};
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 13);

  EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study));
  EXPECT_EQ(0, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_NoExperiments) {
  std::vector<Study::Experiment> experiments;
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 13);

  EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study));
  EXPECT_EQ(0, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_WithGoogleWebTriggerExpID) {
  std::vector<Study::Experiment> experiments = {
      CreateTriggerExperiment(25, 100001), CreateTriggerExperiment(25, 100002),
      CreateExperiment(50)};
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 13);

  EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study));
  EXPECT_EQ(2, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_SessionConsistency) {
  std::vector<Study::Experiment> experiments = {
      CreateTriggerExperiment(25, 100001), CreateTriggerExperiment(25, 100002),
      CreateExperiment(50)};
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(kTestLayerMemberId, {{0, 99}})});
  auto test_study = CreateTestStudy(
      experiments,
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  test_study.set_consistency(Study::SESSION);
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 13);

  EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study));
  EXPECT_EQ(0, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_StudiesOfDifferentEntropyInLayerMember) {
  // TODO(siakabaro): The test name doesn't seems to match the code?
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_study_1 = CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  auto test_study_2 = CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 5);

  // Expecting 5 bits of total usage with 4 bits from the two 2-bit studies, and
  // 1 bit from the layer member.
  EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study_1));
  EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study_2));
  EXPECT_EQ(5, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_MultipleLayerMembers) {
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(1, {{0, 49}}), CreateLayerMember(2, {{50, 74}}),
       CreateLayerMember(3, {{75, 99}})});
  std::vector<Study> studies = {
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                      CreateLayerMemberReference(kTestLayerId, {1})),
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                      CreateLayerMemberReference(kTestLayerId, {2})),
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                      CreateLayerMemberReference(kTestLayerId, {3})),
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                      CreateLayerMemberReference(kTestLayerId, {3})),
  };
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 6);

  // Expecting 6 bits of total usage with 4 bits from the two 2-bit studies in
  // layer member #3, and 2 bit from the 25% layer member itself.
  for (const Study& study : studies) {
    EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(study));
  }
  EXPECT_EQ(6, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_ReferencingMultipleLayerMembers) {
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(1, {{0, 49}}), CreateLayerMember(2, {{50, 74}}),
       CreateLayerMember(3, {{75, 99}})});
  auto test_study_1 =
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                      CreateLayerMemberReference(kTestLayerId,
                                                 /*layer_member_ids=*/{1, 2}));
  auto test_study_2 =
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                      CreateLayerMemberReference(kTestLayerId,
                                                 /*layer_member_ids=*/{3}));
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 4);

  // Entropy usages:
  // - layer member 1:
  //   1 bit from the layer member + 2 bits from the first study = 3 bits.
  // - layer member 2:
  //   2 bit from the layer member + 2 bits from the first study = 4 bits.
  // - layer member 3:
  //   2 bit from the layer member + 2 bits from the second study = 4 bits.
  // - Therefore the layer uses a maximum of 4 bits.
  EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study_1));
  EXPECT_TRUE(limited_entropy_tracker.AddEntropyUsedByStudy(test_study_2));
  EXPECT_EQ(4, limited_entropy_tracker.GetMaxEntropyUsedForTesting());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_InvalidLayerId) {
  auto test_layer = CreateLayer(
      0u, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(1, {{0, 49}}), CreateLayerMember(2, {{50, 99}})});
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 4);
  EXPECT_FALSE(limited_entropy_tracker.IsValid());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_NoEntropy) {
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(1, {{0, 49}}), CreateLayerMember(2, {{50, 99}})});
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 0.0);
  EXPECT_FALSE(limited_entropy_tracker.IsValid());
}

TEST_F(LimitedLayerEntropyCostTrackerTest, TestAddEntropyUsedByStudy_NoSlots) {
  auto test_layer = CreateLayer(kTestLayerId, /*num_slots=*/0u,
                                /*entropy_mode=*/Layer::LIMITED, {});
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 4);
  EXPECT_FALSE(limited_entropy_tracker.IsValid());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_WrongEntropyMode) {
  auto test_layer = CreateLayer(kTestLayerId, /*num_slots=*/100,
                                /*entropy_mode=*/Layer::DEFAULT, {});
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 4);
  EXPECT_FALSE(limited_entropy_tracker.IsValid());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_NoLayerMembers) {
  auto test_layer = CreateLayer(kTestLayerId, /*num_slots=*/100,
                                /*entropy_mode=*/Layer::LIMITED, {});
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 4);
  EXPECT_FALSE(limited_entropy_tracker.IsValid());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_OverlappingLayerMembers) {
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(1, {{0, 49}}), CreateLayerMember(2, {{40, 99}})});
  ASSERT_FALSE(VariationsLayers::AreSlotBoundsValid(test_layer));
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 4);
  EXPECT_FALSE(limited_entropy_tracker.IsValid());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_InvalidLayerMemberId) {
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(0, {{0, 49}}), CreateLayerMember(2, {{50, 99}})});
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 4);
  EXPECT_FALSE(limited_entropy_tracker.IsValid());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_DuplicatedLayerMemberId) {
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(1, {{1, 49}}), CreateLayerMember(1, {{50, 99}})});
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 4);
  EXPECT_FALSE(limited_entropy_tracker.IsValid());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_Reference_InvalidLayerId) {
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(1, {{0, 49}}), CreateLayerMember(2, {{50, 99}})});
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 4);
  EXPECT_TRUE(limited_entropy_tracker.IsValid());
  auto test_study =
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                      CreateLayerMemberReference(0,  // Invalid layer id.
                                                 /*layer_member_ids=*/{1, 2}));
  limited_entropy_tracker.AddEntropyUsedByStudy(test_study);
  EXPECT_FALSE(limited_entropy_tracker.IsValid());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_Reference_DanglingLayerId) {
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(1, {{0, 49}}), CreateLayerMember(2, {{50, 99}})});
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 4);
  EXPECT_TRUE(limited_entropy_tracker.IsValid());
  auto test_study = CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(kTestLayerId + 7,  // Undefined layer id.
                                 /*layer_member_ids=*/{1, 2}));
  limited_entropy_tracker.AddEntropyUsedByStudy(test_study);
  EXPECT_FALSE(limited_entropy_tracker.IsValid());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_Reference_InvalidLayerMemberId) {
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(1, {{0, 49}}), CreateLayerMember(2, {{50, 99}})});
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 4);
  EXPECT_TRUE(limited_entropy_tracker.IsValid());
  auto test_study = CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(
          kTestLayerId,
          /*layer_member_ids=*/{0}));  // Invalid layer member id.
  limited_entropy_tracker.AddEntropyUsedByStudy(test_study);
  EXPECT_FALSE(limited_entropy_tracker.IsValid());
}

TEST_F(LimitedLayerEntropyCostTrackerTest,
       TestAddEntropyUsedByStudy_Reference_DanglingLayerMemberId) {
  auto test_layer = CreateLayer(
      kTestLayerId, /*num_slots=*/100, /*entropy_mode=*/Layer::LIMITED,
      {CreateLayerMember(1, {{0, 49}}), CreateLayerMember(2, {{50, 99}})});
  LimitedLayerEntropyCostTracker limited_entropy_tracker(test_layer, 4);
  EXPECT_TRUE(limited_entropy_tracker.IsValid());
  auto test_study = CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(
          kTestLayerId,
          /*layer_member_ids=*/{7}));  // Undefined layer member id.
  limited_entropy_tracker.AddEntropyUsedByStudy(test_study);
  EXPECT_FALSE(limited_entropy_tracker.IsValid());
}

}  // namespace variations
