// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_INPUT_INPUT_ON_VIZ_STATE_PROCESSING_RESULT_H_
#define COMPONENTS_VIZ_SERVICE_INPUT_INPUT_ON_VIZ_STATE_PROCESSING_RESULT_H_

namespace viz {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class InputOnVizStateProcessingResult {
  kProcessedSuccessfully = 0,
  kCouldNotFindViewForFrameSinkId = 1,
  kFrameSinkIdCorrespondsToChildView = 2,
  kFrameSinkIdNotAttachedToRootCFS = 3,
  kDroppedOutOfOrderDownTime = 4,
  kDroppedTooManyPendingStates = 5,
  kDroppedUnusedOlderStates = 6,
  kTransferBackToBrowserSuccessfully = 7,
  kDroppedTransferBackToBrowserFailed = 8,
  kMaxValue = kDroppedTransferBackToBrowserFailed,
};

void EmitStateProcessingResultHistogram(InputOnVizStateProcessingResult result);

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_INPUT_INPUT_ON_VIZ_STATE_PROCESSING_RESULT_H_
