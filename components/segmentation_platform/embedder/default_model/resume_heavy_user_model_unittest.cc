// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/resume_heavy_user_model.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class ResumeHeavyUserModelTest : public testing::Test {
 public:
  ResumeHeavyUserModelTest() = default;
  ~ResumeHeavyUserModelTest() override = default;

  void SetUp() override {
    resume_heavy_user_model_ = std::make_unique<ResumeHeavyUserModel>();
  }

  void TearDown() override { resume_heavy_user_model_.reset(); }

  void ExpectInitAndFetchModel() {
    base::RunLoop loop;
    resume_heavy_user_model_->InitAndFetchModel(
        base::BindRepeating(&ResumeHeavyUserModelTest::OnInitFinishedCallback,
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
    resume_heavy_user_model_->ExecuteModelWithInput(
        inputs,
        base::BindOnce(&ResumeHeavyUserModelTest::OnExecutionFinishedCallback,
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
  std::unique_ptr<ResumeHeavyUserModel> resume_heavy_user_model_;
};

TEST_F(ResumeHeavyUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(ResumeHeavyUserModelTest, ExecuteModelWithInput) {
  // Input arguments in order: bookmarks_opened, mv_tiles_clicked,
  // opened_ntp_from_tab_groups, opened_item_from_history
  ExpectExecutionWithInput({0, 0, 0, 0, 0}, false, 0);
  ExpectExecutionWithInput({1, 0, 0, 0, 0}, false, 0);
  ExpectExecutionWithInput({2, 0, 0, 0, 0}, false, 1);
  ExpectExecutionWithInput({0, 3, 0, 0, 0}, false, 1);
}

}  // namespace segmentation_platform
