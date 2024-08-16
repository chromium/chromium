// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_H_

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "ui/gfx/frame_data.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"

namespace viz {

class VIZ_SERVICE_EXPORT OutputPresenter {
 public:
  OutputPresenter() = default;
  virtual ~OutputPresenter() = default;

  using BufferPresentedCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback& feedback)>;
  using SwapCompletionCallback =
      base::OnceCallback<void(gfx::SwapCompletionResult)>;

  virtual void InitializeCapabilities(
      OutputSurface::Capabilities* capabilities) = 0;

  using ReshapeParams = SkiaOutputDevice::ReshapeParams;
  virtual bool Reshape(const ReshapeParams& params) = 0;
  virtual void Present(SwapCompletionCallback completion_callback,
                       BufferPresentedCallback presentation_callback,
                       gfx::FrameData data) = 0;

  using OverlayPlaneCandidate = OverlayCandidate;
  using ScopedOverlayAccess = gpu::OverlayImageRepresentation::ScopedReadAccess;
  virtual void ScheduleOverlayPlane(
      const OverlayPlaneCandidate& overlay_plane_candidate,
      ScopedOverlayAccess* access,
      std::unique_ptr<gfx::GpuFence> acquire_fence) = 0;

  virtual void SetVSyncDisplayID(int64_t display_id) {}

#if BUILDFLAG(IS_APPLE)
  virtual void SetMaxPendingSwaps(int max_pending_swaps) {}
#endif
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_H_
