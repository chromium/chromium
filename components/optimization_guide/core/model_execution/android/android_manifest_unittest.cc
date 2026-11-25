// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/android/android_manifest.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/configs/summarize_solution_config.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
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

TEST(AndroidManifestTest, LoadReportsMissingFileReadError) {
  base::ScopedTempDir directory;
  ASSERT_TRUE(directory.CreateUniqueTempDir());
  base::expected<proto::SolutionConfig, AndroidManifest::LoadResult> result =
      AndroidManifest::Load(directory.GetPath());
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error(), AndroidManifest::LoadResult::kReadFailed);
}

TEST(AndroidManifestTest, LoadReportsEmptyDirectoryReadError) {
  base::ScopedTempDir directory;
  ASSERT_TRUE(directory.CreateUniqueTempDir());
  ASSERT_TRUE(base::WriteFile(
      directory.GetPath().Append(kAndroidSummarizerConfigFileName),
      BuildAndroidSummarizeSolutionConfig().SerializeAsString()));

  // Save the current working directory so it can be restored later.
  base::FilePath original_directory;
  ASSERT_TRUE(base::GetCurrentDirectory(&original_directory));
  // Change the working directory to the temporary directory that contains the
  // config.
  ASSERT_TRUE(base::SetCurrentDirectory(directory.GetPath()));
  // An empty directory must fail to load even when the working directory has a
  // config.
  auto result = AndroidManifest::Load(base::FilePath());
  // Restore the working directory so it does not affect later tests.
  ASSERT_TRUE(base::SetCurrentDirectory(original_directory));

  ASSERT_FALSE(result);
  EXPECT_EQ(result.error(), AndroidManifest::LoadResult::kReadFailed);
}

TEST(AndroidManifestTest, LoadReportsOversizedReadError) {
  base::ScopedTempDir directory;
  ASSERT_TRUE(directory.CreateUniqueTempDir());
  const base::FilePath path =
      directory.GetPath().Append(kAndroidSummarizerConfigFileName);
  auto config = BuildAndroidSummarizeSolutionConfig();
  config.mutable_feature()->mutable_input_config()->set_request_base_name(
      std::string(1024 * 1024, 'x'));
  ASSERT_TRUE(base::WriteFile(path, config.SerializeAsString()));

  base::expected<proto::SolutionConfig, AndroidManifest::LoadResult> result =
      AndroidManifest::Load(directory.GetPath());
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error(), AndroidManifest::LoadResult::kReadFailed);
}

TEST(AndroidManifestTest, LoadReportsParseError) {
  base::ScopedTempDir directory;
  ASSERT_TRUE(directory.CreateUniqueTempDir());
  const auto path =
      directory.GetPath().Append(kAndroidSummarizerConfigFileName);
  ASSERT_TRUE(base::WriteFile(path, "invalid protobuf"));
  base::expected<proto::SolutionConfig, AndroidManifest::LoadResult> result =
      AndroidManifest::Load(directory.GetPath());
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error(), AndroidManifest::LoadResult::kParseFailed);
}

TEST(AndroidManifestTest, LoadReportsMissingFeatureError) {
  base::ScopedTempDir directory;
  ASSERT_TRUE(directory.CreateUniqueTempDir());
  const base::FilePath path =
      directory.GetPath().Append(kAndroidSummarizerConfigFileName);
  // An empty protobuf and a config without a feature are both invalid.
  ASSERT_TRUE(base::WriteFile(path, ""));
  base::expected<proto::SolutionConfig, AndroidManifest::LoadResult> result =
      AndroidManifest::Load(directory.GetPath());
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error(), AndroidManifest::LoadResult::kMissingFeature);

  proto::SolutionConfig config;
  config.mutable_model_versions()->set_text_safety_model_version(1);
  ASSERT_TRUE(base::WriteFile(path, config.SerializeAsString()));
  result = AndroidManifest::Load(directory.GetPath());
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error(), AndroidManifest::LoadResult::kMissingFeature);

  config = BuildAndroidSummarizeSolutionConfig();
  config.mutable_feature()->clear_feature();
  ASSERT_TRUE(base::WriteFile(path, config.SerializeAsString()));
  result = AndroidManifest::Load(directory.GetPath());
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error(), AndroidManifest::LoadResult::kMissingFeature);
}

TEST(AndroidManifestTest, LoadReportsMismatchedFeatureError) {
  base::ScopedTempDir directory;
  ASSERT_TRUE(directory.CreateUniqueTempDir());
  const base::FilePath path =
      directory.GetPath().Append(kAndroidSummarizerConfigFileName);
  auto config = BuildAndroidSummarizeSolutionConfig();
  config.mutable_feature()->set_feature(proto::MODEL_EXECUTION_FEATURE_TEST);
  ASSERT_TRUE(base::WriteFile(path, config.SerializeAsString()));
  base::expected<proto::SolutionConfig, AndroidManifest::LoadResult> result =
      AndroidManifest::Load(directory.GetPath());
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error(), AndroidManifest::LoadResult::kFeatureMismatch);

  config = BuildAndroidSummarizeSolutionConfig();
  ASSERT_TRUE(base::WriteFile(path, config.SerializeAsString()));
  result = AndroidManifest::Load(directory.GetPath());
  ASSERT_TRUE(result);
  EXPECT_EQ(result->feature().feature(),
            proto::MODEL_EXECUTION_FEATURE_SUMMARIZE);
}

}  // namespace
}  // namespace optimization_guide
