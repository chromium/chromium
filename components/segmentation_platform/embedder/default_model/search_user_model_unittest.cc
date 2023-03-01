// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/search_user_model.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

class SearchUserModelTest : public DefaultModelTestBase {
 public:
  SearchUserModelTest()
      : DefaultModelTestBase(std::make_unique<SearchUserModel>()) {}
  ~SearchUserModelTest() override = default;
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

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  std::string subsegment_key = GetSubsegmentKey(kSearchUserKey);
  ModelProvider::Request input = {0};
  ExecuteWithInputAndCheckSubsegmentName<SearchUserModel>(
      input, subsegment_key, /*sub_segment_name=*/"None");

  input[0] = 1;
  ExecuteWithInputAndCheckSubsegmentName<SearchUserModel>(
      input, subsegment_key, /*sub_segment_name=*/"Low");

  input[0] = 5;
  ExecuteWithInputAndCheckSubsegmentName<SearchUserModel>(
      input, subsegment_key, /*sub_segment_name=*/"Medium");

  input[0] = 22;
  ExecuteWithInputAndCheckSubsegmentName<SearchUserModel>(
      input, subsegment_key, /*sub_segment_name=*/"High");
}

}  // namespace segmentation_platform
