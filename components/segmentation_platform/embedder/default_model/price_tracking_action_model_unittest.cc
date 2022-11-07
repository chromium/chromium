// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/price_tracking_action_model.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

class PriceTrackingActionModelTest : public DefaultModelTestBase {
 public:
  PriceTrackingActionModelTest()
      : DefaultModelTestBase(std::make_unique<PriceTrackingActionModel>()) {}
  ~PriceTrackingActionModelTest() override = default;
};

TEST_F(PriceTrackingActionModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(PriceTrackingActionModelTest, ExecuteModelWithInput) {
  // Input vector empty.
  ModelProvider::Request input;
  ExpectExecutionWithInput(input, /*expected_error=*/true,
                           /*expected_result=*/{0});

  // Price tracking = 0
  input.assign(1, 0);
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{0});

  // Price tracking = 1
  input.assign(1, 1);
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{1});
}

}  // namespace segmentation_platform
