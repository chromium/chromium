// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_NULL_RENDERER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_NULL_RENDERER_H_

#include <memory>

#include "components/viz/service/display/direct_renderer.h"

namespace viz {

// Empty implementation of the DirectRenderer, used with OutputSurfaceUnified.
// Doesn't support Draw and will crash if Draw of SwapBuffers will be called.
class VIZ_SERVICE_EXPORT NullRenderer : public DirectRenderer {
 public:
  NullRenderer(const RendererSettings* settings,
               const DebugRendererSettings* debug_settings,
               OutputSurface* output_surface,
               DisplayResourceProvider* resource_provider,
               OverlayProcessorInterface* overlay_processor);
  ~NullRenderer() override;

 protected:
  void SetRenderPassBackingDrawnRect(
      const AggregatedRenderPassId& render_pass_id,
      const gfx::Rect& drawn_rect) override {}

  gfx::Rect GetRenderPassBackingDrawnRect(
      const AggregatedRenderPassId& render_pass_id) const override;

 private:
  void SwapBuffers(SwapFrameData swap_frame_data) override;
  bool CanPartialSwap() override;
  void UpdateRenderPassTextures(
      const AggregatedRenderPassList& render_passes_in_draw_order,
      const base::flat_map<AggregatedRenderPassId, RenderPassRequirements>&
          render_passes_in_frame) override {}
  void AllocateRenderPassResourceIfNeeded(
      const AggregatedRenderPassId& render_pass_id,
      const RenderPassRequirements& requirements) override {}
  bool IsRenderPassResourceAllocated(
      const AggregatedRenderPassId& render_pass_id) const override;
  gfx::Size GetRenderPassBackingPixelSize(
      const AggregatedRenderPassId& render_pass_id) override;
  void SetScissorTestRect(const gfx::Rect& scissor_rect) override {}
  void BeginDrawingRenderPass(const AggregatedRenderPass* render_pass,
                              bool needs_clear,
                              const gfx::Rect& render_pass_update_rect,
                              const gfx::Size& viewport_size) override {}
  void DoDrawQuad(const DrawQuad* quad,
                  const gfx::QuadF* clip_region) override {}
  void BeginDrawingFrame() override;
  void FinishDrawingFrame() override {}
  void EnsureScissorTestDisabled() override {}
  void DidChangeVisibility() override {}
  void CopyDrawnRenderPass(const copy_output::RenderPassGeometry& geometry,
                           std::unique_ptr<CopyOutputRequest> request) override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_NULL_RENDERER_H_
