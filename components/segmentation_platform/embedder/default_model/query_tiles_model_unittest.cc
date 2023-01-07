// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/query_tiles_model.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class QueryTilesModelTest : public testing::Test {
 public:
  QueryTilesModelTest() = default;
  ~QueryTilesModelTest() override = default;

  void SetUp() override {
    query_tile_model_ = std::make_unique<QueryTilesModel>();
  }

  void TearDown() override { query_tile_model_.reset(); }

  void ExpectInitAndFetchModel() {
    base::RunLoop loop;
    query_tile_model_->InitAndFetchModel(
        base::BindRepeating(&QueryTilesModelTest::OnInitFinishedCallback,
                            base::Unretained(this), loop.QuitClosure()));
    loop.Run();
  }

  void OnInitFinishedCallback(base::RepeatingClosure closure,
                              proto::SegmentId target,
                              proto::SegmentationModelMetadata metadata,
                              int64_t) {
    EXPECT_EQ(metadata_utils::ValidateMetadataAndFeatures(metadata),
              metadata_utils::ValidationResult::kValidationSuccess);
    std::move(closure).Run();
  }

  void ExpectExecutionWithInput(const std::vector<float>& inputs,
                                bool expected_error,
                                float expected_result) {
    base::RunLoop loop;
    query_tile_model_->ExecuteModelWithInput(
        inputs,
        base::BindOnce(&QueryTilesModelTest::OnExecutionFinishedCallback,
                       base::Unretained(this), loop.QuitClosure(),
                       expected_error, expected_result));
    loop.Run();
  }

  void OnExecutionFinishedCallback(base::RepeatingClosure closure,
                                   bool expected_error,
                                   float expected_result,
                                   const absl::optional<float>& result) {
    if (expected_error) {
      EXPECT_FALSE(result.has_value());
    } else {
      EXPECT_TRUE(result.has_value());
      EXPECT_EQ(result.value(), expected_result);
    }
    std::move(closure).Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<QueryTilesModel> query_tile_model_;
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
  ExpectExecutionWithInput({mv_clicks, qt_clicks}, false, 1);

  // When mv clicks are above threshold, but below qt clicks, query tiles should
  // be enabled.
  mv_clicks = mv_threshold + 1;
  qt_clicks = mv_clicks + 1;
  ExpectExecutionWithInput({mv_clicks, qt_clicks}, false, 1);

  // When mv clicks are above threshold, and above qt clicks, query tiles should
  // be disabled.
  mv_clicks = mv_threshold + 1;
  qt_clicks = mv_clicks - 1;
  ExpectExecutionWithInput({mv_clicks, qt_clicks}, false, 0);

  // When invalid inputs are given, execution should not return a result.
  ExpectExecutionWithInput({mv_clicks}, true, 0);
  ExpectExecutionWithInput({mv_clicks, qt_clicks, qt_clicks}, true, 0);
}

}  // namespace segmentation_platform
