// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/low_user_engagement_model.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

class LowUserEngagementModelTest : public DefaultModelTestBase {
 public:
  LowUserEngagementModelTest()
      : DefaultModelTestBase(std::make_unique<LowUserEngagementModel>()) {}
  ~LowUserEngagementModelTest() override = default;
};

TEST_F(LowUserEngagementModelTest, 2) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);
}

TEST_F(LowUserEngagementModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  ModelProvider::Request input;

  // Low engaged users.
  input.assign(28, 0);
  ExpectClassifierResults(input, {kChromeLowUserEngagementUmaName});

  input.assign(21, 0);
  input.insert(input.end(), 7, 1);
  ExpectClassifierResults(input, {kChromeLowUserEngagementUmaName});

  // Not low engaged users.
  input.assign(28, 0);
  input[1] = 2;
  input[8] = 3;
  input[15] = 4;
  input[22] = 2;
  ExpectClassifierResults(input, {kLegacyNegativeLabel});
}

}  // namespace segmentation_platform
