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
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

class FakeServiceController : public OnDeviceModelServiceController {
 public:
  FakeServiceController()
      : OnDeviceModelServiceController(nullptr, nullptr, base::DoNothing()) {}

  void MaybeUpdateSafetyModel(
      base::optional_ref<const ModelInfo> model_info) override {
    received_safety_info_ = true;
  }

  bool received_safety_info() const { return received_safety_info_; }

  std::optional<base::FilePath> language_detection_model_path() {
    return OnDeviceModelServiceController::language_detection_model_path();
  }

 private:
  ~FakeServiceController() override = default;

  bool received_safety_info_ = false;
};

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
    local_state_.SetInteger(
        model_execution::prefs::localstate::kOnDevicePerformanceClass,
        base::to_underlying(OnDeviceModelPerformanceClass::kHigh));
    service_controller_ = base::MakeRefCounted<FakeServiceController>();
  }

  void CreateComponentManager() {
    component_manager_.get()->OnStartup();
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  void SetModelComponentReady() {
    component_manager_.SetReady(base_model_asset_);
  }

  void CreateAssetManager() {
    asset_manager_ = std::make_unique<OnDeviceAssetManager>(
        &local_state_, service_controller_->GetWeakPtr(),
        component_manager_.get()->GetWeakPtr(), &model_provider_);
  }

  OnDeviceAssetManager* asset_manager() { return asset_manager_.get(); }

  PrefService* local_state() { return &local_state_; }

  FakeModelProvider* model_provider() { return &model_provider_; }

  FakeServiceController* service_controller() {
    return service_controller_.get();
  }

  void Reset() { asset_manager_ = nullptr; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple local_state_;
  FakeBaseModelAsset base_model_asset_;
  scoped_refptr<FakeServiceController> service_controller_;
  TestOnDeviceModelComponentStateManager component_manager_{&local_state_};
  FakeModelProvider model_provider_;
  std::unique_ptr<OnDeviceAssetManager> asset_manager_;
};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_F(OnDeviceAssetManagerTest, RegistersTextSafetyModelWithOverrideModel) {
  // Effectively, when an override is set, the model component will be ready
  // before ModelExecutionManager can be added as an observer.
  CreateComponentManager();
  SetModelComponentReady();

  CreateAssetManager();

  EXPECT_TRUE(model_provider()->was_registered());
}

TEST_F(OnDeviceAssetManagerTest, RegistersTextSafetyModelIfEnabled) {
  CreateAssetManager();

  // Text safety model should not be registered until the base model is ready.
  EXPECT_FALSE(model_provider()->was_registered());

  CreateComponentManager();
  SetModelComponentReady();

  EXPECT_TRUE(model_provider()->was_registered());
}

TEST_F(OnDeviceAssetManagerTest, DoesNotRegisterTextSafetyIfNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {features::kTextSafetyClassifier});
  CreateAssetManager();
  CreateComponentManager();
  SetModelComponentReady();
  EXPECT_FALSE(model_provider()->was_registered());
}
#endif

TEST_F(OnDeviceAssetManagerTest, DoesNotNotifyServiceControllerWrongTarget) {
  CreateAssetManager();
  std::unique_ptr<ModelInfo> model_info =
      TestModelInfoBuilder().SetVersion(123).Build();
  asset_manager()->OnModelUpdated(proto::OPTIMIZATION_TARGET_PAGE_ENTITIES,
                                  *model_info);

  EXPECT_FALSE(service_controller()->received_safety_info());
}

TEST_F(OnDeviceAssetManagerTest, NotifiesServiceController) {
  CreateAssetManager();
  std::unique_ptr<ModelInfo> model_info =
      TestModelInfoBuilder().SetVersion(123).Build();
  asset_manager()->OnModelUpdated(proto::OPTIMIZATION_TARGET_TEXT_SAFETY,
                                  *model_info);

  EXPECT_TRUE(service_controller()->received_safety_info());
}

TEST_F(OnDeviceAssetManagerTest, UpdateLanguageDetection) {
  CreateAssetManager();
  const base::FilePath kTestPath{FILE_PATH_LITERAL("foo")};
  std::unique_ptr<ModelInfo> model_info = TestModelInfoBuilder()
                                              .SetVersion(123)
                                              .SetModelFilePath(kTestPath)
                                              .Build();
  asset_manager()->OnModelUpdated(proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
                                  *model_info);
  EXPECT_EQ(kTestPath, service_controller()->language_detection_model_path());
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
