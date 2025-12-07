// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/resume_heavy_user_model.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

using Feature = ResumeHeavyUserModel::Feature;

class ResumeHeavyUserModelTest : public DefaultModelTestBase {
 public:
  ResumeHeavyUserModelTest()
      : DefaultModelTestBase(std::make_unique<ResumeHeavyUserModel>()) {}
  ~ResumeHeavyUserModelTest() override = default;
};

TEST_F(ResumeHeavyUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);
}

TEST_F(ResumeHeavyUserModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  ModelProvider::Request input(Feature::kFeatureCount, 0);
  // Input arguments in order: bookmarks_opened, mv_tiles_clicked,
  // opened_ntp_from_tab_groups, opened_item_from_history
  ExpectClassifierResults(input, {kLegacyNegativeLabel});
  input[Feature::kFeatureMobileBookmarkManagerOpen] = 1;
  ExpectClassifierResults(input, {kLegacyNegativeLabel});
  input[Feature::kFeatureMobileBookmarkManagerOpen] = 2;
  ExpectClassifierResults(
      input,
      {SegmentIdToHistogramVariant(SegmentId::RESUME_HEAVY_USER_SEGMENT)});
  input[Feature::kFeatureMobileBookmarkManagerOpen] = 0;
  input[Feature::kFeatureNewTabPageMostVisitedClicked] = 3;
  ExpectClassifierResults(
      input,
      {SegmentIdToHistogramVariant(SegmentId::RESUME_HEAVY_USER_SEGMENT)});
}

}  // namespace segmentation_platform
