// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"

#include <memory>

#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/delivery/model_provider_registry.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/model_broker_state.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/on_device_base_model_metadata.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace optimization_guide {

namespace {

proto::OnDeviceBaseModelMetadata MatchingMetadata(
    const OnDeviceBaseModelSpec& spec) {
  return CreateOnDeviceBaseModelMetadata(spec.model_name, spec.model_version,
                                         {spec.selected_performance_hint});
}

}  // namespace

class OnDeviceModelAdaptationLoaderTest : public testing::Test {
 public:
  mojom::OnDeviceFeature feature() { return mojom::OnDeviceFeature::kTest; }

  proto::OptimizationTarget target() {
    return proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION;
  }

  void SendAdaptationModelUpdated(
      std::unique_ptr<optimization_guide::ModelInfo> model_info) {
    provider_.UpdateModelImmediatelyForTesting(
        proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
        std::move(model_info));
    // Wait for asset to load on background sequence.
    task_environment_.FastForwardBy(base::Seconds(1));
  }

 protected:
  void OnModelAdaptationLoaded(mojom::OnDeviceFeature feature,
                               MaybeAdaptationMetadata adaptation_metadata) {
    metadata_.MaybeUpdate(feature, std::move(adaptation_metadata));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple local_state_;
  OptimizationGuideLogger logger_;
  OnDeviceBaseModelSpec spec_{
      "Test", "0.0.1", proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY};
  AdaptationMetadataMap metadata_;
  ModelProviderRegistry provider_{&logger_};
  AdaptationLoaderMap loaders_{
      provider_,
      base::BindRepeating(
          &OnDeviceModelAdaptationLoaderTest::OnModelAdaptationLoaded,
          base::Unretained(this))};
  base::HistogramTester histogram_tester_;
};

TEST_F(OnDeviceModelAdaptationLoaderTest, RegistersWithSpecAndUsage) {
  loaders_.MaybeRegisterModelDownload(feature(), spec_, true);
  ASSERT_TRUE(provider_.HasRegistrations());
  EXPECT_EQ(metadata_.Get(feature()),
            base::unexpected(AdaptationUnavailability::kUpdatePending));
}

TEST_F(OnDeviceModelAdaptationLoaderTest, DoesNotRegisterWithoutSpec) {
  loaders_.MaybeRegisterModelDownload(feature(), std::nullopt, true);
  ASSERT_FALSE(provider_.HasRegistrations());
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kBaseModelUnavailable, 1);
  EXPECT_EQ(metadata_.Get(feature()),
            base::unexpected(AdaptationUnavailability::kUpdatePending));
}

TEST_F(OnDeviceModelAdaptationLoaderTest, DoesNotRegisterWithoutUsage) {
  loaders_.MaybeRegisterModelDownload(feature(), spec_, false);
  ASSERT_FALSE(provider_.HasRegistrations());
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kFeatureNotRecentlyUsed, 1);
  EXPECT_EQ(metadata_.Get(feature()),
            base::unexpected(AdaptationUnavailability::kUpdatePending));
}

TEST_F(OnDeviceModelAdaptationLoaderTest, ProvidesValidAsset) {
  loaders_.MaybeRegisterModelDownload(feature(), spec_, true);
  FakeAdaptationAsset asset{{
      .config = SimpleTestFeatureConfig(),
      .metadata = MatchingMetadata(spec_),
  }};
  SendAdaptationModelUpdated(std::make_unique<ModelInfo>(asset.model_info()));
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kAvailable, 1);
  ASSERT_TRUE(metadata_.Get(feature()).has_value());
  EXPECT_FALSE(metadata_.Get(feature())->asset_paths());
}

TEST_F(OnDeviceModelAdaptationLoaderTest, ProvidesValidAssetWithWeights) {
  loaders_.MaybeRegisterModelDownload(feature(), spec_, true);
  FakeAdaptationAsset asset{{
      .config = SimpleTestFeatureConfig(),
      .weight = 1,
      .metadata = MatchingMetadata(spec_),
  }};
  SendAdaptationModelUpdated(std::make_unique<ModelInfo>(asset.model_info()));
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kAvailable, 1);
  ASSERT_TRUE(metadata_.Get(feature()).has_value());
  EXPECT_TRUE(metadata_.Get(feature())->asset_paths());
}

TEST_F(OnDeviceModelAdaptationLoaderTest, ProvidesValidAssetWithEmptyHints) {
  loaders_.MaybeRegisterModelDownload(feature(), spec_, true);
  FakeAdaptationAsset asset{{
      .config = SimpleTestFeatureConfig(),
      .metadata = CreateOnDeviceBaseModelMetadata(
          spec_.model_name, spec_.model_version,
          // And Empty set of supported hints is semantically "all hints"
          {}),
  }};
  SendAdaptationModelUpdated(std::make_unique<ModelInfo>(asset.model_info()));
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kAvailable, 1);
  ASSERT_TRUE(metadata_.Get(feature()).has_value());
  EXPECT_FALSE(metadata_.Get(feature())->asset_paths());
}

TEST_F(OnDeviceModelAdaptationLoaderTest, RemovedOnInvalidAsset) {
  loaders_.MaybeRegisterModelDownload(feature(), spec_, true);
  TestModelInfoBuilder invalid_builder;
  SendAdaptationModelUpdated(invalid_builder.Build());
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kAdaptationModelInvalid, 1);
  ASSERT_FALSE(metadata_.Get(feature()).has_value());
  EXPECT_EQ(metadata_.Get(feature()).error(),
            AdaptationUnavailability::kUpdatePending);
}

TEST_F(OnDeviceModelAdaptationLoaderTest,
       IgnoresIncompatibleAdaptationVersion) {
  loaders_.MaybeRegisterModelDownload(feature(), spec_, true);
  FakeAdaptationAsset asset{{
      .config = SimpleTestFeatureConfig(),
      .metadata =
          CreateOnDeviceBaseModelMetadata(spec_.model_name,
                                          "0.0.9",  // Not compatible
                                          {spec_.selected_performance_hint}),
  }};
  SendAdaptationModelUpdated(std::make_unique<ModelInfo>(asset.model_info()));
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kAdaptationModelIncompatible, 1);
  ASSERT_FALSE(metadata_.Get(feature()).has_value());
  EXPECT_EQ(metadata_.Get(feature()).error(),
            AdaptationUnavailability::kUpdatePending);
}

TEST_F(OnDeviceModelAdaptationLoaderTest, IgnoresIncompatibleAdaptationHints) {
  loaders_.MaybeRegisterModelDownload(feature(), spec_, true);
  FakeAdaptationAsset asset{{
      .config = SimpleTestFeatureConfig(),
      .metadata = CreateOnDeviceBaseModelMetadata(
          spec_.model_name, spec_.model_version,
          {proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU}),  // Not compatible
  }};
  SendAdaptationModelUpdated(std::make_unique<ModelInfo>(asset.model_info()));
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kAdaptationModelHintsIncompatible,
      1);
  ASSERT_FALSE(metadata_.Get(feature()).has_value());
  EXPECT_EQ(metadata_.Get(feature()).error(),
            AdaptationUnavailability::kUpdatePending);
}

TEST_F(OnDeviceModelAdaptationLoaderTest,
       AdaptationModelInvalidWithoutExecutionConfig) {
  loaders_.MaybeRegisterModelDownload(feature(), spec_, true);
  FakeAdaptationAsset asset{{
      .config = SimpleTestFeatureConfig(),
      .metadata = MatchingMetadata(spec_),
  }};
  SendAdaptationModelUpdated(
      TestModelInfoBuilder(asset.model_info())
          .RemoveAdditionalFileWithBasename(kOnDeviceModelExecutionConfigFile)
          .Build());
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::
          kAdaptationModelExecutionConfigInvalid,
      1);
  ASSERT_FALSE(metadata_.Get(feature()).has_value());
  EXPECT_EQ(metadata_.Get(feature()).error(),
            AdaptationUnavailability::kNotSupported);
}

TEST_F(OnDeviceModelAdaptationLoaderTest,
       AdaptationModelInvalidMissingExecutionConfig) {
  loaders_.MaybeRegisterModelDownload(feature(), spec_, true);
  FakeAdaptationAsset asset{{
      .config = SimpleTestFeatureConfig(),
      .metadata = MatchingMetadata(spec_),
  }};
  base::DeleteFile(asset.dir().Append(kOnDeviceModelExecutionConfigFile));
  SendAdaptationModelUpdated(std::make_unique<ModelInfo>(asset.model_info()));
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::
          kAdaptationModelExecutionConfigInvalid,
      1);
  ASSERT_FALSE(metadata_.Get(feature()).has_value());
  EXPECT_EQ(metadata_.Get(feature()).error(),
            AdaptationUnavailability::kNotSupported);
}

TEST_F(OnDeviceModelAdaptationLoaderTest,
       AdaptationModelInvalidMultipleFeaturesInExecutionConfig) {
  loaders_.MaybeRegisterModelDownload(feature(), spec_, true);
  FakeAdaptationAsset asset{{
      .config = SimpleTestFeatureConfig(),
      .metadata = MatchingMetadata(spec_),
  }};

  // Overwrite the assets config with one that should be invalid, due to
  // multiple feature configs.
  proto::OnDeviceModelExecutionConfig config;
  *config.add_feature_configs() = SimpleTestFeatureConfig();
  *config.add_feature_configs() = SimpleComposeConfig();
  CHECK(base::WriteFile(asset.dir().Append(kOnDeviceModelExecutionConfigFile),
                        config.SerializeAsString()));

  SendAdaptationModelUpdated(std::make_unique<ModelInfo>(asset.model_info()));
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::
          kAdaptationModelExecutionConfigInvalid,
      1);
  ASSERT_FALSE(metadata_.Get(feature()).has_value());
  EXPECT_EQ(metadata_.Get(feature()).error(),
            AdaptationUnavailability::kNotSupported);
}

TEST_F(OnDeviceModelAdaptationLoaderTest, SendsNotSupportedOnRemoval) {
  loaders_.MaybeRegisterModelDownload(feature(), spec_, true);
  // Provider removes the target when the server says no matching model is
  // available.
  provider_.RemoveModel(target());
  ASSERT_FALSE(metadata_.Get(feature()).has_value());
  EXPECT_EQ(metadata_.Get(feature()).error(),
            AdaptationUnavailability::kNotSupported);
}

}  // namespace optimization_guide
