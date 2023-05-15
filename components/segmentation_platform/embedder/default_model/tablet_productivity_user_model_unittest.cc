// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/tablet_productivity_user_model.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

class TabletProductivityUserModelTest : public DefaultModelTestBase {
 public:
  TabletProductivityUserModelTest()
      : DefaultModelTestBase(std::make_unique<TabletProductivityUserModel>()) {}
  ~TabletProductivityUserModelTest() override = default;
};

TEST_F(TabletProductivityUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);
}

TEST_F(TabletProductivityUserModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  ModelProvider::Request input = {};
  // High end tablet productivity users
  ExpectClassifierResults(
      /*input=*/{/*8GB=*/8192, 10, 371, 0, 1, 1, 0, 0, 0, 0},
      {kTabletProductivityUserModelLabelHigh});
  ExpectClassifierResults(
      /*input=*/{/*8GB=*/8192, 10, 371, 0, 0, 1, 0, 1, 0, 0},
      {kTabletProductivityUserModelLabelHigh});
  ExpectClassifierResults(
      /*input=*/{/*6GB=*/6144, 11, 374, 0, 0, 0, 5, 0, 1, 0},
      {kTabletProductivityUserModelLabelHigh});
  ExpectClassifierResults(
      /*input=*/{/*6GB=*/6144, 12, 374, 2, 0, 0, 0, 0, 0, 1},
      {kTabletProductivityUserModelLabelHigh});

  // Medium end tablet productivity users.
  ExpectClassifierResults(
      /*input=*/{/*8GB=*/8192, 11, 371, 0, 0, 1, 0, 0, 0, 0},
      {kTabletProductivityUserModelLabelMedium});
  ExpectClassifierResults(
      /*input=*/{/*8GB=*/8192, 10, 371, 0, 0, 0, 0, 1, 0, 0},
      {kTabletProductivityUserModelLabelMedium});
  ExpectClassifierResults(
      /*input=*/{/*6GB=*/6144, 11, 374, 0, 0, 0, 5, 0, 0, 0},
      {kTabletProductivityUserModelLabelMedium});
  ExpectClassifierResults(
      /*input=*/{/*6GB=*/6144, 12, 374, 2, 0, 0, 0, 0, 0, 0},
      {kTabletProductivityUserModelLabelMedium});

  ExpectClassifierResults(
      /*input=*/{/*3GB=*/3072, 10, 10, 2, 0, 0, 0, 0, 0, 0},
      {kTabletProductivityUserModelLabelMedium});
  ExpectClassifierResults(
      /*input=*/{/*6GB=*/6144, 10, 670, 0, 0, 0, 5, 0, 0, 0},
      {kTabletProductivityUserModelLabelMedium});
  ExpectClassifierResults(
      /*input=*/{/*6GB=*/6144, 11, 370, 0, 0, 0, 0, 0, 0, 1},
      {kTabletProductivityUserModelLabelMedium});

  // All other tablet productivity users.
  ExpectClassifierResults(
      /*input=*/{/*2GB=*/2048, 9, 10, 0, 0, 0, 5, 0, 1, 0},
      {kTabletProductivityUserModelLabelNone});
  ExpectClassifierResults(
      /*input=*/{/*5GB=*/5120, 9, 10, 2, 0, 0, 4, 0, 0, 0},
      {kTabletProductivityUserModelLabelNone});
  ExpectClassifierResults(
      /*input=*/{/*5GB=*/5120, 9, 10, 0, 0, 0, 0, 0, 0, 0},
      {kTabletProductivityUserModelLabelNone});
}

}  // namespace segmentation_platform
