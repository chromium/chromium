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
}

TEST_F(LowUserEngagementModelTest, ExecuteModelWithInput) {
  ModelProvider::Request input;
  ExpectExecutionWithInput(input, /*expected_error=*/true,
                           /*expected_result=*/{0});

  input.assign(27, 0);
  ExpectExecutionWithInput(input, /*expected_error=*/true,
                           /*expected_result=*/{0});

  input.assign(28, 0);
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{1});

  input.assign(21, 0);
  input.insert(input.end(), 7, 1);
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{1});

  input.assign(28, 0);
  input[1] = 2;
  input[8] = 3;
  input[15] = 4;
  input[22] = 2;
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{0});
}

}  // namespace segmentation_platform
