// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

using proto::SegmentId;

struct InputContext;
struct SegmentSelectionResult;
class ExecutionService;

// Central class for segment selection that can be used by clients to find the
// best selected segment. Listens for model execution events, on which it
// selects the best segment and writes it to prefs. The computed results are
// only used in the next session. Current session uses results from the last
// session.
class SegmentSelector : public ModelExecutionScheduler::Observer {
 public:
  virtual ~SegmentSelector() = default;

  using SegmentSelectionCallback =
      base::OnceCallback<void(const SegmentSelectionResult&)>;

  // Called when segmentation platform is initialized.
  virtual void OnPlatformInitialized(ExecutionService* execution_service) = 0;

  // Client API. Returns the selected segment from the last session
  // asynchronously. If none, returns empty result.
  virtual void GetSelectedSegment(SegmentSelectionCallback callback) = 0;

  // Client API. Returns the cached selected segment from the last session
  // synchronously.
  virtual SegmentSelectionResult GetCachedSegmentResult() = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_H_
