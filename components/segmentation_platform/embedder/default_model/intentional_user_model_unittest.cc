// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/intentional_user_model.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class IntentionalUserModelTest : public testing::Test {
 public:
  IntentionalUserModelTest() = default;
  ~IntentionalUserModelTest() override = default;

  void SetUp() override {
    intentional_user_model_ = std::make_unique<IntentionalUserModel>();
  }

  void TearDown() override { intentional_user_model_.reset(); }

  void ExpectInitAndFetchModel() {
    base::RunLoop loop;
    intentional_user_model_->InitAndFetchModel(
        base::BindRepeating(&IntentionalUserModelTest::OnInitFinishedCallback,
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
    intentional_user_model_->ExecuteModelWithInput(
        inputs,
        base::BindOnce(&IntentionalUserModelTest::OnExecutionFinishedCallback,
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
  std::unique_ptr<IntentionalUserModel> intentional_user_model_;
};

TEST_F(IntentionalUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(IntentionalUserModelTest, ExecuteModelWithInput) {
  // Test with empty input.
  ExpectExecutionWithInput({}, true, 0);

  // Test with more inputs than expected.
  ExpectExecutionWithInput({12, 21}, true, 0);

  // If Chrome hasn't been launched from its main launcher icon then the user is
  // not intentional.
  ExpectExecutionWithInput({0}, false, 0);

  ExpectExecutionWithInput({1}, false, 0);

  // If chrome was launched at least twice from its main laincher icon then the
  // user is intentional.
  ExpectExecutionWithInput({2}, false, 1);

  ExpectExecutionWithInput({10}, false, 1);
}

}  // namespace segmentation_platform
