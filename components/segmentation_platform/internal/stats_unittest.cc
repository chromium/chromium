// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/stats.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
namespace stats {

class StatsTest : public testing::Test {
 public:
  ~StatsTest() override = default;
};

TEST_F(StatsTest, SegmentSwitch) {
  std::string histogram("SegmentationPlatform.AdaptiveToolbar.SegmentSwitched");
  base::HistogramTester tester;

  // Share -> New tab.
  RecordSegmentSelectionComputed(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);

  // None -> Share.
  RecordSegmentSelectionComputed(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
      absl::nullopt);

  // Share -> Share.
  RecordSegmentSelectionComputed(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
  tester.ExpectTotalCount(histogram, 2);

  EXPECT_THAT(
      tester.GetAllSamples(histogram),
      testing::ElementsAre(
          base::Bucket(
              static_cast<int>(AdaptiveToolbarSegmentSwitch::kNoneToShare), 1),
          base::Bucket(
              static_cast<int>(AdaptiveToolbarSegmentSwitch::kShareToNewTab),
              1)));
  tester.ExpectTotalCount(
      "SegmentationPlatform.AdaptiveToolbar.SegmentSelection.Computed", 3);
}

}  // namespace stats
}  // namespace segmentation_platform
