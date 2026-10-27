// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/android/android_manifest.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "components/optimization_guide/core/model_execution/configs/summarize_solution_config.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

scoped_refptr<const OnDeviceModelFeatureAdapter> MakeAdapter(
    mojom::OnDeviceFeature feature) {
  auto config = SimpleTestFeatureConfig();
  config.set_feature(ToModelExecutionFeatureProto(feature));
  return base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));
}

TEST(AndroidManifestTest, CopiesAndMovesPreserveSnapshotIdentity) {
  auto original = AndroidManifest::CreateFromBuiltin();
  const auto id = original.id();
  auto copy = original;
  EXPECT_EQ(copy.id(), id);
  auto moved = std::move(copy);
  EXPECT_EQ(moved.id(), id);
  auto assigned = AndroidManifest::CreateFromBuiltin();
  EXPECT_NE(assigned.id(), id);
  assigned = original;
  EXPECT_EQ(assigned.id(), id);
  auto move_assigned = AndroidManifest::CreateFromBuiltin();
  move_assigned = std::move(assigned);
  EXPECT_EQ(move_assigned.id(), id);
  EXPECT_EQ(move_assigned.SelectConfig(mojom::OnDeviceFeature::kSummarize),
            original.SelectConfig(mojom::OnDeviceFeature::kSummarize));
  EXPECT_NE(AndroidManifest::CreateFromComponent({}).id(), id);
}

TEST(AndroidManifestTest, ComponentManifestRetainsBuiltinFallbacks) {
  AndroidManifest::ConfigMap configs;
  configs.emplace(mojom::OnDeviceFeature::kTest,
                  MakeAdapter(mojom::OnDeviceFeature::kTest));

  auto manifest = AndroidManifest::CreateFromComponent(std::move(configs));

  EXPECT_TRUE(manifest.SelectConfig(mojom::OnDeviceFeature::kSummarize));
  EXPECT_TRUE(manifest.SelectConfig(mojom::OnDeviceFeature::kTest));
}

TEST(AndroidManifestTest, EmptyComponentConfigsUseBuiltinConfig) {
  // An empty override map still produces a manifest with built-in configs.
  auto manifest = AndroidManifest::CreateFromComponent({});
  auto adapter = manifest.SelectConfig(mojom::OnDeviceFeature::kSummarize);
  ASSERT_TRUE(adapter);
  const auto& config = adapter->config();
  EXPECT_EQ(config.feature(), proto::MODEL_EXECUTION_FEATURE_SUMMARIZE);
  EXPECT_EQ(config.input_config().max_execute_tokens(),
            kAndroidMaxExecuteTokens);
  EXPECT_EQ(config.output_config().max_output_tokens(),
            kAndroidMaxOutputTokens);
}

TEST(AndroidManifestTest, ComponentOverrideAppliesAllConfigs) {
  auto summarize = MakeAdapter(mojom::OnDeviceFeature::kSummarize);
  auto test = MakeAdapter(mojom::OnDeviceFeature::kTest);
  AndroidManifest::ConfigMap configs;
  configs.emplace(mojom::OnDeviceFeature::kSummarize, summarize);
  configs.emplace(mojom::OnDeviceFeature::kTest, test);
  auto manifest = AndroidManifest::CreateFromComponent(std::move(configs));
  EXPECT_EQ(manifest.SelectConfig(mojom::OnDeviceFeature::kSummarize),
            summarize);
  EXPECT_EQ(manifest.SelectConfig(mojom::OnDeviceFeature::kTest), test);
}

}  // namespace
}  // namespace optimization_guide
