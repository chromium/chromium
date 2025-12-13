// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/limited_entropy_randomization.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/proto/layer.pb.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_layers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

constexpr int kTestLayerId = 101;
constexpr int kTestLayerMemberId = 201;
constexpr int kLowEntropyLayerId = 301;
constexpr int kLimitedEntropyLayerId = 401;

// The entropy limits, in bits, to use for various tests.
constexpr double kEntropyLimit_10bits = 10.0;
constexpr double kEntropyLimit_2bits = 2.0;

// Hard code the enum bucket values, and use these in tests so that any
// renumbering can be detected.
// LimitedEntropySeedRejectionReason::kHighEntropyUsage
constexpr int kHighEntropyUsageBucket = 0;
// LimitedEntropySeedRejectionReason::kMoreThenOneLimitedLayer
constexpr int kMoreThenOneLimitedLayerBucket = 1;
// LimitedEntropySeedRejectionReason::kLayerHasInvalidSlotBounds
constexpr int kLayerHasInvalidSlotBoundsBucket = 2;
// LimitedEntropySeedRejectionReason::kLayerDoesNotContainSlots
constexpr int kLayerDoesNotContainSlotsBucket = 3;
// LimitedEntropySeedRejectionReason::kInvalidId
constexpr int kInvalidLayerIdBucket = 4;
// LimitedEntropySeedRejectionReason::kDuplicatedLayerId
constexpr int kDuplicatedLayerIdBucket = 5;
// LimitedEntropySeedRejectionReason::kInvalidLayerReference
constexpr int kInvalidLayerReferenceBucket = 6;
// LimitedEntropySeedRejectionReason::kDanglingLayerReference
constexpr int kDanglingLayerReferenceBucket = 7;
// LimitedEntropySeedRejectionReason::kDanglingLayerMemberReference
constexpr int kDanglingLayerMemberReferenceBucket = 8;
// LimitedEntropySeedRejectionReason::kEmptyLayerReference
constexpr int kEmptyLayerReferenceBucket = 9;
// LimitedEntropySeedRejectionReason::kInvalidLayerConfiguration
[[maybe_unused]] constexpr int kInvalidLayerConfigurationBucket = 10;
// LimitedEntropySeedRejectionReason::kActiveLowAndLimitedEntropy
constexpr int kActiveLowAndLimitedEntropyBucket = 11;

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
  study.set_consistency(Study::PERMANENT);

  std::vector<Study::Experiment> copied_experiments(experiments);
  for (size_t i = 0; i < copied_experiments.size(); ++i) {
    copied_experiments[i].set_name(
        base::StrCat({"test_experiment_", base::NumberToString(i)}));
    Study_Experiment* experiment_to_add = study.add_experiment();
    experiment_to_add->MergeFrom(copied_experiments[i]);
  }

  // Add all platforms to the study filter.
  auto* platforms = study.mutable_filter()->mutable_platform();
  for (int p = static_cast<int>(Study_Platform_Platform_MIN);
       p <= static_cast<int>(Study_Platform_Platform_MAX); ++p) {
    platforms->Add(static_cast<Study_Platform>(p));
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

class LimitedEntropyRandomizationTest : public ::testing::Test {
 public:
  LimitedEntropyRandomizationTest()
      : client_state_(
            /*is_enterprise_function=*/base::BindOnce([] { return false; }),
            /*google_groups_function=*/base::BindOnce(
                [] { return base::flat_set<uint64_t>(); })) {
    client_state_.platform = ClientFilterableState::GetCurrentPlatform();
    client_state_.channel = Study::STABLE;
    client_state_.version = version_info::GetVersion();
  }

 protected:
  base::HistogramTester histogram_tester_;
  ClientFilterableState client_state_;
};

}  // namespace

TEST_F(LimitedEntropyRandomizationTest,
       ValidConfiguration_WithValidEntropyUse) {
  std::vector<Layer> test_layers;
  for (int i = 1; i <= 4; ++i) {
    test_layers.push_back(CreateLayer(
        /*layer_id=*/i, /*num_slots=*/100,
        /*entropy_mode=*/Layer::LIMITED,
        /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})}));
  }
  auto test_seed = CreateTestSeed(
      test_layers,
      {CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                       CreateLayerMemberReference(2, {kTestLayerMemberId})),
       CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                       CreateLayerMemberReference(2, {kTestLayerMemberId}))});
  const MisconfiguredEntropyResult result = SeedHasMisconfiguredEntropy(
      client_state_, test_seed, kEntropyLimit_10bits);
  EXPECT_FALSE(result.is_misconfigured);
  EXPECT_TRUE(result.seed_has_active_limited_layer.value());
  histogram_tester_.ExpectTotalCount(kSeedRejectionReasonHistogram, 0);
}

TEST_F(LimitedEntropyRandomizationTest,
       ValidConfiguration_UseDeprecatedLayerMemberIdField) {
  auto test_layer = CreateLayer(
      /*layer_id=*/kTestLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::DEFAULT,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  LayerMemberReference layer_member_reference;
  layer_member_reference.set_layer_id(kTestLayerId);
  layer_member_reference.set_layer_member_id(kTestLayerMemberId);

  auto test_seed = CreateTestSeed(
      {test_layer}, {CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                                     layer_member_reference)});
  EXPECT_FALSE(SeedHasMisconfiguredEntropy(client_state_, test_seed,
                                           kEntropyLimit_10bits)
                   .is_misconfigured);
  histogram_tester_.ExpectTotalCount(kSeedRejectionReasonHistogram, 0);
}

TEST_F(LimitedEntropyRandomizationTest,
       ValidConfiguration_NoLimitedLayerReferences) {
  // Creates a layer with DEFAULT entropy mode. It would have taken 1 bit of
  // entropy if it is using LIMITED entropy mode.
  auto test_layer = CreateLayer(
      /*layer_id=*/kTestLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::DEFAULT,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_seed = CreateTestSeed(
      {test_layer}, {CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                                     CreateLayerMemberReference(
                                         kTestLayerId, {kTestLayerMemberId}))});
  // Seed should not be rejected since it's not using LIMITED entropy mode.
  const MisconfiguredEntropyResult result = SeedHasMisconfiguredEntropy(
      client_state_, test_seed, kEntropyLimit_10bits);
  EXPECT_FALSE(result.is_misconfigured);
  EXPECT_FALSE(result.seed_has_active_limited_layer.value());

  histogram_tester_.ExpectTotalCount(kSeedRejectionReasonHistogram, 0);
}

// This is exactly the same test as SeedRejection_EntropyOveruse, below, except
// that the study has session consistency so it does not consume entropy.
TEST_F(LimitedEntropyRandomizationTest, SessionConsistency) {
  // Creates a layer with LIMITED entropy mode that takes 1 bit of entropy from
  // the layer member.
  auto test_layer = CreateLayer(
      /*layer_id=*/kTestLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LIMITED,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_study = CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}));
  test_study.set_consistency(Study::SESSION);
  auto test_seed = CreateTestSeed({test_layer}, {test_study});
  // Seed should not be rejected since the study is session consistency, which
  // does not consume entropy.
  EXPECT_FALSE(
      SeedHasMisconfiguredEntropy(client_state_, test_seed, kEntropyLimit_2bits)
          .is_misconfigured);
  histogram_tester_.ExpectTotalCount(kSeedRejectionReasonHistogram, 0);
}

TEST_F(LimitedEntropyRandomizationTest, SeedRejection_EntropyOveruse) {
  // Creates a layer with LIMITED entropy mode that takes 1 bit of entropy from
  // the layer member.
  auto test_layer = CreateLayer(
      /*layer_id=*/kTestLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LIMITED,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_seed = CreateTestSeed(
      {test_layer}, {CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                                     CreateLayerMemberReference(
                                         kTestLayerId, {kTestLayerMemberId}))});
  // The total entropy used should be 3 bits which is over the limit.
  EXPECT_TRUE(
      SeedHasMisconfiguredEntropy(client_state_, test_seed, kEntropyLimit_2bits)
          .is_misconfigured);
  histogram_tester_.ExpectUniqueSample(kSeedRejectionReasonHistogram,
                                       kHighEntropyUsageBucket, 1);
}

TEST_F(LimitedEntropyRandomizationTest, SeedRejection_InvalidLayerId) {
  // Creates a layer with LIMITED entropy mode that takes 1 bit of entropy from
  // the layer member.
  auto test_layer = CreateLayer(
      /*layer_id=*/0,  // Zero is not a valid layer id.
      /*num_slots=*/100,
      /*entropy_mode=*/Layer::LIMITED,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_seed = CreateTestSeed(
      /*layers=*/{test_layer},
      /*studies=*/{});
  // Rejected because of invalid layer id.
  EXPECT_TRUE(
      SeedHasMisconfiguredEntropy(client_state_, test_seed, kEntropyLimit_2bits)
          .is_misconfigured);
  histogram_tester_.ExpectUniqueSample(kSeedRejectionReasonHistogram,
                                       kInvalidLayerIdBucket, 1);
}

TEST_F(LimitedEntropyRandomizationTest, SeedRejection_DuplicatedLayerId) {
  // Creates a layer with LIMITED entropy mode that takes 1 bit of entropy from
  // the layer member.
  auto test_layer = CreateLayer(
      /*layer_id=*/kTestLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LIMITED,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_seed = CreateTestSeed(
      {test_layer, test_layer},  // Add same layer twice to test duplicated id.
      {CreateTestStudy(
          CreateExperimentsWithTwoBitsOfEntropy(),
          CreateLayerMemberReference(kTestLayerId, {kTestLayerMemberId}))});
  // Rejected because of duplicated layer id.
  EXPECT_TRUE(
      SeedHasMisconfiguredEntropy(client_state_, test_seed, kEntropyLimit_2bits)
          .is_misconfigured);
  histogram_tester_.ExpectUniqueSample(kSeedRejectionReasonHistogram,
                                       kDuplicatedLayerIdBucket, 1);
}

TEST_F(LimitedEntropyRandomizationTest, SeedRejection_InvalidLayerReference) {
  // Creates a layer with LIMITED entropy mode that takes 1 bit of entropy from
  // the layer member.
  auto test_layer = CreateLayer(
      /*layer_id=*/kTestLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LIMITED,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_seed = CreateTestSeed(
      {test_layer},
      {CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                       CreateLayerMemberReference(0,  // Invalid layer id.
                                                  {kTestLayerMemberId}))});
  // Rejected because of duplicated layer id.
  EXPECT_TRUE(
      SeedHasMisconfiguredEntropy(client_state_, test_seed, kEntropyLimit_2bits)
          .is_misconfigured);
  histogram_tester_.ExpectUniqueSample(kSeedRejectionReasonHistogram,
                                       kInvalidLayerReferenceBucket, 1);
}

TEST_F(LimitedEntropyRandomizationTest, SeedRejection_EmptyLayerReference) {
  // Creates a layer with LIMITED entropy mode that takes 1 bit of entropy from
  // the layer member.
  auto test_layer = CreateLayer(
      /*layer_id=*/kTestLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LIMITED,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_seed = CreateTestSeed(
      {test_layer}, {CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                                     CreateLayerMemberReference(
                                         kTestLayerId,
                                         /*layer_member_ids=*/{}))});  // Empty.
  // Rejected because of empty layer member reference.
  EXPECT_TRUE(
      SeedHasMisconfiguredEntropy(client_state_, test_seed, kEntropyLimit_2bits)
          .is_misconfigured);
  histogram_tester_.ExpectUniqueSample(kSeedRejectionReasonHistogram,
                                       kEmptyLayerReferenceBucket, 1);
}

TEST_F(LimitedEntropyRandomizationTest, SeedRejection_DanglingLayerReference) {
  // Creates a layer with LIMITED entropy mode that takes 1 bit of entropy from
  // the layer member.f
  auto test_layer = CreateLayer(
      /*layer_id=*/kTestLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LIMITED,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_seed = CreateTestSeed(
      {test_layer},
      {CreateTestStudy(
          CreateExperimentsWithTwoBitsOfEntropy(),
          CreateLayerMemberReference(
              kTestLayerId + 1,  // Layer id + 1 is not defined above.
              {kTestLayerMemberId}))});
  // Rejected because of dangling layer member reference.
  EXPECT_TRUE(
      SeedHasMisconfiguredEntropy(client_state_, test_seed, kEntropyLimit_2bits)
          .is_misconfigured);
  histogram_tester_.ExpectUniqueSample(kSeedRejectionReasonHistogram,
                                       kDanglingLayerReferenceBucket, 1);
}

TEST_F(LimitedEntropyRandomizationTest,
       SeedRejection_DanglingLayerMemberReference) {
  // Creates a layer with LIMITED entropy mode that takes 1 bit of entropy from
  // the layer member.f
  auto test_layer = CreateLayer(
      /*layer_id=*/kTestLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LIMITED,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_seed = CreateTestSeed(
      {test_layer},
      {CreateTestStudy(
          CreateExperimentsWithTwoBitsOfEntropy(),
          CreateLayerMemberReference(
              kTestLayerId, {kTestLayerMemberId +
                             1}))});  // Layer member id + 1 is not defined.
  // Rejected because of dangling layer member reference.
  EXPECT_TRUE(
      SeedHasMisconfiguredEntropy(client_state_, test_seed, kEntropyLimit_2bits)
          .is_misconfigured);
  histogram_tester_.ExpectUniqueSample(kSeedRejectionReasonHistogram,
                                       kDanglingLayerMemberReferenceBucket, 1);
}

TEST_F(LimitedEntropyRandomizationTest,
       SeedRejection_MultipleActiveReferencedLimitedLayer) {
  std::vector<Layer> test_layers;
  for (int i = 1; i <= 4; ++i) {
    test_layers.push_back(CreateLayer(
        /*layer_id=*/i, /*num_slots=*/100,
        /*entropy_mode=*/Layer::LIMITED,
        /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})}));
  }
  auto test_seed = CreateTestSeed(
      test_layers,
      {CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                       CreateLayerMemberReference(2, {kTestLayerMemberId})),
       CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                       CreateLayerMemberReference(3, {kTestLayerMemberId}))});
  EXPECT_TRUE(SeedHasMisconfiguredEntropy(client_state_, test_seed,
                                          kEntropyLimit_10bits)
                  .is_misconfigured);
  histogram_tester_.ExpectUniqueSample(kSeedRejectionReasonHistogram,
                                       kMoreThenOneLimitedLayerBucket, 1);
}

TEST_F(LimitedEntropyRandomizationTest,
       SeedRejection_MultipleDisjointReferencedLimitedLayer) {
  std::vector<Layer> test_layers;
  for (int i = 1; i <= 4; ++i) {
    test_layers.push_back(CreateLayer(
        /*layer_id=*/i, /*num_slots=*/100,
        /*entropy_mode=*/Layer::LIMITED,
        /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})}));
  }
  // Find the current version value used by this binary, and increment its
  // last component. This way we can have study1 end at the current version,
  // and study2 start one incremented version later. That way, the studies have
  // disjoint client populations.
  //
  // Example versions:
  //
  // * The client is running version 140.0.1111.0
  // * study1 has max_version 140.0.1111.3
  // * study2 has min_version 140.0.1111.4
  auto study1_max_version = client_state_.version;
  auto version_components = client_state_.version.components();
  ASSERT_FALSE(version_components.empty());
  version_components.back() += 1;
  auto study2_min_version = base::Version(version_components);
  auto study1 =
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                      CreateLayerMemberReference(2,  // Not the same as study2.
                                                 {kTestLayerMemberId}));
  study1.mutable_filter()->set_max_version(study1_max_version.GetString());
  auto study2 =
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                      CreateLayerMemberReference(3,  // Not the same as study1.
                                                 {kTestLayerMemberId}));
  study2.mutable_filter()->set_min_version(study2_min_version.GetString());

  auto test_seed = CreateTestSeed(test_layers, {study1, study2});
  EXPECT_FALSE(SeedHasMisconfiguredEntropy(client_state_, test_seed,
                                           kEntropyLimit_10bits)
                   .is_misconfigured);
  histogram_tester_.ExpectTotalCount(kSeedRejectionReasonHistogram, 0);
}

TEST_F(LimitedEntropyRandomizationTest, SeedRejection_InvalidSlotBounds) {
  // A test layer with overlapping layer members.
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 49}, {25, 49}})});
  auto test_seed = CreateTestSeed(
      {test_layer}, {CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                                     CreateLayerMemberReference(
                                         kTestLayerId, {kTestLayerMemberId}))});
  // Seed should be rejected since the actively referenced LIMITED layer is
  // invalid.
  EXPECT_TRUE(SeedHasMisconfiguredEntropy(client_state_, test_seed,
                                          kEntropyLimit_10bits)
                  .is_misconfigured);
  histogram_tester_.ExpectUniqueSample(kSeedRejectionReasonHistogram,
                                       kLayerHasInvalidSlotBoundsBucket, 1);
}

TEST_F(LimitedEntropyRandomizationTest, SeedRejection_NoSlots) {
  // A test layer with no slots.
  auto test_layer = CreateLayer(kTestLayerId, /*num_slots=*/0, Layer::LIMITED,
                                {CreateLayerMember(kTestLayerMemberId, {})});
  auto test_seed = CreateTestSeed(
      {test_layer}, {CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                                     CreateLayerMemberReference(
                                         kTestLayerId, {kTestLayerMemberId}))});
  // Seed should be rejected since the LIMITED layer is invalid.
  EXPECT_TRUE(SeedHasMisconfiguredEntropy(client_state_, test_seed,
                                          kEntropyLimit_10bits)
                  .is_misconfigured);
  histogram_tester_.ExpectUniqueSample(kSeedRejectionReasonHistogram,
                                       kLayerDoesNotContainSlotsBucket, 1);
}

TEST_F(LimitedEntropyRandomizationTest,
       SeedRejection_SimultaneousLowAndLimitedLayers) {
  std::vector<Layer> test_layers;
  std::vector<Study> test_studies;
  // Create LOW and LIMITED entropy layers.
  test_layers.push_back(CreateLayer(
      kLowEntropyLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LOW,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})}));
  test_layers.push_back(CreateLayer(
      kLimitedEntropyLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LIMITED,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})}));

  // Create an entropy consuming study that refers to the LOW entropy layer.
  test_studies.push_back(CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(kLowEntropyLayerId, {kTestLayerMemberId})));

  // Create an entropy consuming study that refers to the LIMITED entropy layer.
  test_studies.push_back(
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                      CreateLayerMemberReference(kLimitedEntropyLayerId,
                                                 {kTestLayerMemberId})));

  // The seed is rejected because both layers would be active and simultaneously
  // active LOW and LIMITED layers are not allowed.
  auto test_seed = CreateTestSeed(test_layers, test_studies);
  auto result = SeedHasMisconfiguredEntropy(client_state_, test_seed,
                                            kEntropyLimit_10bits);
  EXPECT_TRUE(result.is_misconfigured);
  EXPECT_FALSE(result.seed_has_active_low_layer.has_value());
  EXPECT_FALSE(result.seed_has_active_limited_layer.has_value());
  histogram_tester_.ExpectUniqueSample(kSeedRejectionReasonHistogram,
                                       kActiveLowAndLimitedEntropyBucket, 1);
}

TEST_F(LimitedEntropyRandomizationTest,
       SeedRejection_SimultaneousLowAndLimitedStudies) {
  std::vector<Layer> test_layers;
  std::vector<Study> test_studies;
  // Create the LIMITED entropy layer.
  test_layers.push_back(CreateLayer(
      kLimitedEntropyLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LIMITED,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})}));

  // Create an entropy consuming study that refers to the LIMITED entropy layer.
  test_studies.push_back(
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                      CreateLayerMemberReference(kLimitedEntropyLayerId,
                                                 {kTestLayerMemberId})));

  // Create an entropy consuming study with no layer reference.
  test_studies.push_back(
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy()));

  // The seed is rejected because the client will have both low entropy and
  // limited entropy studies active.
  auto test_seed = CreateTestSeed(test_layers, test_studies);
  auto result = SeedHasMisconfiguredEntropy(client_state_, test_seed,
                                            kEntropyLimit_10bits);
  EXPECT_TRUE(result.is_misconfigured);
  EXPECT_FALSE(result.seed_has_active_low_layer.has_value());
  EXPECT_FALSE(result.seed_has_active_limited_layer.has_value());
  histogram_tester_.ExpectUniqueSample(kSeedRejectionReasonHistogram,
                                       kActiveLowAndLimitedEntropyBucket, 1);
}

TEST_F(LimitedEntropyRandomizationTest,
       SeedRejection_AllowNonActiveSimultaneousLowAndLimitedLayers_LOW) {
  std::vector<Layer> test_layers;
  std::vector<Study> test_studies;
  // Create LOW and LIMITED entropy layers.
  test_layers.push_back(CreateLayer(
      kLowEntropyLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LOW,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})}));
  test_layers.push_back(CreateLayer(
      kLimitedEntropyLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LIMITED,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})}));

  // Create multiple entropy consuming studies only referencing the LOW
  // entropy layer.
  test_studies.push_back(CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(kLowEntropyLayerId, {kTestLayerMemberId})));
  test_studies.push_back(CreateTestStudy(
      CreateExperimentsWithTwoBitsOfEntropy(),
      CreateLayerMemberReference(kLowEntropyLayerId, {kTestLayerMemberId})));

  // The seed is not rejected, only the LOW layer is active.
  auto test_seed = CreateTestSeed(test_layers, test_studies);
  auto result = SeedHasMisconfiguredEntropy(client_state_, test_seed,
                                            kEntropyLimit_10bits);
  EXPECT_FALSE(result.is_misconfigured);
  ASSERT_TRUE(result.seed_has_active_low_layer.has_value());
  EXPECT_TRUE(result.seed_has_active_low_layer.value());
  EXPECT_TRUE(result.seed_has_active_limited_layer.has_value());
  EXPECT_FALSE(result.seed_has_active_limited_layer.value());
  histogram_tester_.ExpectTotalCount(kSeedRejectionReasonHistogram, 0);
}

TEST_F(LimitedEntropyRandomizationTest,
       SeedRejection_AllowNonActiveSimultaneousLowAndLimitedLayers_LIMITED) {
  std::vector<Layer> test_layers;
  std::vector<Study> test_studies;
  // Create LOW and LIMITED entropy layers.
  test_layers.push_back(CreateLayer(
      kLowEntropyLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LOW,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})}));
  test_layers.push_back(CreateLayer(
      kLimitedEntropyLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LIMITED,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})}));

  // Create multiple entropy consuming studies only referencing the LIMITED
  // entropy layer.
  test_studies.push_back(
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                      CreateLayerMemberReference(kLimitedEntropyLayerId,
                                                 {kTestLayerMemberId})));
  test_studies.push_back(
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                      CreateLayerMemberReference(kLimitedEntropyLayerId,
                                                 {kTestLayerMemberId})));

  // The seed is not rejected, only the LIMITED layer is active.
  auto test_seed = CreateTestSeed(test_layers, test_studies);
  auto result = SeedHasMisconfiguredEntropy(client_state_, test_seed,
                                            kEntropyLimit_10bits);
  EXPECT_FALSE(result.is_misconfigured);
  ASSERT_TRUE(result.seed_has_active_low_layer.has_value());
  EXPECT_FALSE(result.seed_has_active_low_layer.value());
  EXPECT_TRUE(result.seed_has_active_limited_layer.has_value());
  EXPECT_TRUE(result.seed_has_active_limited_layer.value());
  histogram_tester_.ExpectTotalCount(kSeedRejectionReasonHistogram, 0);
}

TEST_F(LimitedEntropyRandomizationTest,
       SimultaneousLowAndLimitedLayers_LowLayerExperimentIsNotWebVisible) {
  std::vector<Layer> test_layers;
  std::vector<Study> test_studies;
  // Create LOW and LIMITED entropy layers.
  test_layers.push_back(CreateLayer(
      kLowEntropyLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LOW,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})}));
  test_layers.push_back(CreateLayer(
      kLimitedEntropyLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LIMITED,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})}));

  // Create a non-web-visible study that refers to the LOW entropy layer.
  test_studies.push_back(CreateTestStudy(
      {CreateExperiment(50), CreateExperiment(50)},
      CreateLayerMemberReference(kLowEntropyLayerId, {kTestLayerMemberId})));

  // Create an entropy-consuming study that refers to the LIMITED entropy layer.
  test_studies.push_back(
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                      CreateLayerMemberReference(kLimitedEntropyLayerId,
                                                 {kTestLayerMemberId})));

  // Verify that the seed is not rejected. The study in the LOW entropy layer
  // is not entropy-consuming. Only the limited layer is active.
  auto test_seed = CreateTestSeed(test_layers, test_studies);
  auto result = SeedHasMisconfiguredEntropy(client_state_, test_seed,
                                            kEntropyLimit_10bits);
  EXPECT_FALSE(result.is_misconfigured);
  ASSERT_TRUE(result.seed_has_active_low_layer.has_value());
  EXPECT_FALSE(result.seed_has_active_low_layer.value());
  EXPECT_TRUE(result.seed_has_active_limited_layer.has_value());
  EXPECT_TRUE(result.seed_has_active_limited_layer.value());
  histogram_tester_.ExpectTotalCount(kSeedRejectionReasonHistogram, 0);
}

TEST_F(LimitedEntropyRandomizationTest,
       SimultaneousLowAndLimitedLayers_LowLayerExperimentIsNotPermanent) {
  std::vector<Layer> test_layers;
  std::vector<Study> test_studies;
  // Create LOW and LIMITED entropy layers.
  test_layers.push_back(CreateLayer(
      kLowEntropyLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LOW,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})}));
  test_layers.push_back(CreateLayer(
      kLimitedEntropyLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LIMITED,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})}));

  // Create a session-randomized study that refers to the LOW entropy layer.
  {
    auto study = CreateTestStudy(
        CreateExperimentsWithTwoBitsOfEntropy(),
        CreateLayerMemberReference(kLowEntropyLayerId, {kTestLayerMemberId}));
    study.set_consistency(Study::SESSION);
    test_studies.push_back(std::move(study));
  }

  // Create an entropy-consuming study that refers to the LIMITED entropy layer.
  test_studies.push_back(
      CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                      CreateLayerMemberReference(kLimitedEntropyLayerId,
                                                 {kTestLayerMemberId})));

  // Verify that the seed is not rejected. The study in the LOW entropy layer
  // is not entropy-consuming. Only the limited layer is active.
  auto test_seed = CreateTestSeed(test_layers, test_studies);
  auto result = SeedHasMisconfiguredEntropy(client_state_, test_seed,
                                            kEntropyLimit_10bits);
  EXPECT_FALSE(result.is_misconfigured);
  ASSERT_TRUE(result.seed_has_active_low_layer.has_value());
  EXPECT_FALSE(result.seed_has_active_low_layer.value());
  EXPECT_TRUE(result.seed_has_active_limited_layer.has_value());
  EXPECT_TRUE(result.seed_has_active_limited_layer.value());
  histogram_tester_.ExpectTotalCount(kSeedRejectionReasonHistogram, 0);
}

TEST_F(LimitedEntropyRandomizationTest,
       SimultaneousLowAndLimitedLayers_DifferentFormFactors) {
  client_state_.form_factor = Study_FormFactor_DESKTOP;

  std::vector<Layer> test_layers;
  std::vector<Study> test_studies;
  // Create LOW and LIMITED entropy layers.
  test_layers.push_back(CreateLayer(
      kLowEntropyLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LOW,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})}));
  test_layers.push_back(CreateLayer(
      kLimitedEntropyLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LIMITED,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})}));

  // Create a DESKTOP-only entropy-consuming study that refers to the LOW
  // entropy layer.
  {
    auto study = CreateTestStudy(
        CreateExperimentsWithTwoBitsOfEntropy(),
        CreateLayerMemberReference(kLowEntropyLayerId, {kTestLayerMemberId}));
    study.mutable_filter()->add_form_factor(Study_FormFactor_DESKTOP);
    test_studies.push_back(std::move(study));
  }

  // Create a PHONE-only entropy-consuming study that refers to the LIMITED
  // entropy layer.
  {
    auto study = CreateTestStudy(
        CreateExperimentsWithTwoBitsOfEntropy(),
        CreateLayerMemberReference(kLimitedEntropyLayerId,
                                   {kTestLayerMemberId}));
    study.mutable_filter()->add_form_factor(Study_FormFactor_PHONE);
    test_studies.push_back(std::move(study));
  }

  // Verify that the seed is not rejected for a client with either form factor,
  // since the study targeting the other form factor does not apply. Similarly,
  // a client with neither form factor has no active layers and should not
  // reject the seed.
  for (auto form_factor : {Study_FormFactor_DESKTOP, Study_FormFactor_PHONE,
                           Study_FormFactor_TABLET}) {
    client_state_.form_factor = form_factor;
    auto test_seed = CreateTestSeed(test_layers, test_studies);
    auto result = SeedHasMisconfiguredEntropy(client_state_, test_seed,
                                              kEntropyLimit_10bits);
    EXPECT_FALSE(result.is_misconfigured);
    ASSERT_TRUE(result.seed_has_active_low_layer.has_value());
    EXPECT_EQ(result.seed_has_active_low_layer.value(),
              form_factor == Study_FormFactor_DESKTOP);
    EXPECT_TRUE(result.seed_has_active_limited_layer.has_value());
    EXPECT_EQ(result.seed_has_active_limited_layer.value(),
             form_factor == Study_FormFactor_PHONE);
    histogram_tester_.ExpectTotalCount(kSeedRejectionReasonHistogram, 0);
  }
}

TEST(GetGoogleWebEntropyLimitInBits, IsPlatformSpecific) {
  constexpr double kExpectedEntropyLimitInBits =
#if BUILDFLAG(IS_ANDROID)
      21.0;
#elif BUILDFLAG(IS_IOS) || BUILDFLAG(IS_WIN)
      18.0;
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
      16.0;
#else
      1.0;
#endif
  EXPECT_EQ(GetGoogleWebEntropyLimitInBits(), kExpectedEntropyLimitInBits);
}

}  // namespace variations
