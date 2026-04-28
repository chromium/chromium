// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_validation.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_broker_state.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/fake_manifest_broker.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/manifest_builder.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/scenario_builder.h"
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

struct ValidationScenario {
  std::string version = "1.0";
  bool has_prompts = true;
};

void BuildValidationScenario(TestManifestAssetManagerComponentState& state,
                             ValidationScenario scenario) {
  proto::ValidationTask task;
  task.set_version(scenario.version);
  task.set_use_case("test");
  if (scenario.has_prompts) {
    auto* prompt = task.mutable_config()->add_validation_prompts();
    prompt->set_prompt("hello");
    prompt->set_expected_output("matching");
  }

  ScenarioBuilder(state)
      .AddBaseModel("model_A")
      .AddUnsafeSolution("test", "model_A")
      .SetValidationTask(DeviceCategory::kGpuHighTier, std::move(task))
      .Finish();
}

OnDeviceModelValidationResult ConvertToOnDeviceModelValidationResult(
    int value) {
  if (value < 0 ||
      value > static_cast<int>(OnDeviceModelValidationResult::kMaxValue)) {
    return OnDeviceModelValidationResult::kUnknown;
  }
  return static_cast<OnDeviceModelValidationResult>(value);
}

class ManifestValidatorTest : public testing::Test {
 public:
  ManifestValidatorTest() = default;

  std::optional<OnDeviceModelValidationResult> GetValidationResult() {
    const auto& dict = fake_.local_state().GetDict(
        model_execution::prefs::localstate::kOnDeviceModelValidationResult);
    return dict.FindInt("result").transform(
        &ConvertToOnDeviceModelValidationResult);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeManifestBroker fake_;
};

// Validation should run once we get the assets for task's use case.
TEST_F(ManifestValidatorTest, ModelValidationSucceeds) {
  BuildValidationScenario(fake_.component_state(), {});
  fake_.settings().set_execute_result({"matching output"});
  fake_.Startup();
  fake_.client().RequestAssetsFor("test");
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(GetValidationResult(), OnDeviceModelValidationResult::kSuccess);
}

TEST_F(ManifestValidatorTest, ModelValidationSucceedsImmediatelyWithNoPrompts) {
  BuildValidationScenario(fake_.component_state(), {.has_prompts = false});
  fake_.Startup();
  fake_.client().RequestAssetsFor("test");
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(GetValidationResult(), OnDeviceModelValidationResult::kSuccess);
}

// Validation should normally successfully run once per task version.
TEST_F(ManifestValidatorTest, ModelValidationDoesNotRepeatOnSuccess) {
  BuildValidationScenario(fake_.component_state(), {.version = "1.0"});
  fake_.settings().set_execute_result({"matching output"});
  fake_.Startup();
  fake_.client().RequestAssetsFor("test");
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(GetValidationResult(), OnDeviceModelValidationResult::kSuccess);
  fake_.SimulateShutdown();
  fake_.launcher().clear_did_launch_service();
  BuildValidationScenario(fake_.component_state(), {.version = "1.0"});
  fake_.Startup();
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(fake_.launcher().did_launch_service());
}

// We should run validation again if there is a new task version.
TEST_F(ManifestValidatorTest, ModelValidationNewModelVersion) {
  BuildValidationScenario(fake_.component_state(), {.version = "1.0"});
  fake_.settings().set_execute_result({"matching output"});
  fake_.Startup();
  fake_.client().RequestAssetsFor("test");
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(GetValidationResult(), OnDeviceModelValidationResult::kSuccess);

  fake_.SimulateShutdown();
  fake_.launcher().clear_did_launch_service();

  BuildValidationScenario(fake_.component_state(), {.version = "2.0"});
  fake_.Startup();
  fake_.client().RequestAssetsFor("test");
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(GetValidationResult(), OnDeviceModelValidationResult::kSuccess);
}

// If we get a new Manifest, we should cancel validation of the old one.
TEST_F(ManifestValidatorTest,
       ModelValidationNewModelVersionCancelsPreviousValidation) {
  BuildValidationScenario(fake_.component_state(), {.version = "1.0"});
  fake_.settings().set_execute_delay(base::Seconds(10));
  fake_.settings().set_execute_result({"matching output"});
  fake_.Startup();
  fake_.client().RequestAssetsFor("test");

  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_EQ(GetValidationResult(), OnDeviceModelValidationResult::kPending);

  BuildValidationScenario(fake_.component_state(), {.version = "2.0"});

  task_environment_.FastForwardBy(base::Seconds(5));
  // TODO(crbug.com/504749700): The factory getting replaced causes session
  // disconnect, which validator interprets as a service crash. This is benign,
  // but not ideal.
  EXPECT_EQ(GetValidationResult(),
            OnDeviceModelValidationResult::kServiceCrash);

  // TODO(crbug.com/504749700): We should still do the eval for the new version.
  // task_environment_.FastForwardUntilNoTasksRemain();
  // EXPECT_EQ(GetValidationResult(), OnDeviceModelValidationResult::kSuccess);
}

// Failure should be recorded.
TEST_F(ManifestValidatorTest, ModelValidationFails) {
  BuildValidationScenario(fake_.component_state(), {});
  fake_.settings().set_execute_result({"bad output"});
  fake_.Startup();
  fake_.client().RequestAssetsFor("test");
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(GetValidationResult(),
            OnDeviceModelValidationResult::kNonMatchingOutput);
}

// Service crashes should be recorded too.
TEST_F(ManifestValidatorTest, ModelValidationFailsOnCrash) {
  BuildValidationScenario(fake_.component_state(), {});
  fake_.settings().set_execute_delay(base::Seconds(10));
  fake_.settings().set_execute_result({"matching output"});

  fake_.Startup();
  fake_.client().RequestAssetsFor("test");
  task_environment_.FastForwardBy(base::Seconds(1));
  fake_.launcher().CrashService();

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(GetValidationResult(),
            OnDeviceModelValidationResult::kServiceCrash);
}

// Validation failures may be spurious, so retry a few times.
TEST_F(ManifestValidatorTest, ModelValidationRepeatsOnFailure) {
  BuildValidationScenario(fake_.component_state(), {.version = "1.0"});
  fake_.settings().set_execute_result({"bad output"});
  fake_.Startup();
  fake_.client().RequestAssetsFor("test");
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(GetValidationResult(),
            OnDeviceModelValidationResult::kNonMatchingOutput);

  fake_.SimulateShutdown();
  fake_.launcher().clear_did_launch_service();

  BuildValidationScenario(fake_.component_state(), {.version = "1.0"});
  fake_.settings().set_execute_result({"matching output"});
  fake_.Startup();
  fake_.client().RequestAssetsFor("test");
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(GetValidationResult(), OnDeviceModelValidationResult::kSuccess);
}

// Model validation should stop after hitting max attempts.
TEST_F(ManifestValidatorTest, ModelValidationMaximumRetry) {
  BuildValidationScenario(fake_.component_state(), {.version = "1.0"});
  fake_.settings().set_execute_result({"bad output"});

  // Default max attempts is 3.
  for (int i = 0; i < 3; ++i) {
    fake_.Startup();
    fake_.client().RequestAssetsFor("test");
    task_environment_.FastForwardUntilNoTasksRemain();
    EXPECT_EQ(GetValidationResult(),
              OnDeviceModelValidationResult::kNonMatchingOutput);
    fake_.SimulateShutdown();
    fake_.launcher().clear_did_launch_service();
  }

  // 4th attempt should not run!
  fake_.Startup();
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(fake_.launcher().did_launch_service());
}

// Model validation should respect the kill switch flag.
TEST_F(ManifestValidatorTest, ModelValidationDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kOnDeviceModelValidation);

  BuildValidationScenario(fake_.component_state(), {});
  fake_.settings().set_execute_result({"matching output"});
  fake_.Startup();
  fake_.client().RequestAssetsFor("test");
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(GetValidationResult(), std::nullopt);
}

// Model validation should be delayed so it doesn't impact startup or
// other model executions.
TEST_F(ManifestValidatorTest, ModelValidationDelayed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOnDeviceModelValidation,
      {{"on_device_model_validation_delay", "10s"}});

  BuildValidationScenario(fake_.component_state(), {});
  fake_.settings().set_execute_result({"matching output"});
  fake_.Startup();
  fake_.client().RequestAssetsFor("test");

  task_environment_.FastForwardBy(base::Seconds(9));
  EXPECT_EQ(GetValidationResult(), std::nullopt);

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(GetValidationResult(), OnDeviceModelValidationResult::kSuccess);
}

// TODO(crbug.com/504749700): Implement these tests iff we support blocking
// session creation, or remove them if we just remove that code.
// ModelValidationBlocksSession
// ModelValidationBlocksSessionPendingCheck

}  // namespace

}  // namespace optimization_guide
