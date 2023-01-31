// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/service/display_embedder/output_presenter.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"

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
      const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback);

  ~SkiaOutputDeviceBufferQueue() override;

  SkiaOutputDeviceBufferQueue(const SkiaOutputDeviceBufferQueue&) = delete;
  SkiaOutputDeviceBufferQueue& operator=(const SkiaOutputDeviceBufferQueue&) =
      delete;

  // SkiaOutputDevice overrides.
  void Submit(bool sync_cpu, base::OnceClosure callback) override;
  void SwapBuffers(BufferPresentedCallback feedback,
                   OutputSurfaceFrame frame) override;
  void PostSubBuffer(const gfx::Rect& rect,
                     BufferPresentedCallback feedback,
                     OutputSurfaceFrame frame) override;
  void CommitOverlayPlanes(BufferPresentedCallback feedback,
                           OutputSurfaceFrame frame) override;
  bool Reshape(const SkSurfaceCharacterization& characterization,
               const gfx::ColorSpace& color_space,
               float device_scale_factor,
               gfx::OverlayTransform transform) override;
  void SetViewportSize(const gfx::Size& viewport_size) override;
  SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndPaint() override;
  bool EnsureMinNumberOfBuffers(size_t n) override;

  bool IsPrimaryPlaneOverlay() const override;
  void SchedulePrimaryPlane(
      const absl::optional<
          OverlayProcessorInterface::OutputSurfaceOverlayPlane>& plane)
      override;
  void ScheduleOverlays(SkiaOutputSurface::OverlayList overlays) override;

  // SkiaOutputDevice override
  void SetGpuVSyncEnabled(bool enabled) override;
  void SetVSyncDisplayID(int64_t display_id) override;

 private:
  friend class SkiaOutputDeviceBufferQueueTest;

  using CancelableSwapCompletionCallback =
      base::CancelableOnceCallback<void(gfx::SwapCompletionResult)>;

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

  gfx::Size GetSwapBuffersSize();
  bool RecreateImages();

  // Given an overlay mailbox, returns the corresponding OverlayData* from
  // |overlays_|. Inserts an OverlayData if mailbox is not in |overlays_|.
  OverlayData* GetOrCreateOverlayData(const gpu::Mailbox& mailbox,
                                      bool* is_existing = nullptr);

  std::unique_ptr<OutputPresenter> presenter_;

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
  // These cancelable callbacks bind images that have been scheduled to display
  // but are not displayed yet. This deque will be cleared when represented
  // frames are destroyed. Use CancelableOnceCallback to prevent resources
  // from being destructed outside SkiaOutputDeviceBufferQueue life span.
  base::circular_deque<std::unique_ptr<CancelableSwapCompletionCallback>>
      swap_completion_callbacks_;
  // Mailboxes of scheduled overlays for the next SwapBuffers call.
  std::vector<gpu::Mailbox> pending_overlay_mailboxes_;
  // Mailboxes of committed overlays for the last SwapBuffers call.
  std::vector<gpu::Mailbox> committed_overlay_mailboxes_;

  class OverlayDataComparator {
   public:
    using is_transparent = void;
    bool operator()(const OverlayData& lhs, const OverlayData& rhs) const;
    bool operator()(const OverlayData& lhs, const gpu::Mailbox& rhs) const;
    bool operator()(const gpu::Mailbox& lhs, const OverlayData& rhs) const;
  };
  // A set for all overlays. The set uses overlay_data.mailbox() as the unique
  // key.
  base::flat_set<OverlayData, OverlayDataComparator> overlays_;

  // Set to true if no image is to be used for the primary plane of this frame.
  bool current_frame_has_no_primary_plane_ = false;
  // Whether |SchedulePrimaryPlane| needs to wait for a paint before scheduling
  // This works around an edge case for unpromoting fullscreen quads.
  bool primary_plane_waiting_on_paint_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_
