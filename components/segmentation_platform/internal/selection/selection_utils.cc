// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/selection_utils.h"

namespace segmentation_platform::selection_utils {

PredictionStatus ResultStateToPredictionStatus(
    SegmentResultProvider::ResultState result_state) {
  switch (result_state) {
    case SegmentResultProvider::ResultState::kServerModelDatabaseScoreUsed:
    case SegmentResultProvider::ResultState::kDefaultModelDatabaseScoreUsed:
    case SegmentResultProvider::ResultState::kDefaultModelExecutionScoreUsed:
    case SegmentResultProvider::ResultState::kServerModelExecutionScoreUsed:
      return PredictionStatus::kSucceeded;
    case SegmentResultProvider::ResultState::kServerModelSignalsNotCollected:
    case SegmentResultProvider::ResultState::kDefaultModelSignalsNotCollected:
      return PredictionStatus::kNotReady;
    default:
      return PredictionStatus::kFailed;
  }
}

}  // namespace segmentation_platform::selection_utils
