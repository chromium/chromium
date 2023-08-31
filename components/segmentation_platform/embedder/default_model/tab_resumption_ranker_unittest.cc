// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/tab_resumption_ranker.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "components/segmentation_platform/embedder/input_delegate/tab_session_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class TabResumptionRankerTest : public DefaultModelTestBase {
 public:
  TabResumptionRankerTest()
      : DefaultModelTestBase(std::make_unique<TabResumptionRanker>()) {}
  ~TabResumptionRankerTest() override = default;
};

TEST_F(TabResumptionRankerTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);
}

TEST_F(TabResumptionRankerTest, VerifyMetadata) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  ASSERT_GE(fetched_metadata_->input_features_size(), 1);
  const proto::CustomInput& input =
      fetched_metadata_->input_features(0).custom_input();
  EXPECT_EQ(proto::CustomInput::FILL_TAB_METRICS, input.fill_policy());
  EXPECT_EQ(processing::TabSessionSource::kNumInputs, input.tensor_length());
}

TEST_F(TabResumptionRankerTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  ModelProvider::Request input(processing::TabSessionSource::kNumInputs + 11,
                               0);
  ASSERT_TRUE(ExecuteWithInput(input));

  input[processing::TabSessionSource::kInputTimeSinceModifiedSec] = 3;
  auto response1 = ExecuteWithInput(input);
  ASSERT_TRUE(response1);
  input[processing::TabSessionSource::kInputTimeSinceModifiedSec] = 5;
  auto response2 = ExecuteWithInput(input);
  ASSERT_TRUE(response2);
  EXPECT_GT(*response1, *response2);
}

}  // namespace segmentation_platform
