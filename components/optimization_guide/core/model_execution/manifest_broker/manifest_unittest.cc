// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"

#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/core/model_execution/configs/manifest_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

using ::testing::UnorderedElementsAre;

using ManifestTest = testing::Test;

// Recipe args do not have interesting behavior, so we use the same values for
// all tests.
BaseModelRecipeArgs GenericRecipeArgs() {
  return BaseModelRecipeArgs(
      proto::BaseModelRecipe::BACKEND_TYPE_CPU,
      proto::BaseModelRecipe::PERFORMANCE_HINT_FASTEST_INFERENCE, {1}, 1024);
}

// There is no special behavior for different device categories, so we do
// most testing with CPU arbitrarily.
auto CreateCpuManifest(ManifestBuilder builder) {
  return Manifest::Create(base::FilePath(), builder.Build(),
                          DeviceCategory::kCpu);
}

TEST_F(ManifestTest, ManifestDeviceCategory) {
  EXPECT_EQ(base::ToString(DeviceCategory::kGpuHighTier), "gpu_high_tier");
  EXPECT_EQ(base::ToString(DeviceCategory::kGpuLowTier), "gpu_low_tier");
  EXPECT_EQ(base::ToString(DeviceCategory::kCpu), "cpu");
}

// A manifest that defines some valid elements of each type, so that tests
// can reference them.
ManifestBuilder ValidManifest() {
  return ManifestBuilder()
      .Add("valid_component", OnDemandComponent("valid_key", "1.0"))
      .Add("valid_base",
           BaseModelRecipe(FileReference("valid_component", "valid_base.bin"),
                           GenericRecipeArgs()))
      .Add(
          "valid_adaptation",
          AdaptationRecipe("valid_base", FileReference("valid_component",
                                                       "valid_adaptation.bin")))
      .Add({DeviceCategory::kCpu, "feature"},
           SolutionRecipe("valid_base",
                          FileReference(kManifestAssetName, "cpu_config.pb")));
}

TEST_F(ManifestTest, ValidManifest) {
  auto result = CreateCpuManifest(ValidManifest());
  ASSERT_TRUE(result.has_value());
}

TEST_F(ManifestTest, InvalidWithDuplicateAssetId) {
  EXPECT_EQ(CreateCpuManifest(ValidManifest().Add(
                "manifest", OnDemandComponent("bar_key", "1.0"))),
            base::unexpected(Manifest::ParseError::kDuplicateIdentifier));
}

TEST_F(ManifestTest, InvalidWithDuplicateModelRecipeId) {
  EXPECT_EQ(CreateCpuManifest(
                ValidManifest()
                    .Add("foo", BaseModelRecipe(FileReference("valid_component",
                                                              "valid_base.bin"),
                                                GenericRecipeArgs()))
                    .Add("foo", AdaptationRecipe(
                                    "valid_base",
                                    FileReference("valid_component",
                                                  "valid_adaptation.bin")))),
            base::unexpected(Manifest::ParseError::kDuplicateIdentifier));
  EXPECT_EQ(CreateCpuManifest(
                ValidManifest()
                    .Add("foo", BaseModelRecipe(FileReference("valid_component",
                                                              "valid_base.bin"),
                                                GenericRecipeArgs()))
                    .Add("foo",
                         PostprocessedModelRecipe("valid_base", "valid_base"))),
            base::unexpected(Manifest::ParseError::kDuplicateIdentifier));
}

TEST_F(ManifestTest, InvalidWithMissingSolutionReference) {
  EXPECT_EQ(CreateCpuManifest(ValidManifest().Add(
                {DeviceCategory::kCpu, "use_case"}, "missing_solution")),
            base::unexpected(Manifest::ParseError::kMissingIdentifier));
  EXPECT_EQ(CreateCpuManifest(ValidManifest().Add(
                "foo", BaseModelRecipe(
                           FileReference("missing_component", "valid_base.bin"),
                           GenericRecipeArgs()))),
            base::unexpected(Manifest::ParseError::kMissingIdentifier));
  EXPECT_EQ(
      CreateCpuManifest(ValidManifest().Add(
          "foo", AdaptationRecipe("valid_base",
                                  FileReference("missing_component",
                                                "valid_adaptation.bin")))),
      base::unexpected(Manifest::ParseError::kMissingIdentifier));
  EXPECT_EQ(
      CreateCpuManifest(ValidManifest().Add(
          "foo", AdaptationRecipe("missing_base",
                                  FileReference("valid_component",
                                                "valid_adaptation.bin")))),
      base::unexpected(Manifest::ParseError::kMissingIdentifier));
  EXPECT_EQ(CreateCpuManifest(ValidManifest().Add(
                "foo", SolutionRecipe(
                           "missing_base",
                           FileReference(kManifestAssetName, "config.pb")))),
            base::unexpected(Manifest::ParseError::kMissingIdentifier));
  EXPECT_EQ(CreateCpuManifest(ValidManifest().Add(
                "foo", SolutionRecipe(
                           "valid_base",
                           FileReference("missing_component", "config.pb")))),
            base::unexpected(Manifest::ParseError::kMissingIdentifier));
  EXPECT_EQ(CreateCpuManifest(ValidManifest().Add(
                "foo", PostprocessedModelRecipe("missing_base", "valid_base"))),
            base::unexpected(Manifest::ParseError::kMissingIdentifier));
  EXPECT_EQ(CreateCpuManifest(ValidManifest().Add(
                "foo", PostprocessedModelRecipe("valid_base",
                                                "missing_postprocessor"))),
            base::unexpected(Manifest::ParseError::kMissingIdentifier));
  EXPECT_EQ(
      CreateCpuManifest(ValidManifest().Add(
          "foo", PostprocessedModelRecipe("valid_base", "valid_adaptation"))),
      base::unexpected(Manifest::ParseError::kMissingIdentifier));
}

TEST_F(ManifestTest, PostprocessorModelExcludedWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kOnDeviceWebSpeechPolisherModel);

  auto result = CreateCpuManifest(
      ValidManifest()
          .Add("polisher_component", OnDemandComponent("polisher_key", "1.0"))
          .Add("polisher_base",
               BaseModelRecipe(
                   FileReference("polisher_component", "polisher.bin"),
                   GenericRecipeArgs()))
          .Add("postprocessed_model",
               PostprocessedModelRecipe("valid_base", "polisher_base"))
          .Add({DeviceCategory::kCpu, "speech_feature"},
               SolutionRecipe("postprocessed_model",
                              FileReference(kManifestAssetName, "config.pb"))));
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->GetRecipes().postprocessed_models().contains(
      "postprocessed_model"));
  EXPECT_FALSE(result->GetRecipes().base_models().contains("polisher_base"));
  EXPECT_FALSE(result->GetAssets().on_demand_components().contains(
      "polisher_component"));
  EXPECT_EQ(result->GetRecipes()
                .solutions()
                .at("cpu_speech_feature_solution")
                .model_recipe_id(),
            "valid_base");
  auto required_assets = result->GetRequiredAssets("speech_feature");
  ASSERT_TRUE(required_assets.has_value());
  EXPECT_THAT(*required_assets,
              UnorderedElementsAre(kManifestAssetName, "valid_component"));
}

TEST_F(ManifestTest, PostprocessorModelIncludedWhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list(kOnDeviceWebSpeechPolisherModel);

  auto result = CreateCpuManifest(
      ValidManifest()
          .Add("polisher_component", OnDemandComponent("polisher_key", "1.0"))
          .Add("polisher_base",
               BaseModelRecipe(
                   FileReference("polisher_component", "polisher.bin"),
                   GenericRecipeArgs()))
          .Add("postprocessed_model",
               PostprocessedModelRecipe("valid_base", "polisher_base"))
          .Add({DeviceCategory::kCpu, "speech_feature"},
               SolutionRecipe("postprocessed_model",
                              FileReference(kManifestAssetName, "config.pb"))));
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->GetRecipes().postprocessed_models().contains(
      "postprocessed_model"));
  EXPECT_TRUE(result->GetRecipes().base_models().contains("polisher_base"));
  EXPECT_TRUE(result->GetAssets().on_demand_components().contains(
      "polisher_component"));
  EXPECT_EQ(result->GetRecipes()
                .solutions()
                .at("cpu_speech_feature_solution")
                .model_recipe_id(),
            "postprocessed_model");
  auto required_assets = result->GetRequiredAssets("speech_feature");
  ASSERT_TRUE(required_assets.has_value());
  EXPECT_THAT(*required_assets,
              UnorderedElementsAre(kManifestAssetName, "valid_component",
                                   "polisher_component"));
}

TEST_F(ManifestTest, InvalidWithConflictingOnDemandComponents) {
  auto result = CreateCpuManifest(
      ValidManifest()
          // Add two components with the same public key.
          .Add("foo", OnDemandComponent("foo_key", "1.0"))
          .Add("bar", OnDemandComponent("foo_key", "1.0"))
          // Make both used by adding dependencies.
          .Add({DeviceCategory::kCpu, "foo_feature"},
               SolutionRecipe("valid_base", FileReference("foo", "config.pb")))
          .Add({DeviceCategory::kCpu, "bar_feature"},
               SolutionRecipe("valid_base", FileReference("bar", "config.pb"))));
  EXPECT_EQ(result,
            base::unexpected(Manifest::ParseError::kConflictingComponent));
}

TEST_F(ManifestTest, ValidWithConflictingComponentForDifferentDevices) {
  auto result = CreateCpuManifest(
      ValidManifest()
          // Add two components with the same public key.
          .Add("foo", OnDemandComponent("foo_key", "1.0"))
          .Add("bar", OnDemandComponent("foo_key", "1.0"))
          // Split the use cases between devices to avoid conflict.
          .Add({DeviceCategory::kCpu, "foo_feature"},
               SolutionRecipe("valid_base", FileReference("foo", "config.pb")))
          .Add({DeviceCategory::kGpuHighTier, "bar_feature"},
               SolutionRecipe("valid_base", FileReference("bar", "config.pb"))));
  ASSERT_TRUE(result.has_value());
}

TEST_F(ManifestTest, GetAssetByIdAndUseCasesRequiringAsset) {
  auto result = CreateCpuManifest(
      ValidManifest()
          .Add("second_component", OnDemandComponent("second_key", "2.0"))
          .Add("second_base", BaseModelRecipe(FileReference("second_component",
                                                            "second_base.bin"),
                                              GenericRecipeArgs()))
          .Add({DeviceCategory::kCpu, "second_feature"},
               SolutionRecipe("second_base",
                              FileReference("valid_component", "config.pb"))));
  ASSERT_TRUE(result.has_value());

  const proto::OnDemandComponent* comp =
      result->GetAssetById("valid_component");
  ASSERT_NE(comp, nullptr);
  EXPECT_EQ(comp->public_key(), "valid_key");
  EXPECT_EQ(comp->target_version(), "1.0");
  EXPECT_EQ(result->GetAssetById("non_existent_component"), nullptr);

  EXPECT_THAT(result->GetUseCasesRequiringAsset("valid_component"),
              UnorderedElementsAre("feature", "second_feature"));
  EXPECT_THAT(result->GetUseCasesRequiringAsset("second_component"),
              UnorderedElementsAre("second_feature"));
  EXPECT_TRUE(result->GetUseCasesRequiringAsset("unknown_asset").empty());
}

}  // namespace

}  // namespace optimization_guide
