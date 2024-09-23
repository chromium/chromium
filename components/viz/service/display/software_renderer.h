// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SOFTWARE_RENDERER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SOFTWARE_RENDERER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/display_resource_provider_software.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/latency/latency_info.h"

namespace viz {
class DebugBorderDrawQuad;
class OutputSurface;
class PictureDrawQuad;
class AggregatedRenderPassDrawQuad;
class SoftwareOutputDevice;
class SolidColorDrawQuad;
class TextureDrawQuad;
class TileDrawQuad;

class VIZ_SERVICE_EXPORT SoftwareRenderer : public DirectRenderer {
 public:
  SoftwareRenderer(const RendererSettings* settings,
                   const DebugRendererSettings* debug_settings,
                   OutputSurface* output_surface,
                   DisplayResourceProviderSoftware* resource_provider,
                   OverlayProcessorInterface* overlay_processor);

  SoftwareRenderer(const SoftwareRenderer&) = delete;
  SoftwareRenderer& operator=(const SoftwareRenderer&) = delete;

  ~SoftwareRenderer() override;

  void SwapBuffers(SwapFrameData swap_frame_data) override;

 protected:
  bool CanPartialSwap() override;
  void UpdateRenderPassTextures(
      const AggregatedRenderPassList& render_passes_in_draw_order,
      const base::flat_map<AggregatedRenderPassId, RenderPassRequirements>&
          render_passes_in_frame) override;
  void AllocateRenderPassResourceIfNeeded(
      const AggregatedRenderPassId& render_pass_id,
      const RenderPassRequirements& requirements) override;
  bool IsRenderPassResourceAllocated(
      const AggregatedRenderPassId& render_pass_id) const override;
  gfx::Size GetRenderPassBackingPixelSize(
      const AggregatedRenderPassId& render_pass_id) override;
  void SetScissorTestRect(const gfx::Rect& scissor_rect) override;
  void BeginDrawingRenderPass(const AggregatedRenderPass* render_pass,
                              bool needs_clear,
                              const gfx::Rect& render_pass_update_rect,
                              const gfx::Size& viewport_size) override;
  void DoDrawQuad(const DrawQuad* quad, const gfx::QuadF* draw_region) override;
  void BeginDrawingFrame() override;
  void FinishDrawingFrame() override;
  void EnsureScissorTestDisabled() override;
  void CopyDrawnRenderPass(const copy_output::RenderPassGeometry& geometry,
                           std::unique_ptr<CopyOutputRequest> request) override;
  void DidChangeVisibility() override;

 protected:
  void SetRenderPassBackingDrawnRect(
      const AggregatedRenderPassId& render_pass_id,
      const gfx::Rect& drawn_rect) override;

  gfx::Rect GetRenderPassBackingDrawnRect(
      const AggregatedRenderPassId& render_pass_id) const override;

 private:
  struct RenderPassBitmapBacking {
    SkBitmap bitmap;
    gfx::Rect drawn_rect;
  };
  void ClearCanvas(SkColor color);
  void ClearFramebuffer();
  void SetClipRect(const gfx::Rect& rect);
  void SetClipRRect(const gfx::RRectF& rrect);
  bool IsSoftwareResource(ResourceId resource_id);

  void DrawDebugBorderQuad(const DebugBorderDrawQuad* quad);
  void DrawPictureQuad(const PictureDrawQuad* quad);
  void DrawRenderPassQuad(const AggregatedRenderPassDrawQuad* quad);
  void DrawSolidColorQuad(const SolidColorDrawQuad* quad);
  void DrawTextureQuad(const TextureDrawQuad* quad);
  void DrawTileQuad(const TileDrawQuad* quad);
  void DrawUnsupportedQuad(const DrawQuad* quad);
  bool ShouldApplyBackdropFilters(
      const cc::FilterOperations* backdrop_filters,
      const AggregatedRenderPassDrawQuad* quad) const;
  sk_sp<SkImage> ApplyImageFilter(SkImageFilter* filter,
                                  const AggregatedRenderPassDrawQuad* quad,
                                  const SkBitmap& to_filter,
                                  bool offset_expanded_bounds,
                                  SkIRect* auto_bounds) const;
  gfx::Rect GetBackdropBoundingBoxForRenderPassQuad(
      const AggregatedRenderPassDrawQuad* quad,
      const cc::FilterOperations* backdrop_filters,
      std::optional<gfx::RRectF> backdrop_filter_bounds_input,
      gfx::Transform contents_device_transform,
      gfx::Transform* backdrop_filter_bounds_transform,
      std::optional<gfx::RRectF>* backdrop_filter_bounds,
      gfx::Rect* unclipped_rect) const;

  SkBitmap GetBackdropBitmap(const gfx::Rect& bounding_rect) const;
  sk_sp<SkShader> GetBackdropFilterShader(
      const AggregatedRenderPassDrawQuad* quad,
      SkTileMode content_tile_mode) const;

  DisplayResourceProviderSoftware* resource_provider() {
    return static_cast<DisplayResourceProviderSoftware*>(resource_provider_);
  }

  // A map from RenderPass id to the bitmap used to draw the RenderPass from.
  base::flat_map<AggregatedRenderPassId, RenderPassBitmapBacking>
      render_pass_bitmaps_;

  bool is_scissor_enabled_ = false;
  gfx::Rect scissor_rect_;

  raw_ptr<SoftwareOutputDevice> output_device_;
  raw_ptr<SkCanvas, DanglingUntriaged> root_canvas_ = nullptr;
  raw_ptr<SkCanvas, DanglingUntriaged> current_canvas_ = nullptr;
  SkPaint current_paint_;
  SkSamplingOptions current_sampling_;
  std::unique_ptr<SkCanvas> current_framebuffer_canvas_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SOFTWARE_RENDERER_H_
