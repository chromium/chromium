// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/null_renderer.h"

#include "base/notreached.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/service/display/output_surface.h"

namespace viz {

NullRenderer::NullRenderer(const RendererSettings* settings,
                           const DebugRendererSettings* debug_settings,
                           OutputSurface* output_surface,
                           DisplayResourceProvider* resource_provider,
                           OverlayProcessorInterface* overlay_processor)
    : DirectRenderer(settings,
                     debug_settings,
                     output_surface,
                     resource_provider,
                     overlay_processor) {
  DCHECK(output_surface->capabilities().skips_draw);
}
NullRenderer::~NullRenderer() = default;

void NullRenderer::SwapBuffers(SwapFrameData swap_frame_data) {
  NOTREACHED_IN_MIGRATION();
}
void NullRenderer::BeginDrawingFrame() {
  NOTREACHED_IN_MIGRATION();
}

bool NullRenderer::CanPartialSwap() {
  return false;
}

bool NullRenderer::IsRenderPassResourceAllocated(
    const AggregatedRenderPassId& render_pass_id) const {
  return false;
}

gfx::Size NullRenderer::GetRenderPassBackingPixelSize(
    const AggregatedRenderPassId& render_pass_id) {
  return gfx::Size();
}

void NullRenderer::CopyDrawnRenderPass(
    const copy_output::RenderPassGeometry& geometry,
    std::unique_ptr<CopyOutputRequest> request) {}

gfx::Rect NullRenderer::GetRenderPassBackingDrawnRect(
    const AggregatedRenderPassId& render_pass_id) const {
  return gfx::Rect();
}

}  // namespace viz
