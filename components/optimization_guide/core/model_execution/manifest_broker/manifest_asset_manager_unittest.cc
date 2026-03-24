// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/manifest_builder.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/test_manifest_asset_manager_component_state.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

class ManifestAssetManagerTest : public testing::Test {
 public:
  Manifest CreateDummyManifest(const std::vector<std::string>& asset_ids) {
    ManifestBuilder builder;

    // Map these dummy assets to valid DeviceUseCase so they don't get filtered
    // out. To avoid `ParseError::kDuplicateIdentifier` or missing references,
    // we give each dummy asset its own recipe chain.
    for (const std::string& id : asset_ids) {
      // Use a distinct UseCaseName for each so they don't overwrite each other
      DeviceUseCase use_case{DeviceCategory::kCpu, "dummy_use_case_" + id};

      builder.Add(id, OnDemandComponent("dummy_key_" + id, "1.0.0.0"));
      builder.Add(id + "_base_model",
                  BaseModelRecipe(
                      FileReference(id, "weights.bin"),
                      BaseModelRecipeArgs(
                          proto::BaseModelRecipe::BACKEND_TYPE_CPU,
                          proto::BaseModelRecipe::PERFORMANCE_HINT_UNSPECIFIED,
                          {}, 100)));
      builder.Add(id + "_solution",
                  SolutionRecipe(id + "_base_model", "",
                                 FileReference(id, "config.pb")));
      builder.Add(use_case, id + "_solution");
    }

    auto manifest_or = Manifest::Create(builder.Build(), DeviceCategory::kCpu);
    EXPECT_TRUE(manifest_or.has_value());
    return *manifest_or;
  }

 protected:
  TestManifestAssetManagerComponentState component_state_;
  std::unique_ptr<ManifestAssetManager> manager_{
      std::make_unique<ManifestAssetManager>(
          component_state_.CreateDelegate())};
};

TEST_F(ManifestAssetManagerTest, UpdateManifestRegistersComponents) {
  manager_->UpdateManifest(CreateDummyManifest({"asset_1", "asset_2"}));

  EXPECT_TRUE(component_state_.IsRegistered("asset_1"));
  EXPECT_TRUE(component_state_.IsRegistered("asset_2"));
  EXPECT_FALSE(component_state_.IsRegistered("asset_3"));

  EXPECT_TRUE(component_state_.WasOnDemandUpdateRequested("asset_1"));
  EXPECT_TRUE(component_state_.WasOnDemandUpdateRequested("asset_2"));
}

TEST_F(ManifestAssetManagerTest, SimulatesAssetReady) {
  manager_->UpdateManifest(CreateDummyManifest({"asset_1"}));

  EXPECT_FALSE(manager_->GetInstallDirectory("asset_1").has_value());

  base::FilePath fake_path(FILE_PATH_LITERAL("/fake/path"));
  component_state_.SimulateAssetReady("asset_1", base::Version("1.0"),
                                      fake_path);

  auto path = manager_->GetInstallDirectory("asset_1");
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ(path.value(), fake_path);
}

}  // namespace
}  // namespace optimization_guide
