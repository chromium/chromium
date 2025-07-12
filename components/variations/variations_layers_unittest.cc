// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_layers.h"

#include <sys/types.h>

#include <cstdint>
#include <limits>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/processed_study.h"
#include "components/variations/proto/layer.pb.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

const uint32_t kLayerId = 101;
const uint32_t kLayerMemberId = 201;

// If CreateSeedWithLimitedLayer() is used to constructed the layer and the
// seed, the following values are picked so that a particular slot can be
// selected. This is used to catch any error if the entropy provider is not
// selected based on the entropy mode.
const uint32_t kTestLowEntropySource = 502;    // Will select slot 49.
const char kTestClientID[] = "client_id_809";  // Will select slot 99.
const char kTestLimitedEntropyRandomizationSource[] =
    "limited_entropy_randomization_source_964";  // Will select slot 0.

struct LayerMemberSpec {
  uint32_t id;
  uint32_t start;
  uint32_t end;
};

struct LayerSpec {
  uint32_t id;
  uint32_t num_slots;
  Layer::EntropyMode entropy_mode;
  std::vector<LayerMemberSpec> layer_members;
};

struct StudySpec {
  uint32_t layer_id;
  uint32_t layer_member_id;
};

struct SeedSpec {
  std::vector<Layer> layers;
  std::vector<Study> studies;
};

const LayerMemberSpec kSingleSlotLayerMember = {.id = kLayerMemberId,
                                                .start = 0u,
                                                .end = 0u};

Layer CreateLayer(const LayerSpec spec) {
  Layer layer;
  layer.set_id(spec.id);
  layer.set_num_slots(spec.num_slots);
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

// Creates a layer with a single 100% layer member.
Layer CreateSimpleLayer(Layer::EntropyMode entropy_mode) {
  return CreateLayer(
      {.id = kLayerId,
       .num_slots = 100,
       .entropy_mode = entropy_mode,
       .layer_members = {{.id = kLayerMemberId, .start = 0, .end = 99}}});
}

// Adds an experiment with the given name and probability to a study.
Study::Experiment* AddExperiment(const std::string& name,
                                 uint32_t probability,
                                 Study* study) {
  Study::Experiment* experiment = study->add_experiment();
  experiment->set_name(name);
  experiment->set_probability_weight(probability);
  return experiment;
}

Study CreateStudy(const StudySpec& spec) {
  Study study;
  study.set_name("TestStudy");
  AddExperiment("Experiment", 100, &study);

  auto* layer_member_reference = study.mutable_layer();
  layer_member_reference->set_layer_id(spec.layer_id);
  layer_member_reference->add_layer_member_ids(spec.layer_member_id);

  return study;
}

LayerMemberReference CreateLayerMemberReference(
    uint32_t layer_id,
    const std::vector<uint32_t>& layer_member_ids) {
  LayerMemberReference reference;
  reference.set_layer_id(layer_id);
  for (uint32_t layer_member_id : layer_member_ids) {
    reference.add_layer_member_ids(layer_member_id);
  }
  return reference;
}

// Creates a study with the given `consistency`, and two 50% groups.
Study CreateTwoArmStudy(Study_Consistency consistency) {
  Study study;
  study.set_name("TestStudy");
  study.set_consistency(consistency);

  AddExperiment("Group1", 50, &study);
  AddExperiment("Group2", 50, &study);
  return study;
}

// Adds a google_web_experiment_id to each experiment in the given `study`. The
// first experiments will use 100001, the second 100002, and so on.
void AddGoogleExperimentIds(Study* study) {
  for (int i = 0; i < study->experiment_size(); ++i) {
    Study::Experiment* experiment = study->mutable_experiment(i);
    experiment->set_google_web_experiment_id(100001 + i);
  }
}

void ConstrainToLayer(Study* study,
                      const LayerMemberReference& layer_member_reference) {
  LayerMemberReference* reference_to_fill = study->mutable_layer();
  reference_to_fill->MergeFrom(layer_member_reference);
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

// Creates a seed that contains the given study constrained to a layer with the
// given entropy mode. This will modify the given study to be layer constrained,
// but returned seed will have a copy of the modified study.
VariationsSeed CreateSeedWithLayerConstrainedStudy(
    Layer::EntropyMode layer_entropy_mode,
    Study* study_to_be_constrained) {
  Layer layer = CreateSimpleLayer(layer_entropy_mode);
  ConstrainToLayer(study_to_be_constrained,
                   CreateLayerMemberReference(kLayerId, {kLayerMemberId}));
  return CreateSeed({.layers = {layer}, .studies = {*study_to_be_constrained}});
}

VariationsSeed CreateSeedWithLimitedLayer() {
  return CreateSeed(
      {.layers = {CreateLayer({.id = kLayerId,
                               .num_slots = 100u,
                               .entropy_mode = Layer::LIMITED,
                               .layer_members = {kSingleSlotLayerMember}})},
       .studies = {CreateStudy(
           {.layer_id = kLayerId, .layer_member_id = kLayerMemberId})}});
}

enum EntropyProviderSelection {
  NOT_SELECTED = 0,
  SESSION = 1,
  DEFAULT = 2,
  LOW = 3,
  LIMITED = 4,
};

class FakeEntropyProviders : public EntropyProviders {
 public:
  explicit FakeEntropyProviders(
      std::string_view high_entropy_value,
      std::string_view limited_entropy_randomization_source =
          kTestLimitedEntropyRandomizationSource)
      : EntropyProviders(high_entropy_value,
                         {.value = 0, .range = 100},
                         limited_entropy_randomization_source) {}
  ~FakeEntropyProviders() override = default;

  const base::FieldTrial::EntropyProvider& default_entropy() const override {
    selection_ = EntropyProviderSelection::DEFAULT;
    return EntropyProviders::default_entropy();
  }
  const base::FieldTrial::EntropyProvider& low_entropy() const override {
    selection_ = EntropyProviderSelection::LOW;
    return EntropyProviders::low_entropy();
  }
  const base::FieldTrial::EntropyProvider& session_entropy() const override {
    selection_ = EntropyProviderSelection::SESSION;
    return EntropyProviders::session_entropy();
  }
  const base::FieldTrial::EntropyProvider& limited_entropy() const override {
    selection_ = EntropyProviderSelection::LIMITED;
    return EntropyProviders::limited_entropy();
  }
  EntropyProviderSelection selection() { return selection_; }

 private:
  // The "mutable" keyword allows `selection_` to be updated in const functions.
  mutable EntropyProviderSelection selection_ =
      EntropyProviderSelection::NOT_SELECTED;
};

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
  auto layer = CreateLayer({.id = kLayerId,
                            .num_slots = 100u,
                            .entropy_mode = Layer::DEFAULT,
                            .layer_members = {kSingleSlotLayerMember}});
  auto study =
      CreateStudy({.layer_id = kLayerId, .layer_member_id = kLayerMemberId});
  // Creating a seed with 3 layers using the same ID.
  auto seed = CreateSeed({.layers = {layer, layer, layer}, .studies = {study}});

  VariationsLayers layers(seed, entropy_providers_);

  EXPECT_FALSE(layers.IsLayerMemberActive(
      CreateLayerMemberReference(kLayerId, {kLayerMemberId})));
  // InvalidLayerReason::LayerIDNotUnique. Assert on the integer enum value in
  // case the semantics change over time.
  const int expected_bucket = 7;
  // The metric should only be reported once.
  histogram_tester_.ExpectUniqueSample("Variations.InvalidLayerReason",
                                       expected_bucket, 1);
}

TEST_F(VariationsLayersTest, LayersAllHaveUniqueIDs) {
  const uint32_t layer_id_1 = kLayerId;
  const uint32_t layer_id_2 = kLayerId + 1;

  const auto layer_1 = CreateLayer(
      {.id = layer_id_1,
       .num_slots = 100u,
       .entropy_mode = Layer::DEFAULT,
       .layer_members = {{.id = kLayerMemberId, .start = 0u, .end = 99u}}});
  const auto layer_2 = CreateLayer(
      {.id = layer_id_2,
       .num_slots = 100u,
       .entropy_mode = Layer::DEFAULT,
       .layer_members = {{.id = kLayerMemberId, .start = 0u, .end = 99u}}});

  const auto study_1 =
      CreateStudy({.layer_id = layer_id_1, .layer_member_id = kLayerMemberId});
  const auto study_2 =
      CreateStudy({.layer_id = layer_id_2, .layer_member_id = kLayerMemberId});

  auto seed =
      CreateSeed({.layers = {layer_1, layer_2}, .studies = {study_1, study_2}});

  VariationsLayers layers(seed, entropy_providers_);

  EXPECT_TRUE(layers.IsLayerMemberActive(
      CreateLayerMemberReference(layer_id_1, {kLayerMemberId})));
  EXPECT_TRUE(layers.IsLayerMemberActive(
      CreateLayerMemberReference(layer_id_2, {kLayerMemberId})));
  histogram_tester_.ExpectTotalCount("Variations.InvalidLayerReason", 0);
}

TEST_F(VariationsLayersTest, ValidLimitedLayer) {
  VariationsLayers layers(CreateSeedWithLimitedLayer(), entropy_providers_);

  EXPECT_TRUE(layers.IsLayerActive(kLayerId));
  EXPECT_TRUE(layers.IsLayerMemberActive(
      CreateLayerMemberReference(kLayerId, {kLayerMemberId})));
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

  EXPECT_FALSE(layers.IsLayerActive(kLayerId));
  EXPECT_FALSE(layers.IsLayerMemberActive(
      CreateLayerMemberReference(kLayerId, {kLayerMemberId})));
  // InvalidLayerReason::kLimitedLayerDropped. Assert on the
  // integer enum value in case the semantics change over time.
  const int expected_bucket = 8;
  histogram_tester_.ExpectUniqueSample("Variations.InvalidLayerReason",
                                       expected_bucket, 1);
}

TEST_F(VariationsLayersTest, ValidSlotBounds) {
  auto representable_max = std::numeric_limits<uint32_t>::max();
  auto study = CreateStudy({.layer_id = 1u, .layer_member_id = 1u});
  auto layer =
      CreateLayer({.id = 1u,
                   .num_slots = representable_max,
                   .entropy_mode = Layer::DEFAULT,
                   .layer_members = {
                       {.id = 1u, .start = 0u, .end = representable_max - 1}}});
  EXPECT_TRUE(VariationsLayers::AreSlotBoundsValid(layer));
}

TEST_F(VariationsLayersTest, InvalidSlotBounds_ReferringToOutOfBoundsSlot) {
  auto representable_max = std::numeric_limits<uint32_t>::max();
  auto study = CreateStudy({.layer_id = 1u, .layer_member_id = 1u});
  auto layer = CreateLayer(
      {.id = 1u,
       .num_slots = representable_max,
       .entropy_mode = Layer::DEFAULT,
       .layer_members = {
           {.id = 1u, .start = 0u, .end = representable_max - 1},
           // The last slot has index `representable_max - 1` so
           // `representable_max` is out of bound.
           {.id = 2u, .start = representable_max, .end = representable_max}}});
  EXPECT_FALSE(VariationsLayers::AreSlotBoundsValid(layer));
}

TEST_F(VariationsLayersTest, UniqueLayerMemberIDs) {
  const auto layer =
      CreateLayer({.id = 1u,
                   .num_slots = 10u,
                   .entropy_mode = Layer::DEFAULT,
                   .layer_members = {{.id = 1u, .start = 0u, .end = 4u},
                                     {.id = 2u, .start = 5u, .end = 9u}}});
  auto seed = CreateSeed({.layers = {layer}, .studies = {}});

  VariationsLayers layers(seed, entropy_providers_);

  // One of the two layer members must be active since together they are
  // covering all slots.
  EXPECT_TRUE(
      layers.IsLayerMemberActive(CreateLayerMemberReference(1u, {1u})) ||
      layers.IsLayerMemberActive(CreateLayerMemberReference(1u, {2u})));
  histogram_tester_.ExpectTotalCount("Variations.InvalidLayerReason", 0);
}

TEST_F(VariationsLayersTest, DuplicatedLayerMemberIDs) {
  // There are two layer members with ID=1 in this layer.
  const auto layer =
      CreateLayer({.id = 1u,
                   .num_slots = 10u,
                   .entropy_mode = Layer::DEFAULT,
                   .layer_members = {{.id = 1u, .start = 0u, .end = 4u},
                                     {.id = 1u, .start = 5u, .end = 9u}}});
  auto seed = CreateSeed({.layers = {layer}, .studies = {}});

  VariationsLayers layers(seed, entropy_providers_);

  EXPECT_FALSE(
      layers.IsLayerMemberActive(CreateLayerMemberReference(1u, {1u})));
  const int expected_bucket = 9;  // kDuplicatedLayerMemberID
  histogram_tester_.ExpectUniqueSample("Variations.InvalidLayerReason",
                                       expected_bucket, 1);
}

TEST_F(VariationsLayersTest, LowEntropyStudy) {
  Study study = CreateTwoArmStudy(Study_Consistency_PERMANENT);
  AddGoogleExperimentIds(&study);
  EXPECT_FALSE(VariationsLayers::AllowsHighEntropy(study));
}

TEST_F(VariationsLayersTest, HighEntropyStudy) {
  Study study = CreateTwoArmStudy(Study_Consistency_PERMANENT);
  EXPECT_TRUE(VariationsLayers::AllowsHighEntropy(study));
}

TEST_F(VariationsLayersTest, StudyConstrainedToLowEntropyLayer) {
  Study study = CreateTwoArmStudy(Study_Consistency_PERMANENT);
  auto seed = CreateSeedWithLayerConstrainedStudy(Layer::LOW, &study);
  VariationsLayers layers(seed, entropy_providers_);
  EXPECT_FALSE(
      layers.ActiveLayerMemberDependsOnHighEntropy(study.layer().layer_id()));
}

TEST_F(VariationsLayersTest, StudyConstrainedToLimitedEntropyLayer) {
  Study study = CreateTwoArmStudy(Study_Consistency_PERMANENT);
  auto seed = CreateSeedWithLayerConstrainedStudy(Layer::LIMITED, &study);
  VariationsLayers layers(seed, entropy_providers_);
  EXPECT_FALSE(
      layers.ActiveLayerMemberDependsOnHighEntropy(study.layer().layer_id()));
}

TEST_F(VariationsLayersTest, StudyConstrainedToDefaultEntropyLayer) {
  Study study = CreateTwoArmStudy(Study_Consistency_PERMANENT);
  auto seed = CreateSeedWithLayerConstrainedStudy(Layer::DEFAULT, &study);
  VariationsLayers layers(seed, entropy_providers_);
  EXPECT_TRUE(
      layers.ActiveLayerMemberDependsOnHighEntropy(study.layer().layer_id()));
}

TEST_F(VariationsLayersTest, StudyEntropyProviderSelection_SelectLowEntropy) {
  Study study = CreateTwoArmStudy(Study_Consistency_PERMANENT);
  AddGoogleExperimentIds(&study);

  VariationsSeed seed;
  *seed.add_study() = study;
  FakeEntropyProviders slot_entropy_providers("high_entropy_value");
  VariationsLayers layers(seed, slot_entropy_providers);
  EXPECT_EQ(slot_entropy_providers.selection(),
            EntropyProviderSelection::NOT_SELECTED);  // No layers.

  FakeEntropyProviders study_entropy_providers("high_entropy_value");
  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study));
  layers.SelectEntropyProviderForStudy(processed_study,
                                       study_entropy_providers);

  // Permanently consistent, non layer constrained studies with Google
  // experiment IDs should use the low entropy provider.
  EXPECT_EQ(study_entropy_providers.selection(), EntropyProviderSelection::LOW);
}

TEST_F(VariationsLayersTest,
       StudyEntropyProviderSelection_SelectSessionEntropy) {
  Study study = CreateTwoArmStudy(Study_Consistency_SESSION);
  AddGoogleExperimentIds(&study);

  VariationsSeed seed;
  *seed.add_study() = study;
  FakeEntropyProviders slot_entropy_providers("high_entropy_value");
  VariationsLayers layers(seed, slot_entropy_providers);
  EXPECT_EQ(slot_entropy_providers.selection(),
            EntropyProviderSelection::NOT_SELECTED);  // No layers.

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study));
  FakeEntropyProviders study_entropy_providers("high_entropy_value");
  layers.SelectEntropyProviderForStudy(processed_study,
                                       study_entropy_providers);

  // Session consistent, non layer constrained studies with Google
  // experiment IDs should use the session entropy provider.
  EXPECT_EQ(study_entropy_providers.selection(),
            EntropyProviderSelection::SESSION);
}

TEST_F(VariationsLayersTest,
       StudyEntropyProviderSelection_SelectDefaultEntropy) {
  Study study = CreateTwoArmStudy(Study_Consistency_PERMANENT);

  VariationsSeed seed;
  *seed.add_study() = study;
  FakeEntropyProviders slot_entropy_providers("high_entropy_value");
  VariationsLayers layers(seed, slot_entropy_providers);
  EXPECT_EQ(slot_entropy_providers.selection(),
            EntropyProviderSelection::NOT_SELECTED);  // No layers.

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study));
  FakeEntropyProviders study_entropy_providers("high_entropy_value");
  layers.SelectEntropyProviderForStudy(processed_study,
                                       study_entropy_providers);

  // Permanently consistent, non layer constrained studies without Google
  // experiment IDs should use the default entropy provider.
  EXPECT_EQ(study_entropy_providers.selection(),
            EntropyProviderSelection::DEFAULT);
}

TEST_F(VariationsLayersTest, StudyEntropyProviderSelection_NoHighEntropyValue) {
  Study study = CreateTwoArmStudy(Study_Consistency_PERMANENT);

  VariationsSeed seed;
  *seed.add_study() = study;
  FakeEntropyProviders slot_entropy_providers(/*high_entropy_value=*/"");
  VariationsLayers layers(seed, slot_entropy_providers);
  EXPECT_EQ(slot_entropy_providers.selection(),
            EntropyProviderSelection::NOT_SELECTED);  // No layers.

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study));
  FakeEntropyProviders study_entropy_providers(/*high_entropy_value=*/"");
  layers.SelectEntropyProviderForStudy(processed_study,
                                       study_entropy_providers);

  // Without an high entropy value, low entropy provider should be used although
  // the study is eligible for high entropy.
  EXPECT_EQ(study_entropy_providers.selection(), EntropyProviderSelection::LOW);
}

TEST_F(
    VariationsLayersTest,
    StudyEntropyProviderSelection_NoHighEntropy_ConstrainedToHighEntropyLayer) {
  Layer layer = CreateSimpleLayer(/*entropy_mode=*/Layer::DEFAULT);
  Study study = CreateTwoArmStudy(Study_Consistency_PERMANENT);
  ConstrainToLayer(&study,
                   CreateLayerMemberReference(kLayerId, {kLayerMemberId}));

  VariationsSeed seed;
  *seed.add_study() = study;
  *seed.add_layers() = layer;
  FakeEntropyProviders slot_entropy_providers(/*high_entropy_value=*/"");
  VariationsLayers layers(seed, slot_entropy_providers);
  // Default entropy is low entropy when there is no high entropy value.
  EXPECT_EQ(slot_entropy_providers.selection(),
            EntropyProviderSelection::DEFAULT);

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study));
  FakeEntropyProviders study_entropy_providers(/*high_entropy_value=*/"");
  layers.SelectEntropyProviderForStudy(processed_study,
                                       study_entropy_providers);

  // Without a high entropy value, the slot selection should use the default
  // entropy (which is low entropy in this case). A study that allows high
  // entropy should be also randomized with low entropy. Because the de facto
  // low entropy study is constrained to the de facto low entropy layer, the
  // study should use the remainder entropy. The following shows that
  // `study_entropy_providers` is not selected since the remainder entropy from
  // slot selection is used.
  EXPECT_EQ(study_entropy_providers.selection(),
            EntropyProviderSelection::NOT_SELECTED);
}

TEST_F(VariationsLayersTest,
       StudyEntropyProviderSelection_HighEntropyStudyInHighEntropyLayer) {
  Layer layer = CreateSimpleLayer(/*entropy_mode=*/Layer::DEFAULT);
  Study study = CreateTwoArmStudy(Study_Consistency_PERMANENT);
  ConstrainToLayer(&study,
                   CreateLayerMemberReference(kLayerId, {kLayerMemberId}));

  VariationsSeed seed;
  *seed.add_study() = study;
  *seed.add_layers() = layer;
  FakeEntropyProviders slot_entropy_providers("high_entropy_value");
  VariationsLayers layers(seed, slot_entropy_providers);
  EXPECT_EQ(slot_entropy_providers.selection(),
            EntropyProviderSelection::DEFAULT);

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study));
  FakeEntropyProviders study_entropy_providers("high_entropy_value");
  layers.SelectEntropyProviderForStudy(processed_study,
                                       study_entropy_providers);

  // A study that is eligible to use high entropy and constrained to a layer
  // with `EntropyMode.DEFAULT` should use the default entropy provider.
  EXPECT_EQ(study_entropy_providers.selection(),
            EntropyProviderSelection::DEFAULT);
}

TEST_F(VariationsLayersTest,
       StudyEntropyProviderSelection_LowEntropyStudyInLowEntropyLayer) {
  Layer layer = CreateSimpleLayer(/*entropy_mode=*/Layer::LOW);
  Study study = CreateTwoArmStudy(Study_Consistency_PERMANENT);
  AddGoogleExperimentIds(&study);
  ConstrainToLayer(&study,
                   CreateLayerMemberReference(kLayerId, {kLayerMemberId}));

  VariationsSeed seed;
  *seed.add_study() = study;
  *seed.add_layers() = layer;
  FakeEntropyProviders slot_entropy_providers("high_entropy_value");
  VariationsLayers layers(seed, slot_entropy_providers);
  EXPECT_EQ(slot_entropy_providers.selection(), EntropyProviderSelection::LOW);

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study));
  FakeEntropyProviders study_entropy_providers("high_entropy_value");
  layers.SelectEntropyProviderForStudy(processed_study,
                                       study_entropy_providers);

  // A study that is NOT eligible to use high entropy and constrained to a layer
  // with `EntropyMode.LOW` should use the remainder entropy from slot
  // randomization. Therefore a selection is not made on
  // `study_entropy_providers`.
  EXPECT_EQ(study_entropy_providers.selection(),
            EntropyProviderSelection::NOT_SELECTED);
}

TEST_F(VariationsLayersTest,
       StudyEntropyProviderSelection_HighEntropyStudyInLowEntropyLayer) {
  Layer layer = CreateSimpleLayer(/*entropy_mode=*/Layer::LOW);
  Study study = CreateTwoArmStudy(Study_Consistency_PERMANENT);
  ConstrainToLayer(&study,
                   CreateLayerMemberReference(kLayerId, {kLayerMemberId}));

  VariationsSeed seed;
  *seed.add_study() = study;
  *seed.add_layers() = layer;
  FakeEntropyProviders slot_entropy_providers("high_entropy_value");
  VariationsLayers layers(seed, slot_entropy_providers);
  EXPECT_EQ(slot_entropy_providers.selection(), EntropyProviderSelection::LOW);

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study));
  FakeEntropyProviders study_entropy_providers("high_entropy_value");
  layers.SelectEntropyProviderForStudy(processed_study,
                                       study_entropy_providers);

  // A study that is eligible to use high entropy AND constrained to a layer
  // with `EntropyMode.LOW` should use the default entropy provider.
  EXPECT_EQ(study_entropy_providers.selection(),
            EntropyProviderSelection::DEFAULT);
}

TEST_F(VariationsLayersTest,
       StudyEntropyProviderSelection_LimitedEntropyStudyInLimitedEntropyLayer) {
  Layer layer = CreateSimpleLayer(/*entropy_mode=*/Layer::LIMITED);
  Study study = CreateTwoArmStudy(Study_Consistency_PERMANENT);
  AddGoogleExperimentIds(&study);
  ConstrainToLayer(&study,
                   CreateLayerMemberReference(kLayerId, {kLayerMemberId}));

  VariationsSeed seed;
  *seed.add_study() = study;
  *seed.add_layers() = layer;
  FakeEntropyProviders slot_entropy_providers("high_entropy_value");
  VariationsLayers layers(seed, slot_entropy_providers);
  EXPECT_EQ(slot_entropy_providers.selection(),
            EntropyProviderSelection::LIMITED);

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study));
  FakeEntropyProviders study_entropy_providers("high_entropy_value");
  layers.SelectEntropyProviderForStudy(processed_study,
                                       study_entropy_providers);

  // A study with Google web experiment ID and is constrained to a layer with
  // `EntropyMode.LIMITED` should use the limited entropy provider.
  EXPECT_EQ(study_entropy_providers.selection(),
            EntropyProviderSelection::LIMITED);
}

TEST_F(
    VariationsLayersTest,
    StudyEntropyProviderSelection_NonLimitedEntropyStudyInLimitedEntropyLayer) {
  Layer layer = CreateSimpleLayer(/*entropy_mode=*/Layer::LIMITED);
  // Constructs a study without Google web experiment ID.
  Study study = CreateTwoArmStudy(Study_Consistency_PERMANENT);
  ConstrainToLayer(&study,
                   CreateLayerMemberReference(kLayerId, {kLayerMemberId}));

  VariationsSeed seed;
  *seed.add_study() = study;
  *seed.add_layers() = layer;
  FakeEntropyProviders slot_entropy_providers("high_entropy_value");
  VariationsLayers layers(seed, slot_entropy_providers);
  EXPECT_EQ(slot_entropy_providers.selection(),
            EntropyProviderSelection::LIMITED);

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study));
  FakeEntropyProviders study_entropy_providers("high_entropy_value");
  layers.SelectEntropyProviderForStudy(processed_study,
                                       study_entropy_providers);

  // A study constrained to a layer with `EntropyMode.LIMITED` should use the
  // limited entropy provider, regardless of whether it has Google web
  // experiment IDs or not.
  EXPECT_EQ(study_entropy_providers.selection(),
            EntropyProviderSelection::LIMITED);
}

TEST_F(VariationsLayersTest, StudyEntropyProviderSelection_NoLimitedSource) {
  Layer layer = CreateSimpleLayer(/*entropy_mode=*/Layer::LIMITED);
  Study study = CreateTwoArmStudy(Study_Consistency_PERMANENT);
  AddGoogleExperimentIds(&study);
  ConstrainToLayer(&study,
                   CreateLayerMemberReference(kLayerId, {kLayerMemberId}));

  VariationsSeed seed;
  *seed.add_study() = study;
  *seed.add_layers() = layer;
  FakeEntropyProviders slot_entropy_providers(
      "high_entropy_value",
      /*limited_entropy_randomization_source=*/std::string_view());
  VariationsLayers layers(seed, slot_entropy_providers);
  EXPECT_EQ(slot_entropy_providers.selection(),
            EntropyProviderSelection::NOT_SELECTED);
  EXPECT_FALSE(layers.IsLayerActive(kLayerId));

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study));
  FakeEntropyProviders study_entropy_providers(
      "high_entropy_value",
      /*limited_entropy_randomization_source=*/std::string_view());
  auto selected = layers.SelectEntropyProviderForStudy(processed_study,
                                                       study_entropy_providers);
  EXPECT_FALSE(selected.has_value());
  EXPECT_EQ(study_entropy_providers.selection(),
            EntropyProviderSelection::NOT_SELECTED);
}

TEST_F(VariationsLayersTest, IsReferencingLayerMemberId_IncludeLayerMembers) {
  LayerMemberReference layer_member_reference =
      CreateLayerMemberReference(kLayerId, {42, 43});
  EXPECT_FALSE(
      VariationsLayers::IsReferencingLayerMemberId(layer_member_reference, 41));
  EXPECT_TRUE(
      VariationsLayers::IsReferencingLayerMemberId(layer_member_reference, 42));
  EXPECT_TRUE(
      VariationsLayers::IsReferencingLayerMemberId(layer_member_reference, 43));
}

TEST_F(VariationsLayersTest,
       IsReferencingLayerMemberId_IncludeLayerMembers_LegacyField) {
  LayerMemberReference layer_member_reference;
  layer_member_reference.set_layer_id(1);
  // `layer_member_id` is a legacy field that should be replaced with
  // `layer_member_ids`.
  // TODO(crbug.com/TBA): remove this test after the legacy field is deprecated.
  layer_member_reference.set_layer_member_id(42);

  EXPECT_FALSE(
      VariationsLayers::IsReferencingLayerMemberId(layer_member_reference, 41));
  EXPECT_TRUE(
      VariationsLayers::IsReferencingLayerMemberId(layer_member_reference, 42));
}

TEST_F(VariationsLayersTest, IsReferencingLayerMemberId_NoLayerMembers) {
  LayerMemberReference layer_member_reference =
      CreateLayerMemberReference(kLayerId, {});
  EXPECT_FALSE(
      VariationsLayers::IsReferencingLayerMemberId(layer_member_reference, 42));
  EXPECT_FALSE(
      VariationsLayers::IsReferencingLayerMemberId(layer_member_reference, 43));
}

}  // namespace variations
