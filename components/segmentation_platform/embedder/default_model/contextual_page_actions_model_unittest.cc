// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/contextual_page_actions_model.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

class ContextualPageActionsModelTest : public DefaultModelTestBase {
 public:
  ContextualPageActionsModelTest()
      : DefaultModelTestBase(std::make_unique<ContextualPageActionsModel>()) {}
  ~ContextualPageActionsModelTest() override = default;
};

TEST_F(ContextualPageActionsModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(ContextualPageActionsModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();

  // Input vector empty.
  ModelProvider::Request input;
  ExpectExecutionWithInput(input, /*expected_error=*/true,
                           /*expected_result=*/{});

  // Price tracking = 0, reader mode = 0
  input = {0, 0};
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{0, 0});
  ExpectClassifierResults(input, {});

  // Price tracking = 1, reader mode = 0
  input = {1, 0};
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{1, 0});
  ExpectClassifierResults(input,
                          {kContextualPageActionModelLabelPriceTracking});

  // Price tracking = 0, reader mode = 1
  input = {0, 1};
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{0, 1});
  ExpectClassifierResults(input, {kContextualPageActionModelLabelReaderMode});

  // Price tracking = 1, reader mode = 1
  input = {1, 1};
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{1, 1});
  ExpectClassifierResults(input,
                          {kContextualPageActionModelLabelPriceTracking});
}

}  // namespace segmentation_platform
