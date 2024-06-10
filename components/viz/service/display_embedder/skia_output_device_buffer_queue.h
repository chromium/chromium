// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/viz/service/display_embedder/output_presenter.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"

namespace viz {

class SkiaOutputSurfaceDependency;

class VIZ_SERVICE_EXPORT SkiaOutputDeviceBufferQueue : public SkiaOutputDevice {
 public:
  class OverlayData;

  SkiaOutputDeviceBufferQueue(
      std::unique_ptr<OutputPresenter> presenter,
      SkiaOutputSurfaceDependency* deps,
      gpu::SharedImageRepresentationFactory* representation_factory,
      gpu::MemoryTracker* memory_tracker,
      const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
      const ReleaseOverlaysCallback& release_overlays_callback);

  ~SkiaOutputDeviceBufferQueue() override;

  SkiaOutputDeviceBufferQueue(const SkiaOutputDeviceBufferQueue&) = delete;
  SkiaOutputDeviceBufferQueue& operator=(const SkiaOutputDeviceBufferQueue&) =
      delete;

  // SkiaOutputDevice overrides.
  void Submit(bool sync_cpu, base::OnceClosure callback) override;
  void Present(const std::optional<gfx::Rect>& update_rect,
               BufferPresentedCallback feedback,
               OutputSurfaceFrame frame) override;
  bool Reshape(const ReshapeParams& params) override;
  void SetViewportSize(const gfx::Size& viewport_size) override;
  SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndPaint() override;
  bool EnsureMinNumberOfBuffers(size_t n) override;

  bool IsPrimaryPlaneOverlay() const override;
  void SchedulePrimaryPlane(
      const std::optional<OverlayProcessorInterface::OutputSurfaceOverlayPlane>&
          plane) override;
  void ScheduleOverlays(SkiaOutputSurface::OverlayList overlays) override;

  // SkiaOutputDevice override
  void SetVSyncDisplayID(int64_t display_id) override;

  base::OneShotTimer& OverlaysReclaimTimerForTesting() {
    return reclaim_overlays_timer_;
  }
  void SetSwapTimeClockForTesting(base::TickClock* clock) {
    swap_time_clock_ = clock;
  }

 private:
  friend class SkiaOutputDeviceBufferQueueTest;

  OutputPresenter::Image* GetNextImage();
  void PageFlipComplete(OutputPresenter::Image* image,
                        gfx::GpuFenceHandle release_fence);
  void FreeAllSurfaces();
  // Used as callback for SwapBuffersAsync and PostSubBufferAsync to finish
  // operation
  void DoFinishSwapBuffers(const gfx::Size& size,
                           OutputSurfaceFrame frame,
                           const base::WeakPtr<OutputPresenter::Image>& image,
                           std::vector<gpu::Mailbox> overlay_mailboxes,
                           gfx::SwapCompletionResult result);
  void PostReleaseOverlays();
  void ReleaseOverlays();

  gfx::Size GetSwapBuffersSize();
  bool RecreateImages();

  // Given an overlay mailbox, returns the corresponding OverlayData* from
  // |overlays_|. Inserts an OverlayData if mailbox is not in |overlays_|.
  const OverlayData* GetOrCreateOverlayData(const gpu::Mailbox& mailbox,
                                            bool is_root_render_pass,
                                            bool* is_existing = nullptr);

  std::unique_ptr<OutputPresenter> presenter_;
  const gpu::GpuDriverBugWorkarounds workarounds_;

  scoped_refptr<gpu::SharedContextState> context_state_;
  const raw_ptr<gpu::SharedImageRepresentationFactory> representation_factory_;
  // Format of images
  gfx::ColorSpace color_space_;
  gfx::Size image_size_;
  int sample_count_ = 1;
  gfx::Size viewport_size_;
  gfx::OverlayTransform overlay_transform_ = gfx::OVERLAY_TRANSFORM_NONE;

  // Number of images to allocate. Equals to `capabilities_.number_of_buffers`
  // when `capabilities_.supports_dynamic_frame_buffer_allocation` is false.
  // Can be increased with `EnsureMinNumberOfBuffers` when
  // `capabilities_.supports_dynamic_frame_buffer_allocation` is true.
  size_t number_of_images_to_allocate_ = 0u;
  // All allocated images.
  std::vector<std::unique_ptr<OutputPresenter::Image>> images_;
  // This image is currently used by Skia as RenderTarget. This may be nullptr
  // if there is no drawing for the current frame or if allocation failed.
  raw_ptr<OutputPresenter::Image, DanglingUntriaged> current_image_ = nullptr;
  // The last image submitted for presenting.
  raw_ptr<OutputPresenter::Image, DanglingUntriaged> submitted_image_ = nullptr;
  // The image currently on the screen, if any.
  raw_ptr<OutputPresenter::Image, DanglingUntriaged> displayed_image_ = nullptr;
  // These are free for use, and are not nullptr.
  base::circular_deque<OutputPresenter::Image*> available_images_;
  // Mailboxes of scheduled overlays for the next SwapBuffers call.
  std::vector<gpu::Mailbox> pending_overlay_mailboxes_;
  // Mailboxes of committed overlays for the last SwapBuffers call.
  std::vector<gpu::Mailbox> committed_overlay_mailboxes_;

  struct OverlayDataHash {
    using is_transparent = void;
    std::size_t operator()(const OverlayData& o) const;
    std::size_t operator()(const gpu::Mailbox& m) const;
  };

  struct OverlayDataKeyEqual {
    using is_transparent = void;
    bool operator()(const OverlayData& lhs, const OverlayData& rhs) const;
    bool operator()(const OverlayData& lhs, const gpu::Mailbox& rhs) const;
    bool operator()(const gpu::Mailbox& lhs, const OverlayData& rhs) const;
  };

  // A set for all overlays. The set uses overlay_data.mailbox() as the unique
  // key.
  std::unordered_set<OverlayData, OverlayDataHash, OverlayDataKeyEqual>
      overlays_;

  // Set to true if no image is to be used for the primary plane of this frame.
  bool current_frame_has_no_primary_plane_ = false;
  // Whether |SchedulePrimaryPlane| needs to wait for a paint before scheduling
  // This works around an edge case for unpromoting fullscreen quads.
  bool primary_plane_waiting_on_paint_ = false;

  bool has_overlays_scheduled_but_swap_not_finished_ = false;
  raw_ptr<const base::TickClock> swap_time_clock_ =
      base::DefaultTickClock::GetInstance();
  base::TimeTicks last_swap_time_;
  base::OneShotTimer reclaim_overlays_timer_;
  static constexpr base::TimeDelta kDelayForOverlaysReclaim = base::Seconds(1);

  size_t num_pending_swap_completion_callbacks_for_testing_ = 0u;

  base::WeakPtrFactory<SkiaOutputDeviceBufferQueue> weak_ptr_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_
