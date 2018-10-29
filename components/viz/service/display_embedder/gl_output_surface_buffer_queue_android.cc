// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface_buffer_queue_android.h"

#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/service/display_embedder/compositor_overlay_candidate_validator_android.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {
namespace {
class OverlayCandidateValidatorImpl : public OverlayCandidateValidator {
 public:
  OverlayCandidateValidatorImpl() = default;
  ~OverlayCandidateValidatorImpl() override = default;

  void GetStrategies(OverlayProcessor::StrategyList* strategies) override {
    strategies->push_back(std::make_unique<OverlayStrategyUnderlay>(
        this, OverlayStrategyUnderlay::OpaqueMode::AllowTransparentCandidates));
  }
  bool AllowCALayerOverlays() override { return false; }
  bool AllowDCLayerOverlays() override { return false; }
  void CheckOverlaySupport(OverlayCandidateList* surfaces) override {
    DCHECK(!surfaces->empty());

    // Only update the last candidate that was added to the list. All previous
    // overlays should have already been handled.
    auto& candidate = surfaces->back();
    candidate.display_rect =
        gfx::RectF(gfx::ToEnclosingRect(candidate.display_rect));
    candidate.overlay_handled = true;

#if DCHECK_IS_ON()
    for (auto& candidate : *surfaces)
      DCHECK(candidate.overlay_handled);
#endif
  }
};

}  // namespace

GLOutputSurfaceBufferQueueAndroid::GLOutputSurfaceBufferQueueAndroid(
    scoped_refptr<VizProcessContextProvider> context_provider,
    gpu::SurfaceHandle surface_handle,
    SyntheticBeginFrameSource* synthetic_begin_frame_source,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gfx::BufferFormat buffer_format)
    : GLOutputSurfaceBufferQueue(context_provider,
                                 surface_handle,
                                 synthetic_begin_frame_source,
                                 gpu_memory_buffer_manager,
                                 GL_TEXTURE_2D,
                                 GL_RGBA,
                                 buffer_format),
      overlay_candidate_validator_(
          std::make_unique<OverlayCandidateValidatorImpl>()) {}

GLOutputSurfaceBufferQueueAndroid::~GLOutputSurfaceBufferQueueAndroid() =
    default;

void GLOutputSurfaceBufferQueueAndroid::HandlePartialSwap(
    const gfx::Rect& sub_buffer_rect,
    uint32_t flags,
    gpu::ContextSupport::SwapCompletedCallback swap_callback,
    gpu::ContextSupport::PresentationCallback presentation_callback) {
  DCHECK(sub_buffer_rect.IsEmpty());
  context_provider_->ContextSupport()->CommitOverlayPlanes(
      flags, std::move(swap_callback), std::move(presentation_callback));
}

OverlayCandidateValidator*
GLOutputSurfaceBufferQueueAndroid::GetOverlayCandidateValidator() const {
  return overlay_candidate_validator_.get();
}

}  // namespace viz
