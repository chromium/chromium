// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/power_user_segment.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class PowerUserModelTest : public testing::Test {
 public:
  PowerUserModelTest() = default;
  ~PowerUserModelTest() override = default;

  void SetUp() override {
    power_user_model_ = std::make_unique<PowerUserSegment>();
  }

  void TearDown() override {
    power_user_model_.reset();
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void ExpectInitAndFetchModel() {
    base::RunLoop loop;
    power_user_model_->InitAndFetchModel(
        base::BindRepeating(&PowerUserModelTest::OnInitFinishedCallback,
                            base::Unretained(this), loop.QuitClosure()));
    loop.Run();
  }

  void OnInitFinishedCallback(base::RepeatingClosure closure,
                              proto::SegmentId target,
                              proto::SegmentationModelMetadata metadata,
                              int64_t) {
    EXPECT_EQ(metadata_utils::ValidateMetadataAndFeatures(metadata),
              metadata_utils::ValidationResult::kValidationSuccess);
    fetched_metadata_ = metadata;
    std::move(closure).Run();
  }

  absl::optional<float> ExpectExecutionWithInput(
      const std::vector<float>& inputs) {
    absl::optional<float> result;
    base::RunLoop loop;
    power_user_model_->ExecuteModelWithInput(
        inputs,
        base::BindOnce(&PowerUserModelTest::OnExecutionFinishedCallback,
                       base::Unretained(this), loop.QuitClosure(), &result));
    loop.Run();
    return result;
  }

  void OnExecutionFinishedCallback(base::RepeatingClosure closure,
                                   absl::optional<float>* output,
                                   const absl::optional<float>& result) {
    *output = result;
    std::move(closure).Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PowerUserSegment> power_user_model_;
  absl::optional<proto::SegmentationModelMetadata> fetched_metadata_;
};

TEST_F(PowerUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(PowerUserModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  std::vector<float> input(27, 0);

  absl::optional<float> result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ("None",
            PowerUserSegment::GetSubsegmentName(
                metadata_utils::ConvertToDiscreteScore(
                    "power_user_subsegment", *result, *fetched_metadata_)));

  input[1] = 3;    // download
  input[8] = 4;    // share
  input[10] = 4;   // bookmarks
  input[11] = 20;  // voice
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ("Low",
            PowerUserSegment::GetSubsegmentName(
                metadata_utils::ConvertToDiscreteScore(
                    "power_user_subsegment", *result, *fetched_metadata_)));

  input[12] = 2;  // cast
  input[15] = 5;  // autofill
  input[22] = 6;  // media picker
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ("Medium",
            PowerUserSegment::GetSubsegmentName(
                metadata_utils::ConvertToDiscreteScore(
                    "power_user_subsegment", *result, *fetched_metadata_)));

  input[26] = 20 * 60 * 1000;  // 20 min session
  input[17] = 60000;           // 60 sec audio output
  input[23] = 50000;           // 50KB upload
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ("High",
            PowerUserSegment::GetSubsegmentName(
                metadata_utils::ConvertToDiscreteScore(
                    "power_user_subsegment", *result, *fetched_metadata_)));

  EXPECT_FALSE(ExpectExecutionWithInput({}));
  EXPECT_FALSE(ExpectExecutionWithInput({1, 2}));
}

}  // namespace segmentation_platform
