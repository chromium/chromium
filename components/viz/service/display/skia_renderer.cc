// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/skia_renderer.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bits.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/paint/render_surface_filters.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/skia_helper.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/renderer_utils.h"
#include "components/viz/service/display/resource_fence.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "skia/ext/opacity_filter_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkOverdrawCanvas.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/effects/SkOverdrawColorFilter.h"
#include "third_party/skia/include/effects/SkShaderMaskFilter.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/third_party/skcms/skcms.h"
#include "third_party/skia/src/core/SkColorFilterPriv.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"

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

bool IsTextureResource(DisplayResourceProvider* resource_provider,
                       ResourceId resource_id) {
  return !resource_provider->IsResourceSoftwareBacked(resource_id);
}

bool CanExplicitlyScissor(const DrawQuad* quad,
                          const gfx::QuadF* draw_region,
                          const gfx::Transform& contents_device_transform) {
  // PICTURE_CONTENT is not like the others, since it is executing a list of
  // draw calls into the canvas.
  if (quad->material == DrawQuad::Material::kPictureContent)
    return false;
  // Intersection with scissor and a quadrilateral is not necessarily a quad,
  // so don't complicate things
  if (draw_region)
    return false;

  // This is slightly different than
  // gfx::Transform::IsPositiveScaleAndTranslation in that it also allows zero
  // scales. This is because in the common orthographic case the z scale is 0.
  if (!contents_device_transform.IsScaleOrTranslation())
    return false;

  return contents_device_transform.matrix().get(0, 0) >= 0.0 &&
         contents_device_transform.matrix().get(1, 1) >= 0.0 &&
         contents_device_transform.matrix().get(2, 2) >= 0.0;
}

void ApplyExplicitScissor(const DrawQuad* quad,
                          const gfx::Rect& scissor_rect,
                          const gfx::Transform& device_transform,
                          unsigned* aa_flags,
                          gfx::RectF* vis_rect) {
  // Inset rectangular edges and turn off the AA for clipped edges. Operates in
  // the quad's space, so apply inverse of transform to get new scissor
  gfx::RectF scissor(scissor_rect);
  device_transform.TransformRectReverse(&scissor);

  float left_inset = scissor.x() - vis_rect->x();
  float top_inset = scissor.y() - vis_rect->y();
  float right_inset = vis_rect->right() - scissor.right();
  float bottom_inset = vis_rect->bottom() - scissor.bottom();

  if (left_inset >= kAAEpsilon) {
    *aa_flags &= ~SkCanvas::kLeft_QuadAAFlag;
  } else {
    left_inset = 0;
  }
  if (top_inset >= kAAEpsilon) {
    *aa_flags &= ~SkCanvas::kTop_QuadAAFlag;
  } else {
    top_inset = 0;
  }
  if (right_inset >= kAAEpsilon) {
    *aa_flags &= ~SkCanvas::kRight_QuadAAFlag;
  } else {
    right_inset = 0;
  }
  if (bottom_inset >= kAAEpsilon) {
    *aa_flags &= ~SkCanvas::kBottom_QuadAAFlag;
  } else {
    bottom_inset = 0;
  }

  vis_rect->Inset(left_inset, top_inset, right_inset, bottom_inset);
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
    case DrawQuad::Material::kRenderPass:
      return RenderPassDrawQuad::MaterialCast(quad)->force_anti_aliasing_off;
    case DrawQuad::Material::kSolidColor:
      return SolidColorDrawQuad::MaterialCast(quad)->force_anti_aliasing_off;
    case DrawQuad::Material::kTiledContent:
      return TileDrawQuad::MaterialCast(quad)->force_anti_aliasing_off;
    case DrawQuad::Material::kYuvVideoContent:
    case DrawQuad::Material::kStreamVideoContent:
    case DrawQuad::Material::kTextureContent:
      // This is done to match the behaviour of GLRenderer and we can revisit it
      // later.
      return true;
    default:
      return false;
  }
}

SkFilterQuality GetFilterQuality(const DrawQuad* quad) {
  bool nearest_neighbor;
  switch (quad->material) {
    case DrawQuad::Material::kPictureContent:
      nearest_neighbor = PictureDrawQuad::MaterialCast(quad)->nearest_neighbor;
      break;
    case DrawQuad::Material::kTextureContent:
      nearest_neighbor = TextureDrawQuad::MaterialCast(quad)->nearest_neighbor;
      break;
    case DrawQuad::Material::kTiledContent:
      nearest_neighbor = TileDrawQuad::MaterialCast(quad)->nearest_neighbor;
      break;
    default:
      // Other quad types do not expose filter quality, so default to bilinear
      // TODO(penghuang): figure out how to set correct filter quality for YUV
      // and video stream quads.
      nearest_neighbor = false;
      break;
  }

  return nearest_neighbor ? kNone_SkFilterQuality : kLow_SkFilterQuality;
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
  safe_texels.Inset(0.5f, 0.5f);

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
  SkColor alpha_as_color = SkColorSetA(SK_ColorWHITE, 255 * alpha);
  // MakeModeFilter treats fixed color as src, and input color as dst.
  // kDstIn is (srcAlpha * dstColor, srcAlpha * dstAlpha) so this makes the
  // output color equal to input color * alpha.
  sk_sp<SkColorFilter> opacity =
      SkColorFilters::Blend(alpha_as_color, SkBlendMode::kDstIn);
  if (in) {
    return opacity->makeComposed(std::move(in));
  } else {
    return opacity;
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

}  // namespace

// chrome style prevents this from going in skia_renderer.h, but since it
// uses base::Optional, the style also requires it to have a declared ctor
SkiaRenderer::BatchedQuadState::BatchedQuadState() = default;

// Parameters needed to draw a RenderPassDrawQuad.
struct SkiaRenderer::DrawRPDQParams {
  explicit DrawRPDQParams(const gfx::RectF& visible_rect);

  // Root of the calculated image filter DAG to be applied to the render pass.
  sk_sp<SkImageFilter> image_filter = nullptr;
  // If |image_filter| can be represented as a single color filter, this will
  // be that filter. |image_filter| will still be non-null.
  sk_sp<SkColorFilter> color_filter = nullptr;
  // Root of the calculated backdrop filter DAG to be applied to the render pass
  sk_sp<SkImageFilter> backdrop_filter = nullptr;
  // Resolved mask image and calculated transform matrix
  sk_sp<SkImage> mask_image = nullptr;
  SkMatrix mask_to_quad_matrix;
  // Backdrop border box for the render pass, to clip backdrop-filtered content
  base::Optional<gfx::RRectF> backdrop_filter_bounds;
  // The content space bounds that includes any filtered extents. If empty,
  // the draw can be skipped.
  gfx::Rect filter_bounds;
  // The additional matrix to concatenate to the SkCanvas after image filters
  // have been configured so that the DrawQuadParams geometry is properly mapped
  // (i.e. when set, |visible_rect| and |draw_region| must be pre-transformed by
  // this before |content_device_transform|).
  base::Optional<SkMatrix> bypass_transform;
  // The pre-filter clip to apply to the bypassed content of the RenderPass.
  // This limits the bypassed content to the output rect of the RenderPass; it
  // is in the same space as |backdrop_filter_bounds| and |filter_bounds|.
  base::Optional<gfx::RectF> bypass_clip;

  // True when there is an |image_filter| and it's not equivalent to
  // |color_filter|.
  bool has_complex_image_filter() const {
    return image_filter && !color_filter;
  }

  // True if the RenderPass's output rect would clip the visible contents that
  // are bypassing the renderpass' offscreen buffer.
  bool needs_bypass_clip(const gfx::RectF& content_rect) const {
    if (bypass_clip.has_value()) {
      DCHECK(bypass_transform.has_value());
      SkRect content_bounds =
          bypass_transform->mapRect(gfx::RectFToSkRect(content_rect));
      return !bypass_clip->Contains(gfx::SkRectToRectF(content_bounds));
    } else {
      return false;
    }
  }
};

SkiaRenderer::DrawRPDQParams::DrawRPDQParams(const gfx::RectF& visible_rect)
    : filter_bounds(gfx::ToEnclosingRect(visible_rect)) {}

// State calculated from a DrawQuad and current renderer state, that is common
// to all DrawQuad rendering.
struct SkiaRenderer::DrawQuadParams {
  DrawQuadParams() = default;
  DrawQuadParams(const gfx::Transform& cdt,
                 const gfx::RectF& visible_rect,
                 unsigned aa_flags,
                 SkBlendMode blend_mode,
                 float opacity,
                 SkFilterQuality filter_quality,
                 const gfx::QuadF* draw_region);

  // window_matrix * projection_matrix * quad_to_target_transform normally,
  // or quad_to_target_transform if the remaining device transform is held in
  // the DrawRPDQParams for a bypass quad.
  gfx::Transform content_device_transform;
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
  // Resolved filter quality from quad settings
  SkFilterQuality filter_quality;
  // Optional restricted draw geometry, will point to a length 4 SkPoint array
  // with its points in CW order matching Skia's vertex/edge expectations.
  base::Optional<SkDrawRegion> draw_region;
  // Optional rounded corner clip to apply. If present, it will have been
  // transformed to device space and ShouldApplyRoundedCorner returns true.
  base::Optional<gfx::RRectF> rounded_corner_bounds;
  // Optional device space clip to apply. If present, it is equal to the current
  // |scissor_rect_| of the renderer.
  base::Optional<gfx::Rect> scissor_rect;

  SkPaint paint() const {
    SkPaint p;
    p.setFilterQuality(filter_quality);
    p.setBlendMode(blend_mode);
    p.setAlphaf(opacity);
    p.setAntiAlias(aa_flags != SkCanvas::kNone_QuadAAFlags);
    return p;
  }
};

SkiaRenderer::DrawQuadParams::DrawQuadParams(const gfx::Transform& cdt,
                                             const gfx::RectF& visible_rect,
                                             unsigned aa_flags,
                                             SkBlendMode blend_mode,
                                             float opacity,
                                             SkFilterQuality filter_quality,
                                             const gfx::QuadF* draw_region)
    : content_device_transform(cdt),
      visible_rect(visible_rect),
      vis_tex_coords(visible_rect),
      aa_flags(aa_flags),
      blend_mode(blend_mode),
      opacity(opacity),
      filter_quality(filter_quality) {
  if (draw_region) {
    this->draw_region.emplace(*draw_region);
  }
}

enum class SkiaRenderer::BypassMode {
  // The RenderPass's contents' blendmode would have made a transparent black
  // image
  // and the RenderPass's own blend mode has no effect on transparent black.
  kSkip,
  // The renderPass's contents' creates a transparent image, but the
  // RenderPass's
  // own blend mode must still process the transparent pixels (e.g. certain
  // filters
  // affect transparent black).
  kDrawTransparentQuad,
  // Can draw the bypass quad with the modified parameters
  kDrawBypassQuad
};

// Scoped helper class for building SkImage from resource id.
class SkiaRenderer::ScopedSkImageBuilder {
 public:
  ScopedSkImageBuilder(SkiaRenderer* skia_renderer,
                       ResourceId resource_id,
                       SkAlphaType alpha_type = kPremul_SkAlphaType,
                       GrSurfaceOrigin origin = kTopLeft_GrSurfaceOrigin);
  ~ScopedSkImageBuilder() = default;

  const SkImage* sk_image() const { return sk_image_; }

 private:
  base::Optional<DisplayResourceProvider::ScopedReadLockSkImage> lock_;
  const SkImage* sk_image_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScopedSkImageBuilder);
};

SkiaRenderer::ScopedSkImageBuilder::ScopedSkImageBuilder(
    SkiaRenderer* skia_renderer,
    ResourceId resource_id,
    SkAlphaType alpha_type,
    GrSurfaceOrigin origin) {
  if (!resource_id)
    return;
  auto* resource_provider = skia_renderer->resource_provider_;
  DCHECK(IsTextureResource(resource_provider, resource_id));
  if (!skia_renderer->is_using_ddl()) {
    // TODO(penghuang): remove this code when DDL is used everywhere.
    lock_.emplace(resource_provider, resource_id, alpha_type, origin);
    sk_image_ = lock_->sk_image();
  } else {
    auto* image_context =
        skia_renderer->lock_set_for_external_use_->LockResource(resource_id);
    // |ImageContext::image| provides thread safety: (a) this ImageContext is
    // only accessed by GPU thread after |image| is set and (b) the fields of
    // ImageContext that are accessed by both compositor and GPU thread are no
    // longer modified after |image| is set.
    if (!image_context->has_image()) {
      image_context->set_alpha_type(alpha_type);
      image_context->set_origin(origin);
    }
    skia_renderer->skia_output_surface_->MakePromiseSkImage(image_context);
    LOG_IF(ERROR, !image_context->has_image())
        << "Failed to create the promise sk image.";
    sk_image_ = image_context->image().get();
  }
}

class SkiaRenderer::ScopedYUVSkImageBuilder {
 public:
  ScopedYUVSkImageBuilder(SkiaRenderer* skia_renderer,
                          const YUVVideoDrawQuad* quad,
                          sk_sp<SkColorSpace> dst_color_space,
                          bool has_color_conversion_filter) {
    DCHECK(skia_renderer->is_using_ddl());
    DCHECK(IsTextureResource(skia_renderer->resource_provider_,
                             quad->y_plane_resource_id()));
    DCHECK(IsTextureResource(skia_renderer->resource_provider_,
                             quad->u_plane_resource_id()));
    DCHECK(IsTextureResource(skia_renderer->resource_provider_,
                             quad->v_plane_resource_id()));
    DCHECK(quad->a_plane_resource_id() == kInvalidResourceId ||
           IsTextureResource(skia_renderer->resource_provider_,
                             quad->a_plane_resource_id()));

    SkYUVColorSpace yuv_color_space;
    if (has_color_conversion_filter) {
      yuv_color_space = kIdentity_SkYUVColorSpace;
    } else {
      yuv_color_space = kRec601_SkYUVColorSpace;
      quad->video_color_space.ToSkYUVColorSpace(&yuv_color_space);
    }

    const bool is_i420 =
        quad->u_plane_resource_id() != quad->v_plane_resource_id();
    const bool has_alpha = quad->a_plane_resource_id() != kInvalidResourceId;
    const size_t number_of_textures = (is_i420 ? 3 : 2) + (has_alpha ? 1 : 0);
    std::vector<ExternalUseClient::ImageContext*> contexts;
    contexts.reserve(number_of_textures);
    // Skia API ignores the color space information on the individual planes.
    // Dropping them here avoids some LOG spam.
    auto* y_context = skia_renderer->lock_set_for_external_use_->LockResource(
        quad->y_plane_resource_id(), true /* is_video_plane */);
    contexts.push_back(std::move(y_context));
    auto* u_context = skia_renderer->lock_set_for_external_use_->LockResource(
        quad->u_plane_resource_id(), true /* is_video_plane */);
    contexts.push_back(std::move(u_context));
    if (is_i420) {
      auto* v_context = skia_renderer->lock_set_for_external_use_->LockResource(
          quad->v_plane_resource_id(), true /* is_video_plane */);
      contexts.push_back(std::move(v_context));
    }

    if (has_alpha) {
      auto* a_context = skia_renderer->lock_set_for_external_use_->LockResource(
          quad->a_plane_resource_id(), true /* is_video_plane */);
      contexts.push_back(std::move(a_context));
    }

    sk_image_ = skia_renderer->skia_output_surface_->MakePromiseSkImageFromYUV(
        std::move(contexts), yuv_color_space, dst_color_space, has_alpha);
    LOG_IF(ERROR, !sk_image_) << "Failed to create the promise sk yuva image.";
  }

  ~ScopedYUVSkImageBuilder() = default;

  const SkImage* sk_image() const { return sk_image_.get(); }

 private:
  std::unique_ptr<DisplayResourceProvider::ScopedReadLockSkImage> lock_;
  sk_sp<SkImage> sk_image_;

  DISALLOW_COPY_AND_ASSIGN(ScopedYUVSkImageBuilder);
};

SkiaRenderer::SkiaRenderer(const RendererSettings* settings,
                           OutputSurface* output_surface,
                           DisplayResourceProvider* resource_provider,
                           SkiaOutputSurface* skia_output_surface,
                           DrawMode mode)
    : DirectRenderer(settings, output_surface, resource_provider),
      draw_mode_(mode),
      skia_output_surface_(skia_output_surface) {
  switch (draw_mode_) {
    case DrawMode::DDL: {
      DCHECK(skia_output_surface_);
      lock_set_for_external_use_.emplace(resource_provider,
                                         skia_output_surface);
      break;
    }
    case DrawMode::SKPRECORD: {
      DCHECK(output_surface_);
      context_provider_ = output_surface_->context_provider();
      const auto& context_caps = context_provider_->ContextCapabilities();
      use_swap_with_bounds_ = context_caps.swap_buffers_with_bounds;
      if (context_caps.sync_query) {
        sync_queries_ =
            base::Optional<SyncQueryCollection>(context_provider_->ContextGL());
      }
    }
  }
}

SkiaRenderer::~SkiaRenderer() = default;

class SkiaRenderer::FrameResourceFence : public ResourceFence {
 public:
  FrameResourceFence() = default;

  // ResourceFence implementation.
  void Set() override { set_ = true; }
  bool HasPassed() override { return event_.IsSignaled(); }

  bool WasSet() { return set_; }
  void Signal() { event_.Signal(); }

 private:
  ~FrameResourceFence() override = default;

  // Accessed only from compositor thread.
  bool set_ = false;

  base::WaitableEvent event_;

  DISALLOW_COPY_AND_ASSIGN(FrameResourceFence);
};

bool SkiaRenderer::CanPartialSwap() {
  if (draw_mode_ == DrawMode::DDL)
    return output_surface_->capabilities().supports_post_sub_buffer;

  if (draw_mode_ != DrawMode::SKPRECORD)
    return false;

  DCHECK(context_provider_);
  if (use_swap_with_bounds_)
    return false;

  return context_provider_->ContextCapabilities().post_sub_buffer;
}

void SkiaRenderer::BeginDrawingFrame() {
  TRACE_EVENT0("viz", "SkiaRenderer::BeginDrawingFrame");

  DCHECK(!current_frame_resource_fence_);

  // Copied from GLRenderer.
  scoped_refptr<ResourceFence> read_lock_fence;
  if (sync_queries_) {
    read_lock_fence = sync_queries_->StartNewFrame();
    current_frame_resource_fence_ = nullptr;
  } else {
    current_frame_resource_fence_ = base::MakeRefCounted<FrameResourceFence>();
    read_lock_fence = current_frame_resource_fence_;
  }
  resource_provider_->SetReadLockFence(read_lock_fence.get());

#if defined(OS_ANDROID)
  for (const auto& pass : *current_frame()->render_passes_in_draw_order) {
    for (auto* quad : pass->quad_list) {
      for (ResourceId resource_id : quad->resources)
        resource_provider_->InitializePromotionHintRequest(resource_id);
    }
  }
#endif

  if (draw_mode_ != DrawMode::SKPRECORD)
    return;

  // Insert WaitSyncTokenCHROMIUM on quad resources prior to drawing the
  // frame, so that drawing can proceed without GL context switching
  // interruptions.
  for (const auto& pass : *current_frame()->render_passes_in_draw_order) {
    for (auto* quad : pass->quad_list) {
      for (ResourceId resource_id : quad->resources)
        resource_provider_->WaitSyncToken(resource_id);
    }
  }
}

void SkiaRenderer::FinishDrawingFrame() {
  TRACE_EVENT0("viz", "SkiaRenderer::FinishDrawingFrame");
  if (sync_queries_) {
    sync_queries_->EndCurrentFrame();
  }
  current_frame_resource_fence_ = nullptr;
  current_canvas_ = nullptr;
  current_surface_ = nullptr;

  swap_buffer_rect_ = current_frame()->root_damage_rect;

  if (use_swap_with_bounds_)
    swap_content_bounds_ = current_frame()->root_content_bounds;

#if defined(OS_ANDROID)
  if (!current_frame()->overlay_list.empty()) {
    DCHECK_EQ(current_frame()->overlay_list.size(), 1u);
    overlay_resource_locks_.emplace_back(
        DisplayResourceProvider::ScopedReadLockSharedImage(
            resource_provider_,
            current_frame()->overlay_list.front().resource_id));
    skia_output_surface_->RenderToOverlay(
        overlay_resource_locks_.back()->sync_token(),
        overlay_resource_locks_.back()->mailbox(),
        ToNearestRect(current_frame()->overlay_list.front().display_rect));
  } else {
    overlay_resource_locks_.emplace_back(base::nullopt);
  }
#endif

  // TODO(weiliangc): Remove this once OverlayProcessor schedules overlays.
  if (current_frame()->output_surface_plane) {
    skia_output_surface_->ScheduleOutputSurfaceAsOverlay(
        current_frame()->output_surface_plane.value());
  }

#if defined(OS_WIN)
  // Schedule overlay planes to be presented before SwapBuffers().
  ScheduleDCLayers();
#endif
}

void SkiaRenderer::SwapBuffers(std::vector<ui::LatencyInfo> latency_info) {
  DCHECK(visible_);
  TRACE_EVENT0("viz,benchmark", "SkiaRenderer::SwapBuffers");
  OutputSurfaceFrame output_frame;
  output_frame.latency_info = std::move(latency_info);
  output_frame.size = surface_size_for_swap_buffers();
  if (use_swap_with_bounds_) {
    output_frame.content_bounds = std::move(swap_content_bounds_);
  } else if (use_partial_swap_) {
    swap_buffer_rect_.Intersect(gfx::Rect(surface_size_for_swap_buffers()));
    output_frame.sub_buffer_rect = swap_buffer_rect_;
  } else if (swap_buffer_rect_.IsEmpty() && allow_empty_swap_) {
    output_frame.sub_buffer_rect = swap_buffer_rect_;
  }

  // Unlock the overlay resource that was swapped last frame.
  if (overlay_resource_locks_.size() > 1) {
    overlay_resource_locks_.pop_front();
  }

  switch (draw_mode_) {
    case DrawMode::DDL: {
      gpu::SyncToken sync_token = skia_output_surface_->SkiaSwapBuffers(
          std::move(output_frame), has_locked_overlay_resources_);
      if (has_locked_overlay_resources_)
        lock_set_for_external_use_->UnlockResources(sync_token);
      break;
    }
    case DrawMode::SKPRECORD: {
      // write to skp files
      std::string file_name = "composited-frame.skp";
      SkFILEWStream file(file_name.c_str());
      DCHECK(file.isValid());

      auto data = root_picture_->serialize();
      file.write(data->data(), data->size());
      file.fsync();
      root_picture_ = nullptr;
      root_recorder_.reset();
    }
  }

  swap_buffer_rect_ = gfx::Rect();
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
  DCHECK(!output_surface_->HasExternalStencilTest());

  switch (draw_mode_) {
    case DrawMode::DDL: {
      root_canvas_ = skia_output_surface_->BeginPaintCurrentFrame();
      DCHECK(root_canvas_);
      break;
    }
    case DrawMode::SKPRECORD: {
      root_recorder_ = std::make_unique<SkPictureRecorder>();

      current_recorder_ = root_recorder_.get();
      current_picture_ = &root_picture_;
      root_canvas_ = root_recorder_->beginRecording(
          SkRect::MakeWH(current_frame()->device_viewport_size.width(),
                         current_frame()->device_viewport_size.height()));
      break;
    }
  }

  current_canvas_ = root_canvas_;
  current_surface_ = root_surface_.get();

  // For DDL mode, if overdraw feedback is enabled, the root canvas is the nway
  // canvas.
  if (settings_->show_overdraw_feedback && draw_mode_ != DrawMode::DDL) {
    const auto& size = current_frame()->device_viewport_size;
    overdraw_surface_ = root_canvas_->makeSurface(
        SkImageInfo::MakeA8(size.width(), size.height()));
    nway_canvas_ = std::make_unique<SkNWayCanvas>(size.width(), size.height());
    overdraw_canvas_ =
        std::make_unique<SkOverdrawCanvas>(overdraw_surface_->getCanvas());
    nway_canvas_->addCanvas(overdraw_canvas_.get());
    nway_canvas_->addCanvas(root_canvas_);
    current_canvas_ = nway_canvas_.get();
  }
}

void SkiaRenderer::BindFramebufferToTexture(const RenderPassId render_pass_id) {
  auto iter = render_pass_backings_.find(render_pass_id);
  DCHECK(render_pass_backings_.end() != iter);
  // This function is called after AllocateRenderPassResourceIfNeeded, so there
  // should be backing ready.
  RenderPassBacking& backing = iter->second;
  switch (draw_mode_) {
    case DrawMode::DDL: {
      current_canvas_ = skia_output_surface_->BeginPaintRenderPass(
          render_pass_id, backing.size, backing.format, backing.generate_mipmap,
          backing.color_space.ToSkColorSpace());
      break;
    }
    case DrawMode::SKPRECORD: {
      current_recorder_ = backing.recorder.get();
      current_picture_ = &backing.picture;
      current_canvas_ = current_recorder_->beginRecording(
          SkRect::MakeWH(backing.size.width(), backing.size.height()));
    }
  }
}

void SkiaRenderer::SetScissorTestRect(const gfx::Rect& scissor_rect) {
  is_scissor_enabled_ = true;
  scissor_rect_ = scissor_rect;
}

void SkiaRenderer::ClearCanvas(SkColor color) {
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
    ClearCanvas(SkColorSetARGB(0, 0, 0, 0));
  } else {
#if DCHECK_IS_ON()
    // On DEBUG builds, opaque render passes are cleared to blue
    // to easily see regions that were not drawn on the screen.
    ClearCanvas(SkColorSetARGB(255, 0, 0, 255));
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
  gfx::Transform target_to_device =
      current_frame()->window_matrix * current_frame()->projection_matrix;
  const gfx::Rect* scissor = is_scissor_enabled_ ? &scissor_rect_ : nullptr;
  DrawQuadParams params =
      CalculateDrawQuadParams(target_to_device, scissor, quad, draw_region);
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
    case DrawQuad::Material::kRenderPass:
      // RPDQ should only ever be encountered as a top-level quad, not when
      // bypassing another renderpass
      DCHECK(rpdq_params == nullptr);
      DrawRenderPassQuad(RenderPassDrawQuad::MaterialCast(quad), params);
      break;
    case DrawQuad::Material::kSolidColor:
      DrawSolidColorQuad(SolidColorDrawQuad::MaterialCast(quad), rpdq_params,
                         params);
      break;
    case DrawQuad::Material::kStreamVideoContent:
      DrawStreamVideoQuad(StreamVideoDrawQuad::MaterialCast(quad), rpdq_params,
                          params);
      break;
    case DrawQuad::Material::kTextureContent:
      DrawTextureQuad(TextureDrawQuad::MaterialCast(quad), rpdq_params, params);
      break;
    case DrawQuad::Material::kTiledContent:
      DrawTileDrawQuad(TileDrawQuad::MaterialCast(quad), rpdq_params, params);
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
    const base::Optional<gfx::Rect>& scissor_rect,
    const base::Optional<gfx::RRectF>& rounded_corner_bounds,
    const gfx::Transform* cdt) {
  // Scissor is applied in the device space (CTM == I) and since no changes
  // to the canvas persist, CTM should already be the identity
  DCHECK(current_canvas_->getTotalMatrix() == SkMatrix::I());

  if (scissor_rect.has_value()) {
    current_canvas_->clipRect(gfx::RectToSkRect(*scissor_rect));
  }

  if (rounded_corner_bounds.has_value())
    current_canvas_->clipRRect(SkRRect(*rounded_corner_bounds), true /* AA */);

  if (cdt) {
    SkMatrix m;
    gfx::TransformToFlattenedSkMatrix(*cdt, &m);
    current_canvas_->concat(m);
  }
}

void SkiaRenderer::PrepareCanvasForRPDQ(const DrawRPDQParams& rpdq_params,
                                        DrawQuadParams* params) {
  // Clip to the filter bounds prior to saving the layer, which has been
  // constructed to contain the actual filtered contents (visually no
  // clipping effect, but lets Skia minimize internal layer size).
  bool aa = params->aa_flags != SkCanvas::kNone_QuadAAFlags;
  current_canvas_->clipRect(gfx::RectToSkRect(rpdq_params.filter_bounds), aa);

  SkPaint layer_paint = params->paint();
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

  SkRect bounds = gfx::RectFToSkRect(rpdq_params.bypass_clip.has_value()
                                         ? *rpdq_params.bypass_clip
                                         : params->visible_rect);
  current_canvas_->saveLayer(SkCanvas::SaveLayerRec(
      &bounds, &layer_paint, rpdq_params.backdrop_filter.get(),
      rpdq_params.mask_image.get(), &rpdq_params.mask_to_quad_matrix, 0));

  // If we have backdrop filtered content (and not transparent black like with
  // regular render passes), we have to clear out the parts of the layer that
  // shouldn't show the backdrop
  if (rpdq_params.backdrop_filter &&
      (rpdq_params.backdrop_filter_bounds.has_value() ||
       params->draw_region.has_value())) {
    current_canvas_->save();
    if (rpdq_params.backdrop_filter_bounds.has_value()) {
      current_canvas_->clipRRect(SkRRect(*rpdq_params.backdrop_filter_bounds),
                                 SkClipOp::kDifference, aa);
    }
    if (params->draw_region.has_value()) {
      SkPath clipPath;
      clipPath.addPoly(params->draw_region->points, 4, true /* close */);
      if (rpdq_params.bypass_transform.has_value()) {
        clipPath.transform(*rpdq_params.bypass_transform);
      }
      current_canvas_->clipPath(clipPath, SkClipOp::kDifference, aa);
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
  // 2. A complex image filter needs a layer if there is a mask, since Skia
  // applies the mask filter before the paint's image filter.
  // 3. The content bypassing the renderpass needs to be clipped before the
  //    image filter is evaluated.
  bool needs_bypass_clip = rpdq_params.needs_bypass_clip(params->visible_rect);
  bool needs_save_layer = false;
  if (rpdq_params.backdrop_filter)
    needs_save_layer = true;
  else if (rpdq_params.has_complex_image_filter())
    needs_save_layer = rpdq_params.mask_image || needs_bypass_clip;

  if (needs_save_layer) {
    PrepareCanvasForRPDQ(rpdq_params, params);
    // Sync the content's paint with the updated |params|
    paint->setAlphaf(params->opacity);
    paint->setBlendMode(params->blend_mode);
  } else {
    // At this point, the image filter and/or color filter can be set on the
    // paint. If there is a mask image, it can be converted to a mask shader.
    DCHECK(!rpdq_params.backdrop_filter);
    if (rpdq_params.color_filter) {
      // Use the color filter directly, instead of the image filter; since color
      // filters are applied before masks, this could be combined with a mask
      if (paint->getColorFilter()) {
        paint->setColorFilter(
            rpdq_params.color_filter->makeComposed(paint->refColorFilter()));
      } else {
        paint->setColorFilter(rpdq_params.color_filter);
      }
      DCHECK(paint->getColorFilter());
    } else if (rpdq_params.image_filter) {
      // Store the image filter on the paint, but since this effect is applied
      // last it's not compatible with masks as a shader
      DCHECK(!rpdq_params.mask_image);
      if (params->opacity != 1.f) {
        // Apply opacity as the last step of image filter so it is uniform
        // across any overlapping content produced by the image filters.
        paint->setImageFilter(SkColorFilterImageFilter::Make(
            MakeOpacityFilter(params->opacity, nullptr),
            rpdq_params.image_filter));
        paint->setAlphaf(1.f);
        params->opacity = 1.f;
      } else {
        paint->setImageFilter(rpdq_params.image_filter);
      }
    }

    // This is not an else-if since the color filter image filter can be
    // combined with the mask filter correctly.
    if (rpdq_params.mask_image) {
      // The mask shader is evaluated in the bypass'ed quad's local coordinate
      // space, so must update the shader matrix to still sample the image in
      // the RP coordinate space.
      SkMatrix local_matrix = SkMatrix::I();
      if (rpdq_params.bypass_transform.has_value()) {
        bool inverted = rpdq_params.bypass_transform->invert(&local_matrix);
        // Invertibility was a requirement for being bypassable.
        DCHECK(inverted);
      }
      local_matrix.preConcat(rpdq_params.mask_to_quad_matrix);

      auto mask_filter = SkShaderMaskFilter::Make(
          rpdq_params.mask_image->makeShader(&local_matrix));
      // Confirm that the paint didn't already have a mask filter, and that it's
      // captured properly afterwards on the paint.
      DCHECK(!paint->getMaskFilter());
      paint->setMaskFilter(std::move(mask_filter));
      DCHECK(paint->getMaskFilter());
    }
  }

  // Whether or not we saved a layer, clip the bypassed RenderPass's content
  if (needs_bypass_clip) {
    current_canvas_->clipRect(gfx::RectFToSkRect(*rpdq_params.bypass_clip),
                              params->aa_flags != SkCanvas::kNone_QuadAAFlags);
  }
}

void SkiaRenderer::PrepareColorOrCanvasForRPDQ(
    const DrawRPDQParams& rpdq_params,
    DrawQuadParams* params,
    SkColor* content_color) {
  // When the draw call only takes a color and not an SkPaint, rpdq params
  // with just a color filter can be handled directly. Otherwise, the rpdq
  // params must use a layer on the canvas.
  bool needs_save_layer = rpdq_params.has_complex_image_filter() ||
                          rpdq_params.backdrop_filter || rpdq_params.mask_image;
  if (needs_save_layer) {
    PrepareCanvasForRPDQ(rpdq_params, params);
  } else if (rpdq_params.color_filter) {
    // At this point, the RPDQ effect is at most a color filter, so it can
    // modify |content_color| directly.
    *content_color = rpdq_params.color_filter->filterColor(*content_color);
  }

  // Even if the color filter image filter was applied to the content color
  // directly (so no explicit save layer), the draw may need to be clipped to
  // the output rect of the renderpass it is bypassing.
  if (rpdq_params.needs_bypass_clip(params->visible_rect)) {
    current_canvas_->clipRect(gfx::RectFToSkRect(*rpdq_params.bypass_clip),
                              params->aa_flags != SkCanvas::kNone_QuadAAFlags);
  }
}

SkiaRenderer::DrawQuadParams SkiaRenderer::CalculateDrawQuadParams(
    const gfx::Transform& target_to_device,
    const gfx::Rect* scissor_rect,
    const DrawQuad* quad,
    const gfx::QuadF* draw_region) const {
  DrawQuadParams params(
      target_to_device * quad->shared_quad_state->quad_to_target_transform,
      gfx::RectF(quad->visible_rect), SkCanvas::kNone_QuadAAFlags,
      quad->shared_quad_state->blend_mode, quad->shared_quad_state->opacity,
      GetFilterQuality(quad), draw_region);

  params.content_device_transform.FlattenTo2d();

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

  // Applying the scissor explicitly means avoiding a clipRect() call and
  // allows more quads to be batched together in a DrawEdgeAAImageSet call
  if (scissor_rect) {
    if (CanExplicitlyScissor(quad, draw_region,
                             params.content_device_transform)) {
      ApplyExplicitScissor(quad, *scissor_rect, params.content_device_transform,
                           &params.aa_flags, &params.visible_rect);
      params.vis_tex_coords = params.visible_rect;
    } else {
      params.scissor_rect = *scissor_rect;
    }
  }

  // Determine final rounded rect clip geometry. We transform it from target
  // space to window space to make batching and canvas preparation easier
  // (otherwise we'd have to separate those two matrices in the CDT).
  if (ShouldApplyRoundedCorner(quad)) {
    // Transform by the window and projection matrix to go from target to
    // device space, which should always be a scale+translate.
    SkRRect corner_bounds =
        SkRRect(quad->shared_quad_state->rounded_corner_bounds);
    SkMatrix to_device;
    gfx::TransformToFlattenedSkMatrix(target_to_device, &to_device);

    SkRRect device_bounds;
    bool success = corner_bounds.transform(to_device, &device_bounds);
    // Since to_device should just be scale+translate, transform always succeeds
    DCHECK(success);
    if (!device_bounds.isEmpty()) {
      params.rounded_corner_bounds.emplace(device_bounds);
    }
  }

  return params;
}

const DrawQuad* SkiaRenderer::CanPassBeDrawnDirectly(const RenderPass* pass) {
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
  if (quad->material == DrawQuad::Material::kRenderPass ||
      quad->material == DrawQuad::Material::kDebugBorder ||
      quad->material == DrawQuad::Material::kPictureContent)
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
  if (!quad->shared_quad_state->quad_to_target_transform.IsInvertible())
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
  SkMatrix bypass_to_rpdq;
  gfx::TransformToFlattenedSkMatrix(
      bypass_quad->shared_quad_state->quad_to_target_transform,
      &bypass_to_rpdq);
  bool inverted = bypass_to_rpdq.invert(&rpdq_to_bypass);
  // Invertibility was a requirement for being bypassable.
  DCHECK(inverted);

  if (params->draw_region.has_value()) {
    // The draw region was determined by the RPDQ's geometry, so map the
    // quadrilateral to the bypass'ed quad's coordinate space so that BSP
    // splitting is still respected.
    rpdq_to_bypass.mapPoints(params->draw_region->points, 4);
  }

  // Compute draw params for the bypass quad from scratch, but since the
  // bypass_quad would have originally been drawn into the RP, the
  // target_to_device transform is the full transform of the RPDQ. Must also
  // include the RP's output rect as part of the scissor rect, since it would
  // have been clipped to the edges of the RP's offscreen buffer normally.
  DrawQuadParams bypass_params = CalculateDrawQuadParams(
      gfx::Transform() /* identity */, nullptr, bypass_quad, nullptr);

  // |params| already holds the correct |draw_region|, but must be updated to
  // use the bypassed transform and geometry
  rpdq_params->bypass_transform = bypass_to_rpdq;
  rpdq_params->bypass_clip = params->visible_rect;
  // NOTE: params |content_device_transform| remains that of the RPDQ to prepare
  // the canvas' CTM to match what any image filters require. The above
  // bypass_transform is then applied when drawing so that these updated
  // coordinates are correctly transformed to device space.
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
  // RP results itself.
  if (bypass_params.filter_quality > params->filter_quality)
    params->filter_quality = bypass_params.filter_quality;

  // Rounded corner bounds are in device space, which gets tricky when bypassing
  // the device that the RP would have represented
  DCHECK(!bypass_params.rounded_corner_bounds.has_value());
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
      params->filter_quality == kNone_SkFilterQuality) {
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
  if (!params->draw_region.has_value()) {
    params->draw_region.emplace(gfx::QuadF(params->visible_rect));
  }

  // Preserve the src-to-dst transformation for the padded texture coords
  SkMatrix src_to_dst = SkMatrix::MakeRectToRect(
      gfx::RectFToSkRect(params->vis_tex_coords),
      gfx::RectFToSkRect(params->visible_rect), SkMatrix::kFill_ScaleToFit);
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

  if (new_quad->material != DrawQuad::Material::kRenderPass &&
      new_quad->material != DrawQuad::Material::kStreamVideoContent &&
      new_quad->material != DrawQuad::Material::kTextureContent &&
      new_quad->material != DrawQuad::Material::kTiledContent)
    return true;

  if (batched_quad_state_.blend_mode != params.blend_mode ||
      batched_quad_state_.filter_quality != params.filter_quality)
    return true;

  if (batched_quad_state_.scissor_rect != params.scissor_rect) {
    return true;
  }

  if (batched_quad_state_.rounded_corner_bounds !=
      params.rounded_corner_bounds) {
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
    batched_quad_state_.rounded_corner_bounds = params->rounded_corner_bounds;
    batched_quad_state_.blend_mode = params->blend_mode;
    batched_quad_state_.filter_quality = params->filter_quality;
    batched_quad_state_.constraint = constraint;
  }
  DCHECK(batched_quad_state_.constraint == constraint);

  // Add entry, with optional clip quad and shared transform
  if (params->draw_region.has_value()) {
    for (int i = 0; i < 4; ++i) {
      batched_draw_regions_.push_back(params->draw_region->points[i]);
    }
  }

  SkMatrix m;
  gfx::TransformToFlattenedSkMatrix(params->content_device_transform, &m);
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
                batched_quad_state_.rounded_corner_bounds, nullptr);

  SkPaint paint;
  paint.setFilterQuality(batched_quad_state_.filter_quality);
  paint.setBlendMode(batched_quad_state_.blend_mode);
  current_canvas_->experimental_DrawEdgeAAImageSet(
      &batched_quads_.front(), batched_quads_.size(),
      batched_draw_regions_.data(), &batched_cdt_matrices_.front(), &paint,
      batched_quad_state_.constraint);

  batched_quads_.clear();
  batched_draw_regions_.clear();
  batched_cdt_matrices_.clear();
}

void SkiaRenderer::DrawColoredQuad(SkColor color,
                                   const DrawRPDQParams* rpdq_params,
                                   DrawQuadParams* params) {
  DCHECK(batched_quads_.empty());
  TRACE_EVENT0("viz", "SkiaRenderer::DrawColoredQuad");

  SkAutoCanvasRestore acr(current_canvas_, true /* do_save */);
  PrepareCanvas(params->scissor_rect, params->rounded_corner_bounds,
                &params->content_device_transform);

  if (rpdq_params) {
    // This will modify the provided content color as needed for the RP effects,
    // or it will make an explicit save layer on the current canvas
    PrepareColorOrCanvasForRPDQ(*rpdq_params, params, &color);
    if (rpdq_params->bypass_transform.has_value()) {
      // Concatenate the bypass'ed quad's transform after all the RPDQ state
      // has been pushed to the canvas.
      current_canvas_->concat(*rpdq_params->bypass_transform);
    }
  }

  // PrepareCanvasForRPDQ will have updated params->opacity and blend_mode to
  // account for the layer applying those effects.
  color = SkColorSetA(color, params->opacity * SkColorGetA(color));
  const SkPoint* draw_region =
      params->draw_region.has_value() ? params->draw_region->points : nullptr;

  SkColor4f color4f = SkColor4f::FromColor(color);
  float sdr_white_level = current_frame()->sdr_white_level;
  if (sdr_white_level != gfx::ColorSpace::kDefaultSDRWhiteLevel) {
    // SkColor is always sRGB. Use skcms to linearize, scale, and re-encode
    const float scale_factor =
        sdr_white_level / gfx::ColorSpace::kDefaultSDRWhiteLevel;
    const auto* srgb_to_linear = skcms_sRGB_TransferFunction();
    const auto* linear_to_srgb = skcms_sRGB_Inverse_TransferFunction();

    for (int c = 0; c < 3; ++c) {
      color4f[c] = skcms_TransferFunction_eval(
          linear_to_srgb, scale_factor * skcms_TransferFunction_eval(
                                             srgb_to_linear, color4f[c]));
    }
  }

  current_canvas_->experimental_DrawEdgeAAQuad(
      gfx::RectFToSkRect(params->visible_rect), draw_region,
      static_cast<SkCanvas::QuadAAFlags>(params->aa_flags), color4f,
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
  PrepareCanvas(params->scissor_rect, params->rounded_corner_bounds,
                &params->content_device_transform);

  int matrix_index = -1;
  const SkMatrix* bypass_transform = nullptr;
  if (rpdq_params) {
    // This will modify the provided content paint as needed for the RP effects,
    // or it will make an explicit save layer on the current canvas
    PreparePaintOrCanvasForRPDQ(*rpdq_params, params, paint);
    if (rpdq_params->bypass_transform.has_value()) {
      // Incorporate the bypass transform, but unlike for solid color quads, do
      // not modify the SkCanvas's CTM. This is because the RPDQ's filters may
      // have been optimally placed on the SkPaint of the draw, which means the
      // canvas' transform must match that of the RenderPass. The pre-CTM matrix
      // of the image set entry can be used instead to modify the drawn geometry
      // without impacting the filter's coordinate space.
      bypass_transform = &(*rpdq_params->bypass_transform);
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
      params->draw_region.has_value() ? params->draw_region->points : nullptr;
  current_canvas_->experimental_DrawEdgeAAImageSet(
      &entry, 1, draw_region, bypass_transform, paint, constraint);
}

void SkiaRenderer::DrawDebugBorderQuad(const DebugBorderDrawQuad* quad,
                                       DrawQuadParams* params) {
  DCHECK(batched_quads_.empty());

  SkAutoCanvasRestore acr(current_canvas_, true /* do_save */);
  // We need to apply the matrix manually to have pixel-sized stroke width.
  PrepareCanvas(params->scissor_rect, params->rounded_corner_bounds, nullptr);
  SkMatrix cdt;
  gfx::TransformToFlattenedSkMatrix(params->content_device_transform, &cdt);

  SkPath path;
  if (params->draw_region.has_value()) {
    path.addPoly(params->draw_region->points, 4, true /* close */);
  } else {
    path.addRect(gfx::RectFToSkRect(params->visible_rect));
  }
  path.transform(cdt);

  SkPaint paint = params->paint();
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
  const bool disable_image_filtering =
      disable_picture_quad_image_filtering_ ||
      params->filter_quality == kNone_SkFilterQuality;

  SkAutoCanvasRestore acr(current_canvas_, true /* do_save */);
  PrepareCanvas(params->scissor_rect, params->rounded_corner_bounds,
                &params->content_device_transform);

  // Unlike other quads which draw visible_rect or draw_region as their geometry
  // these represent the valid windows of content to show for the display list,
  // so they need to be used as a clip in Skia.
  SkRect visible_rect = gfx::RectFToSkRect(params->visible_rect);
  SkPaint paint = params->paint();
  if (params->draw_region.has_value()) {
    SkPath clip;
    clip.addPoly(params->draw_region->points, 4, true /* close */);
    current_canvas_->clipPath(clip, paint.isAntiAlias());
  } else {
    current_canvas_->clipRect(visible_rect, paint.isAntiAlias());
  }

  if (needs_transparency) {
    // Use the DrawQuadParams' paint for the layer, since that will affect the
    // final draw of the backing image.
    current_canvas_->saveLayer(&visible_rect, &paint);
  }

  SkCanvas* raster_canvas = current_canvas_;
  base::Optional<skia::OpacityFilterCanvas> opacity_canvas;
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

  raster_canvas->concat(SkMatrix::MakeRectToRect(
      gfx::RectFToSkRect(quad->tex_coord_rect), gfx::RectToSkRect(quad->rect),
      SkMatrix::kFill_ScaleToFit));

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

void SkiaRenderer::DrawStreamVideoQuad(const StreamVideoDrawQuad* quad,
                                       const DrawRPDQParams* rpdq_params,
                                       DrawQuadParams* params) {
  DCHECK(!MustFlushBatchedQuads(quad, rpdq_params, *params));

  ScopedSkImageBuilder builder(this, quad->resource_id(),
                               kUnpremul_SkAlphaType);
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

  if (rpdq_params) {
    SkPaint paint = params->paint();
    DrawSingleImage(image, valid_texel_bounds, rpdq_params, &paint, params);
  } else {
    AddQuadToBatch(image, valid_texel_bounds, params);
  }
}

void SkiaRenderer::DrawTextureQuad(const TextureDrawQuad* quad,
                                   const DrawRPDQParams* rpdq_params,
                                   DrawQuadParams* params) {
  ScopedSkImageBuilder builder(
      this, quad->resource_id(),
      quad->premultiplied_alpha ? kPremul_SkAlphaType : kUnpremul_SkAlphaType,
      quad->y_flipped ? kBottomLeft_GrSurfaceOrigin : kTopLeft_GrSurfaceOrigin);
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

  // There are two scenarios where a texture quad cannot be put into a batch:
  // 1. It needs to be blended with a constant background color.
  // 2. The vertex opacities are not all 1s.
  bool blend_background =
      quad->background_color != SK_ColorTRANSPARENT && !image->isOpaque();
  bool vertex_alpha =
      quad->vertex_opacity[0] < 1.f || quad->vertex_opacity[1] < 1.f ||
      quad->vertex_opacity[2] < 1.f || quad->vertex_opacity[3] < 1.f;

  if (!blend_background && !vertex_alpha && !rpdq_params) {
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

  SkPaint paint = params->paint();
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
      SkColor gradient_colors[2] = {SkColor4f({a1, a1, a1, a1}).toSkColor(),
                                    SkColor4f({a2, a2, a2, a2}).toSkColor()};
      sk_sp<SkShader> gradient = SkGradientShader::MakeLinear(
          gradient_pts, gradient_colors, nullptr, 2, SkTileMode::kClamp);
      paint.setMaskFilter(SkShaderMaskFilter::Make(std::move(gradient)));
      // shared quad opacity was folded into the gradient, so this will shorten
      // any color filter chain needed for background blending
      quad_alpha = 1.f;
    }
  }

  // From gl_renderer, the final src color will be
  // vertexAlpha * (textureColor + backgroundColor * (1 - textureAlpha)), where
  // vertexAlpha is the quad's alpha * interpolated per-vertex alpha
  if (blend_background) {
    // Add a color filter that does DstOver blending between texture and the
    // background color. Then, modulate by quad's opacity *after* blending.
    sk_sp<SkColorFilter> cf =
        SkColorFilters::Blend(quad->background_color, SkBlendMode::kDstOver);
    if (quad_alpha < 1.f) {
      cf = MakeOpacityFilter(quad_alpha, std::move(cf));
      quad_alpha = 1.f;
      DCHECK(cf);
    }
    paint.setColorFilter(std::move(cf));
  }

  if (!rpdq_params) {
    // Reset the paint's alpha, since it started as params.opacity and that
    // is now applied outside of the paint's alpha.
    paint.setAlphaf(quad_alpha);
  }

  DrawSingleImage(image, valid_texel_bounds, rpdq_params, &paint, params);
}

void SkiaRenderer::DrawTileDrawQuad(const TileDrawQuad* quad,
                                    const DrawRPDQParams* rpdq_params,
                                    DrawQuadParams* params) {
  DCHECK(!MustFlushBatchedQuads(quad, rpdq_params, *params));
  // |resource_provider_| can be NULL in resourceless software draws, which
  // should never produce tile quads in the first place.
  DCHECK(resource_provider_);
  ScopedSkImageBuilder builder(
      this, quad->resource_id(),
      quad->is_premultiplied ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
  const SkImage* image = builder.sk_image();
  if (!image)
    return;

  params->vis_tex_coords = cc::MathUtil::ScaleRectProportional(
      quad->tex_coord_rect, gfx::RectF(quad->rect), params->visible_rect);
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
    SkPaint paint = params->paint();
    DrawSingleImage(image, valid_texel_bounds, rpdq_params, &paint, params);
  } else {
    AddQuadToBatch(image, valid_texel_bounds, params);
  }
}

void SkiaRenderer::DrawYUVVideoQuad(const YUVVideoDrawQuad* quad,
                                    const DrawRPDQParams* rpdq_params,
                                    DrawQuadParams* params) {
  // Since YUV quads always use a color filter, they require a complex skPaint
  // that precludes batching. If this changes, we could add YUV quads that don't
  // require a filter to the batch instead of drawing one at a time.
  DCHECK(batched_quads_.empty());
  if (draw_mode_ != DrawMode::DDL) {
    NOTIMPLEMENTED();
    return;
  }

  gfx::ColorSpace src_color_space = quad->video_color_space;
  // Invalid or unspecified color spaces should be treated as REC709.
  if (!src_color_space.IsValid())
    src_color_space = gfx::ColorSpace::CreateREC709();
  gfx::ColorSpace dst_color_space =
      current_frame()->current_render_pass->color_space;
  sk_sp<SkColorFilter> color_filter =
      GetColorFilter(src_color_space, dst_color_space, quad->resource_offset,
                     quad->resource_multiplier);

  DCHECK(resource_provider_);
  ScopedYUVSkImageBuilder builder(this, quad, dst_color_space.ToSkColorSpace(),
                                  !!color_filter);
  const SkImage* image = builder.sk_image();
  if (!image)
    return;

  params->vis_tex_coords = cc::MathUtil::ScaleRectProportional(
      quad->ya_tex_coord_rect, gfx::RectF(quad->rect), params->visible_rect);

  SkPaint paint = params->paint();
  if (color_filter)
    paint.setColorFilter(color_filter);

  // Use provided, unclipped texture coordinates as the content area, which will
  // force coord clamping unless the geometry was clipped, or they span the
  // entire YUV image.
  DrawSingleImage(image, quad->ya_tex_coord_rect, rpdq_params, &paint, params);
}

void SkiaRenderer::DrawUnsupportedQuad(const DrawQuad* quad,
                                       const DrawRPDQParams* rpdq_params,
                                       DrawQuadParams* params) {
#ifdef NDEBUG
  DrawColoredQuad(SK_ColorWHITE, rpdq_params, params);
#else
  DrawColoredQuad(SK_ColorMAGENTA, rpdq_params, params);
#endif
}

#if defined(OS_WIN)
void SkiaRenderer::ScheduleDCLayers() {
  if (current_frame()->overlay_list.empty())
    return;

  std::vector<gpu::SyncToken> sync_tokens;
  for (auto& dc_layer_overlay : current_frame()->overlay_list) {
    for (size_t i = 0; i < DCLayerOverlay::kNumResources; ++i) {
      ResourceId resource_id = dc_layer_overlay.resources[i];
      if (resource_id == kInvalidResourceId)
        break;

      // Resources will be unlocked after the next call to SwapBuffers().
      auto* image_context =
          lock_set_for_external_use_->LockResource(resource_id, true);

      // Sync tokens ensure the texture to be overlaid is available before
      // scheduling it for display.
      DCHECK(image_context->mailbox_holder().sync_token.HasData());
      sync_tokens.push_back(image_context->mailbox_holder().sync_token);

      dc_layer_overlay.mailbox[i] = image_context->mailbox_holder().mailbox;
    }
    DCHECK(!dc_layer_overlay.mailbox[0].IsZero());
  }

  has_locked_overlay_resources_ = true;
  skia_output_surface_->ScheduleDCLayers(
      std::move(current_frame()->overlay_list), std::move(sync_tokens));
}
#endif

sk_sp<SkColorFilter> SkiaRenderer::GetColorFilter(const gfx::ColorSpace& src,
                                                  const gfx::ColorSpace& dst,
                                                  float resource_offset,
                                                  float resource_multiplier) {
  gfx::ColorSpace adjusted_src = src;
  float sdr_white_level = current_frame()->sdr_white_level;
  if (src.IsValid() && !src.IsHDR() &&
      sdr_white_level != gfx::ColorSpace::kDefaultSDRWhiteLevel) {
    adjusted_src = src.GetScaledColorSpace(
        sdr_white_level / gfx::ColorSpace::kDefaultSDRWhiteLevel);
  }

  std::unique_ptr<SkRuntimeColorFilterFactory>& factory =
      color_filter_cache_[dst][adjusted_src];
  if (!factory) {
    std::unique_ptr<gfx::ColorTransform> transform =
        gfx::ColorTransform::NewColorTransform(
            adjusted_src, dst, gfx::ColorTransform::Intent::INTENT_PERCEPTUAL);
    // TODO(backer): Support lookup table transforms (e.g.
    // COLOR_CONVERSION_MODE_LUT).
    if (!transform->CanGetShaderSource())
      return nullptr;

    const char* hdr = R"(
layout(ctype=float) uniform half offset;
layout(ctype=float) uniform half multiplier;

void main(inout half4 color) {
  // un-premultiply alpha
  if (color.a > 0)
    color.rgb /= color.a;

  color.rgb -= offset;
  color.rgb *= multiplier;
)";
    const char* ftr = R"(
  // premultiply alpha
  color.rgb *= color.a;
}
)";

    std::string shader = hdr + transform->GetSkShaderSource() + ftr;

    factory.reset(new SkRuntimeColorFilterFactory(
        SkString(shader.c_str(), shader.size())));
  }

  YUVInput input;
  input.offset = resource_offset;
  input.multiplier = resource_multiplier;
  sk_sp<SkData> data = SkData::MakeWithCopy(&input, sizeof(input));

  return factory->make(std::move(data));
}

SkiaRenderer::DrawRPDQParams SkiaRenderer::CalculateRPDQParams(
    const RenderPassDrawQuad* quad,
    DrawQuadParams* params) {
  DrawRPDQParams rpdq_params(params->visible_rect);

  // Prepare mask.
  ResourceId mask_resource_id = quad->mask_resource_id();
  ScopedSkImageBuilder mask_image_builder(this, mask_resource_id);
  const SkImage* mask_image = mask_image_builder.sk_image();
  DCHECK_EQ(!!mask_resource_id, !!mask_image);
  if (mask_image) {
    rpdq_params.mask_image = sk_ref_sp(mask_image);

    // Scale normalized uv rect into absolute texel coordinates.
    SkRect mask_rect = gfx::RectFToSkRect(
        gfx::ScaleRect(quad->mask_uv_rect, quad->mask_texture_size.width(),
                       quad->mask_texture_size.height()));
    // Map to full quad rect so that mask coordinates don't change with clipping
    rpdq_params.mask_to_quad_matrix = SkMatrix::MakeRectToRect(
        mask_rect, gfx::RectToSkRect(quad->rect), SkMatrix::kFill_ScaleToFit);
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
      gfx::Rect clip_rect = quad->shared_quad_state->clip_rect;
      if (clip_rect.IsEmpty()) {
        clip_rect = current_draw_rect_;
      }
      const gfx::Transform& transform =
          quad->shared_quad_state->quad_to_target_transform;
      gfx::QuadF clip_quad = gfx::QuadF(gfx::RectF(clip_rect));
      gfx::QuadF local_clip =
          cc::MathUtil::InverseMapQuadToLocalSpace(transform, clip_quad);

      rpdq_params.filter_bounds.Intersect(
          gfx::ToEnclosingRect(local_clip.BoundingBox()));
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
  if (backdrop_filters) {
    DCHECK(!backdrop_filters->IsEmpty());

    // Must account for clipping that occurs for backdrop filters, since their
    // input content has already been clipped to the output rect.
    gfx::Rect deviceRect = gfx::ToEnclosingRect(cc::MathUtil::MapClippedRect(
        params->content_device_transform, gfx::RectF(quad->rect)));
    gfx::Rect outRect = MoveFromDrawToWindowSpace(
        current_frame()->current_render_pass->output_rect);
    outRect.Intersect(deviceRect);
    gfx::Vector2dF offset = (deviceRect.top_right() - outRect.top_right()) +
                            (deviceRect.bottom_left() - outRect.bottom_left());

    auto bg_paint_filter = cc::RenderSurfaceFilters::BuildImageFilter(
        *backdrop_filters, gfx::SizeF(outRect.size()), offset);
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
    const base::Optional<gfx::RRectF> backdrop_filter_bounds =
        BackdropFilterBoundsForPass(quad->render_pass_id);
    if (backdrop_filter_bounds) {
      rpdq_params.backdrop_filter_bounds = *backdrop_filter_bounds;
      // Scale by the filter's scale, but don't apply filter origin
      rpdq_params.backdrop_filter_bounds->Scale(quad->filters_scale.x(),
                                                quad->filters_scale.y());

      // If there are also regular image filters, they apply to the area of
      // the backdrop_filter_bounds too, so expand the backdrop bounds and join
      // it with the main filter bounds.
      if (rpdq_params.image_filter) {
        gfx::Rect backdrop_rect =
            gfx::ToEnclosingRect(rpdq_params.backdrop_filter_bounds->rect());
        rpdq_params.filter_bounds.Union(
            filters->MapRect(backdrop_rect, local_matrix));
      }
    }
  }

  return rpdq_params;
}

void SkiaRenderer::DrawRenderPassQuad(const RenderPassDrawQuad* quad,
                                      DrawQuadParams* params) {
  DrawRPDQParams rpdq_params = CalculateRPDQParams(quad, params);
  if (rpdq_params.filter_bounds.IsEmpty())
    return;

  auto bypass = render_pass_bypass_quads_.find(quad->render_pass_id);
  // When Render Pass has a single quad inside we would draw that directly.
  if (bypass != render_pass_bypass_quads_.end()) {
    BypassMode mode =
        CalculateBypassParams(bypass->second, &rpdq_params, params);
    if (mode == BypassMode::kDrawTransparentQuad) {
      DrawColoredQuad(SK_ColorTRANSPARENT, &rpdq_params, params);
    } else if (mode == BypassMode::kDrawBypassQuad) {
      DrawQuadInternal(bypass->second, &rpdq_params, params);
    }  // else mode == kSkip
    return;
  }

  // A real render pass that was turned into an image
  auto iter = render_pass_backings_.find(quad->render_pass_id);
  DCHECK(render_pass_backings_.end() != iter);
  // This function is called after AllocateRenderPassResourceIfNeeded, so
  // there should be backing ready.
  RenderPassBacking& backing = iter->second;

  sk_sp<SkImage> content_image;
  switch (draw_mode_) {
    case DrawMode::DDL: {
      content_image = skia_output_surface_->MakePromiseSkImageFromRenderPass(
          quad->render_pass_id, backing.size, backing.format,
          backing.generate_mipmap, backing.color_space.ToSkColorSpace());
      break;
    }
    case DrawMode::SKPRECORD: {
      content_image = SkImage::MakeFromPicture(
          backing.picture,
          SkISize::Make(backing.size.width(), backing.size.height()), nullptr,
          nullptr, SkImage::BitDepth::kU8,
          backing.color_space.ToSkColorSpace());
      return;
    }
  }

  // If the RP generated mipmaps when it was created, set quality to medium,
  // which turns on mipmap filtering in Skia.
  if (backing.generate_mipmap)
    params->filter_quality = kMedium_SkFilterQuality;

  params->vis_tex_coords = cc::MathUtil::ScaleRectProportional(
      quad->tex_coord_rect, gfx::RectF(quad->rect), params->visible_rect);
  gfx::RectF valid_texel_bounds(content_image->width(),
                                content_image->height());

  // When the RPDQ was needed because of a copy request, it may not require any
  // advanced filtering/effects at which point it's basically a tiled quad.
  if (!rpdq_params.image_filter && !rpdq_params.backdrop_filter &&
      !rpdq_params.mask_image) {
    DCHECK(!MustFlushBatchedQuads(quad, nullptr, *params));
    AddQuadToBatch(content_image.get(), valid_texel_bounds, params);
    return;
  }

  // The paint is complex enough that it has to be drawn on its own, and since
  // MustFlushBatchedQuads() was optimistic, we manage the flush here.
  if (!batched_quads_.empty())
    FlushBatchedQuads();

  SkPaint paint = params->paint();
  DrawSingleImage(content_image.get(), valid_texel_bounds, &rpdq_params, &paint,
                  params);
}

void SkiaRenderer::CopyDrawnRenderPass(
    const copy_output::RenderPassGeometry& geometry,
    std::unique_ptr<CopyOutputRequest> request) {
  // TODO(weiliangc): Make copy request work. (crbug.com/644851)
  TRACE_EVENT0("viz", "SkiaRenderer::CopyDrawnRenderPass");

  switch (draw_mode_) {
    case DrawMode::DDL: {
      // Root framebuffer uses id 0 in SkiaOutputSurface.
      RenderPassId render_pass_id = 0;
      const auto* const render_pass = current_frame()->current_render_pass;
      if (render_pass != current_frame()->root_render_pass) {
        render_pass_id = render_pass->id;
      }
      skia_output_surface_->CopyOutput(render_pass_id, geometry,
                                       render_pass->color_space,
                                       std::move(request));
      break;
    }
    case DrawMode::SKPRECORD: {
      NOTIMPLEMENTED();
      break;
    }
  }
}

#if defined(OS_WIN)
void SkiaRenderer::SetEnableDCLayers(bool enable) {
  skia_output_surface_->SetEnableDCLayers(enable);
}
#endif

void SkiaRenderer::DidChangeVisibility() {
  if (visible_)
    output_surface_->EnsureBackbuffer();
  else
    output_surface_->DiscardBackbuffer();
}

void SkiaRenderer::FinishDrawingQuadList() {
  if (!batched_quads_.empty())
    FlushBatchedQuads();
  switch (draw_mode_) {
    case DrawMode::DDL: {
      base::OnceClosure on_finished_callback;

      // Signal |current_frame_resource_fence_| when the root render pass is
      // finished.
      if (current_frame_resource_fence_ &&
          current_frame_resource_fence_->WasSet() &&
          current_frame()->current_render_pass ==
              current_frame()->root_render_pass) {
        on_finished_callback =
            base::BindOnce(&FrameResourceFence::Signal,
                           std::move(current_frame_resource_fence_));
      }
      gpu::SyncToken sync_token =
          skia_output_surface_->SubmitPaint(std::move(on_finished_callback));

      lock_set_for_external_use_->UnlockResources(sync_token);
      break;
    }
    case DrawMode::SKPRECORD: {
      current_canvas_->flush();
      sk_sp<SkPicture> picture = current_recorder_->finishRecordingAsPicture();
      *current_picture_ = picture;
    }
  }
}

void SkiaRenderer::GenerateMipmap() {
  // This is a no-op since setting FilterQuality to high during drawing of
  // RenderPassDrawQuad is what actually generates generate_mipmap.
}

GrContext* SkiaRenderer::GetGrContext() {
  switch (draw_mode_) {
    case DrawMode::DDL:
      return nullptr;
    case DrawMode::SKPRECORD:
      return nullptr;
  }
}

void SkiaRenderer::UpdateRenderPassTextures(
    const RenderPassList& render_passes_in_draw_order,
    const base::flat_map<RenderPassId, RenderPassRequirements>&
        render_passes_in_frame) {
  std::vector<RenderPassId> passes_to_delete;
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
    if (!size_appropriate || !mipmap_appropriate)
      passes_to_delete.push_back(pair.first);
  }

  // Delete RenderPass backings from the previous frame that will not be used
  // again.
  for (size_t i = 0; i < passes_to_delete.size(); ++i) {
    auto it = render_pass_backings_.find(passes_to_delete[i]);
    render_pass_backings_.erase(it);
  }

  if (is_using_ddl() && !passes_to_delete.empty()) {
    skia_output_surface_->RemoveRenderPassResource(std::move(passes_to_delete));
  }
}

void SkiaRenderer::AllocateRenderPassResourceIfNeeded(
    const RenderPassId& render_pass_id,
    const RenderPassRequirements& requirements) {
  auto it = render_pass_backings_.find(render_pass_id);
  if (it != render_pass_backings_.end())
    return;

  // TODO(penghuang): check supported format correctly.
  gpu::Capabilities caps;
  caps.texture_format_bgra8888 = true;
  GrContext* gr_context = GetGrContext();
  switch (draw_mode_) {
    case DrawMode::DDL:
      break;
    case DrawMode::SKPRECORD: {
      render_pass_backings_.emplace(
          render_pass_id,
          RenderPassBacking(requirements.size, requirements.generate_mipmap,
                            current_frame()->current_render_pass->color_space));
      return;
    }
  }
  render_pass_backings_.emplace(
      render_pass_id,
      RenderPassBacking(gr_context, caps, requirements.size,
                        requirements.generate_mipmap,
                        current_frame()->current_render_pass->color_space));
}

SkiaRenderer::RenderPassBacking::RenderPassBacking(
    GrContext* gr_context,
    const gpu::Capabilities& caps,
    const gfx::Size& size,
    bool generate_mipmap,
    const gfx::ColorSpace& color_space)
    : size(size), generate_mipmap(generate_mipmap), color_space(color_space) {
  if (color_space.IsHDR()) {
    // If a platform does not support half-float renderbuffers then it should
    // not should request HDR rendering.
    // DCHECK(caps.texture_half_float_linear);
    // DCHECK(caps.color_buffer_half_float_rgba);
    format = RGBA_F16;
  } else {
    format = PlatformColor::BestSupportedTextureFormat(caps);
  }

  // For DDL, we don't need create teh render_pass_surface here, and we will
  // create the SkSurface by SkiaOutputSurface on Gpu thread.
  if (!gr_context)
    return;

  constexpr uint32_t flags = 0;
  // LegacyFontHost will get LCD text and skia figures out what type to use.
  SkSurfaceProps surface_props(flags, SkSurfaceProps::kLegacyFontHost_InitType);
  int msaa_sample_count = 0;
  SkColorType color_type =
      ResourceFormatToClosestSkColorType(true /* gpu_compositing*/, format);
  SkImageInfo image_info =
      SkImageInfo::Make(size.width(), size.height(), color_type,
                        kPremul_SkAlphaType, color_space.ToSkColorSpace());
  render_pass_surface = SkSurface::MakeRenderTarget(
      gr_context, SkBudgeted::kNo, image_info, msaa_sample_count,
      kTopLeft_GrSurfaceOrigin, &surface_props, generate_mipmap);
}

SkiaRenderer::RenderPassBacking::RenderPassBacking(
    const gfx::Size& size,
    bool generate_mipmap,
    const gfx::ColorSpace& color_space)
    : size(size), generate_mipmap(generate_mipmap), color_space(color_space) {
  recorder = std::make_unique<SkPictureRecorder>();
}

SkiaRenderer::RenderPassBacking::~RenderPassBacking() {}

SkiaRenderer::RenderPassBacking::RenderPassBacking(
    SkiaRenderer::RenderPassBacking&& other)
    : size(other.size),
      generate_mipmap(other.generate_mipmap),
      color_space(other.color_space),
      format(other.format) {
  render_pass_surface = other.render_pass_surface;
  other.render_pass_surface = nullptr;
  recorder = std::move(other.recorder);
}

SkiaRenderer::RenderPassBacking& SkiaRenderer::RenderPassBacking::operator=(
    SkiaRenderer::RenderPassBacking&& other) {
  size = other.size;
  generate_mipmap = other.generate_mipmap;
  color_space = other.color_space;
  format = other.format;
  render_pass_surface = other.render_pass_surface;
  other.render_pass_surface = nullptr;
  recorder = std::move(other.recorder);
  return *this;
}

bool SkiaRenderer::IsRenderPassResourceAllocated(
    const RenderPassId& render_pass_id) const {
  auto it = render_pass_backings_.find(render_pass_id);
  return it != render_pass_backings_.end();
}

gfx::Size SkiaRenderer::GetRenderPassBackingPixelSize(
    const RenderPassId& render_pass_id) {
  auto it = render_pass_backings_.find(render_pass_id);
  DCHECK(it != render_pass_backings_.end());
  return it->second.size;
}

}  // namespace viz
