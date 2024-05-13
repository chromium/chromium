// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_controller.h"

#include <optional>
#include <vector>

#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_test_utils.h"
#include "components/optimization_guide/core/model_execution/test_on_device_model_component.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/cpp/test_support/test_response_holder.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

constexpr auto kFeature = ModelBasedCapabilityKey::kCompose;

class OnDeviceModelServiceAdaptationControllerTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationGuideModelExecution, {}},
         {features::kOptimizationGuideOnDeviceModel,
          {{"on_device_model_min_tokens_for_context", "10"},
           {"on_device_model_max_tokens_for_context", "22"},
           {"on_device_model_context_token_chunk_size", "4"},
           {"on_device_model_topk", "1"},
           {"on_device_model_temperature", "0"}}},
         {features::kTextSafetyClassifier,
          {{"on_device_must_use_safety_model", "false"}}},
         {features::kOptimizationGuideComposeOnDeviceEval, {{}}},
         {features::internal::kModelAdaptationCompose, {{}}}},
        {});

    prefs::RegisterLocalStatePrefs(pref_service_.registry());

    // Fake the requirements to install the model.
    pref_service_.SetInteger(
        prefs::localstate::kOnDevicePerformanceClass,
        base::to_underlying(OnDeviceModelPerformanceClass::kLow));
    pref_service_.SetTime(
        prefs::localstate::kLastTimeOnDeviceEligibleFeatureWasUsed,
        base::Time::Now());
  }

  void Initialize() {
    auto access_controller =
        std::make_unique<OnDeviceModelAccessController>(pref_service_);
    access_controller_ = access_controller.get();
    test_controller_ = base::MakeRefCounted<FakeOnDeviceModelServiceController>(
        &fake_settings_, std::move(access_controller),
        on_device_component_state_manager_.get()->GetWeakPtr());
  }

  void TearDown() override {
    access_controller_ = nullptr;
    test_controller_ = nullptr;
  }

  mojo::Remote<on_device_model::mojom::OnDeviceModel>& GetOrCreateModelRemote(
      ModelBasedCapabilityKey feature,
      on_device_model::ModelAssetPaths model_paths,
      base::optional_ref<const on_device_model::AdaptationAssetPaths>
          adaptation_assets) {
    return test_controller_->GetOrCreateModelRemote(feature, model_paths,
                                                    adaptation_assets);
  }

  void ExecuteModel(
      on_device_model::mojom::OnDeviceModel* model,
      mojo::Remote<on_device_model::mojom::Session>* session,
      mojo::PendingRemote<on_device_model::mojom::StreamingResponder> receiver,
      const std::string& input) {
    auto options = on_device_model::mojom::InputOptions::New();
    options->text = input;

    model->StartSession(session->BindNewPipeAndPassReceiver());
    session->get()->Execute(std::move(options), std::move(receiver));
  }

  mojo::Remote<on_device_model::mojom::OnDeviceModel>& base_model_remote() {
    return test_controller_->base_model_remote();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  FakeOnDeviceServiceSettings fake_settings_;
  // Owned by FakeOnDeviceModelServiceController.
  raw_ptr<OnDeviceModelAccessController> access_controller_ = nullptr;
  TestOnDeviceModelComponentStateManager on_device_component_state_manager_{
      &pref_service_};
  scoped_refptr<FakeOnDeviceModelServiceController> test_controller_;
  base::test::ScopedFeatureList feature_list_;
  OptimizationGuideLogger logger_;
};

TEST_F(OnDeviceModelServiceAdaptationControllerTest, ModelExecutionSuccess) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kOptimizationGuideComposeOnDeviceEval},
      {features::internal::kModelAdaptationCompose});

  Initialize();
  base::HistogramTester histogram_tester;
  auto& model =
      GetOrCreateModelRemote(kFeature, on_device_model::ModelAssetPaths(),
                             /*adaptation_assets=*/std::nullopt);

  mojo::Remote<on_device_model::mojom::Session> session;
  on_device_model::TestResponseHolder response_holder;

  ExecuteModel(model.get(), &session, response_holder.BindRemote(), "foo");
  response_holder.WaitForCompletion();
  EXPECT_THAT(response_holder.responses(),
              testing::ElementsAre("Input: foo\n"));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelLoadResult",
      OnDeviceModelLoadResult::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelAdaptationLoadResult", 0);
}

TEST_F(OnDeviceModelServiceAdaptationControllerTest,
       ModelAdaptationExecutionSuccess) {
  Initialize();
  base::HistogramTester histogram_tester;
  auto& model =
      GetOrCreateModelRemote(kFeature, on_device_model::ModelAssetPaths(),
                             on_device_model::AdaptationAssetPaths());

  mojo::Remote<on_device_model::mojom::Session> session;
  on_device_model::TestResponseHolder response_holder;

  ExecuteModel(model.get(), &session, response_holder.BindRemote(), "foo");
  response_holder.WaitForCompletion();
  EXPECT_THAT(response_holder.responses(),
              testing::ElementsAre("Adaptation model: 1\n", "Input: foo\n"));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelAdaptationLoadResult",
      OnDeviceModelLoadResult::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelLoadResult",
      OnDeviceModelLoadResult::kSuccess, 1);
}

TEST_F(OnDeviceModelServiceAdaptationControllerTest,
       BaseModelAndAdaptationModelExecutionSuccess) {
  Initialize();
  base::HistogramTester histogram_tester;
  auto& model =
      GetOrCreateModelRemote(kFeature, on_device_model::ModelAssetPaths(),
                             on_device_model::AdaptationAssetPaths());

  mojo::Remote<on_device_model::mojom::Session> session;
  on_device_model::TestResponseHolder response_holder;

  ExecuteModel(model.get(), &session, response_holder.BindRemote(), "foo");
  response_holder.WaitForCompletion();
  EXPECT_THAT(response_holder.responses(),
              testing::ElementsAre("Adaptation model: 1\n", "Input: foo\n"));

  mojo::Remote<on_device_model::mojom::Session> base_session;
  on_device_model::TestResponseHolder base_response_holder;

  ExecuteModel(base_model_remote().get(), &base_session,
               base_response_holder.BindRemote(), "bar");
  base_response_holder.WaitForCompletion();
  EXPECT_THAT(base_response_holder.responses(),
              testing::ElementsAre("Input: bar\n"));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelAdaptationLoadResult",
      OnDeviceModelLoadResult::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelLoadResult",
      OnDeviceModelLoadResult::kSuccess, 1);
}

}  // namespace optimization_guide
