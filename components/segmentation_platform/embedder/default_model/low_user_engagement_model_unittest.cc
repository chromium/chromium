// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/low_user_engagement_model.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class LowUserEngagementModelTest : public testing::Test {
 public:
  LowUserEngagementModelTest() = default;
  ~LowUserEngagementModelTest() override = default;

  void SetUp() override {
    low_engagement_model_ = std::make_unique<LowUserEngagementModel>();
  }

  void TearDown() override { low_engagement_model_.reset(); }

  void ExpectInitAndFetchModel() {
    base::RunLoop loop;
    low_engagement_model_->InitAndFetchModel(
        base::BindRepeating(&LowUserEngagementModelTest::OnInitFinishedCallback,
                            base::Unretained(this), loop.QuitClosure()));
    loop.Run();
  }

  void OnInitFinishedCallback(base::RepeatingClosure closure,
                              proto::SegmentId target,
                              proto::SegmentationModelMetadata metadata,
                              int64_t) {
    EXPECT_EQ(metadata_utils::ValidateMetadataAndFeatures(metadata),
              metadata_utils::ValidationResult::kValidationSuccess);
    std::move(closure).Run();
  }

  void ExpectExecutionWithInput(const std::vector<float>& inputs,
                                bool expected_error,
                                float expected_result) {
    base::RunLoop loop;
    low_engagement_model_->ExecuteModelWithInput(
        inputs,
        base::BindOnce(&LowUserEngagementModelTest::OnExecutionFinishedCallback,
                       base::Unretained(this), loop.QuitClosure(),
                       expected_error, expected_result));
    loop.Run();
  }

  void OnExecutionFinishedCallback(base::RepeatingClosure closure,
                                   bool expected_error,
                                   float expected_result,
                                   const absl::optional<float>& result) {
    if (expected_error) {
      EXPECT_FALSE(result.has_value());
    } else {
      EXPECT_TRUE(result.has_value());
      EXPECT_EQ(result.value(), expected_result);
    }
    std::move(closure).Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<LowUserEngagementModel> low_engagement_model_;
};

TEST_F(LowUserEngagementModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(LowUserEngagementModelTest, ExecuteModelWithInput) {
  std::vector<float> input;
  ExpectExecutionWithInput(input, true, 0);

  input.assign(27, 0);
  ExpectExecutionWithInput(input, true, 0);

  input.assign(28, 0);
  ExpectExecutionWithInput(input, false, 1);

  input.assign(21, 0);
  input.insert(input.end(), 7, 1);
  ExpectExecutionWithInput(input, false, 1);

  input.assign(28, 0);
  input[1] = 2;
  input[8] = 3;
  input[15] = 4;
  input[22] = 2;
  ExpectExecutionWithInput(input, false, 0);
}

}  // namespace segmentation_platform
