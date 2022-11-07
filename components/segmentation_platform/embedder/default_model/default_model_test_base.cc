// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

DefaultModelTestBase::DefaultModelTestBase(
    std::unique_ptr<ModelProvider> model_provider)
    : model_(std::move(model_provider)) {}

DefaultModelTestBase::~DefaultModelTestBase() = default;

void DefaultModelTestBase::SetUp() {}

void DefaultModelTestBase::TearDown() {
  model_.reset();
}

void DefaultModelTestBase::ExpectInitAndFetchModel() {
  base::RunLoop loop;
  model_->InitAndFetchModel(
      base::BindRepeating(&DefaultModelTestBase::OnInitFinishedCallback,
                          base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

void DefaultModelTestBase::OnInitFinishedCallback(
    base::RepeatingClosure closure,
    proto::SegmentId target,
    proto::SegmentationModelMetadata metadata,
    int64_t) {
  EXPECT_EQ(metadata_utils::ValidateMetadataAndFeatures(metadata),
            metadata_utils::ValidationResult::kValidationSuccess);
  fetched_metadata_ = metadata;
  std::move(closure).Run();
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
    const absl::optional<ModelProvider::Response>& result) {
  if (expected_error) {
    EXPECT_FALSE(result.has_value());
  } else {
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), expected_result);
  }
  std::move(closure).Run();
}

absl::optional<ModelProvider::Response> DefaultModelTestBase::ExecuteWithInput(
    const ModelProvider::Request& inputs) {
  absl::optional<ModelProvider::Response> result;
  base::RunLoop loop;
  model_->ExecuteModelWithInput(
      inputs,
      base::BindOnce(&DefaultModelTestBase::OnFinishedExecuteWithInput,
                     base::Unretained(this), loop.QuitClosure(), &result));
  loop.Run();
  return result;
}

void DefaultModelTestBase::OnFinishedExecuteWithInput(
    base::RepeatingClosure closure,
    absl::optional<ModelProvider::Response>* output,
    const absl::optional<ModelProvider::Response>& result) {
  *output = result;
  std::move(closure).Run();
}

}  // namespace segmentation_platform