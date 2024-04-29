// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EVICTION_HANDLER_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EVICTION_HANDLER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/frame_sinks/surface_resource_holder.h"

namespace viz {
class CompositorFrameSinkSupport;
class Display;

// Handles eviction of `RootCompositorFrameSinkImpl` by either pushing empty
// solid color content or taking a snapshot then pushing that through the
// graphics pipeline to deref all surfaces.
class EvictionHandler : public ReservedResourceDelegate {
 public:
  EvictionHandler(Display* display,
                  CompositorFrameSinkSupport* support,
                  ReservedResourceIdTracker* id_tracker);

  EvictionHandler(const EvictionHandler&) = delete;
  EvictionHandler& operator=(const EvictionHandler&) = delete;

  ~EvictionHandler() override;

  bool WillEvictSurface(const SurfaceId& surface_id);
  void MaybeFinishEvictionProcess();
  void DisplayDidDrawAndSwap();

  // ReservedResourceDelegate:
  void ReceiveFromChild(
      const std::vector<TransferableResource>& resources) override;
  void RefResources(
      const std::vector<TransferableResource>& resources) override;
  void UnrefResources(const std::vector<ReturnedResource>& resources) override;

 private:
  void TakeSnapshotForEviction(const SurfaceId& surface_id, double scale);

  // Submits a compositor frame with either no content (solid color) if
  // `copy_result` is null, or the copied content if `copy_result` is non-null.
  void SubmitPlaceholderContentForEviction(
      SurfaceId to_evict,
      int64_t snapshot_seq_id,
      std::unique_ptr<CopyOutputResult> copy_result);

  // `RootCompositorFrameSinkImpl` owns `Display` and
  // `CompositorFrameSinkSupport` and outlives `EvictionHandler`.
  const raw_ptr<Display> display_;
  const raw_ptr<CompositorFrameSinkSupport> support_;

  // If we evict the root surface, we want to push an empty compositor
  // frame to it first to unref its resources. This requires a draw
  // and swap to complete to actually unref.
  LocalSurfaceId to_evict_on_next_draw_and_swap_;

  raw_ptr<ReservedResourceIdTracker> id_tracker_;

  base::flat_map<ResourceId, std::unique_ptr<CopyOutputResult>>
      copy_output_results_;

  // True if we are currently doing the eviction process.
  bool in_progress_ = false;

  // It is possible that we receive a notification that a copy request has
  // finished after cancelling then restarting the eviction process, when
  // the display is toggled between not visible and visible quickly. We only
  // want to take the latest snapshot, so store a sequence ID here.
  int64_t snapshot_seq_id_ = 0;

  base::WeakPtrFactory<EvictionHandler> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EVICTION_HANDLER_H_
