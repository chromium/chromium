// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/on_device_base_model_metadata.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

constexpr char kBaseModelName[] = "Test";
constexpr char kBaseModelVersion[] = "0.0.1";

proto::Any CreateOnDeviceBaseModelMetadata(
    const OnDeviceBaseModelSpec& model_spec) {
  proto::OnDeviceBaseModelMetadata model_metadata;
  model_metadata.set_base_model_name(model_spec.model_name);
  model_metadata.set_base_model_version(model_spec.model_version);

  std::string serialized_metadata;
  model_metadata.SerializeToString(&serialized_metadata);
  proto::Any any_proto;
  any_proto.set_value(serialized_metadata);
  any_proto.set_type_url(
      "type.googleapis.com/"
      "optimization_guide.proto.OnDeviceBaseModelMetadata");

  return any_proto;
}

void WriteConfigToFile(const base::FilePath& file_path,
                       const proto::OnDeviceModelExecutionConfig& config) {
  std::string serialized_config;
  ASSERT_TRUE(config.SerializeToString(&serialized_config));
  ASSERT_TRUE(base::WriteFile(file_path, serialized_config));
}

}  // namespace

class FakeOptimizationGuideModelProvider
    : public TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      const std::optional<proto::Any>& model_metadata,
      OptimizationTargetModelObserver* observer) override {
    optimization_target_ = optimization_target;
    model_metadata_ = model_metadata;
  }

  void RemoveObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      OptimizationTargetModelObserver* observer) override {
    EXPECT_EQ(*optimization_target_, optimization_target);
    optimization_target_ = std::nullopt;
  }

  std::optional<proto::Any> model_metadata_;
  std::optional<proto::OptimizationTarget> optimization_target_;
};

class OnDeviceModelAdaptationLoaderTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    feature_list_.InitWithFeaturesAndParameters(
        {{features::internal::kOnDeviceModelTestFeature,
          {{"enable_adaptation", "true"}}},
         {features::kOptimizationGuideModelExecution, {}},
         {features::kOptimizationGuideOnDeviceModel, {}}},
        {});
    model_execution::prefs::RegisterLocalStatePrefs(local_state_.registry());
    local_state_.SetInteger(
        model_execution::prefs::localstate::kOnDevicePerformanceClass,
        base::to_underlying(OnDeviceModelPerformanceClass::kHigh));

    on_device_component_state_manager_.get()->OnDeviceEligibleFeatureUsed(
        ModelBasedCapabilityKey::kTest);
    task_environment_.RunUntilIdle();

    adaptation_loader_ = std::make_unique<OnDeviceModelAdaptationLoader>(
        ModelBasedCapabilityKey::kTest, &model_provider_,
        on_device_component_state_manager_.get()->GetWeakPtr(), &local_state_,
        base::BindRepeating(
            &OnDeviceModelAdaptationLoaderTest::OnModelAdaptationLoaded,
            base::Unretained(this)));
    task_environment_.RunUntilIdle();
  }

  void SetNullBaseModelStateChanged() {
    adaptation_loader_->StateChanged(nullptr);
  }

  void SetBaseModelStateChanged() {
    on_device_component_state_manager_.SetReady(temp_dir());
  }

  void SendAdaptationModelUpdated(
      base::optional_ref<const optimization_guide::ModelInfo> model_info) {
    adaptation_loader_->OnModelUpdated(
        proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
        model_info);
  }

  void InvokeOnDeviceEligibleFeatureFirstUsed() {
    adaptation_loader_->OnDeviceEligibleFeatureFirstUsed(
        ModelBasedCapabilityKey::kTest);
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

 protected:
  void OnModelAdaptationLoaded(
      std::unique_ptr<OnDeviceModelAdaptationMetadata> adaptation_metadata) {
    adaptation_metadata_ = std::move(adaptation_metadata);
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  base::ScopedTempDir temp_dir_;
  TestOnDeviceModelComponentStateManager on_device_component_state_manager_{
      &local_state_};
  FakeOptimizationGuideModelProvider model_provider_;
  std::unique_ptr<OnDeviceModelAdaptationLoader> adaptation_loader_;
  std::unique_ptr<OnDeviceModelAdaptationMetadata> adaptation_metadata_;
  base::HistogramTester histogram_tester_;
};

TEST_F(OnDeviceModelAdaptationLoaderTest, BaseModelUnavailable) {
  SetNullBaseModelStateChanged();
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kBaseModelUnavailable, 1);
  EXPECT_FALSE(adaptation_metadata_);
}

TEST_F(OnDeviceModelAdaptationLoaderTest, AdaptationModelInvalid) {
  SetBaseModelStateChanged();
  EXPECT_EQ(proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
            model_provider_.optimization_target_);

  // Sned with empty model.
  TestModelInfoBuilder model_info_builder;
  SendAdaptationModelUpdated(model_info_builder.Build().get());
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kAdaptationModelInvalid, 1);
  EXPECT_FALSE(adaptation_metadata_);
}

TEST_F(OnDeviceModelAdaptationLoaderTest, AdaptationModelIncompatible) {
  SetBaseModelStateChanged();
  EXPECT_EQ(proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
            model_provider_.optimization_target_);

  // Send with incompatible base model name.
  TestModelInfoBuilder model_info_builder;
  model_info_builder
      .SetModelMetadata(CreateOnDeviceBaseModelMetadata(
          {"different_base_model_name", kBaseModelVersion}))
      .SetAdditionalFiles({
          temp_dir().Append(kOnDeviceModelAdaptationWeightsFile),
      });
  SendAdaptationModelUpdated(model_info_builder.Build().get());
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kAdaptationModelIncompatible, 1);
  EXPECT_FALSE(adaptation_metadata_);
}

TEST_F(OnDeviceModelAdaptationLoaderTest,
       AdaptationModelInvalidWithoutExecutionConfig) {
  SetBaseModelStateChanged();
  EXPECT_EQ(proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
            model_provider_.optimization_target_);

  TestModelInfoBuilder model_info_builder;
  model_info_builder
      .SetModelMetadata(
          CreateOnDeviceBaseModelMetadata({kBaseModelName, kBaseModelVersion}))
      .SetAdditionalFiles({
          temp_dir().Append(kOnDeviceModelAdaptationWeightsFile),
          temp_dir().Append(kOnDeviceModelExecutionConfigFile),
      });

  SendAdaptationModelUpdated(model_info_builder.Build().get());
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::
          kAdaptationModelExecutionConfigInvalid,
      1);
  EXPECT_FALSE(adaptation_metadata_);
}

TEST_F(OnDeviceModelAdaptationLoaderTest,
       AdaptationModelInvalidMissingExecutionConfig) {
  SetBaseModelStateChanged();
  EXPECT_EQ(proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
            model_provider_.optimization_target_);

  TestModelInfoBuilder model_info_builder;
  model_info_builder
      .SetModelMetadata(
          CreateOnDeviceBaseModelMetadata({kBaseModelName, kBaseModelVersion}))
      .SetAdditionalFiles({
          temp_dir().Append(kOnDeviceModelAdaptationWeightsFile),
      });

  proto::OnDeviceModelExecutionConfig config;
  WriteConfigToFile(temp_dir().Append(kOnDeviceModelExecutionConfigFile),
                    config);

  SendAdaptationModelUpdated(model_info_builder.Build().get());
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::
          kAdaptationModelExecutionConfigInvalid,
      1);
  EXPECT_FALSE(adaptation_metadata_);
}

TEST_F(OnDeviceModelAdaptationLoaderTest,
       AdaptationModelInvalidMultipleFeaturesInExecutionConfig) {
  SetBaseModelStateChanged();
  EXPECT_EQ(proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
            model_provider_.optimization_target_);

  TestModelInfoBuilder model_info_builder;
  model_info_builder
      .SetModelMetadata(
          CreateOnDeviceBaseModelMetadata({kBaseModelName, kBaseModelVersion}))
      .SetAdditionalFiles({
          temp_dir().Append(kOnDeviceModelAdaptationWeightsFile),
      });

  proto::OnDeviceModelExecutionConfig config;
  config.add_feature_configs()->set_feature(
      proto::MODEL_EXECUTION_FEATURE_TEST);
  config.add_feature_configs()->set_feature(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  WriteConfigToFile(temp_dir().Append(kOnDeviceModelExecutionConfigFile),
                    config);

  SendAdaptationModelUpdated(model_info_builder.Build().get());
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::
          kAdaptationModelExecutionConfigInvalid,
      1);
  EXPECT_FALSE(adaptation_metadata_);
}

TEST_F(OnDeviceModelAdaptationLoaderTest, AdaptationModelValid) {
  SetBaseModelStateChanged();
  EXPECT_EQ(proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
            model_provider_.optimization_target_);

  TestModelInfoBuilder model_info_builder;
  model_info_builder
      .SetModelMetadata(
          CreateOnDeviceBaseModelMetadata({kBaseModelName, kBaseModelVersion}))
      .SetAdditionalFiles({
          temp_dir().Append(kOnDeviceModelAdaptationWeightsFile),
          temp_dir().Append(kOnDeviceModelExecutionConfigFile),
      });

  proto::OnDeviceModelExecutionConfig config;
  config.add_feature_configs()->set_feature(
      proto::MODEL_EXECUTION_FEATURE_TEST);
  WriteConfigToFile(temp_dir().Append(kOnDeviceModelExecutionConfigFile),
                    config);

  SendAdaptationModelUpdated(model_info_builder.Build().get());
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kAvailable, 1);
  EXPECT_TRUE(adaptation_metadata_);
  EXPECT_EQ(base::FilePath(kOnDeviceModelAdaptationWeightsFile),
            adaptation_metadata_->asset_paths()->weights.BaseName());
}

TEST_F(OnDeviceModelAdaptationLoaderTest, AdaptationModelValidWithoutWeights) {
  SetBaseModelStateChanged();
  EXPECT_EQ(proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
            model_provider_.optimization_target_);

  TestModelInfoBuilder model_info_builder;
  model_info_builder
      .SetModelMetadata(
          CreateOnDeviceBaseModelMetadata({kBaseModelName, kBaseModelVersion}))
      .SetAdditionalFiles({
          temp_dir().Append(kOnDeviceModelExecutionConfigFile),
      });

  proto::OnDeviceModelExecutionConfig config;
  config.add_feature_configs()->set_feature(
      proto::MODEL_EXECUTION_FEATURE_TEST);
  WriteConfigToFile(temp_dir().Append(kOnDeviceModelExecutionConfigFile),
                    config);

  SendAdaptationModelUpdated(model_info_builder.Build().get());
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kAvailable, 1);
  EXPECT_TRUE(adaptation_metadata_);
  EXPECT_FALSE(adaptation_metadata_->asset_paths());
}

TEST_F(OnDeviceModelAdaptationLoaderTest,
       AdaptationModelDownloadRegisteredWhenFeatureFirstUsed) {
  // With the feature as not used yet, model observer won't be registered.
  local_state_.ClearPref(
      model_execution::prefs::localstate::kLastTimeTestFeatureWasUsed);
  SetBaseModelStateChanged();
  EXPECT_FALSE(model_provider_.optimization_target_);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kFeatureNotRecentlyUsed, 1);

  // When the feature is used, observer will be registered.
  local_state_.SetTime(
      model_execution::prefs::localstate::kLastTimeTestFeatureWasUsed,
      base::Time::Now());
  InvokeOnDeviceEligibleFeatureFirstUsed();
  EXPECT_EQ(proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
            model_provider_.optimization_target_);

  TestModelInfoBuilder model_info_builder;
  model_info_builder
      .SetModelMetadata(
          CreateOnDeviceBaseModelMetadata({kBaseModelName, kBaseModelVersion}))
      .SetAdditionalFiles({
          temp_dir().Append(kOnDeviceModelAdaptationWeightsFile),
          temp_dir().Append(kOnDeviceModelExecutionConfigFile),
      });

  proto::OnDeviceModelExecutionConfig config;
  config.add_feature_configs()->set_feature(
      proto::MODEL_EXECUTION_FEATURE_TEST);
  WriteConfigToFile(temp_dir().Append(kOnDeviceModelExecutionConfigFile),
                    config);

  SendAdaptationModelUpdated(model_info_builder.Build().get());
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kAvailable, 1);
  EXPECT_TRUE(adaptation_metadata_);
  EXPECT_EQ(base::FilePath(kOnDeviceModelAdaptationWeightsFile),
            adaptation_metadata_->asset_paths()->weights.BaseName());
}

}  // namespace optimization_guide
