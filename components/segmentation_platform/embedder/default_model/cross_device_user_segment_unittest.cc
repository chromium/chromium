// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/cross_device_user_segment.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class CrossDeviceUserModelTest : public testing::Test {
 public:
  CrossDeviceUserModelTest() = default;
  ~CrossDeviceUserModelTest() override = default;

  void SetUp() override {
    cross_device_user_model_ = std::make_unique<CrossDeviceUserSegment>();
  }

  void TearDown() override { cross_device_user_model_.reset(); }

  void ExpectInitAndFetchModel() {
    base::RunLoop loop;
    cross_device_user_model_->InitAndFetchModel(
        base::BindRepeating(&CrossDeviceUserModelTest::OnInitFinishedCallback,
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
    cross_device_user_model_->ExecuteModelWithInput(
        inputs,
        base::BindOnce(&CrossDeviceUserModelTest::OnExecutionFinishedCallback,
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
  std::unique_ptr<CrossDeviceUserSegment> cross_device_user_model_;
  absl::optional<proto::SegmentationModelMetadata> fetched_metadata_;
};

TEST_F(CrossDeviceUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(CrossDeviceUserModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  std::vector<float> input(4, 0);

  absl::optional<float> result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ("NoCrossDeviceUsage", CrossDeviceUserSegment::GetSubsegmentName(
                                      metadata_utils::ConvertToDiscreteScore(
                                          "cross_device_user_subsegment",
                                          *result, *fetched_metadata_)));

  input[0] = 2;
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ("CrossDeviceOther", CrossDeviceUserSegment::GetSubsegmentName(
                                    metadata_utils::ConvertToDiscreteScore(
                                        "cross_device_user_subsegment", *result,
                                        *fetched_metadata_)));

  input[1] = 2;
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ("CrossDeviceMobile", CrossDeviceUserSegment::GetSubsegmentName(
                                     metadata_utils::ConvertToDiscreteScore(
                                         "cross_device_user_subsegment",
                                         *result, *fetched_metadata_)));

  input[1] = 0;
  input[2] = 2;
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ("CrossDeviceDesktop", CrossDeviceUserSegment::GetSubsegmentName(
                                      metadata_utils::ConvertToDiscreteScore(
                                          "cross_device_user_subsegment",
                                          *result, *fetched_metadata_)));

  input[2] = 0;
  input[3] = 2;
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ("CrossDeviceTablet", CrossDeviceUserSegment::GetSubsegmentName(
                                     metadata_utils::ConvertToDiscreteScore(
                                         "cross_device_user_subsegment",
                                         *result, *fetched_metadata_)));

  input[1] = 2;
  input[2] = 2;
  input[3] = 0;
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(
      "CrossDeviceMobileAndDesktop",
      CrossDeviceUserSegment::GetSubsegmentName(
          metadata_utils::ConvertToDiscreteScore("cross_device_user_subsegment",
                                                 *result, *fetched_metadata_)));

  input[2] = 0;
  input[3] = 2;
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(
      "CrossDeviceMobileAndTablet",
      CrossDeviceUserSegment::GetSubsegmentName(
          metadata_utils::ConvertToDiscreteScore("cross_device_user_subsegment",
                                                 *result, *fetched_metadata_)));
  input[1] = 0;
  input[2] = 2;
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(
      "CrossDeviceDesktopAndTablet",
      CrossDeviceUserSegment::GetSubsegmentName(
          metadata_utils::ConvertToDiscreteScore("cross_device_user_subsegment",
                                                 *result, *fetched_metadata_)));

  input[1] = 2;
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(
      "CrossDeviceAllDeviceTypes",
      CrossDeviceUserSegment::GetSubsegmentName(
          metadata_utils::ConvertToDiscreteScore("cross_device_user_subsegment",
                                                 *result, *fetched_metadata_)));

  EXPECT_FALSE(ExpectExecutionWithInput({}));
  EXPECT_FALSE(ExpectExecutionWithInput({1, 2}));
}

}  // namespace segmentation_platform