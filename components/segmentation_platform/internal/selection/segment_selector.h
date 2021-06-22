// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_H_

#include "base/callback.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform {
struct SegmentSelectionResult;

// Central class for segment selection that can be used by clients to find the
// best selected segment or result of a particular segment. Listens for model
// execution events, on which it selects the best segment and writes it to
// prefs. The computed results are only used in the next session. Current
// session uses results from the last session.
class SegmentSelector : public ModelExecutionScheduler::Observer {
 public:
  virtual ~SegmentSelector() = default;

  using SegmentSelectionCallback =
      base::OnceCallback<void(const SegmentSelectionResult&)>;
  using SingleSegmentResultCallback =
      base::OnceCallback<void(absl::optional<float>)>;

  // Called to initialize the selector. Reads results from last session into
  // memory. Must be invoked before calling any other method except
  // GetSelectedSegment which is served from prefs.
  // TODO(shaktisahu): Remove this method. Save model scores to prefs, and read
  // them back in constructor. After that, none of the public methods will need
  // to wait for the segment DB loading.
  virtual void Initialize(base::OnceClosure callback) = 0;

  // Client API. Returns the selected segment from the last session. If none,
  // returns empty result.
  virtual void GetSelectedSegment(SegmentSelectionCallback callback) = 0;

  // Client API to get the score for a single segment. Returns the cached score
  // from the last session.
  virtual void GetSegmentScore(OptimizationTarget segment_id,
                               SingleSegmentResultCallback callback) = 0;

  // Client API. Called when the segment is actually used by the client
  // features, so that we have to honor segment_selection_ttl_days. Should be
  // saved to prefs. If the selected segment hasn't been used, the pref result
  // can be overwritten any number of times.
  virtual void OnSegmentUsed(OptimizationTarget segment_id) = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_H_
