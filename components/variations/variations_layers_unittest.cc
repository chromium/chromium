// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_layers.h"

#include <cstdint>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

const int kTestLimitedLayerId = 101;
const int kTestLimitedLayerMemberId = 201;

// If CreateSeedWithLimitedLayer() is used to constructed the layer and the
// seed, the following values are picked so that a particular slot can be
// selected. This is used to catch any error if the entropy provider is not
// selected based on the entropy mode.
const uint32_t kTestLowEntropySource = 502;    // Will select slot 49.
const char kTestClientID[] = "client_id_809";  // Will select slot 99.
const char kTestLimitedEntropyRandomizationSource[] =
    "limited_entropy_randomization_source_964";  // Will select slot 0.

struct LayerMemberSpec {
  int id;
  int start;
  int end;
};

struct LayerSpec {
  int id;
  Layer::EntropyMode entropy_mode;
  std::vector<LayerMemberSpec> layer_members;
};

struct StudySpec {
  int layer_id;
  int layer_member_id;
};

struct SeedSpec {
  std::vector<Layer> layers;
  std::vector<Study> studies;
};

const LayerMemberSpec kSingleSlotLayerMember = {.id = kTestLimitedLayerMemberId,
                                                .start = 0,
                                                .end = 0};

Layer CreateLayer(const LayerSpec spec) {
  Layer layer;
  layer.set_id(spec.id);
  layer.set_num_slots(100);
  layer.set_entropy_mode(spec.entropy_mode);

  for (const auto& layer_member_spec : spec.layer_members) {
    auto* layer_member = layer.add_members();
    layer_member->set_id(layer_member_spec.id);
    auto* slot = layer_member->add_slots();
    slot->set_start(layer_member_spec.start);
    slot->set_end(layer_member_spec.end);
  }

  return layer;
}

Study CreateStudy(const StudySpec& spec) {
  Study study;
  study.set_name("TestStudy");

  auto* experiment = study.add_experiment();
  experiment->set_name("Experiment");
  experiment->set_probability_weight(100);

  auto* layer_member_reference = study.mutable_layer();
  layer_member_reference->set_layer_id(spec.layer_id);
  layer_member_reference->set_layer_member_id(spec.layer_member_id);

  return study;
}

VariationsSeed CreateSeed(const SeedSpec& spec) {
  VariationsSeed seed;
  for (const auto& layer : spec.layers) {
    auto* layer_to_add = seed.add_layers();
    layer_to_add->MergeFrom(layer);
  }
  for (const auto& study : spec.studies) {
    auto* study_to_add = seed.add_study();
    study_to_add->MergeFrom(study);
  }
  return seed;
}

VariationsSeed CreateSeedWithLimitedLayer() {
  return CreateSeed(
      {.layers = {CreateLayer({.id = kTestLimitedLayerId,
                               .entropy_mode = Layer::LIMITED,
                               .layer_members = {kSingleSlotLayerMember}})},
       .studies = {
           CreateStudy({.layer_id = kTestLimitedLayerId,
                        .layer_member_id = kTestLimitedLayerMemberId})}});
}

}  // namespace

class VariationsLayersTest : public ::testing::Test {
 public:
  VariationsLayersTest()
      : entropy_providers_(kTestClientID,
                           {kTestLowEntropySource, 8000},
                           kTestLimitedEntropyRandomizationSource) {}

 protected:
  const EntropyProviders entropy_providers_;
  base::HistogramTester histogram_tester_;
};

TEST_F(VariationsLayersTest, LayersHaveDuplicatedID) {
  auto layer = CreateLayer({.id = kTestLimitedLayerId,
                            .entropy_mode = Layer::DEFAULT,
                            .layer_members = {kSingleSlotLayerMember}});
  auto study = CreateStudy({.layer_id = kTestLimitedLayerId,
                            .layer_member_id = kTestLimitedLayerMemberId});
  // Creating a seed with 3 layers using the same ID.
  auto seed = CreateSeed({.layers = {layer, layer, layer}, .studies = {study}});

  VariationsLayers layers(seed, entropy_providers_);

  EXPECT_FALSE(layers.IsLayerMemberActive(kTestLimitedLayerId,
                                          kTestLimitedLayerMemberId));
  // InvalidLayerReason::LayerIDNotUnique. Assert on the integer enum value in
  // case the semantics change over time.
  const int expected_bucket = 7;
  // The metric should only be reported once.
  histogram_tester_.ExpectUniqueSample("Variations.InvalidLayerReason",
                                       expected_bucket, 1);
}

TEST_F(VariationsLayersTest, LayersAllHaveUniqueIDs) {
  const int layer_id_1 = kTestLimitedLayerId;
  const int layer_id_2 = kTestLimitedLayerId + 1;

  const auto layer_1 = CreateLayer(
      {.id = layer_id_1,
       .entropy_mode = Layer::DEFAULT,
       .layer_members = {
           {.id = kTestLimitedLayerMemberId, .start = 0, .end = 99}}});
  const auto layer_2 = CreateLayer(
      {.id = layer_id_2,
       .entropy_mode = Layer::DEFAULT,
       .layer_members = {
           {.id = kTestLimitedLayerMemberId, .start = 0, .end = 99}}});

  const auto study_1 = CreateStudy(
      {.layer_id = layer_id_1, .layer_member_id = kTestLimitedLayerMemberId});
  const auto study_2 = CreateStudy(
      {.layer_id = layer_id_2, .layer_member_id = kTestLimitedLayerMemberId});

  auto seed =
      CreateSeed({.layers = {layer_1, layer_2}, .studies = {study_1, study_2}});

  VariationsLayers layers(seed, entropy_providers_);

  EXPECT_TRUE(
      layers.IsLayerMemberActive(layer_id_1, kTestLimitedLayerMemberId));
  EXPECT_TRUE(
      layers.IsLayerMemberActive(layer_id_2, kTestLimitedLayerMemberId));
  histogram_tester_.ExpectTotalCount("Variations.InvalidLayerReason", 0);
}

TEST_F(VariationsLayersTest, ValidLimitedLayer) {
  VariationsLayers layers(CreateSeedWithLimitedLayer(), entropy_providers_);

  EXPECT_TRUE(layers.IsLayerMemberActive(kTestLimitedLayerId,
                                         kTestLimitedLayerMemberId));
  histogram_tester_.ExpectTotalCount("Variations.InvalidLayerReason", 0);
}

TEST_F(VariationsLayersTest, InvalidLayer_LimitedLayerDropped) {
  // An empty limited entropy randomization indicates that limited entropy
  // randomization is not supported on this platform, or that the client is not
  // in the enabled group of the limited entropy synthetic trial.
  const EntropyProviders entropy_providers(
      kTestClientID, {kTestLowEntropySource, 8000},
      /*limited_entropy_randomization_source=*/std::string_view());

  VariationsLayers layers(CreateSeedWithLimitedLayer(), entropy_providers);

  EXPECT_FALSE(layers.IsLayerMemberActive(kTestLimitedLayerId,
                                          kTestLimitedLayerMemberId));
  // InvalidLayerReason::kLimitedLayerDropped. Assert on the
  // integer enum value in case the semantics change over time.
  const int expected_bucket = 8;
  histogram_tester_.ExpectUniqueSample("Variations.InvalidLayerReason",
                                       expected_bucket, 1);
}

}  // namespace variations
