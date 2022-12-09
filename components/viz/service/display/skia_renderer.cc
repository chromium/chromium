// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/skia_renderer.h"

#include <limits>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/math_util.h"
#include "cc/debug/debug_colors.h"
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
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/skia_helper.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/delegated_ink_handler.h"
#include "components/viz/service/display/delegated_ink_point_renderer_skia.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/display_resource_provider_skia.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/renderer_utils.h"
#include "components/viz/service/display/resource_fence.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "skia/ext/opacity_filter_canvas.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkDeferredDisplayList.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/effects/SkColorMatrix.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/effects/SkImageFilters.h"
#include "third_party/skia/include/effects/SkOverdrawColorFilter.h"
#include "third_party/skia/include/effects/SkRuntimeEffect.h"
#include "third_party/skia/include/effects/SkShaderMaskFilter.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/linear_gradient.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/gpu_fence_handle.h"

namespace viz {

namespace {

// Smallest unit that impacts anti-aliasing output. We use this to determine
// when an exterior edge (with AA) has been clipped (no AA). The specific value
// was chosen to match that used by gl_renderer.
static const float kAAEpsilon = 1.0f / 1024.0f;

// The gfx::QuadF draw_region passed to DoDrawQuad, converted to Skia types
struct SkDrawRegion {
  SkDrawRegion() = default;
  explicit SkDrawRegion(const gfx::QuadF& draw_region);

  SkPoint points[4];
};

// Additional YUV information to skia renderer to draw 9- and 10- bits color.
struct YUVInput {
  YUVInput() { memset(this, 0, sizeof(*this)); }
  float offset;
  float multiplier;
};

SkDrawRegion::SkDrawRegion(const gfx::QuadF& draw_region) {
  points[0] = gfx::PointFToSkPoint(draw_region.p1());
  points[1] = gfx::PointFToSkPoint(draw_region.p2());
  points[2] = gfx::PointFToSkPoint(draw_region.p3());
  points[3] = gfx::PointFToSkPoint(draw_region.p4());
}

bool IsTextureResource(DisplayResourceProviderSkia* resource_provider,
                       ResourceId resource_id) {
  return !resource_provider->IsResourceSoftwareBacked(resource_id);
}

unsigned GetCornerAAFlags(const DrawQuad* quad,
                          const SkPoint& vertex,
                          unsigned edge_mask) {
  // Returns mask of SkCanvas::QuadAAFlags, with bits set for each edge of the
  // shared quad state's quad_layer_rect that vertex is touching.

  unsigned mask = SkCanvas::kNone_QuadAAFlags;
  if (std::abs(vertex.x()) < kAAEpsilon)
    mask |= SkCanvas::kLeft_QuadAAFlag;
  if (std::abs(vertex.x() - quad->shared_quad_state->quad_layer_rect.width()) <
      kAAEpsilon)
    mask |= SkCanvas::kRight_QuadAAFlag;
  if (std::abs(vertex.y()) < kAAEpsilon)
    mask |= SkCanvas::kTop_QuadAAFlag;
  if (std::abs(vertex.y() - quad->shared_quad_state->quad_layer_rect.height()) <
      kAAEpsilon)
    mask |= SkCanvas::kBottom_QuadAAFlag;
  // & with the overall edge_mask to take into account edges that were clipped
  // by the visible rect.
  return mask & edge_mask;
}

bool IsExteriorEdge(unsigned corner_mask1, unsigned corner_mask2) {
  return (corner_mask1 & corner_mask2) != 0;
}

unsigned GetRectilinearEdgeFlags(const DrawQuad* quad) {
  // In the normal case, turn on AA for edges that represent the outside of
  // the layer, and that aren't clipped by the visible rect.
  unsigned mask = SkCanvas::kNone_QuadAAFlags;
  if (quad->IsLeftEdge() &&
      std::abs(quad->rect.x() - quad->visible_rect.x()) < kAAEpsilon)
    mask |= SkCanvas::kLeft_QuadAAFlag;
  if (quad->IsTopEdge() &&
      std::abs(quad->rect.y() - quad->visible_rect.y()) < kAAEpsilon)
    mask |= SkCanvas::kTop_QuadAAFlag;
  if (quad->IsRightEdge() &&
      std::abs(quad->rect.right() - quad->visible_rect.right()) < kAAEpsilon)
    mask |= SkCanvas::kRight_QuadAAFlag;
  if (quad->IsBottomEdge() &&
      std::abs(quad->rect.bottom() - quad->visible_rect.bottom()) < kAAEpsilon)
    mask |= SkCanvas::kBottom_QuadAAFlag;

  return mask;
}

// This also modifies draw_region to clean up any degeneracies
void GetClippedEdgeFlags(const DrawQuad* quad,
                         unsigned* edge_mask,
                         SkDrawRegion* draw_region) {
  // Instead of trying to rotate vertices of draw_region to align with Skia's
  // edge label conventions, turn on an edge's label if it is aligned to any
  // exterior edge.
  unsigned p0Mask = GetCornerAAFlags(quad, draw_region->points[0], *edge_mask);
  unsigned p1Mask = GetCornerAAFlags(quad, draw_region->points[1], *edge_mask);
  unsigned p2Mask = GetCornerAAFlags(quad, draw_region->points[2], *edge_mask);
  unsigned p3Mask = GetCornerAAFlags(quad, draw_region->points[3], *edge_mask);

  unsigned mask = SkCanvas::kNone_QuadAAFlags;
  // The "top" is p0 to p1
  if (IsExteriorEdge(p0Mask, p1Mask))
    mask |= SkCanvas::kTop_QuadAAFlag;
  // The "right" is p1 to p2
  if (IsExteriorEdge(p1Mask, p2Mask))
    mask |= SkCanvas::kRight_QuadAAFlag;
  // The "bottom" is p2 to p3
  if (IsExteriorEdge(p2Mask, p3Mask))
    mask |= SkCanvas::kBottom_QuadAAFlag;
  // The "left" is p3 to p0
  if (IsExteriorEdge(p3Mask, p0Mask))
    mask |= SkCanvas::kLeft_QuadAAFlag;

  // If the clipped draw_region has adjacent non-AA edges that touch the
  // exterior edge (which should be AA'ed), move the degenerate vertex to the
  // appropriate index so that Skia knows to construct a coverage ramp at that
  // corner. This is not an ideal solution, but is the best hint we can give,
  // given our limited information post-BSP splitting.
  if (draw_region->points[2] == draw_region->points[3]) {
    // The BSP splitting always creates degenerate quads with the duplicate
    // vertex in the last two indices.
    if (p0Mask && !(mask & SkCanvas::kLeft_QuadAAFlag) &&
        !(mask & SkCanvas::kTop_QuadAAFlag)) {
      // Rewrite draw_region from p0,p1,p2,p2 to p0,p1,p2,p0; top edge stays off
      // right edge is preserved, bottom edge turns off, left edge turns on
      draw_region->points[3] = draw_region->points[0];
      mask = SkCanvas::kLeft_QuadAAFlag | (mask & SkCanvas::kRight_QuadAAFlag);
    } else if (p1Mask && !(mask & SkCanvas::kTop_QuadAAFlag) &&
               !(mask & SkCanvas::kRight_QuadAAFlag)) {
      // Rewrite draw_region to p0,p1,p1,p2; top edge stays off, right edge
      // turns on, bottom edge turns off, left edge is preserved
      draw_region->points[2] = draw_region->points[1];
      mask = SkCanvas::kRight_QuadAAFlag | (mask & SkCanvas::kLeft_QuadAAFlag);
    }
    // p2 could follow the same process, but if its adjacent edges are AA
    // (skipping the degenerate edge to p3), it's actually already in the
    // desired vertex ordering; and since p3 is in the same location, it's
    // equivalent to p2 so it doesn't need checking either.
  }  // Else not degenerate, so can't to correct non-AA corners touching AA edge

  *edge_mask = mask;
}

bool IsAAForcedOff(const DrawQuad* quad) {
  switch (quad->material) {
    case DrawQuad::Material::kPictureContent:
      return PictureDrawQuad::MaterialCast(quad)->force_anti_aliasing_off;
    case DrawQuad::Material::kCompositorRenderPass:
      // We should not have compositor render passes here.
      NOTREACHED();
      return CompositorRenderPassDrawQuad::MaterialCast(quad)
          ->force_anti_aliasing_off;
    case DrawQuad::Material::kAggregatedRenderPass:
      return AggregatedRenderPassDrawQuad::MaterialCast(quad)
          ->force_anti_aliasing_off;
    case DrawQuad::Material::kSolidColor:
      return SolidColorDrawQuad::MaterialCast(quad)->force_anti_aliasing_off;
    case DrawQuad::Material::kTiledContent:
      return TileDrawQuad::MaterialCast(quad)->force_anti_aliasing_off;
    case DrawQuad::Material::kYuvVideoContent:
    case DrawQuad::Material::kTextureContent:
      // This is done to match the behaviour of GLRenderer and we can revisit it
      // later.
      return true;
    default:
      return false;
  }
}

bool UseNearestNeighborSampling(const DrawQuad* quad) {
  switch (quad->material) {
    case DrawQuad::Material::kPictureContent:
      return PictureDrawQuad::MaterialCast(quad)->nearest_neighbor;
    case DrawQuad::Material::kTextureContent:
      return TextureDrawQuad::MaterialCast(quad)->nearest_neighbor;
    case DrawQuad::Material::kTiledContent:
      return TileDrawQuad::MaterialCast(quad)->nearest_neighbor;
    default:
      // Other quad types do not expose nearest_neighbor.
      return false;
  }
}

SkSamplingOptions GetSampling(const DrawQuad* quad) {
  if (UseNearestNeighborSampling(quad))
    return SkSamplingOptions(SkFilterMode::kNearest);

  // Default to bilinear if the quad doesn't specify nearest_neighbor.
  // TODO(penghuang): figure out how to set correct filter quality for YUV and
  // video stream quads.
  return SkSamplingOptions(SkFilterMode::kLinear);
}

// Returns kFast if sampling outside of vis_tex_coords due to AA or bilerp will
// not go outside of the content area, or if the content area is the full image
// (in which case hardware clamping handles it automatically). Different quad
// types have different rules for the content area within the image.
SkCanvas::SrcRectConstraint GetTextureConstraint(
    const SkImage* image,
    const gfx::RectF& vis_tex_coords,
    const gfx::RectF& valid_texel_bounds) {
  bool fills_left = valid_texel_bounds.x() <= 0.f;
  bool fills_right = valid_texel_bounds.right() >= image->width();
  bool fills_top = valid_texel_bounds.y() <= 0.f;
  bool fills_bottom = valid_texel_bounds.bottom() >= image->height();
  if (fills_left && fills_right && fills_top && fills_bottom) {
    // The entire image is contained in the content area, so hardware clamping
    // ensures only content texels are sampled
    return SkCanvas::kFast_SrcRectConstraint;
  }

  gfx::RectF safe_texels = valid_texel_bounds;
  safe_texels.Inset(0.5f);

  // Check each axis independently; tile quads may only need clamping on one
  // side (e.g. right or bottom) and this logic doesn't fully match a simple
  // contains() check.
  if ((!fills_left && vis_tex_coords.x() < safe_texels.x()) ||
      (!fills_right && vis_tex_coords.right() > safe_texels.right())) {
    return SkCanvas::kStrict_SrcRectConstraint;
  }
  if ((!fills_top && vis_tex_coords.y() < safe_texels.y()) ||
      (!fills_bottom && vis_tex_coords.bottom() > safe_texels.bottom())) {
    return SkCanvas::kStrict_SrcRectConstraint;
  }

  // The texture coordinates are far enough from the content area that even with
  // bilerp and AA, it won't sample outside the content area
  return SkCanvas::kFast_SrcRectConstraint;
}

// Return a color filter that multiplies the incoming color by the fixed alpha
sk_sp<SkColorFilter> MakeOpacityFilter(float alpha, sk_sp<SkColorFilter> in) {
  SkColor4f alpha_as_color = {1.0, 1.0, 1.0, alpha};
  // MakeModeFilter treats fixed color as src, and input color as dst.
  // kDstIn is (srcAlpha * dstColor, srcAlpha * dstAlpha) so this makes the
  // output color equal to input color * alpha.
  // TODO(michaelludwig): Update to pass alpha_as_color as-is once
  // skbug.com/13637 is resolved (adds Blend + SkColor4f variation).
  sk_sp<SkColorFilter> opacity =
      SkColorFilters::Blend(alpha_as_color.toSkColor(), SkBlendMode::kDstIn);
  // Opaque (alpha = 1.0) and kDstIn returns nullptr to signal a no-op, so that
  // case should just return 'in'.
  if (opacity) {
    return opacity->makeComposed(std::move(in));
  } else {
    return in;
  }
}

// Porter-Duff blend mode utility functions, where the final color is
// represented as a weighted sum of the incoming src and existing dst color.
// See [https://skia.org/user/api/SkBlendMode_Reference]

bool IsPorterDuffBlendMode(SkBlendMode blendMode) {
  return blendMode <= SkBlendMode::kLastCoeffMode;
}

// Returns true if drawing transparent black with |blendMode| would modify the
// destination buffer. If false is returned, the draw would have no discernible
// effect on the pixel color so the entire draw can be skipped.
bool TransparentBlackAffectsOutput(SkBlendMode blendMode) {
  SkBlendModeCoeff src, dst;
  if (!SkBlendMode_AsCoeff(blendMode, &src, &dst)) {
    // An advanced blend mode that can't be represented as coefficients, so
    // assume it modifies the output.
    return true;
  }
  // True when the dst coefficient is not equal to 1 (when src = (0,0,0,0))
  return dst != SkBlendModeCoeff::kOne && dst != SkBlendModeCoeff::kISA &&
         dst != SkBlendModeCoeff::kISC;
}

// Returns true if src content drawn with |blendMode| into a RenderPass would
// produce the exact same image as the original src content.
bool RenderPassPreservesContent(SkBlendMode blendMode) {
  SkBlendModeCoeff src, dst;
  if (!SkBlendMode_AsCoeff(blendMode, &src, &dst)) {
    return false;
  }
  // True when src coefficient is equal to 1 (when dst = (0,0,0,0))
  return src == SkBlendModeCoeff::kOne || src == SkBlendModeCoeff::kIDA ||
         src == SkBlendModeCoeff::kIDC;
}

// Returns true if src content draw with |blendMode| into an empty RenderPass
// would produce a transparent black image.
bool RenderPassRemainsTransparent(SkBlendMode blendMode) {
  SkBlendModeCoeff src, dst;
  if (!SkBlendMode_AsCoeff(blendMode, &src, &dst)) {
    return false;
  }

  // True when src coefficient is equal to 0 (when dst = (0,0,0,0))
  return src == SkBlendModeCoeff::kZero || src == SkBlendModeCoeff::kDA ||
         src == SkBlendModeCoeff::kDC;
}

SkYUVAInfo::Subsampling SubsamplingFromTextureSizes(gfx::Size ya_size,
                                                    gfx::Size uv_size) {
  if (uv_size.height() == ya_size.height()) {
    if (uv_size.width() == ya_size.width())
      return SkYUVAInfo::Subsampling::k444;
    if (uv_size.width() == (ya_size.width() + 1) / 2)
      return SkYUVAInfo::Subsampling::k422;
    if (uv_size.width() == (ya_size.width() + 3) / 4)
      return SkYUVAInfo::Subsampling::k411;
  } else if (uv_size.height() == (ya_size.height() + 1) / 2) {
    if (uv_size.width() == ya_size.width())
      return SkYUVAInfo::Subsampling::k440;
    if (uv_size.width() == (ya_size.width() + 1) / 2)
      return SkYUVAInfo::Subsampling::k420;
    if (uv_size.width() == (ya_size.width() + 3) / 4)
      return SkYUVAInfo::Subsampling::k410;
  }
  return SkYUVAInfo::Subsampling::kUnknown;
}

}  // namespace

// chrome style prevents this from going in skia_renderer.h, but since it
// uses absl::optional, the style also requires it to have a declared ctor
SkiaRenderer::BatchedQuadState::BatchedQuadState() = default;

// Parameters needed to draw a CompositorRenderPassDrawQuad.
struct SkiaRenderer::DrawRPDQParams {
  struct BypassGeometry {
    // The additional matrix to concatenate to the SkCanvas after image filters
    // have been configured so that the DrawQuadParams geometry is properly
    // mapped (i.e. when set, |visible_rect| and |draw_region| must be
    // pre-transformed by this before |content_device_transform|).
    SkMatrix transform;

    // `rect` from the bypassed RenderPassDrawQuad.
    gfx::RectF rect;

    // `visible_rect` from the bypassed RenderPassDrawQuad.
    gfx::RectF visible_rect;
  };

  explicit DrawRPDQParams(const gfx::RectF& visible_rect);

  // Root of the calculated image filter DAG to be applied to the render pass.
  sk_sp<SkImageFilter> image_filter = nullptr;
  // If |image_filter| can be represented as a single color filter, this will
  // be that filter. |image_filter| will still be non-null.
  sk_sp<SkColorFilter> color_filter = nullptr;
  // Root of the calculated backdrop filter DAG to be applied to the render pass
  sk_sp<SkImageFilter> backdrop_filter = nullptr;
  // Resolved mask image and calculated transform matrix baked into an SkShader,
  // which will be applied using SkCanvas::clipShader in RPDQ's coord space.
  sk_sp<SkShader> mask_shader = nullptr;
  // Backdrop border box for the render pass, to clip backdrop-filtered content
  absl::optional<gfx::RRectF> backdrop_filter_bounds;
  // The content space bounds that includes any filtered extents. If empty,
  // the draw can be skipped.
  gfx::Rect filter_bounds;

  // Geometry from the bypassed RenderPassDrawQuad.
  absl::optional<BypassGeometry> bypass_geometry;

  // True when there is an |image_filter| and it's not equivalent to
  // |color_filter|.
  bool has_complex_image_filter() const {
    return image_filter && !color_filter;
  }

  // True if the RenderPass's output rect would clip the visible contents that
  // are bypassing the renderpass' offscreen buffer.
  bool needs_bypass_clip(const gfx::RectF& content_rect) const {
    if (!bypass_geometry)
      return false;

    SkRect content_bounds =
        bypass_geometry->transform.mapRect(gfx::RectFToSkRect(content_rect));
    return !bypass_geometry->visible_rect.Contains(
        gfx::SkRectToRectF(content_bounds));
  }
};

SkiaRenderer::DrawRPDQParams::DrawRPDQParams(const gfx::RectF& visible_rect)
    : filter_bounds(gfx::ToEnclosingRect(visible_rect)) {}

// State calculated from a DrawQuad and current renderer state, that is common
// to all DrawQuad rendering.
struct SkiaRenderer::DrawQuadParams {
  DrawQuadParams() = default;
  DrawQuadParams(const gfx::Transform& cdt,
                 const gfx::RectF& rect,
                 const gfx::RectF& visible_rect,
                 unsigned aa_flags,
                 SkBlendMode blend_mode,
                 float opacity,
                 const SkSamplingOptions& sampling,
                 const gfx::QuadF* draw_region);

  // target_to_device_transform * quad_to_target_transform normally, or
  // quad_to_target_transform if the remaining device transform is held in the
  // DrawRPDQParams for a bypass quad.
  gfx::Transform content_device_transform;
  // The DrawQuad's rect.
  gfx::RectF rect;
  // The DrawQuad's visible_rect, possibly explicitly clipped by the scissor
  gfx::RectF visible_rect;
  // Initialized to the visible_rect, relevant quad types should updated based
  // on their specialized properties.
  gfx::RectF vis_tex_coords;
  // SkCanvas::QuadAAFlags, already taking into account settings
  // (but not certain quad type's force_antialias_off bit)
  unsigned aa_flags;
  // Final blend mode to use, respecting quad settings + opacity optimizations
  SkBlendMode blend_mode;
  // Final opacity of quad
  float opacity;
  // Resolved sampling from quad settings
  SkSamplingOptions sampling;
  // Optional restricted draw geometry, will point to a length 4 SkPoint array
  // with its points in CW order matching Skia's vertex/edge expectations.
  absl::optional<SkDrawRegion> draw_region;
  // Optional mask filter info that may contain rounded corner clip and/or a
  // gradient mask to apply. If present, rounded corner clip will have been
  // transformed to device space and ShouldApplyRoundedCorner returns true. If
  // present, gradient mask will have been transformed to device space and
  // ShouldApplyGradientMask returns true.
  absl::optional<gfx::MaskFilterInfo> mask_filter_info;
  // Optional device space clip to apply. If present, it is equal to the current
  // |scissor_rect_| of the renderer.
  absl::optional<gfx::Rect> scissor_rect;

  SkPaint paint(sk_sp<SkColorFilter> color_filter) const {
    SkPaint p;
    if (color_filter) {
      p.setColorFilter(color_filter);
    }
    p.setBlendMode(blend_mode);
    p.setAlphaf(opacity);
    p.setAntiAlias(aa_flags != SkCanvas::kNone_QuadAAFlags);
    return p;
  }

  SkPath draw_region_in_path() const {
    if (draw_region) {
      return SkPath::Polygon(draw_region->points,
                             std::size(draw_region->points),
                             /*isClosed=*/true);
    }
    return SkPath();
  }

  void ApplyScissor(const SkiaRenderer* renderer,
                    const DrawQuad* quad,
                    const gfx::Rect* scissor_to_apply);
};

SkiaRenderer::DrawQuadParams::DrawQuadParams(const gfx::Transform& cdt,
                                             const gfx::RectF& rect,
                                             const gfx::RectF& visible_rect,
                                             unsigned aa_flags,
                                             SkBlendMode blend_mode,
                                             float opacity,
                                             const SkSamplingOptions& sampling,
                                             const gfx::QuadF* draw_region)
    : content_device_transform(cdt),
      rect(rect),
      visible_rect(visible_rect),
      vis_tex_coords(visible_rect),
      aa_flags(aa_flags),
      blend_mode(blend_mode),
      opacity(opacity),
      sampling(sampling) {
  if (draw_region) {
    this->draw_region.emplace(*draw_region);
  }
}

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
struct SkiaRenderer::RenderPassOverlayParams {
  AggregatedRenderPassId render_pass_id;
  RenderPassBacking render_pass_backing;
  AggregatedRenderPassDrawQuad rpdq;
  SharedQuadState shared_quad_state;
  cc::FilterOperations filters;
  cc::FilterOperations backdrop_filters;
};
#endif

enum class SkiaRenderer::BypassMode {
  // The RenderPass's contents' blendmode would have made a transparent black
  // image and the RenderPass's own blend mode does not effect transparent black
  kSkip,
  // The renderPass's contents' creates a transparent image, but the
  // RenderPass's own blend mode must still process the transparent pixels (e.g.
  // certain filters affect transparent black).
  kDrawTransparentQuad,
  // Can draw the bypass quad with the modified parameters
  kDrawBypassQuad
};

// Scoped helper class for building SkImage from resource id.
class SkiaRenderer::ScopedSkImageBuilder {
 public:
  ScopedSkImageBuilder(SkiaRenderer* skia_renderer,
                       ResourceId resource_id,
                       bool maybe_concurrent_reads,
                       SkAlphaType alpha_type = kPremul_SkAlphaType,
                       GrSurfaceOrigin origin = kTopLeft_GrSurfaceOrigin,
                       sk_sp<SkColorSpace> override_color_space = nullptr,
                       bool raw_draw_if_possible = false);

  ScopedSkImageBuilder(const ScopedSkImageBuilder&) = delete;
  ScopedSkImageBuilder& operator=(const ScopedSkImageBuilder&) = delete;

  ~ScopedSkImageBuilder() = default;

  const SkImage* sk_image() const { return sk_image_; }
  const cc::PaintOpBuffer* paint_op_buffer() const { return paint_op_buffer_; }
  const absl::optional<SkColor4f>& clear_color() const { return clear_color_; }

 private:
  raw_ptr<const SkImage, DanglingUntriaged> sk_image_ = nullptr;
  raw_ptr<const cc::PaintOpBuffer> paint_op_buffer_ = nullptr;
  absl::optional<SkColor4f> clear_color_;
};

SkiaRenderer::ScopedSkImageBuilder::ScopedSkImageBuilder(
    SkiaRenderer* skia_renderer,
    ResourceId resource_id,
    bool maybe_concurrent_reads,
    SkAlphaType alpha_type,
    GrSurfaceOrigin origin,
    sk_sp<SkColorSpace> override_color_space,
    bool raw_draw_if_possible) {
  if (!resource_id)
    return;
  auto* resource_provider = skia_renderer->resource_provider();
  DCHECK(IsTextureResource(resource_provider, resource_id));

  auto* image_context = skia_renderer->lock_set_for_external_use_->LockResource(
      resource_id, maybe_concurrent_reads, /*is_video_plane=*/false,
      override_color_space, raw_draw_if_possible);

  // |ImageContext::image| provides thread safety: (a) this ImageContext is
  // only accessed by GPU thread after |image| is set and (b) the fields of
  // ImageContext that are accessed by both compositor and GPU thread are no
  // longer modified after |image| is set.
  if (!image_context->has_image()) {
    image_context->set_alpha_type(alpha_type);
    image_context->set_origin(origin);
  }

  // We need the original TransferableResource.color_space for YUV => RGB
  // conversion.
  skia_renderer->skia_output_surface_->MakePromiseSkImage(
      image_context, resource_provider->GetOverlayColorSpace(resource_id));
  paint_op_buffer_ = image_context->paint_op_buffer();
  clear_color_ = image_context->clear_color();
  sk_image_ = image_context->image().get();
  LOG_IF(ERROR, !image_context->has_image() && !paint_op_buffer_)
      << "Failed to create the promise sk image or get paint ops.";
}

class SkiaRenderer::ScopedYUVSkImageBuilder {
 public:
  ScopedYUVSkImageBuilder(SkiaRenderer* skia_renderer,
                          const YUVVideoDrawQuad* quad,
                          sk_sp<SkColorSpace> dst_color_space) {
    DCHECK(IsTextureResource(skia_renderer->resource_provider(),
                             quad->y_plane_resource_id()));
    DCHECK(IsTextureResource(skia_renderer->resource_provider(),
                             quad->u_plane_resource_id()));
    DCHECK(IsTextureResource(skia_renderer->resource_provider(),
                             quad->v_plane_resource_id()));
    DCHECK(quad->a_plane_resource_id() == kInvalidResourceId ||
           IsTextureResource(skia_renderer->resource_provider(),
                             quad->a_plane_resource_id()));

    // The image is always either NV12 or I420, possibly with a separate alpha
    // plane.
    SkYUVAInfo::PlaneConfig plane_config;
    if (quad->a_plane_resource_id() == kInvalidResourceId) {
      plane_config = quad->u_plane_resource_id() == quad->v_plane_resource_id()
                         ? SkYUVAInfo::PlaneConfig::kY_UV
                         : SkYUVAInfo::PlaneConfig::kY_U_V;
    } else {
      plane_config = quad->u_plane_resource_id() == quad->v_plane_resource_id()
                         ? SkYUVAInfo::PlaneConfig::kY_UV_A
                         : SkYUVAInfo::PlaneConfig::kY_U_V_A;
    }
    SkYUVAInfo::Subsampling subsampling =
        SubsamplingFromTextureSizes(quad->ya_tex_size(), quad->uv_tex_size());
    const int number_of_textures = SkYUVAInfo::NumPlanes(plane_config);
    std::vector<ExternalUseClient::ImageContext*> contexts;
    contexts.reserve(number_of_textures);
    // Skia API ignores the color space information on the individual planes.
    // Dropping them here avoids some LOG spam.
    auto* y_context = skia_renderer->lock_set_for_external_use_->LockResource(
        quad->y_plane_resource_id(), /*maybe_concurrent_reads=*/true,
        /*is_video_plane=*/true);
    contexts.push_back(std::move(y_context));
    auto* u_context = skia_renderer->lock_set_for_external_use_->LockResource(
        quad->u_plane_resource_id(), /*maybe_concurrent_reads=*/true,
        /*is_video_plane=*/true);
    contexts.push_back(std::move(u_context));
    if (plane_config == SkYUVAInfo::PlaneConfig::kY_U_V ||
        plane_config == SkYUVAInfo::PlaneConfig::kY_U_V_A) {
      auto* v_context = skia_renderer->lock_set_for_external_use_->LockResource(
          quad->v_plane_resource_id(), /*maybe_concurrent_reads=*/true,
          /*is_video_plane=*/true);
      contexts.push_back(std::move(v_context));
    }

    if (SkYUVAInfo::HasAlpha(plane_config)) {
      auto* a_context = skia_renderer->lock_set_for_external_use_->LockResource(
          quad->a_plane_resource_id(), /*maybe_concurrent_reads=*/true,
          /*is_video_plane=*/true);
      contexts.push_back(std::move(a_context));
    }

    // Note: YUV to RGB and color conversion is handled by a color filter.
    sk_image_ = skia_renderer->skia_output_surface_->MakePromiseSkImageFromYUV(
        std::move(contexts), dst_color_space, plane_config, subsampling);
    LOG_IF(ERROR, !sk_image_) << "Failed to create the promise sk yuva image.";
  }

  ScopedYUVSkImageBuilder(const ScopedYUVSkImageBuilder&) = delete;
  ScopedYUVSkImageBuilder& operator=(const ScopedYUVSkImageBuilder&) = delete;

  ~ScopedYUVSkImageBuilder() = default;

  const SkImage* sk_image() const { return sk_image_.get(); }

 private:
  sk_sp<SkImage> sk_image_;
};

// A read lock based fence that is signaled after gpu commands are completed
// meaning the resource has been read.
// TODO(fangzhoug): Move this out of this file s.t. it can be referenced in
// display_resource_provider_skia_unittest.cc.
class SkiaRenderer::FrameResourceGpuCommandsCompletedFence
    : public ResourceFence {
 public:
  explicit FrameResourceGpuCommandsCompletedFence(
      DisplayResourceProviderSkia* resource_provider)
      : ResourceFence(resource_provider) {}
  FrameResourceGpuCommandsCompletedFence() = delete;
  FrameResourceGpuCommandsCompletedFence(
      const FrameResourceGpuCommandsCompletedFence&) = delete;
  FrameResourceGpuCommandsCompletedFence& operator=(
      const FrameResourceGpuCommandsCompletedFence&) = delete;

  // ResourceFence implementation.
  bool HasPassed() override { return passed_; }
  gfx::GpuFenceHandle GetGpuFenceHandle() override {
    NOTREACHED();
    return gfx::GpuFenceHandle();
  }

  void Signal() {
    passed_ = true;
    FencePassed();
  }

 private:
  ~FrameResourceGpuCommandsCompletedFence() override = default;

  bool passed_ = false;
};

// FrameResourceFence that gets a ReleaseFence which is later set to returned
// resources.
// TODO(fangzhoug): Move this out of this file s.t. it can be referenced in
// display_resource_provider_skia_unittest.cc.
class SkiaRenderer::FrameResourceReleaseFence : public ResourceFence {
 public:
  explicit FrameResourceReleaseFence(
      DisplayResourceProviderSkia* resource_provider)
      : ResourceFence(resource_provider) {}
  FrameResourceReleaseFence() = delete;
  FrameResourceReleaseFence(const FrameResourceReleaseFence&) = delete;
  FrameResourceReleaseFence& operator=(const FrameResourceReleaseFence&) =
      delete;

  // ResourceFence implementation:
  bool HasPassed() override { return release_fence_.has_value(); }
  gfx::GpuFenceHandle GetGpuFenceHandle() override {
    return HasPassed() ? release_fence_.value().Clone() : gfx::GpuFenceHandle();
  }

  void SetReleaseFenceCallback(gfx::GpuFenceHandle release_fence) {
    release_fence_ = std::move(release_fence);
    FencePassed();
  }

 private:
  ~FrameResourceReleaseFence() override = default;

  // This is made optional so that the value is set after
  // SetReleaseFenceCallback is called. Otherwise, there is no way to know if
  // the fence has been set and a null handle is a "valid" handle.
  absl::optional<gfx::GpuFenceHandle> release_fence_;
};

SkiaRenderer::SkiaRenderer(const RendererSettings* settings,
                           const DebugRendererSettings* debug_settings,
                           OutputSurface* output_surface,
                           DisplayResourceProviderSkia* resource_provider,
                           OverlayProcessorInterface* overlay_processor,
                           SkiaOutputSurface* skia_output_surface)
    : DirectRenderer(settings,
                     debug_settings,
                     output_surface,
                     resource_provider,
                     overlay_processor),
      skia_output_surface_(skia_output_surface),
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
      can_skip_render_pass_overlay_(
          base::FeatureList::IsEnabled(features::kCanSkipRenderPassOverlay)),
#endif
      is_using_raw_draw_(features::IsUsingRawDraw()) {
  DCHECK(skia_output_surface_);
  lock_set_for_external_use_.emplace(resource_provider, skia_output_surface_);

  // There can be different synchronization types requested for different
  // resources. Some of them may require SyncToken, others - ReadLockFence, and
  // others may need ReleaseFence. SyncTokens are set when the output surface
  // is flushed and external resources are released. However, other resources
  // require additional setup, which helps to handle that.
  current_gpu_commands_completed_fence_ =
      base::MakeRefCounted<FrameResourceGpuCommandsCompletedFence>(
          resource_provider);
  current_release_fence_ =
      base::MakeRefCounted<FrameResourceReleaseFence>(resource_provider);
  this->resource_provider()->SetGpuCommandsCompletedFence(
      current_gpu_commands_completed_fence_.get());
  this->resource_provider()->SetReleaseFence(current_release_fence_.get());

  if (output_surface->capabilities().renderer_allocates_images) {
    // When using dynamic frame buffer allocation we'll start with 0 buffers and
    // let EnsureMinNumberOfBuffers() increase it later.
    size_t number_of_buffers =
        output_surface->capabilities().supports_dynamic_frame_buffer_allocation
            ? 0
            : output_surface->capabilities().number_of_buffers;
    buffer_queue_ = std::make_unique<BufferQueue>(
        skia_output_surface_, skia_output_surface_->GetSurfaceHandle(),
        number_of_buffers);
  }

#if OS_ANDROID
  use_real_color_space_for_stream_video_ =
      features::UseRealVideoColorSpaceForDisplay();
#endif
}

SkiaRenderer::~SkiaRenderer() = default;

bool SkiaRenderer::CanPartialSwap() {
  return output_surface_->capabilities().supports_post_sub_buffer;
}

void SkiaRenderer::BeginDrawingFrame() {
  TRACE_EVENT0("viz", "SkiaRenderer::BeginDrawingFrame");

  DCHECK(!current_gpu_commands_completed_fence_->was_set());
  DCHECK(!current_release_fence_->was_set());
}

void SkiaRenderer::FinishDrawingFrame() {
  TRACE_EVENT0("viz", "SkiaRenderer::FinishDrawingFrame");
  current_canvas_ = nullptr;

  swap_buffer_rect_ = current_frame()->root_damage_rect;

#if BUILDFLAG(IS_OZONE)
  MaybeScheduleBackgroundImage(current_frame()->overlay_list);
#endif  // BUILDFLAG(IS_OZONE)

  // TODO(weiliangc): Remove this once OverlayProcessor schedules overlays.
  if (current_frame()->output_surface_plane) {
    auto& surface_plane = current_frame()->output_surface_plane.value();

    if (!buffer_queue_) {
      skia_output_surface_->ScheduleOutputSurfaceAsOverlay(surface_plane);
    } else {
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
      // Windows and Mac have different OverlayList types, but those platforms
      // aren't supported by buffer_queue_ yet, so this won't be reached.
      NOTREACHED();
#else
      auto root_pass_backing =
          render_pass_backings_.find(current_frame()->root_render_pass->id);
      // The root pass backing should always exist.
      DCHECK(root_pass_backing != render_pass_backings_.end());

      OverlayCandidate surface_candidate;
      surface_candidate.mailbox = root_pass_backing->second.mailbox;
      surface_candidate.is_root_render_pass = true;
      surface_candidate.transform = surface_plane.transform;
      surface_candidate.display_rect = surface_plane.display_rect;
      surface_candidate.uv_rect = surface_plane.uv_rect;
      surface_candidate.resource_size_in_pixels = surface_plane.resource_size;
      surface_candidate.format = surface_plane.format;
      surface_candidate.color_space = surface_plane.color_space;
      surface_candidate.is_opaque = !surface_plane.enable_blending;
      surface_candidate.opacity = surface_plane.opacity;
      surface_candidate.priority_hint = surface_plane.priority_hint;
      surface_candidate.rounded_corners = surface_plane.rounded_corners;
      surface_candidate.damage_rect =
          gfx::RectF(surface_plane.damage_rect.value_or(
              gfx::Rect(surface_plane.resource_size)));

      current_frame()->overlay_list.insert(
          current_frame()->overlay_list.begin(), surface_candidate);
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
    }
  }
  ScheduleOverlays();
  debug_tint_modulate_count_++;
}

void SkiaRenderer::SwapBuffers(SwapFrameData swap_frame_data) {
  DCHECK(visible_);
  DCHECK(output_surface_->capabilities().supports_viewporter ||
         viewport_size_for_swap_buffers() == surface_size_for_swap_buffers());
  TRACE_EVENT0("viz,benchmark", "SkiaRenderer::SwapBuffers");

  OutputSurfaceFrame output_frame;
  output_frame.latency_info = std::move(swap_frame_data.latency_info);
  output_frame.top_controls_visible_height_changed =
      swap_frame_data.top_controls_visible_height_changed;
  output_frame.choreographer_vsync_id = swap_frame_data.choreographer_vsync_id;
  output_frame.size = viewport_size_for_swap_buffers();
  output_frame.data.seq = swap_frame_data.seq;
  if (use_partial_swap_) {
    swap_buffer_rect_.Intersect(gfx::Rect(surface_size_for_swap_buffers()));
    output_frame.sub_buffer_rect = swap_buffer_rect_;
  } else if (swap_buffer_rect_.IsEmpty() && allow_empty_swap_) {
    output_frame.sub_buffer_rect = gfx::Rect();
  }
  if (delegated_ink_handler_ && !UsingSkiaForDelegatedInk()) {
    output_frame.delegated_ink_metadata =
        delegated_ink_handler_->TakeMetadata();
  }
#if BUILDFLAG(IS_MAC)
  output_frame.ca_layer_error_code = swap_frame_data.ca_layer_error_code;
#endif

  if (buffer_queue_) {
    gfx::Rect damage_rect = output_frame.sub_buffer_rect.value_or(
        gfx::Rect(surface_size_for_swap_buffers()));
    buffer_queue_->SwapBuffers(damage_rect);
  }

  skia_output_surface_->SwapBuffers(std::move(output_frame));
  swap_buffer_rect_ = gfx::Rect();

  FlushOutputSurface();

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
  // Delete render pass overlay backings from the previous frame that will not
  // be used again.
  for (auto& overlay : available_render_pass_overlay_backings_) {
    skia_output_surface_->DestroySharedImage(
        overlay.render_pass_backing.mailbox);
  }
  available_render_pass_overlay_backings_.clear();
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)

#if BUILDFLAG(IS_OZONE)
  // Clear cached solid color buffers that weren't reused.
  base::EraseIf(solid_color_buffers_, [this](auto entry) {
    SolidColorBuffer& color_buffer = entry.second;
    if (!color_buffer.use_count) {
      skia_output_surface_->DestroySharedImage(color_buffer.mailbox);
      return true;
    }
    return false;
  });
#endif  // BUILDFLAG(IS_OZONE)
}

void SkiaRenderer::SwapBuffersSkipped() {
  gfx::Rect root_pass_damage_rect = gfx::Rect(surface_size_for_swap_buffers());
  if (use_partial_swap_)
    root_pass_damage_rect.Intersect(swap_buffer_rect_);

#if BUILDFLAG(IS_OZONE)
  MaybeDecrementSolidColorBuffers(pending_overlay_locks_.back());
#endif  // BUILDFLAG(IS_OZONE)

  pending_overlay_locks_.pop_back();
  skia_output_surface_->SwapBuffersSkipped(root_pass_damage_rect);
  if (buffer_queue_) {
    buffer_queue_->SwapBuffersSkipped(root_pass_damage_rect);
  }
  swap_buffer_rect_ = gfx::Rect();

  FlushOutputSurface();
}

void SkiaRenderer::SwapBuffersComplete(
    const gpu::SwapBuffersCompleteParams& params,
    gfx::GpuFenceHandle release_fence) {
  auto& read_lock_release_fence_overlay_locks =
      read_lock_release_fence_overlay_locks_.emplace_back();
  auto read_fence_lock_iter = committed_overlay_locks_.end();

  if (buffer_queue_) {
    if (params.swap_response.result ==
        gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS) {
      buffer_queue_->RecreateBuffers();
    }
    buffer_queue_->SwapBuffersComplete();
  }
  if (!release_fence.is_null()) {
    // Set release fences to return overlay resources for last frame.
    for (auto& lock : committed_overlay_locks_) {
      lock.SetReleaseFence(release_fence.Clone());
    }
    // Find all locks that have a read-lock fence associated with them and move
    // them to the back of locks. If we have a release fence, it's not safe to
    // release them here. Release them later in BuffersPresented().
    read_fence_lock_iter = std::partition(
        committed_overlay_locks_.begin(), committed_overlay_locks_.end(),
        [](auto& lock) { return !lock.HasReadLockFence(); });
    read_lock_release_fence_overlay_locks.insert(
        read_lock_release_fence_overlay_locks.end(),
        std::make_move_iterator(read_fence_lock_iter),
        std::make_move_iterator(committed_overlay_locks_.end()));
  }

#if BUILDFLAG(IS_OZONE)
  MaybeDecrementSolidColorBuffers(committed_overlay_locks_);
#endif  // BUILDFLAG(IS_OZONE)

  // Right now, only macOS and Ozone need to return mailboxes of released
  // overlays, so we should not release |committed_overlay_locks_| here. The
  // resources in it will be released in DidReceiveReleasedOverlays() later.
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
  for (auto lock_iter = committed_overlay_locks_.begin();
       lock_iter != read_fence_lock_iter; ++lock_iter) {
    awaiting_release_overlay_locks_.insert(std::move(*lock_iter));
  }
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)

  // Current pending locks should have been committed by the next time
  // SwapBuffers() is completed.
  committed_overlay_locks_.clear();
  std::swap(committed_overlay_locks_, pending_overlay_locks_.front());
  pending_overlay_locks_.pop_front();
}

void SkiaRenderer::BuffersPresented() {
  if (read_lock_release_fence_overlay_locks_.empty()) {
    // Debug crbug.com/1357789.
    base::debug::DumpWithoutCrashing();
    return;
  }
  read_lock_release_fence_overlay_locks_.pop_front();
}

void SkiaRenderer::DidReceiveReleasedOverlays(
    const std::vector<gpu::Mailbox>& released_overlays) {
  // This method is only called on macOS and Ozone right now.
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
  for (const auto& mailbox : released_overlays) {
    // If this mailbox is for render pass overlay, mark the released render pass
    // overlay backing as available to be re-used.
    auto it =
        base::ranges::find(in_flight_render_pass_overlay_backings_, mailbox,
                           [](const RenderPassOverlayParams& overlay) {
                             return overlay.render_pass_backing.mailbox;
                           });
    if (it != in_flight_render_pass_overlay_backings_.end()) {
      available_render_pass_overlay_backings_.push_back(*it);
      in_flight_render_pass_overlay_backings_.erase(it);
    }

    auto iter = base::ranges::find(awaiting_release_overlay_locks_, mailbox,
                                   &OverlayLock::mailbox);
    if (iter == awaiting_release_overlay_locks_.end()) {
// TODO(crbug.com/1299794): Re-enable this DCHECK on Ozone.
#if !BUILDFLAG(IS_OZONE)
      // The released overlay should always be found as awaiting to be released.
      DLOG(FATAL) << "Got an unexpected mailbox";
#endif  // !BUILDFLAG(IS_OZONE)
      continue;
    }
    awaiting_release_overlay_locks_.erase(iter);
  }
#else
  NOTREACHED();
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
}

bool SkiaRenderer::FlippedFramebuffer() const {
  // TODO(weiliangc): Make sure flipped correctly for Windows.
  // (crbug.com/644851)
  return false;
}

void SkiaRenderer::EnsureScissorTestEnabled() {
  is_scissor_enabled_ = true;
}

void SkiaRenderer::EnsureScissorTestDisabled() {
  is_scissor_enabled_ = false;
}

void SkiaRenderer::BindFramebufferToOutputSurface() {
  current_canvas_ = skia_output_surface_->BeginPaintCurrentFrame();
  if (debug_settings_->show_overdraw_feedback) {
    current_canvas_ = skia_output_surface_->RecordOverdrawForCurrentPaint();
  }
}

void SkiaRenderer::BindFramebufferToTexture(
    const AggregatedRenderPassId render_pass_id) {
  auto iter = render_pass_backings_.find(render_pass_id);
  DCHECK(render_pass_backings_.end() != iter);

  bool is_root = render_pass_id == current_frame()->root_render_pass->id;
  // This function is called after AllocateRenderPassResourceIfNeeded, so there
  // should be backing ready.
  RenderPassBacking& backing = iter->second;
  current_canvas_ = skia_output_surface_->BeginPaintRenderPass(
      render_pass_id, backing.size, backing.format, backing.generate_mipmap,
      RenderPassBackingSkColorSpace(backing), /*is_overlay=*/is_root,
      backing.mailbox);

  if (is_root && debug_settings_->show_overdraw_feedback) {
    DCHECK(buffer_queue_);
    current_canvas_ = skia_output_surface_->RecordOverdrawForCurrentPaint();
  }
}

void SkiaRenderer::SetScissorTestRect(const gfx::Rect& scissor_rect) {
  is_scissor_enabled_ = true;
  scissor_rect_ = scissor_rect;
}

void SkiaRenderer::ClearCanvas(SkColor4f color) {
  if (!current_canvas_)
    return;

  if (is_scissor_enabled_) {
    // Limit the clear with the scissor rect.
    SkAutoCanvasRestore autoRestore(current_canvas_, true /* do_save */);
    current_canvas_->clipRect(gfx::RectToSkRect(scissor_rect_));
    current_canvas_->clear(color);
  } else {
    current_canvas_->clear(color);
  }
}

void SkiaRenderer::ClearFramebuffer() {
  if (current_frame()->current_render_pass->has_transparent_background) {
    ClearCanvas(SkColors::kTransparent);
  } else {
#if DCHECK_IS_ON() && !BUILDFLAG(IS_LINUX)
    // On DEBUG builds, opaque render passes are cleared to blue
    // to easily see regions that were not drawn on the screen.
    // ClearCavas() call causes slight pixel difference, so linux-ref and
    // linux-blink-ref bots cannot share the same baseline for webtest.
    // So remove this ClearCanvas() call for dcheck on build for now.
    // TODO(crbug.com/1330278): add it back.
    ClearCanvas(SkColors::kBlue);
#endif
  }
}

void SkiaRenderer::PrepareSurfaceForPass(
    SurfaceInitializationMode initialization_mode,
    const gfx::Rect& render_pass_scissor) {
  switch (initialization_mode) {
    case SURFACE_INITIALIZATION_MODE_PRESERVE:
      EnsureScissorTestDisabled();
      return;
    case SURFACE_INITIALIZATION_MODE_FULL_SURFACE_CLEAR:
      EnsureScissorTestDisabled();
      ClearFramebuffer();
      break;
    case SURFACE_INITIALIZATION_MODE_SCISSORED_CLEAR:
      SetScissorTestRect(render_pass_scissor);
      ClearFramebuffer();
      break;
  }
}

void SkiaRenderer::DoDrawQuad(const DrawQuad* quad,
                              const gfx::QuadF* draw_region) {
  if (!current_canvas_)
    return;
  TRACE_EVENT0("viz", "SkiaRenderer::DoDrawQuad");
  const gfx::Rect* scissor = is_scissor_enabled_ ? &scissor_rect_ : nullptr;
  DrawQuadParams params = CalculateDrawQuadParams(
      current_frame()->target_to_device_transform, scissor, quad, draw_region);
  // The outer DrawQuad will never have RPDQ params
  DrawQuadInternal(quad, /* rpdq */ nullptr, &params);
}

void SkiaRenderer::DrawQuadInternal(const DrawQuad* quad,
                                    const DrawRPDQParams* rpdq_params,
                                    DrawQuadParams* params) {
  if (MustFlushBatchedQuads(quad, rpdq_params, *params)) {
    FlushBatchedQuads();
  }

  switch (quad->material) {
    case DrawQuad::Material::kAggregatedRenderPass:
      // RPDQ should only ever be encountered as a top-level quad, not when
      // bypassing another renderpass
      DCHECK(rpdq_params == nullptr);
      DrawRenderPassQuad(AggregatedRenderPassDrawQuad::MaterialCast(quad),
                         params);
      break;
    case DrawQuad::Material::kDebugBorder:
      // DebugBorders draw directly into the device space, so are not compatible
      // with image filters, so should never have been promoted as a bypass quad
      DCHECK(rpdq_params == nullptr);
      DrawDebugBorderQuad(DebugBorderDrawQuad::MaterialCast(quad), params);
      break;
    case DrawQuad::Material::kPictureContent:
      // PictureQuads represent a collection of drawn elements that are
      // dynamically rasterized by Skia; bypassing a RenderPass to redraw the
      // N elements doesn't make much sense.
      DCHECK(rpdq_params == nullptr);
      DrawPictureQuad(PictureDrawQuad::MaterialCast(quad), params);
      break;
    case DrawQuad::Material::kCompositorRenderPass:
      // RenderPassDrawQuads should be converted to
      // AggregatedRenderPassDrawQuads at this point.
      DrawUnsupportedQuad(quad, rpdq_params, params);
      NOTREACHED();
      break;
    case DrawQuad::Material::kSolidColor:
      DrawSolidColorQuad(SolidColorDrawQuad::MaterialCast(quad), rpdq_params,
                         params);
      break;
    case DrawQuad::Material::kTextureContent:
      DrawTextureQuad(TextureDrawQuad::MaterialCast(quad), rpdq_params, params);
      break;
    case DrawQuad::Material::kTiledContent:
      DrawTileDrawQuad(TileDrawQuad::MaterialCast(quad), rpdq_params, params);
      break;
    case DrawQuad::Material::kSharedElement:
      DrawUnsupportedQuad(quad, rpdq_params, params);
      NOTREACHED();
      break;
    case DrawQuad::Material::kYuvVideoContent:
      DrawYUVVideoQuad(YUVVideoDrawQuad::MaterialCast(quad), rpdq_params,
                       params);
      break;
    case DrawQuad::Material::kInvalid:
      DrawUnsupportedQuad(quad, rpdq_params, params);
      NOTREACHED();
      break;
    case DrawQuad::Material::kVideoHole:
      // VideoHoleDrawQuad should only be used by Cast, and should
      // have been replaced by cast-specific OverlayProcessor before
      // reach here. In non-cast build, an untrusted render could send such
      // Quad and the quad would then reach here unexpectedly. Therefore
      // we should skip NOTREACHED() so an untrusted render is not capable
      // of causing a crash.
      DrawUnsupportedQuad(quad, rpdq_params, params);
      break;
    default:
      // If we've reached here, it's a new quad type that needs a
      // dedicated implementation
      DrawUnsupportedQuad(quad, rpdq_params, params);
      NOTREACHED();
      break;
  }
}

void SkiaRenderer::PrepareCanvas(
    const absl::optional<gfx::Rect>& scissor_rect,
    const absl::optional<gfx::MaskFilterInfo>& mask_filter_info,
    const gfx::Transform* cdt) {
  // Scissor is applied in the device space (CTM == I) and since no changes
  // to the canvas persist, CTM should already be the identity
  DCHECK(current_canvas_->getTotalMatrix() == SkMatrix::I());

  if (scissor_rect.has_value()) {
    current_canvas_->clipRect(gfx::RectToSkRect(*scissor_rect));
  }

  if (mask_filter_info.has_value()) {
    current_canvas_->clipRRect(
        static_cast<SkRRect>(mask_filter_info->rounded_corner_bounds()),
        /*doAntiAlias=*/true);

    if (mask_filter_info->HasGradientMask())
      PrepareGradient(mask_filter_info);
  }

  if (cdt) {
    SkMatrix m = gfx::TransformToFlattenedSkMatrix(*cdt);
    current_canvas_->concat(m);
  }
}

#define MaskColor(a) SkColorSetARGB(a, a, a, a);

void SkiaRenderer::PrepareGradient(
    const absl::optional<gfx::MaskFilterInfo>& mask_filter_info) {
  if (!mask_filter_info || !mask_filter_info->HasGradientMask())
    return;

  const gfx::RectF rect = mask_filter_info->bounds();
  const absl::optional<gfx::LinearGradient>& gradient_mask =
      mask_filter_info->gradient_mask();

  int16_t angle = gradient_mask->angle() % 360;
  if (angle < 0) angle += 360;

  SkPoint start_end[2];

  float rad_angle = gfx::DegToRad(static_cast<float>(angle));
  float s = std::sin(rad_angle);
  float c = std::cos(rad_angle);

  if (angle % 180 > 90) {
    float start_x = rect.width() * c * c;
    float start_y = rect.height() - (rect.width() * s * c);
    float end_x = rect.height() * s * c;
    float end_y = rect.height() * c * c;

    if (angle < 180) {
      start_end[0] = {start_x, start_y};
      start_end[1] = {end_x, end_y};
    } else {
      start_end[0] = {end_x, end_y};
      start_end[1] = {start_x, start_y};
    }

  } else {
    float start_x = -rect.height() * s * c;
    float start_y = rect.height() * s * s;
    float end_x = rect.width() * c * c;
    float end_y = -rect.width() * s * c;

    if (angle < 180) {
      start_end[0] = {start_x, start_y};
      start_end[1] = {end_x, end_y};
    } else {
      start_end[0] = {end_x, end_y};
      start_end[1] = {start_x, start_y};
    }
  }

  SkScalar positions[gfx::LinearGradient::kMaxStepSize];
  SkColor gradient_colors[gfx::LinearGradient::kMaxStepSize];

  size_t i = 0;
  for (; i < gradient_mask->step_count(); ++i) {
    positions[i] = gradient_mask->steps()[i].fraction;
    gradient_colors[i] = MaskColor(gradient_mask->steps()[i].alpha);
  }

  SkPoint::Offset(start_end, /*count=*/2, rect.x(), rect.y());
  sk_sp<SkShader> gradient = SkGradientShader::MakeLinear(
      start_end, gradient_colors, positions, /*count=*/i, SkTileMode::kClamp);
  current_canvas_->clipShader(std::move(gradient));
}

void SkiaRenderer::PrepareCanvasForRPDQ(const DrawRPDQParams& rpdq_params,
                                        DrawQuadParams* params) {
  // Clip to the filter bounds prior to saving the layer, which has been
  // constructed to contain the actual filtered contents (visually no
  // clipping effect, but lets Skia minimize internal layer size).
  bool aa = params->aa_flags != SkCanvas::kNone_QuadAAFlags;
  current_canvas_->clipRect(gfx::RectToSkRect(rpdq_params.filter_bounds), aa);

  SkPaint layer_paint = params->paint(nullptr /* color_filter */);
  // The layer always consumes the opacity, but its blend mode depends on if
  // it was initialized with backdrop content or not.
  params->opacity = 1.f;
  if (rpdq_params.backdrop_filter) {
    layer_paint.setBlendMode(SkBlendMode::kSrcOver);
  } else {
    params->blend_mode = SkBlendMode::kSrcOver;
  }

  if (rpdq_params.color_filter) {
    layer_paint.setColorFilter(rpdq_params.color_filter);
  } else if (rpdq_params.image_filter) {
    layer_paint.setImageFilter(rpdq_params.image_filter);
  }

  // Canocalize the backdrop bounds rrect type; if there's no backdrop filter or
  // filter bounds, this will be empty. If it's a rect or rrect, we must work
  // around Skia's background filter auto-expansion. If it's an rrect, we must
  // also clear out the rounded corners after filtering.
  gfx::RRectF::Type backdrop_bounds_type = gfx::RRectF::Type::kEmpty;
  if (rpdq_params.backdrop_filter &&
      rpdq_params.backdrop_filter_bounds.has_value()) {
    backdrop_bounds_type = rpdq_params.backdrop_filter_bounds->GetType();
  }

  // Initially the backdrop filter fills the entire rect; if we draw less than
  // that we need to clear the excess.
  bool post_backdrop_filter_clear_needed = !!params->draw_region;

  // Explicitly crop the input and the output to the backdrop bounds; this is
  // required for the backdrop-filter spec.
  sk_sp<SkImageFilter> backdrop_filter = rpdq_params.backdrop_filter;
  if (backdrop_bounds_type != gfx::RRectF::Type::kEmpty) {
    DCHECK(backdrop_filter);

    gfx::RectF crop_rect = rpdq_params.backdrop_filter_bounds->rect();

    // Only sample from pixels behind the RPDQ for backdrop filters to avoid
    // color bleeding with pixel-moving filters.
    if (rpdq_params.bypass_geometry) {
      crop_rect.Intersect(rpdq_params.bypass_geometry->rect);
    } else {
      crop_rect.Intersect(params->rect);
    }
    SkIRect sk_crop_rect = gfx::RectToSkIRect(gfx::ToEnclosingRect(crop_rect));

    SkIRect sk_src_rect = backdrop_filter->filterBounds(
        sk_crop_rect, SkMatrix::I(), SkImageFilter::kReverse_MapDirection,
        &sk_crop_rect);
    if (sk_crop_rect == sk_src_rect) {
      // The backdrop filter does not "move" pixels, i.e. a pixel's value only
      // depends on its (x,y) and prior color. Avoid cropping the input in this
      // case since composing a crop rect into the filter DAG forces Skia to
      // map the backdrop content into the local space, which can introduce
      // filtering artifacts: crbug.com/1044032. Instead just post-filter
      // clearing will achieve the same cropping of the output at higher quality
      post_backdrop_filter_clear_needed = true;
    } else {
      // Offsetting (0,0) does nothing to the actual image, but is the most
      // convenient way to embed the crop rect into the filter DAG.
      // TODO(michaelludwig) - Remove this once Skia doesn't always auto-expand
      sk_sp<SkImageFilter> crop =
          SkImageFilters::Offset(0.0f, 0.0f, nullptr, &sk_crop_rect);
      backdrop_filter = SkImageFilters::Compose(
          crop, SkImageFilters::Compose(std::move(backdrop_filter), crop));
      // Update whether or not a post-filter clear is needed (crop didn't
      // completely match bounds)
      post_backdrop_filter_clear_needed |=
          backdrop_bounds_type != gfx::RRectF::Type::kRect ||
          crop_rect != rpdq_params.backdrop_filter_bounds->rect();
    }
  }

  SkRect bounds = gfx::RectFToSkRect(
      rpdq_params.bypass_geometry ? rpdq_params.bypass_geometry->visible_rect
                                  : params->visible_rect);
  current_canvas_->saveLayer(
      SkCanvas::SaveLayerRec(&bounds, &layer_paint, backdrop_filter.get(), 0));

  // If we have backdrop filtered content (and not transparent black like with
  // regular render passes), we have to clear out the parts of the layer that
  // shouldn't show the backdrop
  if (backdrop_filter && post_backdrop_filter_clear_needed) {
    current_canvas_->save();
    if (rpdq_params.backdrop_filter_bounds.has_value()) {
      current_canvas_->clipRRect(SkRRect(*rpdq_params.backdrop_filter_bounds),
                                 SkClipOp::kDifference, aa);
    }
    if (params->draw_region) {
      SkPath clip_path = params->draw_region_in_path();
      if (rpdq_params.bypass_geometry) {
        clip_path.transform(rpdq_params.bypass_geometry->transform);
      }
      current_canvas_->clipPath(clip_path, SkClipOp::kDifference, aa);
    }
    current_canvas_->clear(SK_ColorTRANSPARENT);
    current_canvas_->restore();
  }
}

void SkiaRenderer::PreparePaintOrCanvasForRPDQ(
    const DrawRPDQParams& rpdq_params,
    DrawQuadParams* params,
    SkPaint* paint) {
  // When the draw call accepts an SkPaint, some of the rpdq effects are more
  // efficient to store on the paint instead of making an explicit layer in
  // the canvas. But there are several requirements in order for the order of
  // operations to be consistent with what RenderPasses require:
  // 1. Backdrop filtering always requires a layer.
  // 2. The content bypassing the renderpass needs to be clipped before the
  //    image filter is evaluated.
  bool needs_bypass_clip = rpdq_params.needs_bypass_clip(params->visible_rect);
  bool needs_save_layer = false;
  if (rpdq_params.backdrop_filter)
    needs_save_layer = true;
  else if (rpdq_params.has_complex_image_filter())
    needs_save_layer = needs_bypass_clip;

  if (rpdq_params.mask_shader) {
    // Apply the mask image using clipShader(), this works the same regardless
    // of if we need a saveLayer for image filtering since the clip is applied
    // at the end automatically.
    current_canvas_->clipShader(rpdq_params.mask_shader);
  }

  if (needs_save_layer) {
    PrepareCanvasForRPDQ(rpdq_params, params);
    // Sync the content's paint with the updated |params|
    paint->setAlphaf(params->opacity);
    paint->setBlendMode(params->blend_mode);
  } else {
    // At this point, the image filter and/or color filter are on the paint.
    DCHECK(!rpdq_params.backdrop_filter);
    if (rpdq_params.color_filter) {
      // Use the color filter directly, instead of the image filter.
      if (paint->getColorFilter()) {
        paint->setColorFilter(
            rpdq_params.color_filter->makeComposed(paint->refColorFilter()));
      } else {
        paint->setColorFilter(rpdq_params.color_filter);
      }
      DCHECK(paint->getColorFilter());
    } else if (rpdq_params.image_filter) {
      // Store the image filter on the paint.
      if (params->opacity != 1.f) {
        // Apply opacity as the last step of image filter so it is uniform
        // across any overlapping content produced by the image filters.
        paint->setImageFilter(SkImageFilters::ColorFilter(
            MakeOpacityFilter(params->opacity, nullptr),
            rpdq_params.image_filter));
        paint->setAlphaf(1.f);
        params->opacity = 1.f;
      } else {
        paint->setImageFilter(rpdq_params.image_filter);
      }
    }
  }

  // Whether or not we saved a layer, clip the bypassed RenderPass's content
  if (needs_bypass_clip) {
    current_canvas_->clipRect(
        gfx::RectFToSkRect(rpdq_params.bypass_geometry->visible_rect),
        params->aa_flags != SkCanvas::kNone_QuadAAFlags);
  }
}

void SkiaRenderer::PrepareColorOrCanvasForRPDQ(
    const DrawRPDQParams& rpdq_params,
    DrawQuadParams* params,
    SkColor4f* content_color) {
  // When the draw call only takes a color and not an SkPaint, rpdq params
  // with just a color filter can be handled directly. Otherwise, the rpdq
  // params must use a layer on the canvas.
  bool needs_save_layer =
      rpdq_params.has_complex_image_filter() || rpdq_params.backdrop_filter;
  if (rpdq_params.mask_shader) {
    current_canvas_->clipShader(rpdq_params.mask_shader);
  }

  if (needs_save_layer) {
    PrepareCanvasForRPDQ(rpdq_params, params);
  } else if (rpdq_params.color_filter) {
    // At this point, the RPDQ effect is at most a color filter, so it can
    // modify |content_color| directly.
    SkColorSpace* cs = nullptr;
    *content_color =
        rpdq_params.color_filter->filterColor4f(*content_color, cs, cs);
  }

  // Even if the color filter image filter was applied to the content color
  // directly (so no explicit save layer), the draw may need to be clipped to
  // the output rect of the renderpass it is bypassing.
  if (rpdq_params.needs_bypass_clip(params->visible_rect)) {
    current_canvas_->clipRect(
        gfx::RectFToSkRect(rpdq_params.bypass_geometry->visible_rect),
        params->aa_flags != SkCanvas::kNone_QuadAAFlags);
  }
}

SkiaRenderer::DrawQuadParams SkiaRenderer::CalculateDrawQuadParams(
    const gfx::AxisTransform2d& target_to_device,
    const gfx::Rect* scissor_rect,
    const DrawQuad* quad,
    const gfx::QuadF* draw_region) const {
  DrawQuadParams params(
      quad->shared_quad_state->quad_to_target_transform, gfx::RectF(quad->rect),
      gfx::RectF(quad->visible_rect), SkCanvas::kNone_QuadAAFlags,
      quad->shared_quad_state->blend_mode, quad->shared_quad_state->opacity,
      GetSampling(quad), draw_region);

  params.content_device_transform.PostConcat(target_to_device);
  params.content_device_transform.Flatten();

  // Respect per-quad setting overrides as highest priority setting
  if (!IsAAForcedOff(quad)) {
    if (settings_->force_antialiasing) {
      // This setting makes the entire draw AA, so don't bother checking edges
      params.aa_flags = SkCanvas::kAll_QuadAAFlags;
    } else if (settings_->allow_antialiasing) {
      params.aa_flags = GetRectilinearEdgeFlags(quad);
      if (draw_region && params.aa_flags != SkCanvas::kNone_QuadAAFlags) {
        // Turn off interior edges' AA from the BSP splitting
        GetClippedEdgeFlags(quad, &params.aa_flags, &*params.draw_region);
      }
    }
  }

  if (!quad->ShouldDrawWithBlending()) {
    // The quad layer is src-over with 1.0 opacity and its needs_blending flag
    // has been set to false. However, even if the layer's opacity is 1.0, the
    // contents may not be (e.g. png or a color with alpha).
    if (quad->shared_quad_state->are_contents_opaque) {
      // Visually, this is the same as kSrc but Skia is faster with SrcOver
      params.blend_mode = SkBlendMode::kSrcOver;
    } else {
      // Replaces dst contents with the new color (e.g. no blending); this is
      // just as fast as srcover when there's no AA, but is slow when coverage
      // must be taken into account.
      params.blend_mode = SkBlendMode::kSrc;
    }
    params.opacity = 1.f;
  }

  params.ApplyScissor(this, quad, scissor_rect);

  // Determine final rounded rect clip geometry. We transform it from target
  // space to window space to make batching and canvas preparation easier
  // (otherwise we'd have to separate those two matrices in the CDT).
  if (ShouldApplyRoundedCorner(quad) || ShouldApplyGradientMask(quad)) {
    params.mask_filter_info.emplace(quad->shared_quad_state->mask_filter_info);
    // Transform by the window and projection matrix to go from target to
    // device space, which should always be a scale+translate.
    params.mask_filter_info->ApplyTransform(target_to_device);
  }

  return params;
}

void SkiaRenderer::DrawQuadParams::ApplyScissor(
    const SkiaRenderer* renderer,
    const DrawQuad* quad,
    const gfx::Rect* scissor_to_apply) {
  // No scissor should have been set before calling ApplyScissor
  DCHECK(!scissor_rect.has_value());

  if (!scissor_to_apply) {
    // No scissor at all, which matches the DCHECK'ed state above
    return;
  }

  // Assume at start that the scissor will be applied through the canvas clip,
  // so that this can simply return when it detects the scissor cannot be
  // applied explicitly to |visible_rect|.
  scissor_rect = *scissor_to_apply;

  // PICTURE_CONTENT is not like the others, since it is executing a list of
  // draw calls into the canvas.
  if (quad->material == DrawQuad::Material::kPictureContent)
    return;

  // DebugBorderDrawQuads draw a path so they must be explicitly clipped.
  if (quad->material == DrawQuad::Material::kDebugBorder)
    return;

  // Intersection with scissor and a quadrilateral is not necessarily a quad,
  // so don't complicate things
  if (draw_region.has_value())
    return;

  // This is slightly different than
  // gfx::Transform::IsPositiveScaleAndTranslation in that it also allows zero
  // scales. This is because in the common orthographic case the z scale is 0.
  if (!content_device_transform.IsScaleOrTranslation() ||
      content_device_transform.rc(0, 0) < 0.0f ||
      content_device_transform.rc(1, 1) < 0.0f ||
      content_device_transform.rc(2, 2) < 0.0f) {
    return;
  }

  // State check: should not have a CompositorRenderPassDrawQuad if we got here.
  DCHECK_NE(quad->material, DrawQuad::Material::kCompositorRenderPass);
  if (quad->material == DrawQuad::Material::kAggregatedRenderPass) {
    // If the renderpass has filters, the filters may modify the effective
    // geometry beyond the quad's visible_rect, so it's not safe to pre-clip.
    auto pass_id =
        AggregatedRenderPassDrawQuad::MaterialCast(quad)->render_pass_id;
    if (renderer->FiltersForPass(pass_id) ||
        renderer->BackdropFiltersForPass(pass_id))
      return;
  }

  // If the intersection of the scissor and the quad's visible_rect results in
  // subpixel device-space geometry, do not drop the scissor. Otherwise Skia
  // sees an unclipped anti-aliased hairline and uses different AA methods that
  // would cause the rasterized result to extend beyond the scissor.
  gfx::RectF device_bounds = content_device_transform.MapRect(visible_rect);
  device_bounds.Intersect(gfx::RectF(*scissor_rect));
  if (device_bounds.width() < 1.0f || device_bounds.height() < 1.0f) {
    return;
  }

  // The explicit scissor is applied in the quad's local space. If the transform
  // does not leave sufficient precision to round-trip the scissor rect to-from
  // device->local->device space, the explicitly "clipped" geometry does not
  // necessarily respect the original scissor.
  absl::optional<gfx::RectF> local_scissor =
      content_device_transform.InverseMapRect(gfx::RectF(*scissor_rect));
  if (!local_scissor) {
    return;
  }
  gfx::RectF remapped_scissor =
      content_device_transform.MapRect(*local_scissor);
  if (gfx::ToRoundedRect(remapped_scissor) != *scissor_rect) {
    return;
  }

  // At this point, we've determined that we can transform the scissor rect into
  // the quad's local space and adjust |vis_rect|, such that when it's mapped to
  // device space, it will be contained in in the original scissor.
  // Applying the scissor explicitly means avoiding a clipRect() call and
  // allows more quads to be batched together in a DrawEdgeAAImageSet call
  float x_epsilon = kAAEpsilon / content_device_transform.rc(0, 0);
  float y_epsilon = kAAEpsilon / content_device_transform.rc(1, 1);

  // The scissor is a non-AA clip, so unset the bit flag for clipped edges.
  if (local_scissor->x() - visible_rect.x() >= x_epsilon)
    aa_flags &= ~SkCanvas::kLeft_QuadAAFlag;
  if (local_scissor->y() - visible_rect.y() >= y_epsilon)
    aa_flags &= ~SkCanvas::kTop_QuadAAFlag;
  if (visible_rect.right() - local_scissor->right() >= x_epsilon)
    aa_flags &= ~SkCanvas::kRight_QuadAAFlag;
  if (visible_rect.bottom() - local_scissor->bottom() >= y_epsilon)
    aa_flags &= ~SkCanvas::kBottom_QuadAAFlag;

  visible_rect.Intersect(*local_scissor);
  vis_tex_coords = visible_rect;
  scissor_rect.reset();
}

const DrawQuad* SkiaRenderer::CanPassBeDrawnDirectly(
    const AggregatedRenderPass* pass) {
  bool is_directly_drawable_with_single_rpdq = false;
  const auto* draw_quad = CanPassBeDrawnDirectlyInternal(
      pass, &is_directly_drawable_with_single_rpdq);
  UMA_HISTOGRAM_BOOLEAN(
      "Compositing.SkiaRenderer.DirectlyDrawableRenderPassWithRPDQ",
      is_directly_drawable_with_single_rpdq);
  return draw_quad;
}

const DrawQuad* SkiaRenderer::CanPassBeDrawnDirectlyInternal(
    const AggregatedRenderPass* pass,
    bool* is_directly_drawable_with_single_rpdq) {
  // If render pass bypassing is disabled for testing
  if (settings_->disable_render_pass_bypassing)
    return nullptr;

  // TODO(michaelludwig) - For now, this only supports opaque, src-over quads
  // with invertible transforms and simple content (image or color only).
  // Can only collapse a single tile quad.
  if (pass->quad_list.size() != 1)
    return nullptr;

  // If it there are supposed to be mipmaps, the renderpass must exist
  if (pass->generate_mipmap)
    return nullptr;

  const DrawQuad* quad = *pass->quad_list.BackToFrontBegin();
  // For simplicity in their draw implementations, debug borders, picture quads,
  // and nested render passes cannot bypass a render pass
  // (their draw functions do not accept DrawRPDQParams either).
  // Note: The check for RPDQ is at the end of the function to log whether other
  // vetoes would prevent optimizing this case.
  DCHECK_NE(quad->material, DrawQuad::Material::kCompositorRenderPass);
  if (quad->material == DrawQuad::Material::kDebugBorder ||
      quad->material == DrawQuad::Material::kPictureContent)
    return nullptr;

  // TODO(penghuang): support composite TileDrawQuad in a sub render pass for
  // raw draw directly.
  if (is_using_raw_draw_ && quad->material == DrawQuad::Material::kTiledContent)
    return nullptr;

  // If the quad specifies nearest-neighbor scaling then there could be two
  // scaling operations at different quality levels. This requires drawing to an
  // intermediate render pass. See https://crbug.com/1155338.
  if (UseNearestNeighborSampling(quad))
    return nullptr;

  if (quad->material == DrawQuad::Material::kTextureContent) {
    // Per-vertex opacities complicate bypassing the RP and alpha blending the
    // texture with image filters, so punt under that rare circumstance.
    const TextureDrawQuad* tex = TextureDrawQuad::MaterialCast(quad);
    if (tex->vertex_opacity[0] < 1.f || tex->vertex_opacity[1] < 1.f ||
        tex->vertex_opacity[2] < 1.f || tex->vertex_opacity[3] < 1.f) {
      return nullptr;
    }
  }

  // In order to concatenate the bypass'ed quads transform with RP itself, it
  // needs to be invertible.
  // TODO(michaelludwig) - See crbug.com/1175981 and crbug.com/1186657;
  // We can't use gfx::Transform.IsInvertible() since that checks the 4x4 matrix
  // and the rest of skia_renderer->Skia flattens to a 3x3 matrix, which can
  // change invertibility.
  SkMatrix flattened = gfx::TransformToFlattenedSkMatrix(
      quad->shared_quad_state->quad_to_target_transform);
  if (!flattened.invert(nullptr))
    return nullptr;

  // A renderpass normally draws its content into a transparent destination,
  // using the quad's blend mode, then that result is later drawn into the
  // real dst with the RP's blend mode. In order to bypass the RP and draw
  // correctly, CalculateBypassParams must be able to reason about the quad's
  // blend mode.
  if (!IsPorterDuffBlendMode(quad->shared_quad_state->blend_mode))
    return nullptr;
  // All Porter-Duff blending with transparent black should fall into one of
  // these two categories:
  DCHECK(RenderPassPreservesContent(quad->shared_quad_state->blend_mode) ||
         RenderPassRemainsTransparent(quad->shared_quad_state->blend_mode));

  // The content must not have any rrect clipping, since skia_renderer applies
  // the rrect in device space, and in this case, the bypass quad's device space
  // is the RP's buffer.
  // TODO(michaelludwig) - If this becomes a bottleneck, we can track the
  // bypass rrect separately and update PrepareCanvasForRDQP to apply the
  // additional clip.
  if (ShouldApplyRoundedCorner(quad))
    return nullptr;

  if (ShouldApplyGradientMask(quad))
    return nullptr;

  if (quad->material == DrawQuad::Material::kAggregatedRenderPass) {
    const auto* render_pass_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(quad);
    if (render_pass_quad->mask_resource_id())
      return nullptr;

    const auto nested_render_pass_id = render_pass_quad->render_pass_id;
    auto it = base::ranges::find_if(
        *current_frame()->render_passes_in_draw_order,
        [&nested_render_pass_id](const auto& render_pass) {
          return render_pass->id == nested_render_pass_id;
        });

    DCHECK(it != current_frame()->render_passes_in_draw_order->end());
    const auto& nested_render_pass = *it;
    if (nested_render_pass->filters.IsEmpty() &&
        nested_render_pass->backdrop_filters.IsEmpty()) {
      *is_directly_drawable_with_single_rpdq = true;
    }
    return nullptr;
  }

  // The quad type knows how to apply RPDQ filters, and the quad settings can
  // be merged into the RPDQs settings in CalculateBypassParams.
  return quad;
}

SkiaRenderer::BypassMode SkiaRenderer::CalculateBypassParams(
    const DrawQuad* bypass_quad,
    DrawRPDQParams* rpdq_params,
    DrawQuadParams* params) const {
  // Depending on bypass_quad's blend mode, its content may be irrelevant
  if (RenderPassRemainsTransparent(
          bypass_quad->shared_quad_state->blend_mode)) {
    // NOTE: this uses the pass's blend mode since this refers to the final draw
    // of the render pass itself
    if (TransparentBlackAffectsOutput(params->blend_mode)) {
      return BypassMode::kDrawTransparentQuad;
    } else {
      return BypassMode::kSkip;
    }
  }
  // If we made it here, the bypass blend mode would have just preserved the
  // bypass quad's content, so we can draw it directly using the render pass's
  // blend mode instead.
  DCHECK(
      RenderPassPreservesContent(bypass_quad->shared_quad_state->blend_mode));

  // The bypass quad will be drawn directly, so update |params| and
  // |rpdq_params| to reflect the change of coordinate system and merge settings
  // between the inner and outer quads.
  SkMatrix rpdq_to_bypass;
  SkMatrix bypass_to_rpdq = gfx::TransformToFlattenedSkMatrix(
      bypass_quad->shared_quad_state->quad_to_target_transform);
  bool inverted = bypass_to_rpdq.invert(&rpdq_to_bypass);
  // Invertibility was a requirement for being bypassable.
  DCHECK(inverted);

  if (params->draw_region) {
    // The draw region was determined by the RPDQ's geometry, so map the
    // quadrilateral to the bypass'ed quad's coordinate space so that BSP
    // splitting is still respected.
    rpdq_to_bypass.mapPoints(params->draw_region->points,
                             std::size(params->draw_region->points));
  }

  // Compute draw params for the bypass quad from scratch, but since the
  // bypass_quad would have originally been drawn into the RP, the
  // target_to_device transform is the full transform of the RPDQ. Must also
  // include the RP's output rect as part of the scissor rect, since it would
  // have been clipped to the edges of the RP's offscreen buffer normally.
  DrawQuadParams bypass_params = CalculateDrawQuadParams(
      gfx::AxisTransform2d() /* identity */, nullptr, bypass_quad, nullptr);

  // |params| already holds the correct |draw_region|, but must be updated to
  // use the bypassed transform and geometry.
  rpdq_params->bypass_geometry = DrawRPDQParams::BypassGeometry{
      bypass_to_rpdq, params->rect, params->visible_rect};

  // NOTE: params |content_device_transform| remains that of the RPDQ to prepare
  // the canvas' CTM to match what any image filters require. The above
  // BypassGeometry::transform is then applied when drawing so that these
  // updated coordinates are correctly transformed to device space.
  params->visible_rect = bypass_params.visible_rect;
  params->vis_tex_coords = bypass_params.vis_tex_coords;

  // Combine anti-aliasing policy (use AND so that any draw_region clipping
  // is preserved).
  params->aa_flags &= bypass_params.aa_flags;

  // Blending will use the top-level RPDQ blend mode, but factor in the
  // content's opacity as well, since that would have normally been baked into
  // the RP's buffer.
  params->opacity *= bypass_params.opacity;

  // Take the highest quality filter, since this single draw will reflect the
  // filtering decisions made both when drawing into the RP and when drawing the
  // RP results itself. The ord() lambda simulates this notion of "highest" when
  // we used to use FilterQuality.
  auto ord = [](const SkSamplingOptions& sampling) {
    if (sampling.useCubic)
      return 3;
    if (sampling.mipmap != SkMipmapMode::kNone)
      return 2;
    return sampling.filter == SkFilterMode::kLinear ? 1 : 0;
  };

  if (ord(bypass_params.sampling) > ord(params->sampling))
    params->sampling = bypass_params.sampling;

  // Rounded corner bounds are in device space, which gets tricky when bypassing
  // the device that the RP would have represented
  DCHECK(!bypass_params.mask_filter_info.has_value());

  return BypassMode::kDrawBypassQuad;
}

SkCanvas::ImageSetEntry SkiaRenderer::MakeEntry(
    const SkImage* image,
    int matrix_index,
    const DrawQuadParams& params) const {
  return SkCanvas::ImageSetEntry(
      {sk_ref_sp(image), gfx::RectFToSkRect(params.vis_tex_coords),
       gfx::RectFToSkRect(params.visible_rect), matrix_index, params.opacity,
       params.aa_flags, params.draw_region.has_value()});
}

SkCanvas::SrcRectConstraint SkiaRenderer::ResolveTextureConstraints(
    const SkImage* image,
    const gfx::RectF& valid_texel_bounds,
    DrawQuadParams* params) const {
  if (params->aa_flags == SkCanvas::kNone_QuadAAFlags &&
      params->sampling == SkSamplingOptions()) {
    // Non-AA and no bilinear filtering so rendering won't filter outside the
    // provided texture coordinates.
    return SkCanvas::kFast_SrcRectConstraint;
  }

  // Resolve texture coordinates against the valid content area of the image
  SkCanvas::SrcRectConstraint constraint =
      GetTextureConstraint(image, params->vis_tex_coords, valid_texel_bounds);

  // Skia clamps to the provided texture coordinates, not the content_area. If
  // there is a mismatch, have to update the draw params to account for the new
  // constraint
  if (constraint == SkCanvas::kFast_SrcRectConstraint ||
      valid_texel_bounds == params->vis_tex_coords) {
    return constraint;
  }

  // To get |valid_texel_bounds| as the constraint, it must be sent as the tex
  // coords. To draw the right shape, store |visible_rect| as the |draw_region|
  // and change the visible rect so that the mapping from |visible_rect| to
  // |valid_texel_bounds| causes |draw_region| to map to original
  // |vis_tex_coords|
  if (!params->draw_region) {
    params->draw_region.emplace(gfx::QuadF(params->visible_rect));
  }

  // Preserve the src-to-dst transformation for the padded texture coords
  SkMatrix src_to_dst =
      SkMatrix::RectToRect(gfx::RectFToSkRect(params->vis_tex_coords),
                           gfx::RectFToSkRect(params->visible_rect));
  params->visible_rect = gfx::SkRectToRectF(
      src_to_dst.mapRect(gfx::RectFToSkRect(valid_texel_bounds)));
  params->vis_tex_coords = valid_texel_bounds;

  return SkCanvas::kStrict_SrcRectConstraint;
}

bool SkiaRenderer::MustFlushBatchedQuads(const DrawQuad* new_quad,
                                         const DrawRPDQParams* rpdq_params,
                                         const DrawQuadParams& params) const {
  if (batched_quads_.empty())
    return false;

  // If |new_quad| is the bypass quad for a renderpass with filters, it must be
  // drawn by itself, regardless of if it could otherwise would've been batched.
  if (rpdq_params)
    return true;

  DCHECK_NE(new_quad->material, DrawQuad::Material::kCompositorRenderPass);
  if (new_quad->material != DrawQuad::Material::kAggregatedRenderPass &&
      new_quad->material != DrawQuad::Material::kTextureContent &&
      new_quad->material != DrawQuad::Material::kTiledContent)
    return true;

  if (batched_quad_state_.blend_mode != params.blend_mode ||
      batched_quad_state_.sampling != params.sampling)
    return true;

  if (batched_quad_state_.scissor_rect != params.scissor_rect) {
    return true;
  }

  if (batched_quad_state_.mask_filter_info != params.mask_filter_info) {
    return true;
  }

  return false;
}

void SkiaRenderer::AddQuadToBatch(const SkImage* image,
                                  const gfx::RectF& valid_texel_bounds,
                                  DrawQuadParams* params) {
  SkCanvas::SrcRectConstraint constraint =
      ResolveTextureConstraints(image, valid_texel_bounds, params);
  // Last check for flushing the batch, since constraint can't be known until
  // the last minute.
  if (!batched_quads_.empty() && batched_quad_state_.constraint != constraint) {
    FlushBatchedQuads();
  }

  // Configure batch state if it's the first
  if (batched_quads_.empty()) {
    batched_quad_state_.scissor_rect = params->scissor_rect;
    batched_quad_state_.mask_filter_info = params->mask_filter_info;
    batched_quad_state_.blend_mode = params->blend_mode;
    batched_quad_state_.sampling = params->sampling;
    batched_quad_state_.constraint = constraint;
  }
  DCHECK(batched_quad_state_.constraint == constraint);

  // Add entry, with optional clip quad and shared transform
  if (params->draw_region) {
    for (const auto& point : params->draw_region->points) {
      batched_draw_regions_.push_back(point);
    }
  }

  SkMatrix m =
      gfx::TransformToFlattenedSkMatrix(params->content_device_transform);
  std::vector<SkMatrix>& cdts = batched_cdt_matrices_;
  if (cdts.empty() || cdts[cdts.size() - 1] != m) {
    cdts.push_back(m);
  }
  int matrix_index = cdts.size() - 1;

  batched_quads_.push_back(MakeEntry(image, matrix_index, *params));
}

void SkiaRenderer::FlushBatchedQuads() {
  TRACE_EVENT0("viz", "SkiaRenderer::FlushBatchedQuads");

  SkAutoCanvasRestore acr(current_canvas_, true /* do_save */);
  PrepareCanvas(batched_quad_state_.scissor_rect,
                batched_quad_state_.mask_filter_info, nullptr);

  SkPaint paint;
  sk_sp<SkColorFilter> color_filter = GetContentColorFilter();
  if (color_filter)
    paint.setColorFilter(color_filter);
  paint.setBlendMode(batched_quad_state_.blend_mode);

  current_canvas_->experimental_DrawEdgeAAImageSet(
      &batched_quads_.front(), batched_quads_.size(),
      batched_draw_regions_.data(), &batched_cdt_matrices_.front(),
      batched_quad_state_.sampling, &paint, batched_quad_state_.constraint);

  batched_quads_.clear();
  batched_draw_regions_.clear();
  batched_cdt_matrices_.clear();
}

void SkiaRenderer::DrawColoredQuad(SkColor4f color,
                                   const DrawRPDQParams* rpdq_params,
                                   DrawQuadParams* params) {
  TRACE_EVENT0("viz", "SkiaRenderer::DrawColoredQuad");
  DCHECK(batched_quads_.empty());

  SkAutoCanvasRestore acr(current_canvas_, true /* do_save */);
  PrepareCanvas(params->scissor_rect, params->mask_filter_info,
                &params->content_device_transform);

  if (rpdq_params) {
    // This will modify the provided content color as needed for the RP effects,
    // or it will make an explicit save layer on the current canvas
    PrepareColorOrCanvasForRPDQ(*rpdq_params, params, &color);
    if (rpdq_params->bypass_geometry) {
      // Concatenate the bypass'ed quad's transform after all the RPDQ state
      // has been pushed to the canvas.
      current_canvas_->concat(rpdq_params->bypass_geometry->transform);
    }
  }

  sk_sp<SkColorFilter> content_color_filter = GetContentColorFilter();
  if (content_color_filter) {
    SkColorSpace* color_space = current_canvas_->imageInfo().colorSpace();
    color = content_color_filter->filterColor4f(
        color, SkColorSpace::MakeSRGB().get(), color_space);
    // DrawEdgeAAQuad lacks color filter support via SkPaint, so we apply the
    // color filter to the quad color directly. When applying a color filter
    // via drawRect or drawImage, Skia will first transform the src into the
    // dst color space before applying the color filter. Thus, here we need to
    // apply the color filter in the dst color space and then convert back to
    // the src color space. More formally:
    //    (C * Xfrm * CF * Xfrm_inv)[in Viz] * Xfrm[in Skia] = C * Xfrm * CF
    if (color_space && !color_space->isSRGB()) {
      SkPaint paint;
      paint.setColor(color, color_space);
      color = paint.getColor4f();
    }
  }
  // PrepareCanvasForRPDQ will have updated params->opacity and blend_mode to
  // account for the layer applying those effects. We need to truncate to an
  // integral value of [0, 255] to match the explicit floor workaround in
  // blink::ConversionContext::StartEffect.
  color.fA = floor(params->opacity * color.fA * 255.f) / 255.f;

  const SkPoint* draw_region =
      params->draw_region ? params->draw_region->points : nullptr;

  current_canvas_->experimental_DrawEdgeAAQuad(
      gfx::RectFToSkRect(params->visible_rect), draw_region,
      static_cast<SkCanvas::QuadAAFlags>(params->aa_flags), color,
      params->blend_mode);
}

void SkiaRenderer::DrawSingleImage(const SkImage* image,
                                   const gfx::RectF& valid_texel_bounds,
                                   const DrawRPDQParams* rpdq_params,
                                   SkPaint* paint,
                                   DrawQuadParams* params) {
  DCHECK(batched_quads_.empty());
  TRACE_EVENT0("viz", "SkiaRenderer::DrawSingleImage");

  SkAutoCanvasRestore acr(current_canvas_, true /* do_save */);
  PrepareCanvas(params->scissor_rect, params->mask_filter_info,
                &params->content_device_transform);

  int matrix_index = -1;
  const SkMatrix* bypass_transform = nullptr;
  if (rpdq_params) {
    // This will modify the provided content paint as needed for the RP effects,
    // or it will make an explicit save layer on the current canvas
    PreparePaintOrCanvasForRPDQ(*rpdq_params, params, paint);
    if (rpdq_params->bypass_geometry) {
      // Incorporate the bypass transform, but unlike for solid color quads, do
      // not modify the SkCanvas's CTM. This is because the RPDQ's filters may
      // have been optimally placed on the SkPaint of the draw, which means the
      // canvas' transform must match that of the RenderPass. The pre-CTM matrix
      // of the image set entry can be used instead to modify the drawn geometry
      // without impacting the filter's coordinate space.
      bypass_transform = &rpdq_params->bypass_geometry->transform;
      matrix_index = 0;
    }
  }

  // At this point, the params' opacity should be handled by |paint| (either
  // as its alpha or in a color filter), or in an image filter from the RPDQ,
  // or in the saved layer for the RPDQ. Set opacity to 1 to ensure the image
  // set entry does not doubly-apply the opacity then.
  params->opacity = 1.f;

  SkCanvas::SrcRectConstraint constraint =
      ResolveTextureConstraints(image, valid_texel_bounds, params);

  // Use -1 for matrix index since the cdt is set on the canvas.
  SkCanvas::ImageSetEntry entry = MakeEntry(image, matrix_index, *params);
  const SkPoint* draw_region =
      params->draw_region ? params->draw_region->points : nullptr;
  current_canvas_->experimental_DrawEdgeAAImageSet(
      &entry, 1, draw_region, bypass_transform, params->sampling, paint,
      constraint);
}

void SkiaRenderer::DrawPaintOpBuffer(
    const cc::PaintOpBuffer* buffer,
    const absl::optional<SkColor4f>& clear_color,
    const TileDrawQuad* quad,
    const DrawQuadParams* params) {
  TRACE_EVENT0("viz", "SkiaRenderer::DrawPaintOpBuffer");
  if (!batched_quads_.empty())
    FlushBatchedQuads();

  SkAutoCanvasRestore auto_canvas_restore(current_canvas_, true /* do_save */);
  PrepareCanvas(params->scissor_rect, params->mask_filter_info,
                &params->content_device_transform);

  auto visible_rect = gfx::RectFToSkRect(params->visible_rect);
  current_canvas_->clipRect(visible_rect);

  if (params->draw_region) {
    bool aa = params->aa_flags != SkCanvas::kNone_QuadAAFlags;
    current_canvas_->clipPath(params->draw_region_in_path(), aa);
  }

  if (quad->ShouldDrawWithBlending()) {
    auto paint = params->paint(nullptr);
    // TODO(penghuang): saveLayer() is expensive, try to avoid it as much as
    // possible.
    current_canvas_->saveLayer(&visible_rect, &paint);
  }

  if (clear_color)
    current_canvas_->drawColor(*clear_color);

  float scale_x = params->rect.width() / quad->tex_coord_rect.width();
  float scale_y = params->rect.height() / quad->tex_coord_rect.height();

  float offset_x =
      params->visible_rect.x() - params->vis_tex_coords.x() * scale_x;
  float offset_y =
      params->visible_rect.y() - params->vis_tex_coords.y() * scale_y;

  current_canvas_->translate(offset_x, offset_y);
  current_canvas_->scale(scale_x, scale_y);

  cc::PlaybackParams playback_params(nullptr, SkM44());
  buffer->Playback(current_canvas_, playback_params);
}

void SkiaRenderer::DrawDebugBorderQuad(const DebugBorderDrawQuad* quad,
                                       DrawQuadParams* params) {
  TRACE_EVENT0("viz", "SkiaRenderer::DrawDebugBorderQuad");
  DCHECK(batched_quads_.empty());

  SkAutoCanvasRestore acr(current_canvas_, true /* do_save */);
  // We need to apply the matrix manually to have pixel-sized stroke width.
  PrepareCanvas(params->scissor_rect, params->mask_filter_info, nullptr);
  SkMatrix cdt =
      gfx::TransformToFlattenedSkMatrix(params->content_device_transform);

  SkPath path = params->draw_region
                    ? params->draw_region_in_path()
                    : SkPath::Rect(gfx::RectFToSkRect(params->visible_rect));
  path.transform(cdt);

  SkPaint paint = params->paint(nullptr /* color_filter */);
  paint.setColor(quad->color);  // Must correct alpha afterwards
  paint.setAlphaf(params->opacity * paint.getAlphaf());
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeJoin(SkPaint::kMiter_Join);
  paint.setStrokeWidth(quad->width);

  current_canvas_->drawPath(path, paint);
}

void SkiaRenderer::DrawPictureQuad(const PictureDrawQuad* quad,
                                   DrawQuadParams* params) {
  DCHECK(batched_quads_.empty());
  TRACE_EVENT0("viz", "SkiaRenderer::DrawPictureQuad");

  // If the layer is transparent or needs a non-SrcOver blend mode, saveLayer
  // must be used so that the display list is drawn into a transient image and
  // then blended as a single layer at the end.
  const bool needs_transparency =
      params->opacity < 1.f || params->blend_mode != SkBlendMode::kSrcOver;
  const bool disable_image_filtering = disable_picture_quad_image_filtering_ ||
                                       params->sampling == SkSamplingOptions();

  SkAutoCanvasRestore acr(current_canvas_, true /* do_save */);
  PrepareCanvas(params->scissor_rect,
                params->mask_filter_info,
                &params->content_device_transform);

  // Unlike other quads which draw visible_rect or draw_region as their geometry
  // these represent the valid windows of content to show for the display list,
  // so they need to be used as a clip in Skia.
  SkRect visible_rect = gfx::RectFToSkRect(params->visible_rect);
  SkPaint paint = params->paint(GetContentColorFilter());

  if (params->draw_region) {
    current_canvas_->clipPath(params->draw_region_in_path(),
                              paint.isAntiAlias());
  } else {
    current_canvas_->clipRect(visible_rect, paint.isAntiAlias());
  }

  if (needs_transparency) {
    // Use the DrawQuadParams' paint for the layer, since that will affect the
    // final draw of the backing image.
    current_canvas_->saveLayer(&visible_rect, &paint);
  }

  SkCanvas* raster_canvas = current_canvas_;
  absl::optional<skia::OpacityFilterCanvas> opacity_canvas;
  if (disable_image_filtering) {
    // TODO(vmpstr): Fold this canvas into playback and have raster source
    // accept a set of settings on playback that will determine which canvas to
    // apply. (http://crbug.com/594679)
    // saveLayer applies the opacity, this filter is only used for quality
    // overriding in the display list, hence the fixed 1.f for alpha.
    opacity_canvas.emplace(raster_canvas, 1.f, disable_image_filtering);
    raster_canvas = &*opacity_canvas;
  }

  // Treat all subnormal values as zero for performance.
  cc::ScopedSubnormalFloatDisabler disabler;

  raster_canvas->concat(SkMatrix::RectToRect(
      gfx::RectFToSkRect(quad->tex_coord_rect), gfx::RectToSkRect(quad->rect)));

  raster_canvas->translate(-quad->content_rect.x(), -quad->content_rect.y());
  raster_canvas->clipRect(gfx::RectToSkRect(quad->content_rect));
  raster_canvas->scale(quad->contents_scale, quad->contents_scale);
  quad->display_item_list->Raster(raster_canvas);
}

void SkiaRenderer::DrawSolidColorQuad(const SolidColorDrawQuad* quad,
                                      const DrawRPDQParams* rpdq_params,
                                      DrawQuadParams* params) {
  DrawColoredQuad(quad->color, rpdq_params, params);
}

void SkiaRenderer::DrawTextureQuad(const TextureDrawQuad* quad,
                                   const DrawRPDQParams* rpdq_params,
                                   DrawQuadParams* params) {
  TRACE_EVENT0("viz", "SkiaRenderer::DrawTextureQuad");
  const gfx::ColorSpace& src_color_space =
      resource_provider()->GetSamplerColorSpace(quad->resource_id());
  const bool needs_color_conversion_filter =
      ((quad->is_video_frame && src_color_space.IsHDR()) ||
       src_color_space.IsToneMappedByDefault()) &&
      // Don't do color conversions for stream video.
      !quad->is_stream_video;

  sk_sp<SkColorSpace> override_color_space;
  if (needs_color_conversion_filter) {
    override_color_space = CurrentRenderPassSkColorSpace();
  }

    // TODO(b/221643955): Some Chrome OS tests rely on the old GLRenderer
    // behavior of skipping color space conversions if the quad's color space is
    // invalid. Once these tests are migrated, we can remove the override here
    // and revert to Skia's default behavior of assuming sRGB on invalid.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!src_color_space.IsValid())
    override_color_space = CurrentRenderPassSkColorSpace();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Force SRGB color space if we don't want real color space from media
  // decoder.
  if (!use_real_color_space_for_stream_video_ && quad->is_stream_video) {
    override_color_space = SkColorSpace::MakeSRGB();
  }

  ScopedSkImageBuilder builder(
      this, quad->resource_id(), /*maybe_concurrent_reads=*/true,
      quad->premultiplied_alpha ? kPremul_SkAlphaType : kUnpremul_SkAlphaType,
      quad->y_flipped ? kBottomLeft_GrSurfaceOrigin : kTopLeft_GrSurfaceOrigin,
      override_color_space);
  const SkImage* image = builder.sk_image();
  if (!image)
    return;
  gfx::RectF uv_rect = gfx::ScaleRect(
      gfx::BoundingRect(quad->uv_top_left, quad->uv_bottom_right),
      image->width(), image->height());
  params->vis_tex_coords = cc::MathUtil::ScaleRectProportional(
      uv_rect, gfx::RectF(quad->rect), params->visible_rect);

  // Use provided resource size if not empty, otherwise use the full image size
  // as the content area
  gfx::RectF valid_texel_bounds =
      quad->resource_size_in_pixels().IsEmpty()
          ? gfx::RectF(image->width(), image->height())
          : gfx::RectF(gfx::SizeF(quad->resource_size_in_pixels()));

  // There are three scenarios where a texture quad cannot be put into a batch:
  // 1. It needs to be blended with a constant background color.
  // 2. The vertex opacities are not all 1s.
  // 3. The quad contains video which might need special white level adjustment.
  const bool blend_background =
      quad->background_color != SkColors::kTransparent && !image->isOpaque();
  const bool vertex_alpha =
      quad->vertex_opacity[0] < 1.f || quad->vertex_opacity[1] < 1.f ||
      quad->vertex_opacity[2] < 1.f || quad->vertex_opacity[3] < 1.f;

  if (!blend_background && !vertex_alpha && !needs_color_conversion_filter &&
      !rpdq_params) {
    // This is a simple texture draw and can go into the batching system
    DCHECK(!MustFlushBatchedQuads(quad, rpdq_params, *params));
    AddQuadToBatch(image, valid_texel_bounds, params);
    return;
  }
  // This needs a color filter for background blending and/or a mask filter
  // to simulate the vertex opacity, which requires configuring a full SkPaint
  // and is incompatible with anything batched, but since MustFlushBatchedQuads
  // was optimistic for TextureQuad's, we're responsible for flushing now.
  if (!batched_quads_.empty())
    FlushBatchedQuads();

  SkPaint paint = params->paint(GetContentColorFilter());

  float quad_alpha;
  if (rpdq_params) {
    // The added color filters for background blending will not apply the
    // layer's opacity, but make sure there's no vertex_alpha since
    // CanPassBeDrawnDirectly() should have caught that case.
    DCHECK(!vertex_alpha);
    quad_alpha = 1.f;
  } else {
    // We will entirely handle the quad's opacity with the mask or color filter
    quad_alpha = params->opacity;
    params->opacity = 1.f;
  }

  if (vertex_alpha) {
    // If they are all the same value, combine it with the overall opacity,
    // otherwise use a mask filter to emulate vertex opacity interpolation
    if (quad->vertex_opacity[0] == quad->vertex_opacity[1] &&
        quad->vertex_opacity[0] == quad->vertex_opacity[2] &&
        quad->vertex_opacity[0] == quad->vertex_opacity[3]) {
      quad_alpha *= quad->vertex_opacity[0];
    } else {
      // The only occurrences of non-constant vertex opacities come from unit
      // tests and src/chrome/browser/android/compositor/decoration_title.cc,
      // but they always produce the effect of a linear alpha gradient.
      // All signs indicate point order is [BL, TL, TR, BR]
      SkPoint gradient_pts[2];
      if (quad->vertex_opacity[0] == quad->vertex_opacity[1] &&
          quad->vertex_opacity[2] == quad->vertex_opacity[3]) {
        // Left to right gradient
        float y =
            params->visible_rect.y() + 0.5f * params->visible_rect.height();
        gradient_pts[0] = {params->visible_rect.x(), y};
        gradient_pts[1] = {params->visible_rect.right(), y};
      } else if (quad->vertex_opacity[0] == quad->vertex_opacity[3] &&
                 quad->vertex_opacity[1] == quad->vertex_opacity[2]) {
        // Top to bottom gradient
        float x =
            params->visible_rect.x() + 0.5f * params->visible_rect.width();
        gradient_pts[0] = {x, params->visible_rect.y()};
        gradient_pts[1] = {x, params->visible_rect.bottom()};
      } else {
        // Not sure how to emulate
        NOTIMPLEMENTED();
        return;
      }

      float a1 = quad->vertex_opacity[0] * quad_alpha;
      float a2 = quad->vertex_opacity[2] * quad_alpha;
      SkColor4f gradient_colors[2] = {SkColor4f({a1, a1, a1, a1}),
                                      SkColor4f({a2, a2, a2, a2})};
      sk_sp<SkShader> gradient = SkGradientShader::MakeLinear(
          gradient_pts, gradient_colors, nullptr /*sk_sp<SkColorSpace>*/,
          nullptr, 2, SkTileMode::kClamp);
      paint.setMaskFilter(SkShaderMaskFilter::Make(std::move(gradient)));
      // shared quad opacity was folded into the gradient, so this will shorten
      // any color filter chain needed for background blending
      quad_alpha = 1.f;
    }
  }

  if (needs_color_conversion_filter) {
    // Skia won't perform color conversion.
    const gfx::ColorSpace dst_color_space = CurrentRenderPassColorSpace();
    DCHECK(SkColorSpace::Equals(image->colorSpace(),
                                CurrentRenderPassSkColorSpace().get()));
    sk_sp<SkColorFilter> color_filter = GetColorSpaceConversionFilter(
        src_color_space, absl::nullopt, quad->hdr_metadata, dst_color_space);
    paint.setColorFilter(color_filter->makeComposed(paint.refColorFilter()));
  }

  // From gl_renderer, the final src color will be
  // vertexAlpha * (textureColor + backgroundColor * (1 - textureAlpha)), where
  // vertexAlpha is the quad's alpha * interpolated per-vertex alpha
  if (blend_background) {
    // Add a color filter that does DstOver blending between texture and the
    // background color. Then, modulate by quad's opacity *after* blending.
    // TODO(crbug/1308932) remove toSkColor and make all SkColor4f
    sk_sp<SkColorFilter> cf = SkColorFilters::Blend(
        quad->background_color.toSkColor(), SkBlendMode::kDstOver);
    if (quad_alpha < 1.f) {
      cf = MakeOpacityFilter(quad_alpha, std::move(cf));
      quad_alpha = 1.f;
      DCHECK(cf);
    }
    // |cf| could be null if alpha in |quad->background_color| is 0.
    if (cf)
      paint.setColorFilter(cf->makeComposed(paint.refColorFilter()));
  }

  if (!rpdq_params && !quad->is_stream_video) {
    // Reset the paint's alpha, since it started as params.opacity and that
    // is now applied outside of the paint's alpha.
    paint.setAlphaf(quad_alpha);
  }

  DrawSingleImage(image, valid_texel_bounds, rpdq_params, &paint, params);
}

void SkiaRenderer::DrawTileDrawQuad(const TileDrawQuad* quad,
                                    const DrawRPDQParams* rpdq_params,
                                    DrawQuadParams* params) {
  TRACE_EVENT0("viz", "SkiaRenderer::DrawTileDrawQuad");
  DCHECK(!MustFlushBatchedQuads(quad, rpdq_params, *params));
  // |resource_provider()| can be NULL in resourceless software draws, which
  // should never produce tile quads in the first place.
  DCHECK(resource_provider());

  // If quad->ShouldDrawWithBlending() is true, we need to raster tile paint ops
  // to an offscreen texture first, and then blend it with content behind the
  // tile. Since a tile could be used cross frames, so it would better to not
  // use raw draw.
  bool raw_draw_if_possible =
      is_using_raw_draw_ && !quad->ShouldDrawWithBlending();
  ScopedSkImageBuilder builder(
      this, quad->resource_id(), /*maybe_concurrent_reads=*/false,
      quad->is_premultiplied ? kPremul_SkAlphaType : kUnpremul_SkAlphaType,
      /*origin=*/kTopLeft_GrSurfaceOrigin,
      /*override_color_space=*/nullptr, raw_draw_if_possible);

  params->vis_tex_coords = cc::MathUtil::ScaleRectProportional(
      quad->tex_coord_rect, gfx::RectF(quad->rect), params->visible_rect);

  bool using_raw_draw = builder.paint_op_buffer();
  if (is_using_raw_draw_) {
    UMA_HISTOGRAM_BOOLEAN(
        "Compositing.SkiaRenderer.DrawTileDrawQuad.UsingRawDraw",
        using_raw_draw);
  }
  if (using_raw_draw) {
    DCHECK(!rpdq_params);
    DrawPaintOpBuffer(builder.paint_op_buffer(), builder.clear_color(), quad,
                      params);
    return;
  }

  const SkImage* image = builder.sk_image();
  if (!image)
    return;

  // When a tile is at the right or bottom edge of the entire tiled area, its
  // images won't be fully filled so use the unclipped texture coords. On
  // interior tiles or left/top tiles, the image has been filled with
  // overlapping content so the entire image is valid for sampling.
  gfx::RectF valid_texel_bounds(gfx::SizeF(quad->texture_size));
  if (quad->IsRightEdge()) {
    // Restrict the width to match far side of texture coords
    valid_texel_bounds.set_width(quad->tex_coord_rect.right());
  }
  if (quad->IsBottomEdge()) {
    // Restrict the height to match far side of texture coords
    valid_texel_bounds.set_height(quad->tex_coord_rect.bottom());
  }

  if (rpdq_params) {
    SkPaint paint = params->paint(GetContentColorFilter());
    DrawSingleImage(image, valid_texel_bounds, rpdq_params, &paint, params);
  } else {
    AddQuadToBatch(image, valid_texel_bounds, params);
  }
}

void SkiaRenderer::DrawYUVVideoQuad(const YUVVideoDrawQuad* quad,
                                    const DrawRPDQParams* rpdq_params,
                                    DrawQuadParams* params) {
  TRACE_EVENT0("viz", "SkiaRenderer::DrawYUVVideoQuad");
  // Since YUV quads always use a color filter, they require a complex skPaint
  // that precludes batching. If this changes, we could add YUV quads that don't
  // require a filter to the batch instead of drawing one at a time.
  DCHECK(batched_quads_.empty());

  gfx::ColorSpace src_color_space = quad->video_color_space;
  // Invalid or unspecified color spaces should be treated as REC709.
  if (!src_color_space.IsValid())
    src_color_space = gfx::ColorSpace::CreateREC709();

  // We might modify |dst_color_space| to be something other than the
  // destination color space for the frame. The generated SkColorFilter does the
  // real color space adjustment. To avoid having skia also try to adjust the
  // color space we lie and say the SkImage destination color space is always
  // the same as the rest of the frame. Otherwise the two color space
  // adjustments combined will produce the wrong result.
  gfx::ColorSpace dst_color_space = CurrentRenderPassColorSpace();

#if BUILDFLAG(IS_WIN)
  // Force sRGB output on Windows for overlay candidate video quads to match
  // DirectComposition behavior in case these switch between overlays and
  // compositing. See https://crbug.com/811118 for details.
  // Currently if HDR is supported, OverlayProcessor doesn't promote HDR video
  // frame as overlay candidate. So it's unnecessary to worry about the
  // compositing-overlay switch here. In addition drawing a HDR video using sRGB
  // can cancel the advantages of HDR.
  const bool supports_dc_layers =
      output_surface_->capabilities().supports_dc_layers;
  if (supports_dc_layers && !src_color_space.IsHDR() &&
      resource_provider()->IsOverlayCandidate(quad->y_plane_resource_id())) {
    DCHECK(
        resource_provider()->IsOverlayCandidate(quad->u_plane_resource_id()));
    dst_color_space = gfx::ColorSpace::CreateSRGB();
  }
#endif

  DCHECK(resource_provider());
  // Pass in |CurrentRenderPassSkColorSpace()| here instead of |dst_color_space|
  // so the color space transform going from SkImage to SkSurface is identity.
  // The SkColorFilter already handles color space conversion so this avoids
  // applying the conversion twice.
  ScopedYUVSkImageBuilder builder(this, quad, CurrentRenderPassSkColorSpace());
  const SkImage* image = builder.sk_image();
  if (!image)
    return;

  params->vis_tex_coords = cc::MathUtil::ScaleRectProportional(
      quad->ya_tex_coord_rect(), gfx::RectF(quad->rect), params->visible_rect);

  sk_sp<SkColorFilter> color_filter = GetColorSpaceConversionFilter(
      src_color_space, quad->bits_per_channel, quad->hdr_metadata,
      dst_color_space, quad->resource_offset, quad->resource_multiplier);

  auto content_color_filter = GetContentColorFilter();
  if (content_color_filter)
    color_filter = content_color_filter->makeComposed(color_filter);

  // Use provided, unclipped texture coordinates as the content area, which will
  // force coord clamping unless the geometry was clipped, or they span the
  // entire YUV image.
  SkPaint paint = params->paint(color_filter);

  DrawSingleImage(image, quad->ya_tex_coord_rect(), rpdq_params, &paint,
                  params);
}

void SkiaRenderer::DrawUnsupportedQuad(const DrawQuad* quad,
                                       const DrawRPDQParams* rpdq_params,
                                       DrawQuadParams* params) {
#ifdef NDEBUG
  DrawColoredQuad(SkColors::kWhite, rpdq_params, params);
#else
  DrawColoredQuad(SkColors::kMagenta, rpdq_params, params);
#endif
}

void SkiaRenderer::ScheduleOverlays() {
  DCHECK(!current_gpu_commands_completed_fence_->was_set());
  DCHECK(!current_release_fence_->was_set());

  // Always add an empty set of locks to be used in either SwapBuffersSkipped()
  // or SwapBuffersComplete().
  pending_overlay_locks_.emplace_back();
  [[maybe_unused]] auto& locks = pending_overlay_locks_.back();

  if (current_frame()->overlay_list.empty())
    return;

  std::vector<gpu::SyncToken> sync_tokens;

#if !BUILDFLAG(IS_WIN)
  DCHECK(output_surface_->capabilities().supports_surfaceless);
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
  // CrOS and Android SurfaceControl use this code path. Android classic has
  // switched over to OverlayProcessor.
  // TODO(weiliangc): Remove this when CrOS and Android SurfaceControl switch
  // to OverlayProcessor as well.
  for (auto& overlay : current_frame()->overlay_list) {
    if (overlay.is_root_render_pass) {
      continue;
    }
    // Resources will be unlocked after the next SwapBuffers() is completed.
    locks.emplace_back(resource_provider(), overlay.resource_id);
    auto& lock = locks.back();

    // Sync tokens ensure the texture to be overlaid is available before
    // scheduling it for display.
    if (lock.sync_token().HasData())
      sync_tokens.push_back(lock.sync_token());

    overlay.mailbox = lock.mailbox();
    DCHECK(!overlay.mailbox.IsZero());
  }
#elif BUILDFLAG(IS_WIN)
  for (auto& dc_layer_overlay : current_frame()->overlay_list) {
    for (size_t i = 0; i < DCLayerOverlay::kNumResources; ++i) {
      ResourceId resource_id = dc_layer_overlay.resources[i];
      if (resource_id == kInvalidResourceId)
        break;

      // Resources will be unlocked after the next SwapBuffers() is completed.
      locks.emplace_back(resource_provider(), resource_id);
      auto& lock = locks.back();

      // Sync tokens ensure the texture to be overlaid is available before
      // scheduling it for display.
      if (lock.sync_token().HasData())
        sync_tokens.push_back(lock.sync_token());

      dc_layer_overlay.mailbox[i] = lock.mailbox();
    }
    DCHECK(!dc_layer_overlay.mailbox[0].IsZero());
  }
#elif BUILDFLAG(IS_APPLE)
  for (CALayerOverlay& ca_layer_overlay : current_frame()->overlay_list) {
    if (ca_layer_overlay.rpdq) {
      PrepareRenderPassOverlay(&ca_layer_overlay);
      locks.emplace_back(ca_layer_overlay.mailbox);
      continue;
    }
    // Some overlays are for solid-color layers.
    if (!ca_layer_overlay.contents_resource_id)
      continue;

    // TODO(https://crbug.com/894929): Track IOSurface in-use instead of just
    // unlocking after the next SwapBuffers is completed.
    locks.emplace_back(resource_provider(),
                       ca_layer_overlay.contents_resource_id);
    auto& lock = locks.back();

    // Sync tokens ensure the texture to be overlaid is available before
    // scheduling it for display.
    if (lock.sync_token().HasData())
      sync_tokens.push_back(lock.sync_token());

    // Populate the |mailbox| of the CALayerOverlay which will be used to look
    // up the corresponding GLImageIOSurface when building the CALayer tree.
    ca_layer_overlay.mailbox = lock.mailbox();
    DCHECK(!ca_layer_overlay.mailbox.IsZero());
  }
#elif BUILDFLAG(IS_OZONE)
  // Only Wayland uses this code path.
  for (auto& overlay : current_frame()->overlay_list) {
    if (overlay.is_root_render_pass) {
      continue;
    }
    if (overlay.rpdq) {
      PrepareRenderPassOverlay(&overlay);
      locks.emplace_back(overlay.mailbox);
      continue;
    }
    // If non-backed solid color overlays aren't supported (e.g. Lacros on
    // Linux) then we need to create buffers to send over Wayland.
    if (overlay.is_solid_color) {
      if (!output_surface_->capabilities()
               .supports_non_backed_solid_color_overlays) {
        DCHECK(overlay.color);
        overlay.mailbox = GetImageMailboxForColor(*overlay.color);
        // This can now be treated as a regular overlay with a mailbox backing.
        overlay.is_solid_color = false;
        locks.emplace_back(overlay.mailbox);
      }
      continue;
    }

    // Resources will be unlocked after the next SwapBuffers() is completed.
    locks.emplace_back(resource_provider(), overlay.resource_id);
    auto& lock = locks.back();

    // Sync tokens ensure the texture to be overlaid is available before
    // scheduling it for display.
    if (lock.sync_token().HasData())
      sync_tokens.push_back(lock.sync_token());

    overlay.mailbox = lock.mailbox();
    DCHECK(!overlay.mailbox.IsZero());
  }
#else   // BUILDFLAG(IS_ANDROID)
  // For platforms that don't support overlays, the
  // current_frame()->overlay_list should be empty, and this code should not be
  // reached.
  NOTREACHED();
#endif  // BUILDFLAG(IS_ANDROID)

  DCHECK(!current_gpu_commands_completed_fence_->was_set());
  DCHECK(!current_release_fence_->was_set());

  skia_output_surface_->ScheduleOverlays(
      std::move(current_frame()->overlay_list), std::move(sync_tokens));
}

sk_sp<SkColorFilter> SkiaRenderer::GetColorSpaceConversionFilter(
    const gfx::ColorSpace& src,
    absl::optional<uint32_t> src_bit_depth,
    absl::optional<gfx::HDRMetadata> src_hdr_metadata,
    const gfx::ColorSpace& dst,
    float resource_offset,
    float resource_multiplier) {
  return color_filter_cache_.Get(
      src, dst, resource_offset, resource_multiplier, src_bit_depth,
      src_hdr_metadata,
      current_frame()->display_color_spaces.GetSDRMaxLuminanceNits(),
      current_frame()->display_color_spaces.GetHDRMaxLuminanceRelative());
}

namespace {
SkColorMatrix ToColorMatrix(const SkM44& mat) {
  std::array<float, 20> values;
  values.fill(0.0f);
  for (uint32_t r = 0; r < 4; r++) {
    for (uint32_t c = 0; c < 4; c++) {
      values[r * 5 + c] = mat.rc(r, c);
    }
  }
  SkColorMatrix mat_out;
  mat_out.setRowMajor(values.data());
  return mat_out;
}
}  // namespace

sk_sp<SkColorFilter> SkiaRenderer::GetContentColorFilter() {
  sk_sp<SkColorFilter> color_transform = nullptr;
  bool is_root =
      current_frame()->current_render_pass == current_frame()->root_render_pass;

  if (is_root && output_surface_->color_matrix() != SkM44()) {
    color_transform =
        SkColorFilters::Matrix(ToColorMatrix(output_surface_->color_matrix()));
  }

  sk_sp<SkColorFilter> tint_transform = nullptr;
  if (is_root && debug_settings_->tint_composited_content) {
    if (debug_settings_->tint_composited_content_modulate) {
      // Integer counter causes modulation through rgb dimming variations.
      std::array<float, 3> rgb;
      uint32_t ci = debug_tint_modulate_count_ % 7u;
      for (int rc = 0; rc < 3; rc++) {
        rgb[rc] = (ci & (1u << rc)) ? 0.7f : 1.0f;
      }
      SkColorMatrix color_mat;
      color_mat.setScale(rgb[0], rgb[1], rgb[2]);
      tint_transform = SkColorFilters::Matrix(color_mat);
    } else {
      SkM44 mat44 = SkM44::ColMajor(
          cc::DebugColors::TintCompositedContentColorTransformMatrix().data());
      tint_transform = SkColorFilters::Matrix(ToColorMatrix(mat44));
    }
  }

  if (color_transform) {
    return tint_transform ? color_transform->makeComposed(tint_transform)
                          : color_transform;
  } else {
    return tint_transform;
  }
}

SkiaRenderer::DrawRPDQParams SkiaRenderer::CalculateRPDQParams(
    const AggregatedRenderPassDrawQuad* quad,
    DrawQuadParams* params) {
  DrawRPDQParams rpdq_params(params->visible_rect);

  // Prepare mask.
  ResourceId mask_resource_id = quad->mask_resource_id();
  ScopedSkImageBuilder mask_image_builder(this, mask_resource_id,
                                          /*maybe_concurrent_reads=*/false);
  const SkImage* mask_image = mask_image_builder.sk_image();
  DCHECK_EQ(!!mask_resource_id, !!mask_image);
  if (mask_image) {
    // Scale normalized uv rect into absolute texel coordinates.
    SkRect mask_rect = gfx::RectFToSkRect(
        gfx::ScaleRect(quad->mask_uv_rect, quad->mask_texture_size.width(),
                       quad->mask_texture_size.height()));
    // Map to full quad rect so that mask coordinates don't change with clipping
    SkMatrix mask_to_quad_matrix =
        SkMatrix::RectToRect(mask_rect, gfx::RectToSkRect(quad->rect));

    rpdq_params.mask_shader = mask_image->makeShader(
        SkTileMode::kClamp, SkTileMode::kClamp,
        SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone),
        &mask_to_quad_matrix);
  }

  const cc::FilterOperations* filters = FiltersForPass(quad->render_pass_id);
  const cc::FilterOperations* backdrop_filters =
      BackdropFiltersForPass(quad->render_pass_id);
  // Early out if there are no filters to convert to SkImageFilters
  if (!filters && !backdrop_filters) {
    return rpdq_params;
  }

  // Calculate local matrix that's shared by filters and backdrop_filters
  SkMatrix local_matrix;
  local_matrix.setTranslate(quad->filters_origin.x(), quad->filters_origin.y());
  local_matrix.postScale(quad->filters_scale.x(), quad->filters_scale.y());

  gfx::SizeF filter_size(quad->rect.width(), quad->rect.height());

  // Convert CC image filters into a SkImageFilter root node
  if (filters) {
    DCHECK(!filters->IsEmpty());
    auto paint_filter =
        cc::RenderSurfaceFilters::BuildImageFilter(*filters, filter_size);
    auto sk_filter = paint_filter ? paint_filter->cached_sk_filter_ : nullptr;

    if (sk_filter) {
      // Update the filter bounds based to account for how the image filters
      // grow or expand the area touched by drawing.
      rpdq_params.filter_bounds =
          filters->MapRect(rpdq_params.filter_bounds, local_matrix);

      // If after applying the filter we would be clipped out, skip the draw.
      gfx::Rect clip_rect =
          quad->shared_quad_state->clip_rect.value_or(current_draw_rect_);
      gfx::Transform transform =
          quad->shared_quad_state->quad_to_target_transform;
      transform.Flatten();
      if (!transform.IsInvertible()) {
        return rpdq_params;
      }

      // If the transform has perspective, there might be visible content
      // outside of the bounds of the quad.
      if (!transform.HasPerspective()) {
        gfx::QuadF clip_quad = gfx::QuadF(gfx::RectF(clip_rect));
        gfx::QuadF local_clip =
            cc::MathUtil::InverseMapQuadToLocalSpace(transform, clip_quad);

        rpdq_params.filter_bounds.Intersect(
            gfx::ToEnclosingRect(local_clip.BoundingBox()));
      }

      // If we've been fully clipped out (by crop rect or clipping), there's
      // nothing to draw.
      if (rpdq_params.filter_bounds.IsEmpty()) {
        return rpdq_params;
      }

      rpdq_params.image_filter = sk_filter->makeWithLocalMatrix(local_matrix);

      // Attempt to simplify the image filter to a color filter, which enables
      // the RPDQ effects to be applied more efficiently.
      SkColorFilter* color_filter_ptr = nullptr;
      if (rpdq_params.image_filter) {
        if (rpdq_params.image_filter->asAColorFilter(&color_filter_ptr)) {
          // asAColorFilter already ref'ed the filter when true is returned,
          // reset() does not add a ref itself, so everything is okay.
          rpdq_params.color_filter.reset(color_filter_ptr);
        }
      }
    }
  }

  // Convert CC image filters for the backdrop into a SkImageFilter root node
  // TODO(weiliangc): ChromeOS would need backdrop_filter_quality implemented
  if (backdrop_filters) {
    DCHECK(!backdrop_filters->IsEmpty());

    // Must account for clipping that occurs for backdrop filters, since their
    // input content has already been clipped to the output rect.
    gfx::Rect device_rect = gfx::ToEnclosingRect(cc::MathUtil::MapClippedRect(
        params->content_device_transform, gfx::RectF(quad->rect)));
    gfx::Rect out_rect = MoveFromDrawToWindowSpace(
        current_frame()->current_render_pass->output_rect);
    out_rect.Intersect(device_rect);
    gfx::Vector2dF offset =
        (device_rect.top_right() - out_rect.top_right()) +
        (device_rect.bottom_left() - out_rect.bottom_left());

    auto bg_paint_filter = cc::RenderSurfaceFilters::BuildImageFilter(
        *backdrop_filters, gfx::SizeF(out_rect.size()), offset);
    auto sk_bg_filter =
        bg_paint_filter ? bg_paint_filter->cached_sk_filter_ : nullptr;

    if (sk_bg_filter) {
      rpdq_params.backdrop_filter =
          sk_bg_filter->makeWithLocalMatrix(local_matrix);
    }
  }

  // Determine if the backdrop filter has its own clip (which only needs to be
  // checked when we have a backdrop filter to apply)
  if (rpdq_params.backdrop_filter) {
    const absl::optional<gfx::RRectF> backdrop_filter_bounds =
        BackdropFilterBoundsForPass(quad->render_pass_id);
    if (backdrop_filter_bounds) {
      // The backdrop filters effect will be cropped by these bounds. If the
      // bounds are empty, discard the backdrop filter now since none of it
      // would have been visible anyways.
      if (backdrop_filter_bounds->IsEmpty()) {
        rpdq_params.backdrop_filter = nullptr;
      } else {
        rpdq_params.backdrop_filter_bounds = *backdrop_filter_bounds;
        // Scale by the filter's scale, but don't apply filter origin
        rpdq_params.backdrop_filter_bounds->Scale(quad->filters_scale.x(),
                                                  quad->filters_scale.y());

        // If there are also regular image filters, they apply to the area of
        // the backdrop_filter_bounds too, so expand the backdrop bounds and
        // join it with the main filter bounds.
        if (rpdq_params.image_filter) {
          gfx::Rect backdrop_rect =
              gfx::ToEnclosingRect(rpdq_params.backdrop_filter_bounds->rect());
          rpdq_params.filter_bounds.Union(
              filters->MapRect(backdrop_rect, local_matrix));
        }
      }
    }
  }

  return rpdq_params;
}

void SkiaRenderer::DrawRenderPassQuad(const AggregatedRenderPassDrawQuad* quad,
                                      DrawQuadParams* params) {
  TRACE_EVENT0("viz", "SkiaRenderer::DrawRenderPassQuad");
  DrawRPDQParams rpdq_params = CalculateRPDQParams(quad, params);

  // |filter_bounds| is the content space bounds that includes any filtered
  // extents. If empty, the draw can be skipped.
  if (rpdq_params.filter_bounds.IsEmpty())
    return;

  auto bypass = render_pass_bypass_quads_.find(quad->render_pass_id);
  // When Render Pass has a single quad inside we would draw that directly.
  if (bypass != render_pass_bypass_quads_.end()) {
    BypassMode mode =
        CalculateBypassParams(bypass->second, &rpdq_params, params);
    if (mode == BypassMode::kDrawTransparentQuad) {
      // The RPDQ is masquerading as a solid color quad, which do not support
      // batching.
      if (!batched_quads_.empty())
        FlushBatchedQuads();
      DrawColoredQuad(SkColors::kTransparent, &rpdq_params, params);
    } else if (mode == BypassMode::kDrawBypassQuad) {
      DrawQuadInternal(bypass->second, &rpdq_params, params);
    }  // else mode == kSkip
    return;
  }

  // A real render pass that was turned into an image
  auto iter = render_pass_backings_.find(quad->render_pass_id);
  DCHECK(render_pass_backings_.end() != iter)
      << "Could not find render pass id # " << quad->render_pass_id
      << " in the render pass overlay backings";
  // This function is called after AllocateRenderPassResourceIfNeeded, so
  // there should be backing ready.
  RenderPassBacking& backing = iter->second;

  sk_sp<SkImage> content_image =
      skia_output_surface_->MakePromiseSkImageFromRenderPass(
          quad->render_pass_id, backing.size, backing.format,
          backing.generate_mipmap, RenderPassBackingSkColorSpace(backing),
          backing.mailbox);
  DLOG_IF(ERROR, !content_image)
      << "MakePromiseSkImageFromRenderPass() failed for render pass";

  if (!content_image)
    return;

  if (backing.generate_mipmap)
    params->sampling =
        SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kLinear);

  params->vis_tex_coords = cc::MathUtil::ScaleRectProportional(
      quad->tex_coord_rect, gfx::RectF(quad->rect), params->visible_rect);
  gfx::RectF valid_texel_bounds(content_image->width(),
                                content_image->height());

  // When the RPDQ was needed because of a copy request, it may not require any
  // advanced filtering/effects at which point it's basically a tiled quad.
  if (!rpdq_params.image_filter && !rpdq_params.backdrop_filter &&
      !rpdq_params.mask_shader) {
    DCHECK(!MustFlushBatchedQuads(quad, nullptr, *params));
    AddQuadToBatch(content_image.get(), valid_texel_bounds, params);
    return;
  }

  // The paint is complex enough that it has to be drawn on its own, and since
  // MustFlushBatchedQuads() was optimistic, we manage the flush here.
  if (!batched_quads_.empty())
    FlushBatchedQuads();

  SkPaint paint = params->paint(GetContentColorFilter());

  DrawSingleImage(content_image.get(), valid_texel_bounds, &rpdq_params, &paint,
                  params);
}

void SkiaRenderer::CopyDrawnRenderPass(
    const copy_output::RenderPassGeometry& geometry,
    std::unique_ptr<CopyOutputRequest> request) {
  // TODO(weiliangc): Make copy request work. (crbug.com/644851)
  TRACE_EVENT0("viz", "SkiaRenderer::CopyDrawnRenderPass");

  // Root framebuffer uses a zero-mailbox in SkiaOutputSurface.
  gpu::Mailbox mailbox;
  const auto* const render_pass = current_frame()->current_render_pass;
  AggregatedRenderPassId render_pass_id = render_pass->id;
  auto it = render_pass_backings_.find(render_pass_id);
  if (it != render_pass_backings_.end()) {
    mailbox = it->second.mailbox;
  }

  skia_output_surface_->CopyOutput(geometry, CurrentRenderPassColorSpace(),
                                   std::move(request), mailbox);
}

void SkiaRenderer::DidChangeVisibility() {
  if (visible_)
    output_surface_->EnsureBackbuffer();
  else
    output_surface_->DiscardBackbuffer();
}

void SkiaRenderer::FinishDrawingQuadList() {
  TRACE_EVENT0("viz", "SkiaRenderer::FinishDrawingQuadList");
  if (!current_canvas_)
    return;

  if (!batched_quads_.empty())
    FlushBatchedQuads();

  bool is_root_render_pass =
      current_frame()->current_render_pass == current_frame()->root_render_pass;

  // Drawing the delegated ink trail must happen after the final
  // FlushBatchedQuads() call so that the trail can always be on top of
  // everything else that has already been drawn on the page. For the same
  // reason, it should only happen on the root render pass.
  if (is_root_render_pass && UsingSkiaForDelegatedInk())
    DrawDelegatedInkTrail();

  current_canvas_ = nullptr;
  EndPaint(/*failed=*/false);

  // Defer flushing drawing task for root render pass, to avoid extra
  // MakeCurrent() call. It is expensive on GL.
  // TODO(https://crbug.com/1141008): Consider deferring drawing tasks for
  // all render passes.
  if (is_root_render_pass)
    return;

  FlushOutputSurface();
}

void SkiaRenderer::GenerateMipmap() {
  // This is a no-op since setting FilterQuality to high during drawing of
  // CompositorRenderPassDrawQuad is what actually generates generate_mipmap.
}

void SkiaRenderer::UpdateRenderPassTextures(
    const AggregatedRenderPassList& render_passes_in_draw_order,
    const base::flat_map<AggregatedRenderPassId, RenderPassRequirements>&
        render_passes_in_frame) {
  std::vector<AggregatedRenderPassId> passes_to_delete;
  for (const auto& pair : render_pass_backings_) {
    auto render_pass_it = render_passes_in_frame.find(pair.first);
    if (render_pass_it == render_passes_in_frame.end()) {
      passes_to_delete.push_back(pair.first);
      continue;
    }

    const RenderPassRequirements& requirements = render_pass_it->second;
    const RenderPassBacking& backing = pair.second;
    bool size_appropriate = backing.size.width() >= requirements.size.width() &&
                            backing.size.height() >= requirements.size.height();
    bool mipmap_appropriate =
        !requirements.generate_mipmap || backing.generate_mipmap;
    bool no_change_in_format = requirements.format == backing.format;
    bool no_change_in_color_space =
        requirements.color_space == backing.color_space;

    if (!size_appropriate || !mipmap_appropriate || !no_change_in_format ||
        !no_change_in_color_space) {
      passes_to_delete.push_back(pair.first);
    }
  }

  // Delete RenderPass backings from the previous frame that will not be used
  // again.
  for (size_t i = 0; i < passes_to_delete.size(); ++i) {
    auto it = render_pass_backings_.find(passes_to_delete[i]);
    auto& backing = it->second;
    // Buffers for root render pass backings are managed by |buffer_queue_|, not
    // DisplayResourceProvider, so we should not destroy them here. This
    // reallocation is done in Reshape before drawing the frame
    if (!backing.is_root) {
      skia_output_surface_->DestroySharedImage(backing.mailbox);
    }
    render_pass_backings_.erase(it);
  }

  if (!passes_to_delete.empty()) {
    skia_output_surface_->RemoveRenderPassResource(std::move(passes_to_delete));
  }
}

void SkiaRenderer::AllocateRenderPassResourceIfNeeded(
    const AggregatedRenderPassId& render_pass_id,
    const RenderPassRequirements& requirements) {
  if (render_pass_id == current_frame()->root_render_pass->id) {
    DCHECK(buffer_queue_);
    auto& root_pass_backing = render_pass_backings_[render_pass_id];
    root_pass_backing.is_root = true;
    root_pass_backing.mailbox = buffer_queue_->GetCurrentBuffer();
    root_pass_backing.generate_mipmap = false;
    root_pass_backing.size = surface_size_for_swap_buffers();
    root_pass_backing.format = GetResourceFormat(reshape_buffer_format());
    root_pass_backing.color_space = reshape_color_space();
    return;
  }

  auto it = render_pass_backings_.find(render_pass_id);
  if (it != render_pass_backings_.end()) {
    DCHECK(gfx::Rect(it->second.size).Contains(gfx::Rect(requirements.size)));
    return;
  }

  uint32_t usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                   gpu::SHARED_IMAGE_USAGE_DISPLAY_WRITE;
  if (requirements.generate_mipmap)
    usage |= gpu::SHARED_IMAGE_USAGE_MIPMAP;
  auto mailbox = skia_output_surface_->CreateSharedImage(
      requirements.format, requirements.size, requirements.color_space, usage,
      gpu::kNullSurfaceHandle);
  render_pass_backings_.emplace(
      render_pass_id,
      RenderPassBacking({requirements.size, requirements.generate_mipmap,
                         requirements.color_space, requirements.format, mailbox,
                         /*is_root=*/false}));
}

void SkiaRenderer::FlushOutputSurface() {
  auto sync_token = skia_output_surface_->Flush();
  lock_set_for_external_use_->UnlockResources(sync_token);
}

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
bool SkiaRenderer::CanSkipRenderPassOverlay(
    AggregatedRenderPassId render_pass_id,
    const AggregatedRenderPassDrawQuad* rpdq,
    RenderPassOverlayParams** output_render_pass_overlay) {
  // The render pass draw quad can be skipped if (1) the render pass has no
  // damage and is skipped in DirectRender and (2) the parameters of drawing the
  // render pass has not changed.

  if (!can_skip_render_pass_overlay_)
    return false;

  // Check if the render pass has been re-drawn.
  if (skipped_render_pass_ids_.count(render_pass_id) == 0)
    return false;

  // Every time a new render_pass_overlay is allocated, it's added to the back
  // of the list. In order to get the render_pass_overlay of the previous frame,
  // loop through the list in a reverse order since there might be multiple
  // render pass overlays with the same render pass id.
  RenderPassOverlayParams* overlay_found = nullptr;
  bool found_in_available_backings = false;
  for (auto rit = in_flight_render_pass_overlay_backings_.rbegin();
       rit != in_flight_render_pass_overlay_backings_.rend(); ++rit) {
    if (rit->render_pass_id == render_pass_id) {
      overlay_found = &*rit;
      break;
    }
  }

  // The backing of the previous frame might be complete and moved to
  // available_render_pass_overlay_backings_ when this frame starts.
  std::vector<RenderPassOverlayParams>::iterator it_to_delete;
  if (!overlay_found) {
    int index = 0;
    for (auto rit = available_render_pass_overlay_backings_.rbegin();
         rit != available_render_pass_overlay_backings_.rend();
         ++rit, ++index) {
      if (rit->render_pass_id == render_pass_id) {
        found_in_available_backings = true;
        // Cannot use reverse_iterator. Convert it to const_iterator.
        it_to_delete =
            available_render_pass_overlay_backings_.begin() +
            (available_render_pass_overlay_backings_.size() - index - 1);
        overlay_found = &*rit;
        break;
      }
    }
  }

  if (!overlay_found) {
    return false;
  }

  // Compare RenderPassDrawQuads of the previous frame and the current frame.
  const cc::FilterOperations* filters = FiltersForPass(render_pass_id);
  const cc::FilterOperations* backdrop_filters =
      BackdropFiltersForPass(render_pass_id);
  overlay_found->rpdq.shared_quad_state = &(overlay_found->shared_quad_state);

  bool no_change_in_rpdq = overlay_found->rpdq.Equals(*rpdq);
  bool no_change_in_filters =
      filters ? (overlay_found->filters == *filters)
              : (overlay_found->filters == cc::FilterOperations());
  bool no_change_in_backdrop_filters =
      backdrop_filters
          ? (overlay_found->backdrop_filters == *backdrop_filters)
          : (overlay_found->backdrop_filters == cc::FilterOperations());

  if (no_change_in_rpdq && no_change_in_filters &&
      no_change_in_backdrop_filters) {
    if (found_in_available_backings) {
      in_flight_render_pass_overlay_backings_.push_back(*overlay_found);
      available_render_pass_overlay_backings_.erase(it_to_delete);
      *output_render_pass_overlay =
          &in_flight_render_pass_overlay_backings_.back();
    } else {
      *output_render_pass_overlay = overlay_found;
    }
    return true;
  } else {
    return false;
  }
}

SkiaRenderer::RenderPassOverlayParams*
SkiaRenderer::GetOrCreateRenderPassOverlayBacking(
    AggregatedRenderPassId render_pass_id,
    const AggregatedRenderPassDrawQuad* rpdq,
    ResourceFormat buffer_format,
    gfx::ColorSpace color_space,
    const gfx::Size& buffer_size) {
  RenderPassOverlayParams overlay_params;
  auto it = base::ranges::find_if(available_render_pass_overlay_backings_,
                                  [&buffer_format, &buffer_size, &color_space](
                                      const RenderPassOverlayParams& overlay) {
                                    auto& backing = overlay.render_pass_backing;
                                    return backing.format == buffer_format &&
                                           backing.size == buffer_size &&
                                           backing.color_space == color_space;
                                  });
  if (it == available_render_pass_overlay_backings_.end()) {
    // Allocate the image for render pass overlay if there is no existing
    // available one.
    constexpr auto kOverlayUsage =
        gpu::SHARED_IMAGE_USAGE_SCANOUT | gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
        gpu::SHARED_IMAGE_USAGE_DISPLAY_WRITE | gpu::SHARED_IMAGE_USAGE_RASTER;
    auto mailbox = skia_output_surface_->CreateSharedImage(
        buffer_format, buffer_size, color_space, kOverlayUsage,
        gpu::kNullSurfaceHandle);
    overlay_params.render_pass_backing = {
        buffer_size, /*generate_mipmap=*/false, color_space, buffer_format,
        mailbox,     /*is_root=*/false};
  } else {
    overlay_params = *it;
    available_render_pass_overlay_backings_.erase(it);
  }

  // Add current rpdq to RenderPassOverlayParams.
  overlay_params.render_pass_id = render_pass_id;
  overlay_params.shared_quad_state.SetAll(*rpdq->shared_quad_state);
  overlay_params.rpdq.SetAll(*rpdq);

  if (const cc::FilterOperations* filters = FiltersForPass(render_pass_id);
      filters) {
    overlay_params.filters = *filters;
  }
  if (const cc::FilterOperations* backdrop_filters =
          BackdropFiltersForPass(render_pass_id);
      backdrop_filters) {
    overlay_params.backdrop_filters = *backdrop_filters;
  }

  in_flight_render_pass_overlay_backings_.push_back(overlay_params);

  return &in_flight_render_pass_overlay_backings_.back();
}

void SkiaRenderer::PrepareRenderPassOverlay(
    OverlayProcessorInterface::PlatformOverlayCandidate* overlay) {
  DCHECK(!current_canvas_);
  DCHECK(batched_quads_.empty());
  DCHECK(overlay->rpdq);

  auto* const quad = overlay->rpdq;

  // The overlay will be sent to GPU the thread, so set rpdq to nullptr to avoid
  // being accessed on the GPU thread.
  overlay->rpdq = nullptr;

  // The |current_render_pass| could be used for calculating destination
  // color space or clipping rect for backdrop filters. However
  // the |current_render_pass| is nullptr during ScheduleOverlays(), since all
  // overlay quads should be in the |root_render_pass|, before they are promoted
  // to overlays, so set the |root_render_pass| to the |current_render_pass|.
  base::AutoReset<const AggregatedRenderPass*> auto_reset_current_render_pass(
      &current_frame()->current_render_pass, current_frame()->root_render_pass);

  auto* shared_quad_state =
      const_cast<SharedQuadState*>(quad->shared_quad_state);

  absl::optional<gfx::Transform> quad_to_target_transform_inverse;
  if (shared_quad_state->clip_rect ||
      !shared_quad_state->mask_filter_info.IsEmpty()) {
    // We cannot handle rotation with clip rect or mask filter.
    DCHECK(
        shared_quad_state->quad_to_target_transform.Preserves2dAxisAlignment());
    quad_to_target_transform_inverse.emplace();
    // Flatten before inverting, since we're interested in how points
    // with z=0 in local space map to the clip rect, not in how the clip
    // rect at z=0 in device space maps to some other z in local space.
    gfx::Transform flat_quad_to_target_transform(
        shared_quad_state->quad_to_target_transform);
    flat_quad_to_target_transform.Flatten();
    quad_to_target_transform_inverse =
        flat_quad_to_target_transform.GetCheckedInverse();
  }

  // The |clip_rect| is in the device coordinate and with all transforms
  // (translation, scaling, rotation, etc), so remove them.
  absl::optional<base::AutoReset<gfx::Rect>> auto_reset_clip_rect;
  if (shared_quad_state->clip_rect) {
    // TODO(dbaron): This operation is likely not to be valid if
    // quad_to_target_transform_inverse.HasPerspective().
    gfx::RectF clip_rect(*shared_quad_state->clip_rect);
    clip_rect = quad_to_target_transform_inverse->MapRect(clip_rect);
    auto_reset_clip_rect.emplace(&shared_quad_state->clip_rect.value(),
                                 gfx::ToEnclosedRect(clip_rect));
  }

  // The |mask_filter_info| is in the device coordinate and with all transforms
  // (translation, scaling, rotation, etc), so remove them.
  if (!shared_quad_state->mask_filter_info.IsEmpty()) {
    bool result = shared_quad_state->mask_filter_info.ApplyTransform(
        *quad_to_target_transform_inverse);
    DCHECK(result) << "shared_quad_state->mask_filter_info.Transform() failed.";
  }

  const auto& viewport_size = current_frame()->device_viewport_size;
  gfx::AxisTransform2d target_to_device = gfx::OrthoProjectionTransform(
      /*left=*/0, /*right=*/viewport_size.width(), /*bottom=*/0,
      /*top=*/viewport_size.height());
  target_to_device.PostConcat(
      gfx::WindowTransform(/*x=*/0, /*y=*/0, /*width=*/viewport_size.width(),
                           /*height=*/viewport_size.height()));

  DrawQuadParams params;
  DrawRPDQParams rpdq_params{gfx::RectF()};
  {
    // Reset |quad_to_target_transform|, so the quad will be rendered at the
    // origin (0,0) without all transforms (translation, scaling, rotation, etc)
    // and then we will use OS compositor to do those transforms.
    base::AutoReset<gfx::Transform> auto_reset_transform(
        &shared_quad_state->quad_to_target_transform, gfx::Transform());
    // Use nullptr scissor, so we can always render the whole render pass in an
    // overlay backing.
    // TODO(penghuang): reusing overlay backing from previous frame to avoid
    // reproducing the overlay backing if the render pass content quad
    // properties and content are not changed.
    params = CalculateDrawQuadParams(target_to_device, /*scissor_rect=*/nullptr,
                                     quad, /*draw_region=*/nullptr);
    rpdq_params = CalculateRPDQParams(quad, &params);
  }

  const auto& filter_bounds = rpdq_params.filter_bounds;

  // |filter_bounds| is the content space bounds that includes any filtered
  // extents. If empty, the draw can be skipped.
  if (filter_bounds.IsEmpty())
    return;

  ResourceFormat buffer_format{};
  gfx::ColorSpace color_space;

  RenderPassBacking* src_quad_backing = nullptr;
  auto bypass = render_pass_bypass_quads_.find(quad->render_pass_id);
  BypassMode bypass_mode = BypassMode::kSkip;
  // When Render Pass has a single quad inside we would draw that directly.
  if (bypass != render_pass_bypass_quads_.end()) {
    bypass_mode = CalculateBypassParams(bypass->second, &rpdq_params, &params);
    if (bypass_mode == BypassMode::kSkip)
      return;

    // For bypassed render pass, we use the same format and color space for the
    // framebuffer.
    buffer_format = GetResourceFormat(reshape_buffer_format());
    color_space = reshape_color_space();
  } else {
    // A real render pass that was turned into an image
    auto it = render_pass_backings_.find(quad->render_pass_id);
    DCHECK(render_pass_backings_.end() != it);
    // This function is called after AllocateRenderPassResourceIfNeeded, so
    // there should be backing ready.
    src_quad_backing = &it->second;
    buffer_format = src_quad_backing->format;
    color_space = src_quad_backing->color_space;
  }

  // Adjust the overlay |buffer_size| to reduce memory fragmentation. It also
  // increases buffer reusing possibilities.
  constexpr int kBufferMultiple = 64;
  gfx::Size buffer_size(
      cc::MathUtil::CheckedRoundUp(filter_bounds.width(), kBufferMultiple),
      cc::MathUtil::CheckedRoundUp(filter_bounds.height(), kBufferMultiple));

  RenderPassOverlayParams* overlay_params = nullptr;
  bool can_skip_render_pass =
      CanSkipRenderPassOverlay(quad->render_pass_id, quad, &overlay_params);
  if (!can_skip_render_pass) {
    overlay_params = GetOrCreateRenderPassOverlayBacking(
        quad->render_pass_id, quad, buffer_format, color_space, buffer_size);
  }
  DCHECK(overlay_params);
  UMA_HISTOGRAM_BOOLEAN(
      "Compositing.SkiaRenderer.SkipOverlayRenderPassDrawQuad",
      can_skip_render_pass);

  const RenderPassBacking& dst_overlay_backing =
      overlay_params->render_pass_backing;
  overlay->mailbox = dst_overlay_backing.mailbox;

  if (!can_skip_render_pass) {
    current_canvas_ = skia_output_surface_->BeginPaintRenderPass(
        quad->render_pass_id, dst_overlay_backing.size,
        dst_overlay_backing.format, /*mipmap=*/false,
        RenderPassBackingSkColorSpace(dst_overlay_backing),
        /*is_overlay=*/true, overlay->mailbox);
    if (!current_canvas_) {
      DLOG(ERROR)
          << "BeginPaintRenderPass() in PrepareRenderPassOverlay() failed.";
      return;
    }

    // Clear the backing to ARGB(0,0,0,0).
    current_canvas_->clear(SkColorSetARGB(0, 0, 0, 0));

    // Adjust the |content_device_transform| to make sure filter extends are
    // drawn inside of the buffer.
    params.content_device_transform.Translate(-filter_bounds.x(),
                                              -filter_bounds.y());

    // Also adjust the |rounded_corner_bounds| to the new location.
    if (params.mask_filter_info) {
      params.mask_filter_info->ApplyTransform(params.content_device_transform);
    }

    // When Render Pass has a single quad inside we would draw that directly.
    if (bypass != render_pass_bypass_quads_.end()) {
      if (bypass_mode == BypassMode::kDrawTransparentQuad) {
        DrawColoredQuad(SkColors::kTransparent, &rpdq_params, &params);
      } else if (bypass_mode == BypassMode::kDrawBypassQuad) {
        DrawQuadInternal(bypass->second, &rpdq_params, &params);
      } else {
        NOTREACHED();
      }
    } else {
      DCHECK(src_quad_backing);
      auto content_image =
          skia_output_surface_->MakePromiseSkImageFromRenderPass(
              quad->render_pass_id, src_quad_backing->size,
              src_quad_backing->format, src_quad_backing->generate_mipmap,
              RenderPassBackingSkColorSpace(*src_quad_backing),
              src_quad_backing->mailbox);
      if (!content_image) {
        DLOG(ERROR) << "MakePromiseSkImageFromRenderPass() in "
                       "PrepareRenderPassOverlay() failed.";
        EndPaint(/*failed=*/true);
        return;
      }

      if (src_quad_backing->generate_mipmap) {
        params.sampling =
            SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kLinear);
      }

      params.vis_tex_coords = cc::MathUtil::ScaleRectProportional(
          quad->tex_coord_rect, gfx::RectF(quad->rect), params.visible_rect);

      gfx::RectF valid_texel_bounds(content_image->width(),
                                    content_image->height());

      SkPaint paint = params.paint(GetContentColorFilter());

      DrawSingleImage(content_image.get(), valid_texel_bounds, &rpdq_params,
                      &paint, &params);
    }

    current_canvas_ = nullptr;
    EndPaint(/*failed=*/false);
  }

#if BUILDFLAG(IS_APPLE)
  // Adjust |bounds_rect| to contain the whole buffer and at the right location.
  overlay->bounds_rect.set_origin(gfx::PointF(filter_bounds.origin()));
  overlay->bounds_rect.set_size(gfx::SizeF(buffer_size));
#else   // BUILDFLAG(IS_OZONE)
  // Adjust |display_rect| to be include the expanded |filter_bounds|, and
  // transformed.
  // TODO(fangzhoug): Merge Ozone and Apple code paths of delegated compositing.
  overlay->display_rect =
      quad->shared_quad_state->quad_to_target_transform.MapRect(
          gfx::RectF(filter_bounds));
  // Set |uv_rect| to reflect rounding from |filter_bounds| to |buffer_size|.
  overlay->uv_rect = gfx::RectF{
      static_cast<float>(filter_bounds.width()) / buffer_size.width(),
      static_cast<float>(filter_bounds.height()) / buffer_size.height()};
  // TODO(rivr): Handle the case where the overlay has an arbitrary transform
  // applied.
  if (absl::holds_alternative<gfx::OverlayTransform>(overlay->transform)) {
    gfx::Rect apply_clip = gfx::Rect(current_frame()->device_viewport_size);
    if (overlay->clip_rect.has_value())
      apply_clip.Intersect(overlay->clip_rect.value());

    OverlayCandidate::ApplyClip(*overlay, gfx::RectF(apply_clip));
    overlay->clip_rect = absl::nullopt;
  }
  // Assume full damage every time the pass is rendered.
  overlay->damage_rect = gfx::RectF(filter_bounds);
  // Fill in |format| and |color_space| information based on selected backing.
  overlay->color_space = color_space;
  overlay->format = BufferFormat(buffer_format);
#endif  // BUILDFLAG(IS_APPLE)
}
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)

void SkiaRenderer::EndPaint(bool failed) {
  base::OnceClosure on_finished_callback;
  base::OnceCallback<void(gfx::GpuFenceHandle)> on_return_release_fence_cb;
  // If SkiaRenderer has not failed, prepare callbacks and pass them to
  // SkiaOutputSurface.
  if (!failed) {
    // Signal |current_frame_resource_fence_| when the root render pass is
    // finished.
    if (current_gpu_commands_completed_fence_->was_set()) {
      on_finished_callback = base::BindPostTask(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindOnce(&FrameResourceGpuCommandsCompletedFence::Signal,
                         std::move(current_gpu_commands_completed_fence_)));
      current_gpu_commands_completed_fence_ =
          base::MakeRefCounted<FrameResourceGpuCommandsCompletedFence>(
              resource_provider());
      resource_provider()->SetGpuCommandsCompletedFence(
          current_gpu_commands_completed_fence_.get());
    }

    // Return a release fence to the |current_release_fence_|
    // when the root render pass is finished.
    if (current_release_fence_->was_set()) {
      on_return_release_fence_cb = base::BindPostTask(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindOnce(&FrameResourceReleaseFence::SetReleaseFenceCallback,
                         std::move(current_release_fence_)));
      current_release_fence_ =
          base::MakeRefCounted<FrameResourceReleaseFence>(resource_provider());
      resource_provider()->SetReleaseFence(current_release_fence_.get());
    }
  }
  bool is_overlay = buffer_queue_ && current_frame()->current_render_pass ==
                                         current_frame()->root_render_pass;
  skia_output_surface_->EndPaint(std::move(on_finished_callback),
                                 std::move(on_return_release_fence_cb),
                                 is_overlay);
}

bool SkiaRenderer::IsRenderPassResourceAllocated(
    const AggregatedRenderPassId& render_pass_id) const {
  auto it = render_pass_backings_.find(render_pass_id);
  return it != render_pass_backings_.end();
}

gfx::Size SkiaRenderer::GetRenderPassBackingPixelSize(
    const AggregatedRenderPassId& render_pass_id) {
  auto it = render_pass_backings_.find(render_pass_id);
  DCHECK(it != render_pass_backings_.end());
  return it->second.size;
}

void SkiaRenderer::SetDelegatedInkPointRendererSkiaForTest(
    std::unique_ptr<DelegatedInkPointRendererSkia> renderer) {
  DCHECK(!delegated_ink_handler_);
  delegated_ink_handler_ = std::make_unique<DelegatedInkHandler>(
      output_surface_->capabilities().supports_delegated_ink);
  delegated_ink_handler_->SetDelegatedInkPointRendererForTest(
      std::move(renderer));
}

void SkiaRenderer::DrawDelegatedInkTrail() {
  if (!delegated_ink_handler_ || !delegated_ink_handler_->GetInkRenderer())
    return;

  delegated_ink_handler_->GetInkRenderer()->DrawDelegatedInkTrail(
      current_canvas_);
}

DelegatedInkPointRendererBase* SkiaRenderer::GetDelegatedInkPointRenderer(
    bool create_if_necessary) {
  if (!delegated_ink_handler_ && !create_if_necessary)
    return nullptr;

  if (!delegated_ink_handler_) {
    delegated_ink_handler_ = std::make_unique<DelegatedInkHandler>(
        output_surface_->capabilities().supports_delegated_ink);
  }

  return delegated_ink_handler_->GetInkRenderer();
}

void SkiaRenderer::SetDelegatedInkMetadata(
    std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
  if (!delegated_ink_handler_) {
    delegated_ink_handler_ = std::make_unique<DelegatedInkHandler>(
        output_surface_->capabilities().supports_delegated_ink);
  }
  delegated_ink_handler_->SetDelegatedInkMetadata(std::move(metadata));
}

bool SkiaRenderer::UsingSkiaForDelegatedInk() const {
  return delegated_ink_handler_ && delegated_ink_handler_->GetInkRenderer();
}

gfx::Rect SkiaRenderer::GetCurrentFramebufferDamage() const {
  if (buffer_queue_) {
    return buffer_queue_->CurrentBufferDamage();
  } else {
    return skia_output_surface_->GetCurrentFramebufferDamage();
  }
}

void SkiaRenderer::Reshape(const OutputSurface::ReshapeParams& reshape_params) {
  if (buffer_queue_) {
    buffer_queue_->Reshape(reshape_params.size, reshape_params.color_space,
                           reshape_params.format);
  }
  // Even if we have our own BufferQueue, we still need to forward the Reshape()
  // call down to the OutputPresenter.
  skia_output_surface_->Reshape(reshape_params);
}

void SkiaRenderer::EnsureMinNumberOfBuffers(int n) {
  if (buffer_queue_) {
    buffer_queue_->EnsureMinNumberOfBuffers(n);
  } else if (skia_output_surface_->EnsureMinNumberOfBuffers(n)) {
    ReallocatedFrameBuffers();
  }
}

gpu::Mailbox SkiaRenderer::GetPrimaryPlaneOverlayTestingMailbox() {
  // For the purpose of testing the overlay configuration, the mailbox for ANY
  // buffer from BufferQueue is good enough because they're all created with
  // identical properties.
  // At the time we're testing overlays we don't yet know which mailbox will be
  // presented this frame so we'll just use the last swapped buffer. (We might
  // present a new frame's mailbox, or if we empty-swap we'll present the
  // previous frame's mailbox.)
  if (buffer_queue_) {
    return buffer_queue_->GetLastSwappedBuffer();
  } else {
    // OutputSurface::GetOverlayMailbox() returns the mailbox for the last
    // swapped buffer.
    return skia_output_surface_->GetOverlayMailbox();
  }
}

#if BUILDFLAG(IS_OZONE)
const gpu::Mailbox SkiaRenderer::GetImageMailboxForColor(
    const SkColor4f& color) {
  // Currently the Wayland protocol does not have protocol to support solid
  // color quads natively as surfaces. Here we create tiny 1x1 image buffers
  // in the color space of the frame buffer and fill them with the quad's solid
  // color. These freshly created buffers are then treated like any other
  // overlay via the mailbox interface.
  gpu::Mailbox solid_color_mailbox;
  // First try for an existing same color image.
  auto it = solid_color_buffers_.find(color.toSkColor());
  if (it != solid_color_buffers_.end()) {
    solid_color_mailbox = it->second.mailbox;
    it->second.use_count++;
  } else {
    solid_color_mailbox = skia_output_surface_->CreateSolidColorSharedImage(
        color, reshape_color_space());

    solid_color_buffers_.insert({color.toSkColor(), {solid_color_mailbox, 1}});
  }
  return solid_color_mailbox;
}

void SkiaRenderer::MaybeScheduleBackgroundImage(
    OverlayProcessorInterface::CandidateList& overlay_list) {
  if (!output_surface_->capabilities().needs_background_image) {
    return;
  }

  OverlayCandidate background_candidate;
  background_candidate.color_space = reshape_color_space();
  background_candidate.display_rect =
      gfx::RectF(gfx::SizeF(viewport_size_for_swap_buffers()));
  background_candidate.color = SkColors::kTransparent;
  background_candidate.plane_z_order = INT32_MIN;
  // ScheduleOverlays() will convert this to a buffer-backed solid color overlay
  // if necessary.
  background_candidate.is_solid_color = true;

  overlay_list.push_back(background_candidate);
}

void SkiaRenderer::MaybeDecrementSolidColorBuffers(
    std::vector<OverlayLock>& finished_locks) {
  if (output_surface_->capabilities()
          .supports_non_backed_solid_color_overlays) {
    return;
  }
  for (auto& lock : finished_locks) {
    for (auto& entry : solid_color_buffers_) {
      if (entry.second.mailbox == lock.mailbox()) {
        entry.second.use_count--;
        break;
      }
    }
  }
}
#endif  // BUILDFLAG(IS_OZONE)

SkiaRenderer::OverlayLock::OverlayLock(
    DisplayResourceProvider* resource_provider,
    ResourceId resource_id) {
  resource_lock.emplace(resource_provider, resource_id);
}

SkiaRenderer::OverlayLock::~OverlayLock() = default;

SkiaRenderer::OverlayLock::OverlayLock(SkiaRenderer::OverlayLock&& other) {
  resource_lock = std::move(other.resource_lock);

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
  render_pass_lock = std::move(other.render_pass_lock);
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
}

SkiaRenderer::OverlayLock& SkiaRenderer::OverlayLock::OverlayLock::operator=(
    SkiaRenderer::OverlayLock&& other) {
  resource_lock = std::move(other.resource_lock);

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
  render_pass_lock = std::move(other.render_pass_lock);
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)

  return *this;
}

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
SkiaRenderer::OverlayLock::OverlayLock(gpu::Mailbox mailbox) {
  render_pass_lock.emplace(mailbox);
}

bool SkiaRenderer::OverlayLockComparator::operator()(
    const OverlayLock& lhs,
    const OverlayLock& rhs) const {
  return lhs.mailbox() < rhs.mailbox();
}
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)

}  // namespace viz
