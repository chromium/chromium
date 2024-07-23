// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_validator.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/test/fake_on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

class OnDeviceModelValidatorTest : public testing::Test {
 public:
  OnDeviceModelValidatorTest() {
    service_ = std::make_unique<on_device_model::FakeOnDeviceModelService>(
        service_remote_.BindNewPipeAndPassReceiver(), &fake_settings_);
    service_remote_->LoadModel(on_device_model::mojom::LoadModelParams::New(),
                               model_remote_.BindNewPipeAndPassReceiver(),
                               base::DoNothing());
  }

  mojo::Remote<on_device_model::mojom::Session> StartSession() {
    mojo::Remote<on_device_model::mojom::Session> session_remote;
    model_remote_->StartSession(session_remote.BindNewPipeAndPassReceiver());
    return session_remote;
  }

  OnDeviceModelValidationResult WaitForValidation(
      const proto::OnDeviceModelValidationConfig& config) {
    base::test::TestFuture<OnDeviceModelValidationResult> result_future;
    auto validator = std::make_unique<OnDeviceModelValidator>(
        config, result_future.GetCallback(), StartSession());
    return result_future.Get();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<on_device_model::mojom::OnDeviceModelService> service_remote_;
  mojo::Remote<on_device_model::mojom::OnDeviceModel> model_remote_;
  on_device_model::FakeOnDeviceServiceSettings fake_settings_;
  std::unique_ptr<on_device_model::FakeOnDeviceModelService> service_;
};

TEST_F(OnDeviceModelValidatorTest, Succeeds) {
  EXPECT_EQ(OnDeviceModelValidationResult::kSuccess,
            WaitForValidation(WillPassValidationConfig()));
}

TEST_F(OnDeviceModelValidatorTest, SucceedsWithEmptyConfig) {
  EXPECT_EQ(OnDeviceModelValidationResult::kSuccess,
            WaitForValidation(proto::OnDeviceModelValidationConfig()));
}

TEST_F(OnDeviceModelValidatorTest, FailsWhenSessionKilled) {
  base::test::TestFuture<OnDeviceModelValidationResult> result_future;
  auto validator = std::make_unique<OnDeviceModelValidator>(
      WillPassValidationConfig(), result_future.GetCallback(), StartSession());
  model_remote_.reset();
  EXPECT_EQ(OnDeviceModelValidationResult::kInterrupted, result_future.Get());
}

TEST_F(OnDeviceModelValidatorTest, Fails) {
  EXPECT_EQ(OnDeviceModelValidationResult::kNonMatchingOutput,
            WaitForValidation(WillFailValidationConfig()));
}

TEST_F(OnDeviceModelValidatorTest, SucceedsWithChunkedResponse) {
  // The response is sent in multiple chunks, make sure the validator can put
  // these together.
  fake_settings_.set_execute_result({
      "some text",
      " more text foo",
      "bar! ",
      "other",
  });
  proto::OnDeviceModelValidationConfig validation_config;
  auto* prompt = validation_config.add_validation_prompts();
  prompt->set_prompt("hello");
  // This should pass because the execute result contains foobar.
  prompt->set_expected_output("foobar");

  EXPECT_EQ(OnDeviceModelValidationResult::kSuccess,
            WaitForValidation(validation_config));
}

TEST_F(OnDeviceModelValidatorTest, SucceedsWithMultiplePrompts) {
  proto::OnDeviceModelValidationConfig validation_config;
  {
    auto* prompt = validation_config.add_validation_prompts();
    // Passes because the model will echo input.
    prompt->set_prompt("foo");
    prompt->set_expected_output("foo");
  }
  {
    auto* prompt = validation_config.add_validation_prompts();
    // Passes because the model will echo input.
    prompt->set_prompt("bar");
    prompt->set_expected_output("bar");
  }

  EXPECT_EQ(OnDeviceModelValidationResult::kSuccess,
            WaitForValidation(validation_config));
}

TEST_F(OnDeviceModelValidatorTest, FailsWithSecondPromptNotMatching) {
  proto::OnDeviceModelValidationConfig validation_config;
  {
    auto* prompt = validation_config.add_validation_prompts();
    // Passes because the model will echo input.
    prompt->set_prompt("foo");
    prompt->set_expected_output("foo");
  }
  {
    auto* prompt = validation_config.add_validation_prompts();
    // Fails because the model will echo input.
    prompt->set_prompt("bar");
    prompt->set_expected_output("foo");
  }

  EXPECT_EQ(OnDeviceModelValidationResult::kNonMatchingOutput,
            WaitForValidation(validation_config));
}

}  // namespace
}  // namespace optimization_guide
