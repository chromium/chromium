// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/software_renderer.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/process/memory.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/render_surface_filters.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/skia_helper.h"
#include "components/viz/common/viz_utils.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/renderer_utils.h"
#include "components/viz/service/display/software_output_device.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/legacy_display_globals.h"
#include "skia/ext/opacity_filter_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkMaskFilter.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/core/SkSwizzle.h"
#include "third_party/skia/include/effects/SkShaderMaskFilter.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace viz {
namespace {
class AnimatedImagesProvider : public cc::ImageProvider {
 public:
  AnimatedImagesProvider(
      const PictureDrawQuad::ImageAnimationMap* image_animation_map)
      : image_animation_map_(image_animation_map) {}
  ~AnimatedImagesProvider() override = default;

  ImageProvider::ScopedResult GetRasterContent(
      const cc::DrawImage& draw_image) override {
    // TODO(xidachen): Ensure this function works for paint worklet generated
    // images.
    const auto& paint_image = draw_image.paint_image();
    auto it = image_animation_map_->find(paint_image.stable_id());
    size_t frame_index = it == image_animation_map_->end()
                             ? cc::PaintImage::kDefaultFrameIndex
                             : it->second;
    return ScopedResult(cc::DecodedDrawImage(
        paint_image.GetSkImageForFrame(
            frame_index, cc::PaintImage::kDefaultGeneratorClientId),
        nullptr, SkSize::Make(0, 0), SkSize::Make(1.f, 1.f),
        draw_image.filter_quality(), true /* is_budgeted */));
  }

 private:
  raw_ptr<const PictureDrawQuad::ImageAnimationMap> image_animation_map_;
};

}  // namespace

SoftwareRenderer::SoftwareRenderer(
    const RendererSettings* settings,
    const DebugRendererSettings* debug_settings,
    OutputSurface* output_surface,
    DisplayResourceProviderSoftware* resource_provider,
    OverlayProcessorInterface* overlay_processor)
    : DirectRenderer(settings,
                     debug_settings,
                     output_surface,
                     resource_provider,
                     overlay_processor),
      output_device_(output_surface->software_device()) {}

SoftwareRenderer::~SoftwareRenderer() {}

bool SoftwareRenderer::CanPartialSwap() {
  return true;
}

void SoftwareRenderer::BeginDrawingFrame() {
  TRACE_EVENT0("viz", "SoftwareRenderer::BeginDrawingFrame");
}

void SoftwareRenderer::FinishDrawingFrame() {
  TRACE_EVENT0("viz", "SoftwareRenderer::FinishDrawingFrame");
  // `current_canvas_` may be pointing to `current_framebuffer_canvas_`. Make
  // sure to reset it before destroying `current_framebuffer_canvas_`.
  current_canvas_ = nullptr;
  current_framebuffer_canvas_.reset();

  if (root_canvas_)
    output_device_->EndPaint();
  root_canvas_ = nullptr;
}

void SoftwareRenderer::SwapBuffers(SwapFrameData swap_frame_data) {
  DCHECK(visible_);
  TRACE_EVENT0("viz", "SoftwareRenderer::SwapBuffers");
  OutputSurfaceFrame output_frame;
  output_frame.latency_info = std::move(swap_frame_data.latency_info);
  output_frame.data.swap_trace_id = swap_frame_data.swap_trace_id;
  output_surface_->SwapBuffers(std::move(output_frame));
}

void SoftwareRenderer::EnsureScissorTestDisabled() {
  is_scissor_enabled_ = false;
}

void SoftwareRenderer::SetScissorTestRect(const gfx::Rect& scissor_rect) {
  is_scissor_enabled_ = true;
  scissor_rect_ = scissor_rect;
}

void SoftwareRenderer::SetClipRect(const gfx::Rect& rect) {
  if (!current_canvas_)
    return;
  // Skia applies the current matrix to clip rects so we reset it temporarily.
  SkMatrix current_matrix = current_canvas_->getTotalMatrix();
  current_canvas_->resetMatrix();

  // Checks below are incompatible with WebView as the canvas size and clip
  // provided by Android or embedder app. And Chrome doesn't use
  // SoftwareRenderer on Android.
#if !BUILDFLAG(IS_ANDROID)
  // SetClipRect is assumed to be applied temporarily, on an
  // otherwise-unclipped canvas.
  DCHECK_EQ(current_canvas_->getDeviceClipBounds().width(),
            current_canvas_->imageInfo().width());
  DCHECK_EQ(current_canvas_->getDeviceClipBounds().height(),
            current_canvas_->imageInfo().height());
#endif
  current_canvas_->clipRect(gfx::RectToSkRect(rect));
  current_canvas_->setMatrix(current_matrix);
}

void SoftwareRenderer::SetClipRRect(const gfx::RRectF& rrect) {
  if (!current_canvas_)
    return;

  SkRRect result;
  if (SkRRect(rrect).transform(gfx::AxisTransform2dToSkMatrix(
                                   current_frame()->target_to_device_transform),
                               &result)) {
    // Skia applies the current matrix to clip rects so we reset it temporarily.
    SkMatrix current_matrix = current_canvas_->getTotalMatrix();
    current_canvas_->resetMatrix();
    current_canvas_->clipRRect(result, true);
    current_canvas_->setMatrix(current_matrix);
  }
}

void SoftwareRenderer::ClearCanvas(SkColor color) {
  if (!current_canvas_)
    return;

  if (is_scissor_enabled_) {
    // The same paint used by SkCanvas::clear, but applied to the scissor rect.
    SkPaint clear_paint;
    clear_paint.setColor(color);
    clear_paint.setBlendMode(SkBlendMode::kSrc);
    current_canvas_->drawRect(gfx::RectToSkRect(scissor_rect_), clear_paint);
  } else {
    current_canvas_->clear(color);
  }
}

void SoftwareRenderer::ClearFramebuffer() {
  if (current_frame()->current_render_pass->has_transparent_background) {
    ClearCanvas(SkColorSetARGB(0, 0, 0, 0));
  } else {
#ifndef NDEBUG
    // On DEBUG builds, opaque render passes are cleared to blue
    // to easily see regions that were not drawn on the screen.
    ClearCanvas(SkColorSetARGB(255, 0, 0, 255));
#endif
  }
}

void SoftwareRenderer::BeginDrawingRenderPass(
    const AggregatedRenderPass* render_pass,
    bool needs_clear,
    const gfx::Rect& render_pass_update_rect,
    const gfx::Size& viewport_size) {
  if (render_pass == current_frame()->root_render_pass) {
    DCHECK(!root_canvas_);

    root_canvas_ =
        output_device_->BeginPaint(current_frame()->root_damage_rect);
    if (!root_canvas_) {
      output_device_->EndPaint();
    }
    // `current_canvas_` may be pointing to `current_framebuffer_canvas_`. Make
    // sure to re-assign it before destroying `current_framebuffer_canvas_`
    current_canvas_ = root_canvas_;
    current_framebuffer_canvas_.reset();
  } else {
    auto it = render_pass_bitmaps_.find(render_pass->id);
    CHECK(it != render_pass_bitmaps_.end(), base::NotFatalUntil::M130);
    SkBitmap& bitmap = it->second.bitmap;

    current_framebuffer_canvas_ = std::make_unique<SkCanvas>(
        bitmap, skia::LegacyDisplayGlobals::GetSkSurfaceProps());
    current_canvas_ = current_framebuffer_canvas_.get();
  }

  if (render_pass_update_rect == gfx::Rect(viewport_size)) {
    EnsureScissorTestDisabled();
  } else {
    SetScissorTestRect(render_pass_update_rect);
  }

  if (needs_clear) {
    ClearFramebuffer();
  }
}

bool SoftwareRenderer::IsSoftwareResource(ResourceId resource_id) {
  return resource_provider()->IsResourceSoftwareBacked(resource_id);
}

void SoftwareRenderer::DoDrawQuad(const DrawQuad* quad,
                                  const gfx::QuadF* draw_region) {
  if (!current_canvas_)
    return;

  TRACE_EVENT0("viz", "SoftwareRenderer::DoDrawQuad");
  const bool should_apply_rounded_corner = ShouldApplyRoundedCorner(quad);
  bool do_save =
      draw_region || is_scissor_enabled_ || should_apply_rounded_corner;
  SkAutoCanvasRestore canvas_restore(current_canvas_, do_save);
  if (is_scissor_enabled_) {
    SetClipRect(scissor_rect_);
  }

  if (should_apply_rounded_corner)
    SetClipRRect(
        quad->shared_quad_state->mask_filter_info.rounded_corner_bounds());

  gfx::Transform quad_rect_matrix;
  QuadRectTransform(&quad_rect_matrix,
                    quad->shared_quad_state->quad_to_target_transform,
                    gfx::RectF(quad->rect));
  gfx::Transform contents_device_transform = quad_rect_matrix;
  contents_device_transform.PostConcat(
      current_frame()->target_to_device_transform);
  contents_device_transform.Flatten();
  SkMatrix sk_device_matrix =
      gfx::TransformToFlattenedSkMatrix(contents_device_transform);
  current_canvas_->setMatrix(sk_device_matrix);

  current_paint_.reset();
  if (settings_->force_antialiasing ||
      !IsScaleAndIntegerTranslate(sk_device_matrix)) {
    // TODO(danakj): Until we can enable AA only on exterior edges of the
    // layer, disable AA if any interior edges are present. crbug.com/248175
    bool all_four_edges_are_exterior =
        quad->IsTopEdge() && quad->IsLeftEdge() && quad->IsBottomEdge() &&
        quad->IsRightEdge();
    if (settings_->allow_antialiasing &&
        (settings_->force_antialiasing || all_four_edges_are_exterior))
      current_paint_.setAntiAlias(true);
    current_sampling_ = SkSamplingOptions(SkFilterMode::kLinear);
  }

  if (quad->ShouldDrawWithBlending() ||
      quad->shared_quad_state->blend_mode != SkBlendMode::kSrcOver) {
    current_paint_.setAlpha(quad->shared_quad_state->opacity * 255);
    current_paint_.setBlendMode(quad->shared_quad_state->blend_mode);
  } else {
    current_paint_.setBlendMode(SkBlendMode::kSrc);
  }

  if (draw_region) {
    gfx::QuadF local_draw_region(*draw_region);
    SkPath draw_region_clip_path;
    local_draw_region -=
        gfx::Vector2dF(quad->visible_rect.x(), quad->visible_rect.y());
    local_draw_region.Scale(1.0f / quad->visible_rect.width(),
                            1.0f / quad->visible_rect.height());
    local_draw_region -= gfx::Vector2dF(0.5f, 0.5f);

    SkPoint clip_points[4];
    QuadFToSkPoints(local_draw_region, clip_points);
    draw_region_clip_path.addPoly(clip_points, 4, true);

    current_canvas_->clipPath(draw_region_clip_path);
  }

  switch (quad->material) {
    case DrawQuad::Material::kAggregatedRenderPass:
      DrawRenderPassQuad(AggregatedRenderPassDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::Material::kDebugBorder:
      DrawDebugBorderQuad(DebugBorderDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::Material::kPictureContent:
      DrawPictureQuad(PictureDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::Material::kCompositorRenderPass:
      // At this point, all RenderPassDrawQuads should be converted to
      // AggregatedRenderPassDrawQuads.
      NOTREACHED_IN_MIGRATION();
      break;
    case DrawQuad::Material::kSolidColor:
      DrawSolidColorQuad(SolidColorDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::Material::kTextureContent:
      DrawTextureQuad(TextureDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::Material::kTiledContent:
      DrawTileQuad(TileDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::Material::kSurfaceContent:
      // Surface content should be fully resolved to other quad types before
      // reaching a direct renderer.
      NOTREACHED_IN_MIGRATION();
      break;
    case DrawQuad::Material::kInvalid:
    case DrawQuad::Material::kSharedElement:
      DrawUnsupportedQuad(quad);
      NOTREACHED_IN_MIGRATION();
      break;
    case DrawQuad::Material::kVideoHole:
      // VideoHoleDrawQuad should only be used by Cast, and should
      // have been replaced by cast-specific OverlayProcessor before
      // reach here. In non-cast build, an untrusted render could send such
      // Quad and the quad would then reach here unexpectedly. Therefore
      // we should skip NOTREACHED() so an untrusted render is not capable
      // of causing a crash.
      DrawUnsupportedQuad(quad);
      break;
  }

  current_canvas_->resetMatrix();
}

void SoftwareRenderer::DrawDebugBorderQuad(const DebugBorderDrawQuad* quad) {
  SkMatrix m = current_canvas_->getTotalMatrix();
  current_canvas_->resetMatrix();

  SkPath path;
  path.addRect(gfx::RectFToSkRect(QuadVertexRect()));
  path.transform(m);

  current_paint_.setColor(quad->color);
  current_paint_.setAlphaf(quad->shared_quad_state->opacity * quad->color.fA);
  current_paint_.setStyle(SkPaint::kStroke_Style);
  current_paint_.setStrokeJoin(SkPaint::kMiter_Join);
  current_paint_.setStrokeWidth(quad->width);

  current_canvas_->drawPath(path, current_paint_);
}

void SoftwareRenderer::DrawPictureQuad(const PictureDrawQuad* quad) {
  current_canvas_->concat(
      SkMatrix::RectToRect(gfx::RectFToSkRect(quad->tex_coord_rect),
                           gfx::RectFToSkRect(QuadVertexRect())));

  const bool needs_transparency =
      SkScalarRoundToInt(quad->shared_quad_state->opacity * 255) < 255;
  const bool disable_image_filtering = quad->nearest_neighbor;

  TRACE_EVENT0("viz", "SoftwareRenderer::DrawPictureQuad");

  SkCanvas* raster_canvas = current_canvas_;

  std::optional<skia::OpacityFilterCanvas> opacity_canvas;
  if (needs_transparency || disable_image_filtering) {
    // TODO(aelias): This isn't correct in all cases. We should detect these
    // cases and fall back to a persistent bitmap backing
    // (http://crbug.com/280374).
    // TODO(vmpstr): Fold this canvas into playback and have raster source
    // accept a set of settings on playback that will determine which canvas to
    // apply. (http://crbug.com/594679)
    opacity_canvas.emplace(raster_canvas, quad->shared_quad_state->opacity,
                           disable_image_filtering);
    raster_canvas = &*opacity_canvas;
  }

  // Treat all subnormal values as zero for performance.
  cc::ScopedSubnormalFloatDisabler disabler;

  // Use an image provider to select the correct frame for animated images.
  AnimatedImagesProvider image_provider(&quad->image_animation_map);

  raster_canvas->save();
  raster_canvas->translate(-quad->content_rect.x(), -quad->content_rect.y());
  raster_canvas->clipRect(gfx::RectToSkRect(quad->content_rect));
  raster_canvas->scale(quad->contents_scale, quad->contents_scale);
  quad->display_item_list->Raster(raster_canvas, &image_provider,
                                  &quad->raster_inducing_scroll_offsets);
  raster_canvas->restore();
}

void SoftwareRenderer::DrawSolidColorQuad(const SolidColorDrawQuad* quad) {
  gfx::RectF visible_quad_vertex_rect = cc::MathUtil::ScaleRectProportional(
      QuadVertexRect(), gfx::RectF(quad->rect), gfx::RectF(quad->visible_rect));
  current_paint_.setColor(quad->color);
  current_paint_.setAlphaf(quad->shared_quad_state->opacity * quad->color.fA);
  current_canvas_->drawRect(gfx::RectFToSkRect(visible_quad_vertex_rect),
                            current_paint_);
}

void SoftwareRenderer::DrawTextureQuad(const TextureDrawQuad* quad) {
  if (!IsSoftwareResource(quad->resource_id()) || quad->is_stream_video) {
    DrawUnsupportedQuad(quad);
    return;
  }

  DisplayResourceProviderSoftware::ScopedReadLockSkImage lock(
      resource_provider(), quad->resource_id(),
      quad->premultiplied_alpha ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);

  if (!lock.valid())
    return;
  const SkImage* image = lock.sk_image();
  gfx::RectF uv_rect = gfx::ScaleRect(
      gfx::BoundingRect(quad->uv_top_left, quad->uv_bottom_right),
      image->width(), image->height());
  gfx::RectF visible_uv_rect = cc::MathUtil::ScaleRectProportional(
      uv_rect, gfx::RectF(quad->rect), gfx::RectF(quad->visible_rect));
  SkRect sk_uv_rect = gfx::RectFToSkRect(visible_uv_rect);
  gfx::RectF visible_quad_vertex_rect = cc::MathUtil::ScaleRectProportional(
      QuadVertexRect(), gfx::RectF(quad->rect), gfx::RectF(quad->visible_rect));
  SkRect quad_rect = gfx::RectFToSkRect(visible_quad_vertex_rect);

  if (quad->y_flipped)
    current_canvas_->scale(1, -1);

  bool blend_background =
      quad->background_color != SkColors::kTransparent && !image->isOpaque();
  bool needs_layer = blend_background && (current_paint_.getAlphaf() != 1.f);
  if (needs_layer) {
    current_canvas_->saveLayerAlphaf(&quad_rect, current_paint_.getAlphaf());
    current_paint_.setAlphaf(1.f);
  }
  if (blend_background) {
    SkPaint background_paint;
    background_paint.setColor(quad->background_color);
    current_canvas_->drawRect(quad_rect, background_paint);
  }
  SkSamplingOptions sampling(quad->nearest_neighbor ? SkFilterMode::kNearest
                                                    : SkFilterMode::kLinear);
  current_canvas_->drawImageRect(image, sk_uv_rect, quad_rect, sampling,
                                 &current_paint_,
                                 SkCanvas::kStrict_SrcRectConstraint);
  if (needs_layer)
    current_canvas_->restore();
}

DBG_FLAG_FBOOL("software.toggle.capture", software_toggle_capture)

void SoftwareRenderer::DrawTileQuad(const TileDrawQuad* quad) {
  // |resource_provider_| can be NULL in resourceless software draws, which
  // should never produce tile quads in the first place.
  DCHECK(resource_provider_);
  DCHECK(IsSoftwareResource(quad->resource_id()));

  DisplayResourceProviderSoftware::ScopedReadLockSkImage lock(
      resource_provider(), quad->resource_id(),
      quad->is_premultiplied ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
  if (!lock.valid())
    return;

  gfx::RectF visible_tex_coord_rect = cc::MathUtil::ScaleRectProportional(
      quad->tex_coord_rect, gfx::RectF(quad->rect),
      gfx::RectF(quad->visible_rect));
  gfx::RectF visible_quad_vertex_rect = cc::MathUtil::ScaleRectProportional(
      QuadVertexRect(), gfx::RectF(quad->rect), gfx::RectF(quad->visible_rect));

  SkRect uv_rect = gfx::RectFToSkRect(visible_tex_coord_rect);
  SkSamplingOptions sampling(quad->nearest_neighbor ? SkFilterMode::kNearest
                                                    : SkFilterMode::kLinear);
  current_canvas_->drawImageRect(
      lock.sk_image(), uv_rect, gfx::RectFToSkRect(visible_quad_vertex_rect),
      sampling, &current_paint_, SkCanvas::kStrict_SrcRectConstraint);

  if (software_toggle_capture()) {
    SkPixmap pixmap;
    if (lock.sk_image()->peekPixels(&pixmap)) {
      int buff_id = 0;
      auto& transform = quad->shared_quad_state->quad_to_target_transform;
      auto display_rect = gfx::RectF(quad->visible_rect);
      display_rect = transform.MapRect(display_rect);
      visible_tex_coord_rect.Scale(1.0f / pixmap.width(),
                                   1.0f / pixmap.height());
      DBG_DRAW_RECT_BUFF_UV("software.tile.buffer", display_rect, &buff_id,
                            visible_tex_coord_rect);
      VizDebugger::BufferInfo buffer_info;
      buffer_info.bitmap.setInfo(
          SkImageInfo::Make(pixmap.width(), pixmap.height(),
                            kBGRA_8888_SkColorType, kPremul_SkAlphaType));
      buffer_info.bitmap.allocPixels();
      buffer_info.bitmap.writePixels(pixmap);
      DBG_COMPLETE_BUFFERS(buff_id, buffer_info);
    }
  }
}

void SoftwareRenderer::DrawRenderPassQuad(
    const AggregatedRenderPassDrawQuad* quad) {
  auto it = render_pass_bitmaps_.find(quad->render_pass_id);
  if (it == render_pass_bitmaps_.end())
    return;
  SkBitmap& source_bitmap = it->second.bitmap;

  SkRect dest_rect = gfx::RectFToSkRect(QuadVertexRect());
  SkRect dest_visible_rect =
      gfx::RectFToSkRect(cc::MathUtil::ScaleRectProportional(
          QuadVertexRect(), gfx::RectF(quad->rect),
          gfx::RectF(quad->visible_rect)));
  SkRect content_rect = RectFToSkRect(quad->tex_coord_rect);

  sk_sp<SkImage> filter_image;
  const cc::FilterOperations* filters = FiltersForPass(quad->render_pass_id);
  if (filters) {
    DCHECK(!filters->IsEmpty());
    auto paint_filter = cc::RenderSurfaceFilters::BuildImageFilter(*filters);
    auto image_filter =
        paint_filter ? paint_filter->cached_sk_filter_ : nullptr;
    if (image_filter) {
      SkIRect result_rect;
      // TODO(ajuma): Apply the filter in the same pass as the content where
      // possible (e.g. when there's no origin offset). See crbug.com/308201.
      filter_image =
          ApplyImageFilter(image_filter.get(), quad, source_bitmap,
                           /* offset_expanded_bounds = */ true, &result_rect);
      if (result_rect.isEmpty()) {
        return;
      }
      if (filter_image) {
        gfx::RectF rect = gfx::SkRectToRectF(SkRect::Make(result_rect));
        dest_rect = dest_visible_rect =
            gfx::RectFToSkRect(cc::MathUtil::ScaleRectProportional(
                QuadVertexRect(), gfx::RectF(quad->rect), rect));
        content_rect =
            SkRect::MakeWH(result_rect.width(), result_rect.height());
      }
    }
  }

  SkMatrix content_mat = SkMatrix::RectToRect(content_rect, dest_rect);

  sk_sp<SkShader> shader;
  if (!filter_image) {
    shader = source_bitmap.makeShader(current_sampling_, content_mat);
  } else {
    shader = filter_image->makeShader(current_sampling_, content_mat);
  }

  if (quad->mask_resource_id()) {
    DisplayResourceProviderSoftware::ScopedReadLockSkImage mask_lock(
        resource_provider(), quad->mask_resource_id(), kPremul_SkAlphaType);
    if (!mask_lock.valid())
      return;

    // Scale normalized uv rect into absolute texel coordinates.
    SkRect mask_rect = gfx::RectFToSkRect(
        gfx::ScaleRect(quad->mask_uv_rect, quad->mask_texture_size.width(),
                       quad->mask_texture_size.height()));

    SkMatrix mask_mat = SkMatrix::RectToRect(mask_rect, dest_rect);

    current_paint_.setMaskFilter(SkShaderMaskFilter::Make(
        mask_lock.sk_image()->makeShader(current_sampling_, mask_mat)));
  }

  // If we have a backdrop filter shader, render its results first.
  sk_sp<SkShader> backdrop_filter_shader =
      GetBackdropFilterShader(quad, SkTileMode::kClamp);
  if (backdrop_filter_shader) {
    SkPaint paint;
    paint.setShader(std::move(backdrop_filter_shader));
    paint.setMaskFilter(current_paint_.refMaskFilter());
    current_canvas_->drawRect(dest_visible_rect, paint);
  }
  current_paint_.setShader(std::move(shader));
  current_canvas_->drawRect(dest_visible_rect, current_paint_);
}

void SoftwareRenderer::DrawUnsupportedQuad(const DrawQuad* quad) {
#ifdef NDEBUG
  current_paint_.setColor(SkColors::kWhite);
#else
  current_paint_.setColor(SkColors::kMagenta);
#endif
  current_paint_.setAlpha(quad->shared_quad_state->opacity * 255);
  current_canvas_->drawRect(gfx::RectFToSkRect(QuadVertexRect()),
                            current_paint_);
}

void SoftwareRenderer::CopyDrawnRenderPass(
    const copy_output::RenderPassGeometry& geometry,
    std::unique_ptr<CopyOutputRequest> request) {
  sk_sp<SkColorSpace> color_space = CurrentRenderPassSkColorSpace();
  DCHECK(color_space);

  SkBitmap bitmap;
  if (request->is_scaled()) {
    // Resolve the source for the scaling input: Initialize a SkPixmap that
    // selects the current RenderPass's output rect within the current canvas
    // and provides access to its pixels.
    SkPixmap render_pass_output;
    if (!current_canvas_->peekPixels(&render_pass_output))
      return;
    {
      render_pass_output =
          SkPixmap(render_pass_output.info()
                       .makeWH(geometry.sampling_bounds.width(),
                               geometry.sampling_bounds.height())
                       .makeColorSpace(std::move(color_space)),
                   render_pass_output.addr(geometry.sampling_bounds.x(),
                                           geometry.sampling_bounds.y()),
                   render_pass_output.rowBytes());
    }

    // Execute the scaling: For downscaling, use the RESIZE_BETTER strategy
    // (appropriate for thumbnailing); and, for upscaling, use the RESIZE_BEST
    // strategy. Note that processing is only done on the subset of the
    // RenderPass output that contributes to the result.
    using skia::ImageOperations;
    const bool is_downscale_in_both_dimensions =
        request->scale_to().x() < request->scale_from().x() &&
        request->scale_to().y() < request->scale_from().y();
    const ImageOperations::ResizeMethod method =
        is_downscale_in_both_dimensions ? ImageOperations::RESIZE_BETTER
                                        : ImageOperations::RESIZE_BEST;
    bitmap = ImageOperations::Resize(
        render_pass_output, method, geometry.result_bounds.width(),
        geometry.result_bounds.height(),
        SkIRect{geometry.result_selection.x(), geometry.result_selection.y(),
                geometry.result_selection.right(),
                geometry.result_selection.bottom()});
  } else /* if (!request->is_scaled()) */ {
    SkImageInfo info = SkImageInfo::MakeN32Premul(
        geometry.result_selection.width(), geometry.result_selection.height(),
        std::move(color_space));
    if (!bitmap.tryAllocPixels(info))
      return;

    if (!current_canvas_->readPixels(bitmap, geometry.readback_offset.x(),
                                     geometry.readback_offset.y()))
      return;
  }

  bitmap.setImmutable();

  // Returning kNativeTextures results is only supported with blit requests, so
  // we copy to client provided image.
  if (request->result_destination() ==
          CopyOutputResult::Destination::kNativeTextures &&
      request->has_blit_request()) {
    const auto& blit_request = request->blit_request();

    auto representation = resource_provider()->GetSharedImageRepresentation(
        blit_request.mailbox(), blit_request.sync_token());

    if (!representation) {
      DLOG(ERROR) << "BlitRequest: Couldn't create shared image representation";
      return;
    }

    const auto dest_rect =
        gfx::Rect(blit_request.destination_region_offset(),
                  gfx::Size(bitmap.width(), bitmap.height()));

    if (!gfx::Rect(representation->size()).Contains(dest_rect)) {
      DLOG(ERROR) << "BlitRequest: Destination is outside of shared image";
      return;
    }

    // TODO(crbug.com/330920521): This should be write access, but
    // MemoryImageRepresentation doesn't have one and there are no
    // synchronization requirements.
    auto read_access = representation->BeginScopedReadAccess();
    if (!read_access) {
      DLOG(ERROR) << "BlitRequest: Couldn't access the shared image";
      return;
    }

    auto surface = SkSurfaces::WrapPixels(
        read_access->pixmap().info(), read_access->pixmap().writable_addr(),
        read_access->pixmap().rowBytes(), nullptr);

    CHECK(surface);

    if (blit_request.letterboxing_behavior() ==
        LetterboxingBehavior::kLetterbox) {
      surface->getCanvas()->clear(SK_ColorBLACK);
    }

    SkPaint blit_request_paint;
    blit_request_paint.setBlendMode(SkBlendMode::kSrc);
    surface->getCanvas()->drawImage(SkImages::RasterFromBitmap(bitmap),
                                    dest_rect.x(), dest_rect.y(),
                                    SkSamplingOptions(), &blit_request_paint);

    for (const BlendBitmap& blend_bitmap : blit_request.blend_bitmaps()) {
      SkPaint blend_bitmap_paint;
      blend_bitmap_paint.setBlendMode(SkBlendMode::kSrcOver);

      surface->getCanvas()->drawImageRect(
          blend_bitmap.image(), gfx::RectToSkRect(blend_bitmap.source_region()),
          gfx::RectToSkRect(blend_bitmap.destination_region()),
          SkSamplingOptions(SkFilterMode::kLinear), &blend_bitmap_paint,
          SkCanvas::kFast_SrcRectConstraint);
    }

    request->SendResult(std::make_unique<CopyOutputTextureResult>(
        CopyOutputResult::Format::RGBA, geometry.result_selection,
        CopyOutputResult::TextureResult(request->blit_request().mailbox(),
                                        representation->color_space()),
        CopyOutputResult::ReleaseCallbacks()));

    return;
  }

  // Deliver the result. SoftwareRenderer supports system memory destinations
  // only. For legacy reasons, if a RGBA texture request is being made, clients
  // are prepared to accept system memory results.

  // Note: The CopyOutputSkBitmapResult already implies that results are
  // returned in system memory and automatically provides I420 format
  // conversion, if needed.
  request->SendResult(std::make_unique<CopyOutputSkBitmapResult>(
      request->result_format(), geometry.result_selection, std::move(bitmap)));
}

void SoftwareRenderer::DidChangeVisibility() {
  if (visible_)
    output_surface_->EnsureBackbuffer();
  else
    output_surface_->DiscardBackbuffer();
}

bool SoftwareRenderer::ShouldApplyBackdropFilters(
    const cc::FilterOperations* backdrop_filters,
    const AggregatedRenderPassDrawQuad* quad) const {
  if (!backdrop_filters)
    return false;
  if (quad->shared_quad_state->opacity == 0.f)
    return false;
  DCHECK(!backdrop_filters->IsEmpty());
  return true;
}

// Applies |filter| to |to_filter| bitmap. |result_rect| will be filled with the
// automatically-computed destination bounds. If |offset_expanded_bounds| is
// true, the bitmap will be offset for any pixel-moving filters. This function
// is called for both filters and backdrop_filters. The difference between those
// two paths is that the filter path wants to offset to the expanded bounds
// (including border for pixel moving filters) when drawing the bitmap into the
// canvas, while the backdrop filter path needs to keep the origin unmoved (at
// quad->rect origin) so that it gets put in the right spot relative to the
// underlying backdrop.
sk_sp<SkImage> SoftwareRenderer::ApplyImageFilter(
    SkImageFilter* filter,
    const AggregatedRenderPassDrawQuad* quad,
    const SkBitmap& to_filter,
    bool offset_expanded_bounds,
    SkIRect* result_rect) const {
  DCHECK(result_rect);
  if (!filter)
    return nullptr;

  SkMatrix local_matrix;
  local_matrix.setTranslate(quad->filters_origin.x(), quad->filters_origin.y());
  local_matrix.postScale(quad->filters_scale.x(), quad->filters_scale.y());
  *result_rect =
      filter->filterBounds(gfx::RectToSkIRect(quad->rect), local_matrix,
                           SkImageFilter::kForward_MapDirection);
  gfx::Point canvas_offset =
      offset_expanded_bounds ? gfx::Point(result_rect->x(), result_rect->y())
                             : quad->rect.origin();
  SkImageInfo dst_info =
      SkImageInfo::MakeN32Premul(result_rect->width(), result_rect->height());
  sk_sp<SkSurface> surface = SkSurfaces::Raster(dst_info);
  if (!surface)
    return nullptr;

  SkPaint paint;
  // Treat subnormal float values as zero for performance.
  cc::ScopedSubnormalFloatDisabler disabler;
  paint.setImageFilter(filter->makeWithLocalMatrix(local_matrix));
  surface->getCanvas()->translate(-canvas_offset.x(), -canvas_offset.y());
  surface->getCanvas()->drawImage(to_filter.asImage(), quad->rect.x(),
                                  quad->rect.y(), SkSamplingOptions(), &paint);
  return surface->makeImageSnapshot();
}

SkBitmap SoftwareRenderer::GetBackdropBitmap(
    const gfx::Rect& bounding_rect) const {
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(bounding_rect.width(), bounding_rect.height());
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(info))
    base::TerminateBecauseOutOfMemory(info.computeMinByteSize());

  if (!current_canvas_->readPixels(bitmap, bounding_rect.x(),
                                   bounding_rect.y()))
    bitmap.reset();
  return bitmap;
}

gfx::Rect SoftwareRenderer::GetBackdropBoundingBoxForRenderPassQuad(
    const AggregatedRenderPassDrawQuad* quad,
    const cc::FilterOperations* backdrop_filters,
    std::optional<gfx::RRectF> backdrop_filter_bounds_input,
    gfx::Transform contents_device_transform,
    gfx::Transform* backdrop_filter_bounds_transform,
    std::optional<gfx::RRectF>* backdrop_filter_bounds,
    gfx::Rect* unclipped_rect) const {
  DCHECK(backdrop_filter_bounds_transform);
  DCHECK(backdrop_filter_bounds);
  DCHECK(unclipped_rect);

  // |backdrop_filter_bounds| is a rounded rect in [-0.5,0.5] space that
  // represents |backdrop_filter_bounds_input| as a fraction of the space
  // defined by |quad->rect|, not including its offset.
  *backdrop_filter_bounds = gfx::RRectF();
  if (!backdrop_filter_bounds_input ||
      !GetScaledRRectF(quad->rect, backdrop_filter_bounds_input.value(),
                       &backdrop_filter_bounds->value())) {
    backdrop_filter_bounds->reset();
  }

  // |backdrop_rect| is now the bounding box of clip_region, in window pixel
  // coordinates, and with flip applied.
  gfx::Rect backdrop_rect = gfx::ToEnclosingRect(cc::MathUtil::MapClippedRect(
      contents_device_transform, QuadVertexRect()));

  *unclipped_rect = backdrop_rect;
  backdrop_rect.Intersect(MoveFromDrawToWindowSpace(
      current_frame()->current_render_pass->output_rect));

  // Shift to the space of the captured backdrop image.
  *backdrop_filter_bounds_transform = contents_device_transform;
  backdrop_filter_bounds_transform->PostTranslate(-backdrop_rect.x(),
                                                  -backdrop_rect.y());

  return backdrop_rect;
}

sk_sp<SkShader> SoftwareRenderer::GetBackdropFilterShader(
    const AggregatedRenderPassDrawQuad* quad,
    SkTileMode content_tile_mode) const {
  const cc::FilterOperations* backdrop_filters =
      BackdropFiltersForPass(quad->render_pass_id);
  if (!ShouldApplyBackdropFilters(backdrop_filters, quad))
    return nullptr;
  std::optional<gfx::RRectF> backdrop_filter_bounds_input =
      BackdropFilterBoundsForPass(quad->render_pass_id);

  if (backdrop_filter_bounds_input.has_value()) {
    backdrop_filter_bounds_input->Scale(quad->filters_scale.x(),
                                        quad->filters_scale.y());
  }

  gfx::Transform quad_rect_matrix;
  QuadRectTransform(&quad_rect_matrix,
                    quad->shared_quad_state->quad_to_target_transform,
                    gfx::RectF(quad->rect));
  gfx::Transform contents_device_transform = quad_rect_matrix;
  contents_device_transform.PostConcat(
      current_frame()->target_to_device_transform);
  contents_device_transform.Flatten();

  std::optional<gfx::RRectF> backdrop_filter_bounds;
  gfx::Transform backdrop_filter_bounds_transform;
  gfx::Rect unclipped_rect;
  gfx::Rect backdrop_rect = GetBackdropBoundingBoxForRenderPassQuad(
      quad, backdrop_filters, backdrop_filter_bounds_input,
      contents_device_transform, &backdrop_filter_bounds_transform,
      &backdrop_filter_bounds, &unclipped_rect);

  // Figure out the transformations to move it back to pixel space.
  gfx::Transform contents_device_transform_inverse;
  if (!contents_device_transform.GetInverse(&contents_device_transform_inverse))
    return nullptr;

  SkMatrix filter_backdrop_transform =
      gfx::TransformToFlattenedSkMatrix(contents_device_transform_inverse);
  filter_backdrop_transform.preTranslate(backdrop_rect.x(), backdrop_rect.y());

  SkBitmap backdrop_bitmap = GetBackdropBitmap(backdrop_rect);
  gfx::Point image_offset = gfx::Point(0, 0);
  if (backdrop_filter_bounds.has_value()) {
    gfx::Rect filter_clip = gfx::ToEnclosingRect(cc::MathUtil::MapClippedRect(
        backdrop_filter_bounds_transform, backdrop_filter_bounds->rect()));
    filter_clip.Intersect(
        gfx::Rect(backdrop_bitmap.width(), backdrop_bitmap.height()));
    if (filter_clip.IsEmpty())
      return nullptr;
    // Crop the source image to the backdrop_filter_bounds.
    sk_sp<SkImage> cropped_image = SkImages::RasterFromBitmap(backdrop_bitmap);
    cropped_image = cropped_image->makeSubset(
        static_cast<GrDirectContext*>(nullptr), RectToSkIRect(filter_clip));
    cropped_image->asLegacyBitmap(&backdrop_bitmap);
    image_offset = filter_clip.origin();
  }

  // TODO (crbug.com/1451898): software_renderer doesn't apply backdrop filters
  // correctly in the context of the ZOOM_FILTER operation (the lens bounds are
  // not applied correctly). The ZOOM_FILTER is never used on platforms that
  // use software_renderer, so skip calculating the filter bounds to pass
  // to BuildImageFilter().
  sk_sp<cc::PaintFilter> paint_filter =
      cc::RenderSurfaceFilters::BuildImageFilter(*backdrop_filters);
  if (!paint_filter)
    return nullptr;
  sk_sp<SkImageFilter> filter = paint_filter->cached_sk_filter_;

  // software_renderer doesn't support render passes with combined effects.
  // While the effect node tree currently doesn't combine them, it may in the
  // future (crbug.com1495777 and UI layers can combine them. Currently, only
  // the magnifier widget does so, which mixes a ZOOM backdrop filter with an
  // OFFSET filter. Due to crbug.com/1451898, that scenario never reaches this
  // check.
  DCHECK(!FiltersForPass(quad->render_pass_id))
      << "Filters should always be in a separate Effect node";

  // TODO(crbug.com/40036319): Software renderer does not support/implement
  // kClamp_TileMode.
  SkIRect result_rect;
  sk_sp<SkImage> filtered_image =
      ApplyImageFilter(filter.get(), quad, backdrop_bitmap,
                       /* offset_expanded_bounds = */ false, &result_rect);
  if (!filtered_image)
    return nullptr;

  // Use an SkBitmap to paint the rrect-clipped filtered image.
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(backdrop_rect.width(), backdrop_rect.height());
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(info))
    base::TerminateBecauseOutOfMemory(info.computeMinByteSize());

  SkCanvas canvas(bitmap, skia::LegacyDisplayGlobals::GetSkSurfaceProps());

  // Clip the filtered image to the (rounded) bounding box of the element.
  if (backdrop_filter_bounds) {
    canvas.setMatrix(
        gfx::TransformToFlattenedSkMatrix(backdrop_filter_bounds_transform));
    canvas.clipRRect(SkRRect(*backdrop_filter_bounds), SkClipOp::kIntersect,
                     true /* antialias */);
    canvas.resetMatrix();
  }

  // Paint the filtered backdrop image with opacity.
  SkPaint paint;
  if (quad->shared_quad_state->opacity < 1.0) {
    paint.setImageFilter(
        SkiaHelper::BuildOpacityFilter(quad->shared_quad_state->opacity));
  }

  // Now paint the pre-filtered image onto the canvas.
  SkRect src_rect =
      SkRect::MakeXYWH(0, 0, backdrop_bitmap.width(), backdrop_bitmap.height());
  SkRect dst_rect = src_rect.makeOffset(image_offset.x(), image_offset.y());
  canvas.drawImageRect(filtered_image, src_rect, dst_rect, SkSamplingOptions(),
                       &paint, SkCanvas::kStrict_SrcRectConstraint);

  return SkImages::RasterFromBitmap(bitmap)->makeShader(
      content_tile_mode, content_tile_mode, SkSamplingOptions(),
      &filter_backdrop_transform);
}

void SoftwareRenderer::UpdateRenderPassTextures(
    const AggregatedRenderPassList& render_passes_in_draw_order,
    const base::flat_map<AggregatedRenderPassId, RenderPassRequirements>&
        render_passes_in_frame) {
  std::vector<AggregatedRenderPassId> passes_to_delete;
  for (const auto& pair : render_pass_bitmaps_) {
    auto render_pass_it = render_passes_in_frame.find(pair.first);
    if (render_pass_it == render_passes_in_frame.end()) {
      passes_to_delete.push_back(pair.first);
      continue;
    }

    gfx::Size required_size = render_pass_it->second.size;
    // The RenderPassRequirements have a hint, which is only used for gpu
    // compositing so it is ignored here.
    const SkBitmap& bitmap = pair.second.bitmap;

    bool size_appropriate = bitmap.width() >= required_size.width() &&
                            bitmap.height() >= required_size.height();
    if (!size_appropriate)
      passes_to_delete.push_back(pair.first);
  }

  // Delete RenderPass bitmaps from the previous frame that will not be used
  // again.
  for (const AggregatedRenderPassId& id : passes_to_delete)
    render_pass_bitmaps_.erase(id);
}

void SoftwareRenderer::AllocateRenderPassResourceIfNeeded(
    const AggregatedRenderPassId& render_pass_id,
    const RenderPassRequirements& requirements) {
  auto it = render_pass_bitmaps_.find(render_pass_id);
  if (it != render_pass_bitmaps_.end()) {
    DCHECK(it->second.bitmap.width() >= requirements.size.width() &&
           it->second.bitmap.height() >= requirements.size.height());
    return;
  }

  // The |requirements.mipmap| is only used for gpu-based rendering, so not used
  // here.
  //
  // ColorSpace correctness for software compositing is a performance nightmare,
  // so we don't do it. If we did, then the color space of the current frame's
  // |current_render_pass| should be stored somewhere, but we should not set it
  // on the bitmap itself. Instead, we'd use it with a SkColorSpaceXformCanvas
  // that wraps the SkCanvas drawing into the bitmap.
  SkImageInfo info =
      SkImageInfo::MakeN32(requirements.size.width(),
                           requirements.size.height(), kPremul_SkAlphaType);
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(info))
    base::TerminateBecauseOutOfMemory(info.computeMinByteSize());

  render_pass_bitmaps_.emplace(render_pass_id, std::move(bitmap));
}

bool SoftwareRenderer::IsRenderPassResourceAllocated(
    const AggregatedRenderPassId& render_pass_id) const {
  auto it = render_pass_bitmaps_.find(render_pass_id);
  return it != render_pass_bitmaps_.end();
}

gfx::Size SoftwareRenderer::GetRenderPassBackingPixelSize(
    const AggregatedRenderPassId& render_pass_id) {
  SkBitmap& bitmap = render_pass_bitmaps_.at(render_pass_id).bitmap;
  return gfx::Size(bitmap.width(), bitmap.height());
}

void SoftwareRenderer::SetRenderPassBackingDrawnRect(
    const AggregatedRenderPassId& render_pass_id,
    const gfx::Rect& drawn_rect) {
  render_pass_bitmaps_.at(render_pass_id).drawn_rect = drawn_rect;
}

gfx::Rect SoftwareRenderer::GetRenderPassBackingDrawnRect(
    const AggregatedRenderPassId& render_pass_id) const {
  auto it = render_pass_bitmaps_.find(render_pass_id);
  if (it != render_pass_bitmaps_.end()) {
    return it->second.drawn_rect;
  } else {
    // DirectRenderer can call this before it has allocated a render pass
    // backing if this is the first contiguous frame we're seeing
    // |render_pass_id|. This can happen because it calculates the render pass
    // scissor rect before it actually allocates the backing.
    return gfx::Rect();
  }
}

}  // namespace viz
