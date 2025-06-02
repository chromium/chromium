// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/chrome_user_engagement.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

class ChromeUserEngagementTest : public DefaultModelTestBase {
 public:
  ChromeUserEngagementTest()
      : DefaultModelTestBase(std::make_unique<ChromeUserEngagement>()) {}
  ~ChromeUserEngagementTest() override = default;
};

TEST_F(ChromeUserEngagementTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(ChromeUserEngagementTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  std::string subsegment_key = GetSubsegmentKey(kPowerUserKey);
  ModelProvider::Request input(28, 0);
  ExpectExecutionWithInput(input, /*expected_error=*/false, {1});
  ExpectClassifierResults(input, {"None"});

  // 1 day:
  input[3] = 3;
  ExpectExecutionWithInput(input, /*expected_error=*/false, {2});
  ExpectClassifierResults(input, {"OneDay"});

  // 3 days:
  input[7] = 5;
  input[8] = 5;
  ExpectExecutionWithInput(input, /*expected_error=*/false, {3});
  ExpectClassifierResults(input, {"Low"});

  // 10 days:
  for (unsigned i = 3; i < 13; ++i) {
    input[i] = i;
  }
  ExpectExecutionWithInput(input, /*expected_error=*/false, {4});
  ExpectClassifierResults(input, {"Medium"});

  // 25 days:
  for (unsigned i = 3; i < 28; ++i) {
    input[i] = i;
  }
  ExpectExecutionWithInput(input, /*expected_error=*/false, {5});
  ExpectClassifierResults(input, {"Power"});

  // Error:
  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));
  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{1, 2}));
}

}  // namespace segmentation_platform
