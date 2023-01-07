// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/price_tracking_action_model.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class PriceTrackingActionModelTest : public testing::Test {
 public:
  PriceTrackingActionModelTest() = default;
  ~PriceTrackingActionModelTest() override = default;

  void SetUp() override {
    model_ = std::make_unique<PriceTrackingActionModel>();
  }

  void TearDown() override { model_.reset(); }

  void ExpectInitAndFetchModel() {
    base::RunLoop loop;
    model_->InitAndFetchModel(base::BindRepeating(
        &PriceTrackingActionModelTest::OnInitFinishedCallback,
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
    model_->ExecuteModelWithInput(
        inputs, base::BindOnce(
                    &PriceTrackingActionModelTest::OnExecutionFinishedCallback,
                    base::Unretained(this), loop.QuitClosure(), expected_error,
                    expected_result));
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
  std::unique_ptr<PriceTrackingActionModel> model_;
};

TEST_F(PriceTrackingActionModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(PriceTrackingActionModelTest, ExecuteModelWithInput) {
  // Input vector empty.
  std::vector<float> input;
  ExpectExecutionWithInput(input, true, 0);

  // Price tracking = 0
  input.assign(1, 0);
  ExpectExecutionWithInput(input, false, 0);

  // Price tracking = 1
  input.assign(1, 1);
  ExpectExecutionWithInput(input, false, 1);
}

}  // namespace segmentation_platform
