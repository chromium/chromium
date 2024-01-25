// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_layers.h"

#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

const int kTestLimitedLayerId = 1;
const int kTestLimitedLayerMemberId = 1;

Layer CreateLayer(int layer_id, int layer_member_id) {
  Layer layer;
  layer.set_id(layer_id);
  layer.set_num_slots(100);
  layer.set_entropy_mode(Layer::DEFAULT);

  auto* layer_member = layer.add_members();
  layer_member->set_id(layer_member_id);
  auto* slot = layer_member->add_slots();
  slot->set_start(0);
  slot->set_end(99);

  return layer;
}

Study CreateStudy(int layer_id, int layer_member_id) {
  Study study;
  study.set_name("TestStudy");

  auto* experiment = study.add_experiment();
  experiment->set_name("Experiment");
  experiment->set_probability_weight(100);

  auto* layer_member_reference = study.mutable_layer();
  layer_member_reference->set_layer_id(layer_id);
  layer_member_reference->set_layer_member_id(layer_member_id);

  return study;
}

VariationsSeed CreateSeed(const std::vector<Layer>& layers,
                          const std::vector<Study> studies) {
  VariationsSeed seed;
  for (const auto& layer : layers) {
    auto* layer_to_add = seed.add_layers();
    layer_to_add->MergeFrom(layer);
  }
  for (const auto& study : studies) {
    auto* study_to_add = seed.add_study();
    study_to_add->MergeFrom(study);
  }
  return seed;
}

}  // namespace

class VariationsLayersTest : public ::testing::Test {
 public:
  VariationsLayersTest() : entropy_providers_("client_id", {0, 8000}) {}

 protected:
  const EntropyProviders entropy_providers_;
  base::HistogramTester histogram_tester_;
};

TEST_F(VariationsLayersTest, LayersHaveDuplicatedID) {
  // Creating a seed with 3 layers using the same ID.
  auto test_seed =
      CreateSeed({CreateLayer(kTestLimitedLayerId, kTestLimitedLayerMemberId),
                  CreateLayer(kTestLimitedLayerId, kTestLimitedLayerMemberId),
                  CreateLayer(kTestLimitedLayerId, kTestLimitedLayerMemberId)},
                 {CreateStudy(kTestLimitedLayerId, kTestLimitedLayerMemberId)});

  VariationsLayers layers(test_seed, entropy_providers_);

  EXPECT_FALSE(layers.IsLayerMemberActive(kTestLimitedLayerId,
                                          kTestLimitedLayerMemberId));
  // InvalidLayerReason::LayerIDNotUnique
  const int expected_bucket = 7;
  // The metric should only be reported once.
  histogram_tester_.ExpectUniqueSample("Variations.InvalidLayerReason",
                                       expected_bucket, 1);
}

TEST_F(VariationsLayersTest, LayersAllHaveUniqueIDs) {
  const int layer_id_1 = kTestLimitedLayerId;
  const int layer_id_2 = kTestLimitedLayerId + 1;
  auto test_seed =
      CreateSeed({CreateLayer(layer_id_1, kTestLimitedLayerMemberId),
                  CreateLayer(layer_id_2, kTestLimitedLayerMemberId)},
                 {CreateStudy(layer_id_1, kTestLimitedLayerMemberId),
                  CreateStudy(layer_id_2, kTestLimitedLayerMemberId)});

  VariationsLayers layers(test_seed, entropy_providers_);

  EXPECT_TRUE(
      layers.IsLayerMemberActive(layer_id_1, kTestLimitedLayerMemberId));
  EXPECT_TRUE(
      layers.IsLayerMemberActive(layer_id_2, kTestLimitedLayerMemberId));
  histogram_tester_.ExpectTotalCount("Variations.InvalidLayerReason", 0);
}

}  // namespace variations
