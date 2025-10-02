// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_ANDROID_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_ANDROID_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/overlay_processor_using_strategy.h"

namespace base {
class WaitableEvent;
}

namespace gpu {
class DisplayCompositorMemoryAndTaskControllerOnGpu;
}

namespace viz {
class OverlayProcessorOnGpu;

// This class is used on Android for the pre-SurfaceControl case.
// This is an overlay processor for supporting fullscreen video underlays on
// Android. Things are a bit different on Android compared with other platforms.
// By the time a video frame is marked as overlayable it means the video decoder
// was outputting to a Surface that we can't read back from. As a result, the
// overlay must always succeed, or the video won't be visible. This is one of
// the reasons that only fullscreen is supported: we have to be sure that
// nothing will cause the overlay to be rejected, because there's no fallback to
// gl compositing.
class VIZ_SERVICE_EXPORT OverlayProcessorAndroid
    : public OverlayProcessorUsingStrategy {
 public:
  explicit OverlayProcessorAndroid(
      DisplayCompositorMemoryAndTaskController* display_controller);
  ~OverlayProcessorAndroid() override;

  bool IsOverlaySupported() const override;

  bool NeedsSurfaceDamageRectList() const override;

  void ScheduleOverlays(
      DisplayResourceProvider* display_resource_provider) override;
  void OverlayPresentationComplete() override;

  // Override OverlayProcessorUsingStrategy.
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  void SetViewportSize(const gfx::Size& size) override {}

  void CheckOverlaySupportImpl(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* candidates) override;
  gfx::Rect GetOverlayDamageRectForOutputSurface(
      const OverlayCandidate& overlay) const override;

 private:
  // OverlayProcessor needs to send overlay candidate information to the gpu
  // thread. These two methods are scheduled on the gpu thread to setup and
  // teardown the gpu side receiver.
  void InitializeOverlayProcessorOnGpu(
      gpu::DisplayCompositorMemoryAndTaskControllerOnGpu*
          display_controller_on_gpu,
      base::WaitableEvent* event);
  void DestroyOverlayProcessorOnGpu(base::WaitableEvent* event);
  void TakeOverlayCandidates(CandidateList* candidate_list) override;
  void NotifyOverlayPromotion(
      DisplayResourceProvider* display_resource_provider,
      const OverlayCandidateList& candidate_list,
      const QuadList& quad_list) override;

  // [id] == candidate's |display_rect| for all promotable resources.
  using PromotionHintInfoMap = std::map<ResourceId, gfx::RectF>;

  // For android, this provides a set of resources that could be promoted to
  // overlay, if one backs them with a SurfaceView.
  PromotionHintInfoMap promotion_hint_info_map_;

  raw_ptr<gpu::GpuTaskSchedulerHelper> gpu_task_scheduler_;
  // This class is created, accessed, and destroyed on the gpu thread.
  std::unique_ptr<OverlayProcessorOnGpu> processor_on_gpu_;

  OverlayCandidateList overlay_candidates_;

  using OverlayResourceLock =
      DisplayResourceProvider::ScopedReadLockSharedImage;

  // Keep locks on overlay resources to keep them alive. Since we don't have
  // an exact signal on when the overlays are done presenting, use
  // OverlayPresentationComplete as a signal to clear locks from the older
  // frames.
  base::circular_deque<std::vector<OverlayResourceLock>> pending_overlay_locks_;
  // Locks for overlays have been committed. |pending_overlay_locks_| will
  // be moved to |committed_overlay_locks_| after OverlayPresentationComplete.
  std::vector<OverlayResourceLock> committed_overlay_locks_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_ANDROID_H_
