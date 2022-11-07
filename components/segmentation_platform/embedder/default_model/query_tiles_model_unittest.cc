// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/query_tiles_model.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

class QueryTilesModelTest : public DefaultModelTestBase {
 public:
  QueryTilesModelTest()
      : DefaultModelTestBase(std::make_unique<QueryTilesModel>()) {}
  ~QueryTilesModelTest() override = default;
};

TEST_F(QueryTilesModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(QueryTilesModelTest, ExecuteModelWithInput) {
  const float mv_threshold = 1;

  // When mv clicks are below the minimum threshold, query tiles should be
  // enabled.
  float mv_clicks = 0;
  float qt_clicks = 0;
  ExpectExecutionWithInput(/*inputs=*/{mv_clicks, qt_clicks},
                           /*expected_error=*/false, /*expected_result=*/{1});

  // When mv clicks are above threshold, but below qt clicks, query tiles should
  // be enabled.
  mv_clicks = mv_threshold + 1;
  qt_clicks = mv_clicks + 1;
  ExpectExecutionWithInput(/*inputs=*/{mv_clicks, qt_clicks},
                           /*expected_error=*/false, /*expected_result=*/{1});

  // When mv clicks are above threshold, and above qt clicks, query tiles should
  // be disabled.
  mv_clicks = mv_threshold + 1;
  qt_clicks = mv_clicks - 1;
  ExpectExecutionWithInput(/*inputs=*/{mv_clicks, qt_clicks},
                           /*expected_error=*/false, /*expected_result=*/{0});

  // When invalid inputs are given, execution should not return a result.
  ExpectExecutionWithInput(/*inputs=*/{mv_clicks}, /*expected_error=*/true,
                           /*expected_result=*/{0});
  ExpectExecutionWithInput(/*inputs=*/{mv_clicks, qt_clicks, qt_clicks},
                           /*expected_error=*/true, /*expected_result=*/{0});
}

}  // namespace segmentation_platform
