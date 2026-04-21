// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_broker_state.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/manifest_builder.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/test_manifest_asset_manager_component_state.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/model_execution/test/response_holder.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/example_for_testing.pb.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

class ScenarioBuilder final {
 public:
  explicit ScenarioBuilder(
      TestManifestAssetManagerComponentState& component_state)
      : state_(component_state) {
    manifest_directory_ =
        std::make_unique<ManifestComponentDirectory>(proto::Manifest{});
  }

  ScenarioBuilder& AddBaseModel(const std::string& name) {
    state_->UpdateBaseModel(name + "_key", []() {
      auto base_model_asset = std::make_unique<FakeBaseModelAsset>();
      base_model_asset->set_version("1.0.0.0");
      return base_model_asset;
    }());
    builder.Add(name + "_asset", OnDemandComponent(name + "_key", "1.0.0.0"));
    builder.Add(name + "_recipe",
                BaseModelRecipe(
                    FileReference(name + "_asset", "weights.bin"),
                    BaseModelRecipeArgs(
                        proto::BaseModelRecipe::BACKEND_TYPE_CPU,
                        proto::BaseModelRecipe::PERFORMANCE_HINT_UNSPECIFIED,
                        {}, 100)));
    return *this;
  }

  ScenarioBuilder& AddSafetyModel(const std::string& name) {
    state_->UpdateSafetyModel(
        name + "_key",
        std::make_unique<FakeSafetyModelAsset>(FakeSafetyModelAsset::Content{
            .model_info_version = 1,
        }));
    builder.Add(name + "_asset", OnDemandComponent(name + "_key", "1"));
    builder.Add(name + "_recipe",
                SafetyModelRecipe(FileReference(name + "_asset", "ts.bin"),
                                  FileReference(name + "_asset", "lang.bin")));
    return *this;
  }

  ScenarioBuilder& AddAdaptation(const std::string& name,
                                 const std::string& base_model) {
    // TODO: Add
    return *this;
  }

  ScenarioBuilder& AddUnsafeSolution(const std::string& use_case,
                                     const std::string& model) {
    manifest_directory_->Add(use_case + "config.pb", []() {
      proto::SolutionConfig solution_config;
      *solution_config.mutable_feature() = SimpleTestFeatureConfig();
      return solution_config;
    }());
    builder.Add(
        use_case + "_solution",
        SolutionRecipe(model + "_recipe", "",
                       FileReference("manifest", use_case + "config.pb")));
    builder.Add(DeviceUseCase{DeviceCategory::kGpuHighTier, use_case},
                use_case + "_solution");
    return *this;
  }

  ScenarioBuilder& AddSafeSolution(const std::string& use_case,
                                   const std::string& model,
                                   const std::string& safety_model) {
    manifest_directory_->Add(use_case + "config.pb", []() {
      proto::SolutionConfig solution_config;
      *solution_config.mutable_feature() = SimpleComposeConfig();
      *solution_config.mutable_safety() = ComposeSafetyConfig();
      return solution_config;
    }());
    builder.Add(
        use_case + "_solution",
        SolutionRecipe(model + "_recipe", safety_model + "_recipe",
                       FileReference("manifest", use_case + "config.pb")));
    builder.Add(DeviceUseCase{DeviceCategory::kGpuHighTier, use_case},
                use_case + "_solution");
    return *this;
  }

  ScenarioBuilder& SetFeatureConfig(DeviceCategory category,
                                    const std::string& use_case,
                                    const proto::Any& config) {
    builder.SetFeatureConfig(category, use_case, config);
    return *this;
  }

  void Finish() {
    manifest_directory_->Add(builder.Build());
    state_->UpdateManifest(std::move(manifest_directory_));
  }

  raw_ref<TestManifestAssetManagerComponentState> state_;
  std::unique_ptr<ManifestComponentDirectory> manifest_directory_;
  ManifestBuilder builder;
};

}  // namespace

class ManifestBrokerStateTest : public testing::Test {
 public:
  ManifestBrokerStateTest() {}

  void Startup() {
    manifest_broker_state_ = std::make_unique<ManifestBrokerState>(
        local_state_.local_state(), component_state_.CreateDelegate(),
        fake_launcher_.LaunchFn());
    model_broker_client_ = std::make_unique<ModelBrokerClient>(
        manifest_broker_state_->BindAndPassRemoteBroker(), nullptr);
  }

  void SimulateShutdown() {
    model_broker_client_.reset();
    manifest_broker_state_.reset();
    component_state_.SimulateRestart();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedModelBrokerFeatureList scoped_feature_list_;
  ModelBrokerPrefService local_state_;
  TestManifestAssetManagerComponentState component_state_;
  on_device_model::FakeOnDeviceServiceSettings fake_settings_;
  on_device_model::FakeServiceLauncher fake_launcher_{&fake_settings_};
  // ManifestBrokerState pieces:
  std::unique_ptr<ManifestBrokerState> manifest_broker_state_;
  std::unique_ptr<ModelBrokerClient> model_broker_client_;
};

// Test that a simple feature can be executed successfully.
TEST_F(ManifestBrokerStateTest, ExecuteTestFeature) {
  ScenarioBuilder(component_state_)
      .AddBaseModel("model_A")
      .AddUnsafeSolution("test", "model_A")
      .Finish();
  Startup();
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> session_future;
  model_broker_client_->CreateSession(mojom::OnDeviceFeature::kTest,
                                      SessionConfigParams{},
                                      session_future.GetCallback());

  auto session = session_future.Take();
  ASSERT_TRUE(session);

  proto::ExampleForTestingRequest request;
  request.set_string_value("hello");

  ResponseHolder response;
  session->ExecuteModel(request, response.GetStreamingCallback());

  EXPECT_TRUE(response.GetFinalStatus());

  std::string expected_response =
      ("CPU backendFastest inference"
       "hello max:1024"
       "TopK: 3, Temp: 0.800000011920929");
  EXPECT_EQ(*response.value(), expected_response);
}

// Tests that the feature config is propagated correctly from the manifest to
// the client.
TEST_F(ManifestBrokerStateTest, PropagatesFeatureConfig) {
  proto::Any config;
  config.set_type_url("type.googleapis.com/test.Config");
  config.set_value("test_value");

  ScenarioBuilder(component_state_)
      .SetFeatureConfig(DeviceCategory::kGpuHighTier, "summarizer_api", config)
      .Finish();
  Startup();
  base::test::TestFuture<std::optional<mojo_base::ProtoWrapper>> future;
  model_broker_client_->GetConfig(mojom::OnDeviceFeature::kSummarize,
                                  future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->As<proto::Any>()->value(), "test_value");
}

// Tests that the ManifestBrokerState can handle arbitrary use cases that are
// not part of the pre-defined `mojom::OnDeviceFeature` enum.
TEST_F(ManifestBrokerStateTest, SupportsArbitraryUseCases) {
  ScenarioBuilder(component_state_)
      .AddBaseModel("model_A")
      .AddUnsafeSolution("custom_use_case", "model_A")
      .Finish();
  Startup();

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> session_future;
  model_broker_client_->CreateSession("custom_use_case", SessionConfigParams{},
                                      session_future.GetCallback());

  auto session = session_future.Take();
  ASSERT_TRUE(session);
}

}  // namespace optimization_guide
