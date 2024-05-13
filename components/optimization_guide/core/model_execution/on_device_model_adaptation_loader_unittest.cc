// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/test_on_device_model_component.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/on_device_base_model_metadata.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

constexpr char kBaseModelName[] = "test_model";
constexpr char kBaseModelVersion[] = "1";

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
          {{"enable_adaptation", "true"}}}},
        {});

    adaptation_loader_ = std::make_unique<OnDeviceModelAdaptationLoader>(
        ModelBasedCapabilityKey::kTest, &model_provider_,
        on_device_component_state_manager_.get()->GetWeakPtr(),
        base::BindRepeating(
            &OnDeviceModelAdaptationLoaderTest::OnModelAdaptationLoaded,
            base::Unretained(this)));
  }

  void SetNullBaseModelStateChanged() {
    adaptation_loader_->StateChanged(nullptr);
  }

  void SetBaseModelStateChanged(
      std::optional<OnDeviceBaseModelSpec> model_spec) {
    OnDeviceModelComponentState component_state;
    component_state.model_spec_ = model_spec;
    adaptation_loader_->StateChanged(&component_state);
  }

  void SendAdaptationModelUpdated(
      base::optional_ref<const optimization_guide::ModelInfo> model_info) {
    adaptation_loader_->OnModelUpdated(
        proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
        model_info);
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

 protected:
  void OnModelAdaptationLoaded(
      std::unique_ptr<on_device_model::AdaptationAssetPaths>
          adaptations_assets) {
    adaptations_assets_ = std::move(adaptations_assets);
  }

  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple pref_service_;
  base::ScopedTempDir temp_dir_;
  TestOnDeviceModelComponentStateManager on_device_component_state_manager_{
      &pref_service_};
  FakeOptimizationGuideModelProvider model_provider_;
  std::unique_ptr<OnDeviceModelAdaptationLoader> adaptation_loader_;
  std::unique_ptr<on_device_model::AdaptationAssetPaths> adaptations_assets_;
  base::HistogramTester histogram_tester_;
};

TEST_F(OnDeviceModelAdaptationLoaderTest, BaseModelUnavailable) {
  SetNullBaseModelStateChanged();
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kBaseModelUnavailable, 1);
  EXPECT_FALSE(adaptations_assets_);
}

TEST_F(OnDeviceModelAdaptationLoaderTest, BaseModelSpecInvalid) {
  SetBaseModelStateChanged(std::nullopt);
  EXPECT_FALSE(model_provider_.optimization_target_);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kBaseModelSpecInvalid, 1);
  EXPECT_FALSE(adaptations_assets_);
}

TEST_F(OnDeviceModelAdaptationLoaderTest, AdaptationModelUnavailable) {
  SetBaseModelStateChanged(
      OnDeviceBaseModelSpec{kBaseModelName, kBaseModelVersion});
  EXPECT_EQ(proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
            model_provider_.optimization_target_);
  SendAdaptationModelUpdated(std::nullopt);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kAdaptationModelUnavailable, 1);
  EXPECT_FALSE(adaptations_assets_);
}

TEST_F(OnDeviceModelAdaptationLoaderTest, AdaptationModelInvalid) {
  SetBaseModelStateChanged(
      OnDeviceBaseModelSpec{kBaseModelName, kBaseModelVersion});
  EXPECT_EQ(proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
            model_provider_.optimization_target_);

  // Sned with empty model.
  TestModelInfoBuilder model_info_builder;
  SendAdaptationModelUpdated(model_info_builder.Build().get());
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kAdaptationModelInvalid, 1);
  EXPECT_FALSE(adaptations_assets_);
}

TEST_F(OnDeviceModelAdaptationLoaderTest, AdaptationModelIncompatible) {
  SetBaseModelStateChanged(
      OnDeviceBaseModelSpec{kBaseModelName, kBaseModelVersion});
  EXPECT_EQ(proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
            model_provider_.optimization_target_);

  // Send with incompatible base model name.
  TestModelInfoBuilder model_info_builder;
  model_info_builder
      .SetModelMetadata(CreateOnDeviceBaseModelMetadata(
          {"different_base_model_name", kBaseModelVersion}))
      .SetAdditionalFiles({
          temp_dir().Append(kOnDeviceModelAdaptationModelFile),
      });
  SendAdaptationModelUpdated(model_info_builder.Build().get());
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kAdaptationModelIncompatible, 1);
  EXPECT_FALSE(adaptations_assets_);
}

TEST_F(OnDeviceModelAdaptationLoaderTest, AdaptationModelValid) {
  SetBaseModelStateChanged(
      OnDeviceBaseModelSpec{kBaseModelName, kBaseModelVersion});
  EXPECT_EQ(proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
            model_provider_.optimization_target_);

  TestModelInfoBuilder model_info_builder;
  model_info_builder
      .SetModelMetadata(
          CreateOnDeviceBaseModelMetadata({kBaseModelName, kBaseModelVersion}))
      .SetAdditionalFiles({
          temp_dir().Append(kOnDeviceModelAdaptationModelFile),
      });
  SendAdaptationModelUpdated(model_info_builder.Build().get());
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kAvailable, 1);
  EXPECT_TRUE(adaptations_assets_);
  EXPECT_EQ(base::FilePath(kOnDeviceModelAdaptationModelFile),
            adaptations_assets_->model.BaseName());
  EXPECT_TRUE(adaptations_assets_->weights.empty());
}

TEST_F(OnDeviceModelAdaptationLoaderTest, AdaptationModelValidWithWeights) {
  SetBaseModelStateChanged(
      OnDeviceBaseModelSpec{kBaseModelName, kBaseModelVersion});
  EXPECT_EQ(proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
            model_provider_.optimization_target_);

  TestModelInfoBuilder model_info_builder;
  model_info_builder
      .SetModelMetadata(
          CreateOnDeviceBaseModelMetadata({kBaseModelName, kBaseModelVersion}))
      .SetAdditionalFiles({
          temp_dir().Append(kOnDeviceModelAdaptationModelFile),
          temp_dir().Append(kOnDeviceModelAdaptationWeightsFile),
      });
  SendAdaptationModelUpdated(model_info_builder.Build().get());
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceAdaptationModelAvailability."
      "Test",
      OnDeviceModelAdaptationAvailability::kAvailable, 1);
  EXPECT_TRUE(adaptations_assets_);
  EXPECT_EQ(base::FilePath(kOnDeviceModelAdaptationModelFile),
            adaptations_assets_->model.BaseName());
  EXPECT_EQ(base::FilePath(kOnDeviceModelAdaptationWeightsFile),
            adaptations_assets_->weights.BaseName());
}

}  // namespace optimization_guide
