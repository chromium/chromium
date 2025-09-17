// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/delivery/model_provider_registry.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/fake_remote.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

class OnDeviceAssetManagerTest : public testing::Test {
 public:
  OnDeviceAssetManagerTest() {
    UpdatePerformanceClassPref(local_state(),
                               OnDeviceModelPerformanceClass::kHigh);
    model_broker_state_.Init();
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  void SetModelComponentReady() {
    base_model_asset_.SetReadyIn(model_broker_state_.component_state_manager());
  }

  void CreateAssetManager() {
    asset_manager_ = model_broker_state_.CreateAssetManager(&model_provider_);
  }

  OnDeviceAssetManager* asset_manager() { return asset_manager_.get(); }

  PrefService* local_state() { return &local_state_.local_state(); }

  OnDeviceModelServiceController& service_controller() {
    return model_broker_state_.service_controller();
  }

  void Reset() { asset_manager_ = nullptr; }

  bool IsSupplementalModelRegistered() {
    return model_provider_.IsRegistered(
        proto::OptimizationTarget::OPTIMIZATION_TARGET_LANGUAGE_DETECTION);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedModelBrokerFeatureList scoped_feature_list_;
  ModelBrokerPrefService local_state_;
  OptimizationGuideLogger logger_;
  FakeBaseModelAsset base_model_asset_;
  TestComponentState component_state_;
  ModelBrokerState model_broker_state_{&local_state_.local_state(),
                                       component_state_.CreateDelegate(),
                                       base::DoNothing()};
  ModelProviderRegistry model_provider_{&logger_};
  std::unique_ptr<OnDeviceAssetManager> asset_manager_;
};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_F(OnDeviceAssetManagerTest, RegistersTextSafetyModelWithOverrideModel) {
  // Effectively, when an override is set, the model component will be ready
  // before ModelExecutionManager can be added as an observer.
  SetModelComponentReady();

  CreateAssetManager();

  EXPECT_TRUE(IsSupplementalModelRegistered());
}

TEST_F(OnDeviceAssetManagerTest, RegistersTextSafetyModelIfEnabled) {
  CreateAssetManager();

  // Text safety model should not be registered until the base model is ready.
  EXPECT_FALSE(IsSupplementalModelRegistered());

  SetModelComponentReady();

  EXPECT_TRUE(IsSupplementalModelRegistered());
}

TEST_F(OnDeviceAssetManagerTest, DoesNotRegisterTextSafetyIfNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {features::kTextSafetyClassifier});
  CreateAssetManager();
  SetModelComponentReady();
  EXPECT_FALSE(IsSupplementalModelRegistered());
}
#endif

TEST_F(OnDeviceAssetManagerTest, DoesNotNotifyServiceControllerWrongTarget) {
  CreateAssetManager();
  FakeSafetyModelAsset fake_safety(ComposeSafetyConfig());
  asset_manager()->OnModelUpdated(proto::OPTIMIZATION_TARGET_PAGE_ENTITIES,
                                  fake_safety.model_info());

  EXPECT_FALSE(
      service_controller().GetSafetyClientForTesting().safety_model_info());
}

TEST_F(OnDeviceAssetManagerTest, NotifiesServiceController) {
  CreateAssetManager();
  FakeSafetyModelAsset fake_safety(ComposeSafetyConfig());
  asset_manager()->OnModelUpdated(proto::OPTIMIZATION_TARGET_TEXT_SAFETY,
                                  fake_safety.model_info());
  ASSERT_TRUE(
      service_controller().GetSafetyClientForTesting().safety_model_info());
}

TEST_F(OnDeviceAssetManagerTest, UpdateLanguageDetection) {
  CreateAssetManager();
  FakeLanguageModelAsset fake_language;
  asset_manager()->OnModelUpdated(proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
                                  fake_language.model_info());

  EXPECT_EQ(fake_language.model_path(), service_controller()
                                            .GetSafetyClientForTesting()
                                            .language_detection_model_path());
}

TEST_F(OnDeviceAssetManagerTest, UpdateSafetyModel) {
  CreateAssetManager();
  FakeSafetyModelAsset fake_safety_asset(ComposeSafetyConfig());
  // Safety model info is valid but no metadata.
  {
    base::HistogramTester histogram_tester;

    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetVersion(10)
            .SetAdditionalFiles(fake_safety_asset.AdditionalFiles())
            .Build();
    asset_manager()->OnModelUpdated(proto::OPTIMIZATION_TARGET_TEXT_SAFETY,
                                    *model_info);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kNoMetadata, 1);
  }

  // Safety model info is valid but metadata is of wrong type.
  {
    base::HistogramTester histogram_tester;

    proto::Any any;
    any.set_type_url("garbagetype");
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetVersion(20)
            .SetAdditionalFiles(fake_safety_asset.AdditionalFiles())
            .SetModelMetadata(any)
            .Build();
    asset_manager()->OnModelUpdated(proto::OPTIMIZATION_TARGET_TEXT_SAFETY,
                                    *model_info);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kMetadataWrongType, 1);
  }

  // Safety model info is valid but no feature configs.
  {
    base::HistogramTester histogram_tester;

    proto::TextSafetyModelMetadata model_metadata;
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetVersion(30)
            .SetAdditionalFiles(fake_safety_asset.AdditionalFiles())
            .SetModelMetadata(AnyWrapProto(model_metadata))
            .Build();
    asset_manager()->OnModelUpdated(proto::OPTIMIZATION_TARGET_TEXT_SAFETY,
                                    *model_info);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kNoFeatureConfigs, 1);
  }

  // Safety model info is valid and metadata has feature configs.
  {
    base::HistogramTester histogram_tester;

    proto::TextSafetyModelMetadata model_metadata;
    model_metadata.add_feature_text_safety_configurations()->set_feature(
        ToModelExecutionFeatureProto(ModelBasedCapabilityKey::kCompose));
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetVersion(40)
            .SetAdditionalFiles(fake_safety_asset.AdditionalFiles())
            .SetModelMetadata(AnyWrapProto(model_metadata))
            .Build();
    asset_manager()->OnModelUpdated(proto::OPTIMIZATION_TARGET_TEXT_SAFETY,
                                    *model_info);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kValid, 1);
  }

  // Duplicate model info is ignored.
  {
    base::HistogramTester histogram_tester;

    proto::TextSafetyModelMetadata model_metadata;
    model_metadata.add_feature_text_safety_configurations()->set_feature(
        ToModelExecutionFeatureProto(ModelBasedCapabilityKey::kCompose));
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetVersion(40)
            .SetAdditionalFiles(fake_safety_asset.AdditionalFiles())
            .SetModelMetadata(AnyWrapProto(model_metadata))
            .Build();
    asset_manager()->OnModelUpdated(proto::OPTIMIZATION_TARGET_TEXT_SAFETY,
                                    *model_info);

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecution.OnDeviceTextSafetyUpdateSkipped", 1);
  }
}

TEST_F(OnDeviceAssetManagerTest, NotRegisteredWhenDisabledByEnterprisePolicy) {
  SetModelComponentReady();

  CreateAssetManager();
  EXPECT_TRUE(IsSupplementalModelRegistered());
  local_state()->SetInteger(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(model_execution::prefs::
                           GenAILocalFoundationalModelEnterprisePolicySettings::
                               kDisallowed));
  Reset();
  EXPECT_FALSE(IsSupplementalModelRegistered());
  CreateAssetManager();
  EXPECT_FALSE(IsSupplementalModelRegistered());

  // Reset manager to make sure removing observer doesn't crash.
  Reset();
  EXPECT_FALSE(IsSupplementalModelRegistered());
}

TEST_F(OnDeviceAssetManagerTest,
       AdaptationModelDownloadRegisteredWhenFeatureFirstUsed) {
  // With the feature as not used yet, model observer won't be registered.
  local_state()->ClearPref(
      model_execution::prefs::localstate::kLastUsageByFeature);
  SetModelComponentReady();
  CreateAssetManager();
  auto target = *features::internal::GetOptimizationTargetForCapability(
      ModelBasedCapabilityKey::kTest);
  EXPECT_FALSE(model_provider_.IsRegistered(target));
  model_broker_state_.usage_tracker().OnDeviceEligibleFeatureUsed(
      ModelBasedCapabilityKey::kTest);
  EXPECT_TRUE(model_provider_.IsRegistered(target));
}

}  // namespace

}  // namespace optimization_guide
