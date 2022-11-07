// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/frequent_feature_user_model.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

class FrequentFeatureUserModelTest : public DefaultModelTestBase {
 public:
  FrequentFeatureUserModelTest()
      : DefaultModelTestBase(std::make_unique<FrequentFeatureUserModel>()) {}
  ~FrequentFeatureUserModelTest() override = default;
};

TEST_F(FrequentFeatureUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(FrequentFeatureUserModelTest, ExecuteModelWithInput) {
  ExpectExecutionWithInput(/*inputs=*/{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                           /*expected_error=*/false, /*expected_result=*/{0});
  ExpectExecutionWithInput(/*inputs=*/{1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                           /*expected_error=*/false, /*expected_result=*/{0});
  ExpectExecutionWithInput(/*inputs=*/{0, 0, 2, 0, 0, 0, 0, 0, 0, 0},
                           /*expected_error=*/false, /*expected_result=*/{0});
  ExpectExecutionWithInput(/*inputs=*/{0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
                           /*expected_error=*/false, /*expected_result=*/{0});
  ExpectExecutionWithInput(/*inputs=*/{0, 0, 0, 0, 0, 0, 3, 0, 1, 0},
                           /*expected_error=*/false, /*expected_result=*/{0});
  ExpectExecutionWithInput(/*inputs=*/{0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
                           /*expected_error=*/false, /*expected_result=*/{0});
  ExpectExecutionWithInput(/*inputs=*/{0, 0, 0, 0, 0, 1, 0, 0, 0, 1},
                           /*expected_error=*/false, /*expected_result=*/{1});
  ExpectExecutionWithInput(/*inputs=*/{0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
                           /*expected_error=*/false, /*expected_result=*/{1});
  ExpectExecutionWithInput(/*inputs=*/{0, 0, 1, 0, 2, 0, 0, 0, 0, 1},
                           /*expected_error=*/false, /*expected_result=*/{1});
}

}  // namespace segmentation_platform
