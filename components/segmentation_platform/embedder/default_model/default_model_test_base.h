// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DEFAULT_MODEL_TEST_BASE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DEFAULT_MODEL_TEST_BASE_H_

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

// This is a base class for default models unittest where all the setup for
// writing unit tests is present.
// How to use this class??
// As a base class in default model unit test class instead of whole setup
// work and write the tests only in the unit test class.
class DefaultModelTestBase : public testing::Test {
 public:
  explicit DefaultModelTestBase(
      std::unique_ptr<DefaultModelProvider> model_provider);
  ~DefaultModelTestBase() override;

  void SetUp() override;

  void TearDown() override;

 protected:
  void ExpectInitAndFetchModel();

  // This function is used to execute the model with set of inputs.
  // It should do one of the following cases:
  // 1. If `expected_error` is true, it expects error to be the thrown and
  // hence the model won't have any result.
  // 2. Else `expected_result` is checked against the actual result given by the
  // model after executing.
  void ExpectExecutionWithInput(const ModelProvider::Request& inputs,
                                bool expected_error,
                                ModelProvider::Response expected_result);

  // Executes the model with inputs and return the output.
  std::optional<ModelProvider::Response> ExecuteWithInput(
      const ModelProvider::Request& inputs);

  // Executes the model with inputs, applies classifier and checks against
  // the expected ordered labels.
  void ExpectClassifierResults(
      const ModelProvider::Request& input,
      const std::vector<std::string>& expected_ordered_labels);

  // `sub_segment_key` is combination of `segmentation_key` +
  // `kSubSegmentDiscreteMappingSuffix`. Use `GetSubsegmentKey()`  from
  // constants.h. `sub_segment_name` is the name of the segment expected to be
  // returned as result from model execution. `T` indicates the segment class
  // for which we need to evaluate subsegment based on inputs.
  template <typename T>
  void ExecuteWithInputAndCheckSubsegmentName(
      const ModelProvider::Request& inputs,
      std::string sub_segment_key,
      std::string sub_segment_name) {
    std::optional<ModelProvider::Response> result =
        DefaultModelTestBase::ExecuteWithInput(inputs);
    ASSERT_TRUE(result);
    EXPECT_EQ(sub_segment_name,
              T::GetSubsegmentName(metadata_utils::ConvertToDiscreteScore(
                  sub_segment_key, result.value()[0], *fetched_metadata_)));
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<DefaultModelProvider> model_;
  std::optional<proto::SegmentationModelMetadata> fetched_metadata_;

 private:
  void OnFinishedExpectExecutionWithInput(
      base::RepeatingClosure closure,
      bool expected_error,
      ModelProvider::Response expected_result,
      const std::optional<ModelProvider::Response>& result);

  void OnFinishedExecuteWithInput(
      base::RepeatingClosure closure,
      std::optional<ModelProvider::Response>* output,
      const std::optional<ModelProvider::Response>& result);
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_DEFAULT_MODEL_TEST_BASE_H_
