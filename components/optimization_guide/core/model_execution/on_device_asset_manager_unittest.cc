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
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

class FakeModelProvider : public TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& model_metadata,
      OptimizationTargetModelObserver* observer) override {
    switch (optimization_target) {
      case proto::OPTIMIZATION_TARGET_TEXT_SAFETY:
        registered_for_text_safety_ = true;
        break;

      case proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION:
        registered_for_language_detection_ = true;
        break;

      default:
        NOTREACHED();
    }
  }

  void Reset() {
    registered_for_text_safety_ = false;
    registered_for_language_detection_ = false;
  }

  bool was_registered() const {
    return registered_for_text_safety_ && registered_for_language_detection_;
  }

 private:
  bool registered_for_text_safety_ = false;
  bool registered_for_language_detection_ = false;
};

class OnDeviceAssetManagerTest : public testing::Test {
 public:
  OnDeviceAssetManagerTest() {
    scoped_feature_list_.InitWithFeatures({features::kTextSafetyClassifier},
                                          {});
    model_execution::prefs::RegisterLocalStatePrefs(local_state_.registry());
    UpdatePerformanceClassPref(&local_state_,
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

  PrefService* local_state() { return &local_state_; }

  FakeModelProvider* model_provider() { return &model_provider_; }

  OnDeviceModelServiceController& service_controller() {
    return model_broker_state_.service_controller();
  }

  void Reset() { asset_manager_ = nullptr; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple local_state_;
  FakeBaseModelAsset base_model_asset_;
  TestComponentState component_state_;
  ModelBrokerState model_broker_state_{&local_state_,
                                       component_state_.CreateDelegate(),
                                       base::DoNothing()};
  FakeModelProvider model_provider_;
  std::unique_ptr<OnDeviceAssetManager> asset_manager_;
};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_F(OnDeviceAssetManagerTest, RegistersTextSafetyModelWithOverrideModel) {
  // Effectively, when an override is set, the model component will be ready
  // before ModelExecutionManager can be added as an observer.
  SetModelComponentReady();

  CreateAssetManager();

  EXPECT_TRUE(model_provider()->was_registered());
}

TEST_F(OnDeviceAssetManagerTest, RegistersTextSafetyModelIfEnabled) {
  CreateAssetManager();

  // Text safety model should not be registered until the base model is ready.
  EXPECT_FALSE(model_provider()->was_registered());

  SetModelComponentReady();

  EXPECT_TRUE(model_provider()->was_registered());
}

TEST_F(OnDeviceAssetManagerTest, DoesNotRegisterTextSafetyIfNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {features::kTextSafetyClassifier});
  CreateAssetManager();
  SetModelComponentReady();
  EXPECT_FALSE(model_provider()->was_registered());
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

TEST_F(OnDeviceAssetManagerTest, NotRegisteredWhenDisabledByEnterprisePolicy) {
  CreateAssetManager();
  model_provider()->Reset();
  local_state()->SetInteger(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(model_execution::prefs::
                           GenAILocalFoundationalModelEnterprisePolicySettings::
                               kDisallowed));
  CreateAssetManager();
  EXPECT_FALSE(model_provider()->was_registered());

  // Reset manager to make sure removing observer doesn't crash.
  Reset();
}

}  // namespace

}  // namespace optimization_guide
