// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/search_user_model.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class SearchUserModelTest : public testing::Test {
 public:
  SearchUserModelTest() = default;
  ~SearchUserModelTest() override = default;

  void SetUp() override {
    search_user_model_ = std::make_unique<SearchUserModel>();
  }

  void TearDown() override { search_user_model_.reset(); }

  void ExpectInitAndFetchModel() {
    base::RunLoop loop;
    search_user_model_->InitAndFetchModel(
        base::BindRepeating(&SearchUserModelTest::OnInitFinishedCallback,
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
    search_user_model_->ExecuteModelWithInput(
        inputs,
        base::BindOnce(&SearchUserModelTest::OnExecutionFinishedCallback,
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
  std::unique_ptr<SearchUserModel> search_user_model_;
  absl::optional<proto::SegmentationModelMetadata> fetched_metadata_;
};

TEST_F(SearchUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);
}

TEST_F(SearchUserModelTest, VerifyMetadata) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  ASSERT_EQ(1, fetched_metadata_.value().input_features_size());
  const proto::UMAFeature feature =
      fetched_metadata_.value().input_features(0).uma_feature();

  EXPECT_EQ(proto::SignalType::HISTOGRAM_ENUM, feature.type());
  EXPECT_EQ("Omnibox.SuggestionUsed.ClientSummarizedResultType",
            feature.name());
  EXPECT_EQ(proto::Aggregation::COUNT, feature.aggregation());
  EXPECT_EQ(1u, feature.tensor_length());
  ASSERT_EQ(1, feature.enum_ids_size());
  // This must match the `Search` entry in `ClientSummaryResultGroup` in
  // //tools/metrics/histograms/enums.xml.
  EXPECT_EQ(1, feature.enum_ids(0));
}

TEST_F(SearchUserModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExpectExecutionWithInput({}));

  std::vector<float> input = {0};
  absl::optional<float> result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(1, result.value());
  EXPECT_EQ(
      "None",
      SearchUserModel::GetSubsegmentName(metadata_utils::ConvertToDiscreteScore(
          "search_user_subsegment", *result, *fetched_metadata_)));

  input[0] = 1;
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(2, result.value());
  EXPECT_EQ(
      "Low",
      SearchUserModel::GetSubsegmentName(metadata_utils::ConvertToDiscreteScore(
          "search_user_subsegment", *result, *fetched_metadata_)));

  input[0] = 5;
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(3, result.value());
  EXPECT_EQ(
      "Medium",
      SearchUserModel::GetSubsegmentName(metadata_utils::ConvertToDiscreteScore(
          "search_user_subsegment", *result, *fetched_metadata_)));

  input[0] = 22;
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(4, result.value());
  EXPECT_EQ(
      "High",
      SearchUserModel::GetSubsegmentName(metadata_utils::ConvertToDiscreteScore(
          "search_user_subsegment", *result, *fetched_metadata_)));
}

}  // namespace segmentation_platform
