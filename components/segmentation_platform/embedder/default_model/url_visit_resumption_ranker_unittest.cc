// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/url_visit_resumption_ranker.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "components/visited_url_ranking/public/url_visit_schema.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class URLVisitResumptionRankerTest : public DefaultModelTestBase {
 public:
  URLVisitResumptionRankerTest()
      : DefaultModelTestBase(std::make_unique<URLVisitResumptionRanker>()) {}
  ~URLVisitResumptionRankerTest() override = default;
};

TEST_F(URLVisitResumptionRankerTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);
}

TEST_F(URLVisitResumptionRankerTest, VerifyMetadata) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  ASSERT_EQ(fetched_metadata_->input_features_size(),
            static_cast<int>(visited_url_ranking::kTabResumptionNumInputs));
  for (int i = 0; i < fetched_metadata_->input_features_size(); i++) {
    const proto::CustomInput& input =
        fetched_metadata_->input_features(i).custom_input();
    EXPECT_EQ(input.tensor_length(), 1);
  }
}

TEST_F(URLVisitResumptionRankerTest, ModelScore) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  ModelProvider::Request inputs(visited_url_ranking::kTabResumptionNumInputs,
                                -1);
  ExpectExecutionWithInput(inputs, false, {0});

  for (const auto& input_signal :
       {visited_url_ranking::kTimeSinceLastActiveSec,
        visited_url_ranking::kTimeSinceLastModifiedSec}) {
    ModelProvider::Request request_inputs(
        visited_url_ranking::kTabResumptionNumInputs, -1);
    request_inputs[input_signal] = 0;
    ExpectExecutionWithInput(request_inputs, false, {1});

    request_inputs[input_signal] = 10;
    ExpectExecutionWithInput(request_inputs, false, {0.1});
  }
}

}  // namespace segmentation_platform
