// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/shopping_user_model.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class ShoppingUserModelTest : public testing::Test {
 public:
  ShoppingUserModelTest() = default;
  ~ShoppingUserModelTest() override = default;

  void SetUp() override {
    shopping_user_model_ = std::make_unique<ShoppingUserModel>();
  }

  void TearDown() override { shopping_user_model_.reset(); }

  void ExpectInitAndFetchModel() {
    base::RunLoop loop;
    shopping_user_model_->InitAndFetchModel(
        base::BindRepeating(&ShoppingUserModelTest::OnInitFinishedCallback,
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
    shopping_user_model_->ExecuteModelWithInput(
        inputs,
        base::BindOnce(&ShoppingUserModelTest::OnExecutionFinishedCallback,
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
  std::unique_ptr<ShoppingUserModel> shopping_user_model_;
};

TEST_F(ShoppingUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(ShoppingUserModelTest, ExecuteModelWithInput) {
  // When shopping related features count is less than or equal to 1,
  // the user shouldn't be considered a shopping user.
  ExpectExecutionWithInput({0, 0}, false, 0);
  ExpectExecutionWithInput({1, 0}, false, 0);
  ExpectExecutionWithInput({1, 1}, false, 0);

  // When shopping related features count is greater than or equal to 1,
  // the user should be considered shopping user.
  ExpectExecutionWithInput({1, 2}, false, 1);
  ExpectExecutionWithInput({2, 2}, false, 1);

  // Invalid input
  ExpectExecutionWithInput({1, 1, 1, 1, 1}, true, 0);
  ExpectExecutionWithInput({0}, true, 0);
  ExpectExecutionWithInput({2, 2, 2, 2, 2, 2, 2, 2}, true, 0);
}

}  // namespace segmentation_platform
