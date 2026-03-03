// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"

#include "components/optimization_guide/core/model_execution/manifest_broker/test/manifest_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

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
  return Manifest::Create(builder.Build(), DeviceCategory::kCpu);
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
      .Add("valid_safety_model",
           SafetyModelRecipe(
               FileReference("valid_component", "valid_safety_model.bin")))
      .Add("valid_base",
           BaseModelRecipe(FileReference("valid_component", "valid_base.bin"),
                           GenericRecipeArgs()))
      .Add(
          "valid_adaptation",
          AdaptationRecipe("valid_base", FileReference("valid_component",
                                                       "valid_adaptation.bin")))
      .Add({DeviceCategory::kCpu, "feature"},
           SolutionRecipe("valid_base", "valid_safety_model",
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
                "foo", SafetyModelRecipe(FileReference(
                           "missing_component", "valid_safety_model.bin")))),
            base::unexpected(Manifest::ParseError::kMissingIdentifier));
  EXPECT_EQ(CreateCpuManifest(ValidManifest().Add(
                "foo", SolutionRecipe(
                           "missing_base", kNoSafetyModel,
                           FileReference(kManifestAssetName, "config.pb")))),
            base::unexpected(Manifest::ParseError::kMissingIdentifier));
  EXPECT_EQ(CreateCpuManifest(ValidManifest().Add(
                "foo", SolutionRecipe(
                           "valid_base", "missing_safety",
                           FileReference(kManifestAssetName, "config.pb")))),
            base::unexpected(Manifest::ParseError::kMissingIdentifier));
  EXPECT_EQ(CreateCpuManifest(ValidManifest().Add(
                "foo", SolutionRecipe(
                           "valid_base", kNoSafetyModel,
                           FileReference("missing_component", "config.pb")))),
            base::unexpected(Manifest::ParseError::kMissingIdentifier));
}

TEST_F(ManifestTest, InvalidWithConflictingOnDemandComponents) {
  auto result = CreateCpuManifest(
      ValidManifest()
          // Add two components with the same public key.
          .Add("foo", OnDemandComponent("foo_key", "1.0"))
          .Add("bar", OnDemandComponent("foo_key", "1.0"))
          // Make both used by adding dependencies.
          .Add({DeviceCategory::kCpu, "foo_feature"},
               SolutionRecipe("valid_base", kNoSafetyModel,
                              FileReference("foo", "config.pb")))
          .Add({DeviceCategory::kCpu, "bar_feature"},
               SolutionRecipe("valid_base", kNoSafetyModel,
                              FileReference("bar", "config.pb"))));
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
               SolutionRecipe("valid_base", kNoSafetyModel,
                              FileReference("foo", "config.pb")))
          .Add({DeviceCategory::kGpuHighTier, "bar_feature"},
               SolutionRecipe("valid_base", kNoSafetyModel,
                              FileReference("bar", "config.pb"))));
  ASSERT_TRUE(result.has_value());
}

}  // namespace

}  // namespace optimization_guide
