// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/post_processor/post_processor.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/proto/output_config.pb.h"
#include "components/segmentation_platform/public/proto/prediction_result.pb.h"

namespace segmentation_platform {

DefaultModelTestBase::DefaultModelTestBase(
    std::unique_ptr<DefaultModelProvider> model_provider)
    : model_(std::move(model_provider)) {}

DefaultModelTestBase::~DefaultModelTestBase() = default;

void DefaultModelTestBase::SetUp() {}

void DefaultModelTestBase::TearDown() {
  model_.reset();
}

void DefaultModelTestBase::ExpectInitAndFetchModel() {
  std::unique_ptr<DefaultModelProvider::ModelConfig> config =
      model_->GetModelConfig();
  EXPECT_EQ(
      static_cast<int>(
          metadata_utils::ValidateMetadataAndFeatures(config->metadata)),
      static_cast<int>(metadata_utils::ValidationResult::kValidationSuccess));
  fetched_metadata_ = std::move(config->metadata);
}

void DefaultModelTestBase::ExpectExecutionWithInput(
    const ModelProvider::Request& inputs,
    bool expected_error,
    ModelProvider::Response expected_result) {
  base::RunLoop loop;
  model_->ExecuteModelWithInput(
      inputs,
      base::BindOnce(&DefaultModelTestBase::OnFinishedExpectExecutionWithInput,
                     base::Unretained(this), loop.QuitClosure(), expected_error,
                     expected_result));
  loop.Run();
}

void DefaultModelTestBase::OnFinishedExpectExecutionWithInput(
    base::RepeatingClosure closure,
    bool expected_error,
    ModelProvider::Response expected_result,
    const std::optional<ModelProvider::Response>& result) {
  if (expected_error) {
    EXPECT_FALSE(result.has_value());
  } else {
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), expected_result);
  }
  std::move(closure).Run();
}

std::optional<ModelProvider::Response> DefaultModelTestBase::ExecuteWithInput(
    const ModelProvider::Request& inputs) {
  std::optional<ModelProvider::Response> result;
  base::RunLoop loop;
  model_->ExecuteModelWithInput(
      inputs,
      base::BindOnce(&DefaultModelTestBase::OnFinishedExecuteWithInput,
                     base::Unretained(this), loop.QuitClosure(), &result));
  loop.Run();
  return result;
}

void DefaultModelTestBase::ExpectClassifierResults(
    const ModelProvider::Request& input,
    const std::vector<std::string>& expected_ordered_labels) {
  ASSERT_TRUE(fetched_metadata_)
      << "Please call ExpectInitAndFetchModel() in each test";
  auto result = ExecuteWithInput(input);
  EXPECT_TRUE(result.has_value());

  EXPECT_TRUE(fetched_metadata_);
  EXPECT_TRUE(fetched_metadata_->has_output_config());
  auto prediction_result = metadata_utils::CreatePredictionResult(
      result.value(), fetched_metadata_->output_config(), base::Time::Now(),
      /*model_version=*/1);

  auto winning_labels = PostProcessor().GetClassifierResults(prediction_result);
  EXPECT_EQ(expected_ordered_labels, winning_labels);
}

void DefaultModelTestBase::OnFinishedExecuteWithInput(
    base::RepeatingClosure closure,
    std::optional<ModelProvider::Response>* output,
    const std::optional<ModelProvider::Response>& result) {
  *output = result;
  std::move(closure).Run();
}

}  // namespace segmentation_platform
