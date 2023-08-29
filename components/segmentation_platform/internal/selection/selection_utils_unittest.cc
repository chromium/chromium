// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/selection_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class SelectionUtilsTest : public testing::Test {
 public:
  ~SelectionUtilsTest() override = default;
};

TEST_F(SelectionUtilsTest, ResultStateToPredictionStatus) {
  // Success
  ASSERT_EQ(
      PredictionStatus::kSucceeded,
      selection_utils::ResultStateToPredictionStatus(
          SegmentResultProvider::ResultState::kServerModelDatabaseScoreUsed));
  ASSERT_EQ(
      PredictionStatus::kSucceeded,
      selection_utils::ResultStateToPredictionStatus(
          SegmentResultProvider::ResultState::kDefaultModelDatabaseScoreUsed));
  ASSERT_EQ(
      PredictionStatus::kSucceeded,
      selection_utils::ResultStateToPredictionStatus(
          SegmentResultProvider::ResultState::kServerModelExecutionScoreUsed));
  ASSERT_EQ(
      PredictionStatus::kSucceeded,
      selection_utils::ResultStateToPredictionStatus(
          SegmentResultProvider::ResultState::kDefaultModelExecutionScoreUsed));

  // Not ready
  ASSERT_EQ(
      PredictionStatus::kNotReady,
      selection_utils::ResultStateToPredictionStatus(
          SegmentResultProvider::ResultState::kServerModelSignalsNotCollected));
  ASSERT_EQ(PredictionStatus::kNotReady,
            selection_utils::ResultStateToPredictionStatus(
                SegmentResultProvider::ResultState::
                    kDefaultModelSignalsNotCollected));

  // Failed
  ASSERT_EQ(PredictionStatus::kFailed,
            selection_utils::ResultStateToPredictionStatus(
                SegmentResultProvider::ResultState::
                    kServerModelSegmentInfoNotAvailable));
  ASSERT_EQ(PredictionStatus::kFailed,
            selection_utils::ResultStateToPredictionStatus(
                SegmentResultProvider::ResultState::
                    kDefaultModelSegmentInfoNotAvailable));
  ASSERT_EQ(
      PredictionStatus::kFailed,
      selection_utils::ResultStateToPredictionStatus(
          SegmentResultProvider::ResultState::kServerModelExecutionFailed));
  ASSERT_EQ(
      PredictionStatus::kFailed,
      selection_utils::ResultStateToPredictionStatus(
          SegmentResultProvider::ResultState::kDefaultModelExecutionFailed));
}

}  // namespace segmentation_platform
