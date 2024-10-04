// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/skia_renderer.h"

#include <array>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/numerics/angle_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/math_util.h"
#include "cc/debug/debug_colors.h"
#include "cc/paint/render_surface_filters.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
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
#include "gpu/config/gpu_finch_features.h"
#include "media/base/media_switches.h"
#include "skia/ext/opacity_filter_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorFilter.h"
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
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/private/chromium/GrDeferredDisplayList.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "third_party/skia/src/core/SkCanvasPriv.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/linear_gradient.h"
#include "ui/gfx/geometry/point_conversions.h"
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
#include "ui/gfx/hdr_metadata.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/viz/service/display/overlay_processor_surface_control.h"
#endif

namespace viz {

namespace {

// Feature to temporarily create a dump in the case where we try and sample from
// a render pass that has never been drawn to.
// See: crbug.com/344458294, crbug.com/345673794
// TODO(crbug.com/347909405): Remove this
BASE_FEATURE(kDumpWithoutCrashingOnMissingRenderPassBacking,
             "DumpWithoutCrashingOnMissingRenderPassBacking",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Use BufferQueue for the primary plane instead of a DXGI swap chain or DComp
// surface.
BASE_FEATURE(kBufferQueue, "BufferQueue", base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Smallest unit that impacts anti-aliasing output. We use this to determine
// when an exterior edge (with AA) has been clipped (no AA). The specific value
// was chosen to match that used by gl_renderer.
static const float kAAEpsilon = 1.0f / 1024.0f;

// The gfx::QuadF draw_region passed to DoDrawQuad, converted to Skia types
struct SkDrawRegion {
  SkDrawRegion() = default;
  explicit SkDrawRegion(const gfx::QuadF& draw_region);

  std::array<SkPoint, 4> points;
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

// This is slightly different than Transform::IsPositiveScaleAndTranslation()
// in that it also allows zero scales. This is because in the common
// orthographic case the z scale is 0.
bool Is2dScaleTranslateTransform(const gfx::Transform& transform) {
  return transform.IsScaleOrTranslation() && transform.rc(0, 0) >= 0.0f &&
         transform.rc(1, 1) >= 0.0f && transform.rc(2, 2) >= 0.0f;
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
      NOTREACHED_IN_MIGRATION();
      return CompositorRenderPassDrawQuad::MaterialCast(quad)
          ->force_anti_aliasing_off;
    case DrawQuad::Material::kAggregatedRenderPass:
      return AggregatedRenderPassDrawQuad::MaterialCast(quad)
          ->force_anti_aliasing_off;
    case DrawQuad::Material::kSolidColor:
      return SolidColorDrawQuad::MaterialCast(quad)->force_anti_aliasing_off;
    case DrawQuad::Material::kTiledContent:
      return TileDrawQuad::MaterialCast(quad)->force_anti_aliasing_off;
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

#if BUILDFLAG(ENABLE_VULKAN) && BUILDFLAG(IS_CHROMEOS) && \
    BUILDFLAG(USE_V4L2_CODEC)
constexpr size_t kMaxProtectedContentWidth = 3840;
constexpr size_t kMaxProtectedContentHeight = 2160;
#endif

}  // namespace

// A helper class to emit Viz debugger messages that has access to SkiaRenderer
// internals.
class SkiaRenderer::VizDebuggerLog {
 public:
  static void DebugLogDumpRenderPassBackings(
      const base::flat_map<AggregatedRenderPassId, RenderPassBacking>&
          render_pass_backings) {
    bool enabled;
    DBG_CONNECTED_OR_TRACING(enabled);
    if (enabled) {
      DBG_LOG("renderer.skia.render_pass_backings",
              "render_pass_backings_ = [");
      for (auto& [render_pass_id, backing] : render_pass_backings) {
        base::trace_event::TracedValueJSON value;
        base::trace_event::TracedValue::Dictionary(
            {
                {"size", backing.size.ToString()},
                {"generate_mipmap", backing.generate_mipmap},
                {"color_space", backing.color_space.ToString()},
                {"alpha_type",
                 backing.alpha_type == RenderPassAlphaType::kPremul ? "premul"
                                                                    : "opaque"},
                {"format", backing.format.ToString()},
                {"mailbox", backing.mailbox.ToDebugString()},
                {"is_root", backing.is_root},
                {"is_scanout", backing.is_scanout},
                {"scanout_dcomp_surface", backing.scanout_dcomp_surface},
                {"drawn_rect", backing.drawn_rect.ToString()},
            })
            .WriteToValue(&value);
        DBG_LOG("renderer.skia.render_pass_backings", "%" PRIu64 ": %s",
                render_pass_id.value(), value.ToFormattedJSON().c_str());
      }
      DBG_LOG("renderer.skia.render_pass_backings", "]");
    }
  }

  static void DebugLogNewRenderPassBacking(
      const AggregatedRenderPassId& render_pass_id,
      const RenderPassRequirements& requirements) {
    bool enabled;
    DBG_CONNECTED_OR_TRACING(enabled);
    if (enabled) {
      base::trace_event::TracedValueJSON value;
      base::trace_event::TracedValue::Dictionary(
          {
              {"size", requirements.size.ToString()},
              {"generate_mipmap", requirements.generate_mipmap},
              {"format", requirements.format.ToString()},
              {"color_space", requirements.color_space.ToString()},
              {"alpha_type", static_cast<int>(requirements.alpha_type)},
              {"is_scanout", requirements.is_scanout},
              {"scanout_dcomp_surface", requirements.scanout_dcomp_surface},
          })
          .WriteToValue(&value);
      DBG_LOG("renderer.skia.render_pass_backings",
              "allocate backing for render_pass %" PRIu64 ", %s",
              render_pass_id.value(), value.ToFormattedJSON().c_str());
    }
  }
};

SkiaRenderer::RenderPassBacking::RenderPassBacking() = default;

SkiaRenderer::RenderPassBacking::RenderPassBacking(
    const SkiaRenderer::RenderPassBacking&) = default;

SkiaRenderer::RenderPassBacking& SkiaRenderer::RenderPassBacking::operator=(
    const SkiaRenderer::RenderPassBacking&) = default;

SkiaRenderer::RenderPassBacking::RenderPassBacking(
    gfx::Size size,
    bool generate_mipmap,
    gfx::ColorSpace color_space,
    RenderPassAlphaType alpha_type,
    SharedImageFormat format,
    gpu::Mailbox mailbox,
    bool is_root,
    bool is_scanout,
    bool scanout_dcomp_surface)
    : size(size),
      generate_mipmap(generate_mipmap),
      color_space(color_space),
      alpha_type(alpha_type),
      format(format),
      mailbox(mailbox),
      is_root(is_root),
      is_scanout(is_scanout),
      scanout_dcomp_surface(scanout_dcomp_surface) {}
// chrome style prevents this from going in skia_renderer.h, but since it
// uses std::optional, the style also requires it to have a declared ctor
SkiaRenderer::BatchedQuadState::BatchedQuadState() = default;

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
  std::optional<SkDrawRegion> draw_region;
  // Optional mask filter info that may contain rounded corner clip and/or a
  // gradient mask to apply. If present, rounded corner clip will have been
  // transformed to device space and ShouldApplyRoundedCorner returns true. If
  // present, gradient mask will have been transformed to device space and
  // ShouldApplyGradientMask returns true.
  std::optional<gfx::MaskFilterInfo> mask_filter_info;
  // Optional device space clip to apply. If present, it is equal to the current
  // |scissor_rect_| of the renderer.
  std::optional<gfx::Rect> scissor_rect;

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
      return SkPath::Polygon(draw_region->points.data(),
                             std::size(draw_region->points),
                             /*isClosed=*/true);
    }
    return SkPath();
  }

  void ApplyScissor(const SkiaRenderer* renderer,
                    const DrawQuad* quad,
                    const std::optional<gfx::Rect>& scissor_to_apply);
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

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
struct SkiaRenderer::RenderPassOverlayParams {
  AggregatedRenderPassId render_pass_id;
  RenderPassBacking render_pass_backing;
  AggregatedRenderPassDrawQuad rpdq;
  SharedQuadState shared_quad_state;
  cc::FilterOperations filters;
  cc::FilterOperations backdrop_filters;

  // Represents the number of |OverlayLock|s (i.e. number of distinct frames)
  // that reference this.
  int ref_count = 0;
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
                       bool raw_draw_if_possible = false,
                       bool force_rgbx = false);

  ScopedSkImageBuilder(const ScopedSkImageBuilder&) = delete;
  ScopedSkImageBuilder& operator=(const ScopedSkImageBuilder&) = delete;

  ~ScopedSkImageBuilder() = default;

  const SkImage* sk_image() const { return sk_image_.get(); }
  const cc::PaintOpBuffer* paint_op_buffer() const { return paint_op_buffer_; }
  const std::optional<SkColor4f>& clear_color() const { return clear_color_; }

 private:
  sk_sp<SkImage> sk_image_;
  raw_ptr<const cc::PaintOpBuffer> paint_op_buffer_ = nullptr;
  std::optional<SkColor4f> clear_color_;
};

SkiaRenderer::ScopedSkImageBuilder::ScopedSkImageBuilder(
    SkiaRenderer* skia_renderer,
    ResourceId resource_id,
    bool maybe_concurrent_reads,
    SkAlphaType alpha_type,
    GrSurfaceOrigin origin,
    sk_sp<SkColorSpace> override_color_space,
    bool raw_draw_if_possible,
    bool force_rgbx) {
  if (!resource_id)
    return;
  auto* resource_provider = skia_renderer->resource_provider();
  DCHECK(IsTextureResource(resource_provider, resource_id));

  auto* image_context = skia_renderer->lock_set_for_external_use_.LockResource(
      resource_id, maybe_concurrent_reads, raw_draw_if_possible);

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
      image_context, resource_provider->GetColorSpace(resource_id), force_rgbx);
  paint_op_buffer_ = image_context->paint_op_buffer();
  clear_color_ = image_context->clear_color();
  sk_image_ = image_context->image();
  LOG_IF(ERROR, !image_context->has_image() && !paint_op_buffer_)
      << "Failed to create the promise sk image or get paint ops.";

  if (sk_image_ && override_color_space) {
    sk_image_ = sk_image_->reinterpretColorSpace(override_color_space);
  }
}

// Parameters needed to draw a CompositorRenderPassDrawQuad.
struct SkiaRenderer::DrawRPDQParams {
  struct BypassGeometry {
    // The additional matrix to concatenate to the SkCanvas after image filters
    // have been configured so that the DrawQuadParams geometry is properly
    // mapped (i.e. when set, |visible_rect| and |draw_region| must be
    // pre-transformed by this before |content_device_transform|).
    SkMatrix transform;

    // Clipping in bypassed render pass coordinate space. This can come from
    // RenderPassDrawQuad::visible_rect and bypass quads clip_rect.
    gfx::RectF clip_rect;
  };

  class MaskShader {
   public:
    MaskShader(ResourceId mask_resource_id, SkMatrix mask_to_quad_matrix)
        : mask_resource_id_(mask_resource_id),
          mask_to_quad_matrix_(mask_to_quad_matrix) {
      CHECK(!mask_resource_id_.is_null());
    }

    // Get the resolved mask image and calculated transform matrix baked into an
    // SkShader
    sk_sp<SkShader> GetOrCreateSkShader(SkiaRenderer* renderer) const;

   private:
    ResourceId mask_resource_id_;

    // Map mask rect to full quad rect so that mask coordinates don't change
    // with clipping.
    SkMatrix mask_to_quad_matrix_;

    mutable sk_sp<SkShader> sk_shader_ = nullptr;
  };

  DrawRPDQParams() : filter_bounds(SkRect::MakeEmpty()) {}

  explicit DrawRPDQParams(const gfx::RectF& visible_rect)
      : filter_bounds(gfx::RectFToSkRect(visible_rect)) {}

  // Root of the calculated image filter DAG to be applied to the render pass.
  sk_sp<SkImageFilter> image_filter = nullptr;
  // If |image_filter| can be represented as a single color filter, this will
  // be that filter. |image_filter| will still be non-null.
  sk_sp<SkColorFilter> color_filter = nullptr;
  // Root of the calculated backdrop filter DAG to be applied to the render pass
  // Backdrop filtered content must be clipped to |backdrop_filter_bounds| and
  // the DrawQuad's rect (or draw_region if BSP-clipped).
  sk_sp<SkImageFilter> backdrop_filter = nullptr;
  // If present, the mask image, which can be applied using SkCanvas::clipShader
  // in the RPDQ's coord space.
  std::optional<MaskShader> mask_shader;
  // Backdrop border box for the render pass, to clip backdrop-filtered content
  // (but not the rest of the RPDQ itself).
  std::optional<SkRRect> backdrop_filter_bounds;
  // The content space bounds that includes any filtered extents. If empty,
  // the draw can be skipped.It may represent fractional pixel coverage.
  SkRect filter_bounds;

  // Multiplier used for downscaling backdrop filter.
  float backdrop_filter_quality = 1.0f;

  // Geometry from the bypassed RenderPassDrawQuad.
  std::optional<BypassGeometry> bypass_geometry;

  // True when there is an |image_filter| and it's not equivalent to
  // |color_filter|.
  bool has_complex_image_filter() const {
    return image_filter && !color_filter;
  }

  // True if the RenderPass's output rect would clip the visible contents that
  // are bypassing the renderpass' offscreen buffer.
  bool needs_bypass_clip(const gfx::RectF& content_rect) const {
    if (!bypass_geometry) {
      return false;
    }

    SkRect content_bounds =
        bypass_geometry->transform.mapRect(gfx::RectFToSkRect(content_rect));
    return !bypass_geometry->clip_rect.Contains(
        gfx::SkRectToRectF(content_bounds));
  }

  // Returns either |params->visible_rect| or |bypass_geometry->clip_rect|,
  // which corresponds to the visible_rect of the originating RPDQ.
  SkRect GetContentBounds(const DrawQuadParams* params) const {
    return gfx::RectFToSkRect(bypass_geometry ? bypass_geometry->clip_rect
                                              : params->visible_rect);
  }

  // Sets a clip on the canvas to restrict the size of the Skia layer that holds
  // the backdrop filtered content to the size of the DrawQuad. When possible
  // this is an exact clip to reduce operations performed within the backdrop
  // layer; otherwise it's conservative to constrain size without impacting
  // visuals.
  void SetBackdropFilterClip(SkCanvas* canvas,
                             const DrawQuadParams* params) const;

  // Erases backdrop filtered content outside of the DrawQuad and backdrop
  // filter bounds rrect within the backdrop layer. This is a no-op if exact
  // clipping was used in SetBackdropFilterClip to achieve the same effect.
  // Otherwise, this is necessary to limit the backdrop content without
  // impacting the DrawQuad or regular filter output.
  void ClearOutsideBackdropBounds(SkCanvas* canvas,
                                  const DrawQuadParams* params) const;
};

sk_sp<SkShader> SkiaRenderer::DrawRPDQParams::MaskShader::GetOrCreateSkShader(
    SkiaRenderer* renderer) const {
  if (sk_shader_) {
    return sk_shader_;
  }

  ScopedSkImageBuilder mask_image_builder(renderer, mask_resource_id_,
                                          /*maybe_concurrent_reads=*/false);
  const SkImage* mask_image = mask_image_builder.sk_image();
  CHECK(mask_image);
  sk_shader_ = mask_image->makeShader(
      SkTileMode::kClamp, SkTileMode::kClamp,
      SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone),
      &mask_to_quad_matrix_);

  return sk_shader_;
}

void SkiaRenderer::DrawRPDQParams::SetBackdropFilterClip(
    SkCanvas* canvas,
    const DrawQuadParams* params) const {
  if (!backdrop_filter) {
    return;  // No clipping necessary
  }

  const bool aa = params->aa_flags != SkCanvas::kNone_QuadAAFlags;
  // filter_bounds is either a conservative bounding box that will not restrict
  // the output of any forward-filters applied to the RPDQ, or it is exactly
  // the content bounds that the backdrop filter should be limited to.
  canvas->clipRect(filter_bounds, aa);
}

void SkiaRenderer::DrawRPDQParams::ClearOutsideBackdropBounds(
    SkCanvas* canvas,
    const DrawQuadParams* params) const {
  if (!backdrop_filter) {
    return;  // Nothing to clear within the layer
  }

  // Must erase pixels not in the intersection of the backdrop_filter_bounds,
  // visible_rect, and any draw_region. This is the union of the inverse fills
  // of those shapes, which can be accomplished most efficiently by clipping
  // the shape with the kDifference op and then clearing the canvas, per shape
  const bool aa = params->aa_flags != SkCanvas::kNone_QuadAAFlags;

  if (backdrop_filter_bounds) {
    canvas->save();
    canvas->clipRRect(*backdrop_filter_bounds, SkClipOp::kDifference, aa);
    canvas->clear(SK_ColorTRANSPARENT);
    canvas->restore();
  }

  if (params->draw_region) {
    canvas->save();
    if (bypass_geometry) {
      // If there's a bypass geometry, the draw_region is relative to that
      // coordinate space.
      canvas->concat(bypass_geometry->transform);
    }
    canvas->clipPath(params->draw_region_in_path(), SkClipOp::kDifference, aa);
    canvas->clear(SK_ColorTRANSPARENT);
    canvas->restore();
  } else {
    SkRect content = GetContentBounds(params);
    if (!content.contains(filter_bounds) &&
        (!backdrop_filter_bounds ||
         !content.contains(backdrop_filter_bounds->rect()))) {
      // If the |draw_region| is defined, it's already a subset of |rect|, so
      // we don't have to clear both. Similarly, if |filter_bounds| is contained
      // within the quad, the clip set in BackdropFilterClip() discards anything
      // that would be cleared here. And if |backdrop_filter_bounds| is
      // contained within the quad, the first clear was sufficient. Otherwise,
      // there is some excess backdrop content that must still be erased.
      canvas->save();
      canvas->clipRect(content, SkClipOp::kDifference, aa);
      canvas->clear(SK_ColorTRANSPARENT);
      canvas->restore();
    }
  }
}

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
    NOTREACHED_IN_MIGRATION();
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
  std::optional<gfx::GpuFenceHandle> release_fence_;
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
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
      can_skip_render_pass_overlay_(
          base::FeatureList::IsEnabled(features::kCanSkipRenderPassOverlay)),
#endif
      lock_set_for_external_use_(resource_provider, skia_output_surface_),
      is_using_raw_draw_(features::IsUsingRawDraw()) {
#if BUILDFLAG(IS_WIN)
  // |OverlayProcessorWin| can cause a render pass to reallocate during partial
  // delegation, so we need to ensure the contents of all render passes are
  // valid if we need to redraw one (that can potentially embed others). This
  // behavior is not required for full delegation since |OverlayProcessorWin|
  // does not modify non-root render pass damage in that case.
  use_render_pass_drawn_rect_ |=
      features::IsDelegatedCompositingEnabled() &&
      features::kDelegatedCompositingModeParam.Get() ==
          features::DelegatedCompositingMode::kLimitToUi;
#endif
  DCHECK(skia_output_surface_);

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

#if BUILDFLAG(IS_WIN)
  // Windows does not normally use buffer queue because swap chains and DComp
  // surfaces internally manage buffers and cross-frame damage. It instead lets
  // the renderer allocate the root surface like a normal render pass backing.

  // It's possible to use BufferQueue with DComp textures, so we can optionally
  // enable it behind a feature flag.
  const bool want_buffer_queue =
      base::FeatureList::IsEnabled(kBufferQueue) &&
      output_surface_->capabilities().dc_support_level >=
          OutputSurface::DCSupportLevel::kDCompTexture;
#else
  const bool want_buffer_queue = true;
#endif
  if (want_buffer_queue &&
      output_surface->capabilities().renderer_allocates_images) {
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

#if BUILDFLAG(ENABLE_VULKAN) && BUILDFLAG(IS_CHROMEOS) && \
    BUILDFLAG(USE_V4L2_CODEC)
  protected_buffer_queue_ = std::make_unique<BufferQueue>(
      skia_output_surface_, skia_output_surface_->GetSurfaceHandle(), 3,
      /*is_protected=*/true);
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

  VizDebuggerLog::DebugLogDumpRenderPassBackings(render_pass_backings_);

#if BUILDFLAG(IS_OZONE)
  MaybeScheduleBackgroundImage(current_frame()->overlay_list);
#endif  // BUILDFLAG(IS_OZONE)

  // TODO(weiliangc): Remove this once OverlayProcessor schedules overlays.
  if (current_frame()->output_surface_plane) {
    CHECK(output_surface_->capabilities().renderer_allocates_images);

    auto& surface_plane = current_frame()->output_surface_plane.value();

    auto root_pass_backing =
        render_pass_backings_.find(current_frame()->root_render_pass->id);
    // The root pass backing should always exist.
    DCHECK(root_pass_backing != render_pass_backings_.end());

    OverlayCandidate surface_candidate;
    surface_candidate.mailbox = root_pass_backing->second.mailbox;
    surface_candidate.is_root_render_pass = true;
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
    surface_candidate.transform = gfx::Transform();
#else
    surface_candidate.transform = surface_plane.transform;
#endif
    surface_candidate.display_rect = surface_plane.display_rect;
    surface_candidate.uv_rect = surface_plane.uv_rect;
    surface_candidate.resource_size_in_pixels = surface_plane.resource_size;
    surface_candidate.format = surface_plane.format;
    surface_candidate.color_space = surface_plane.color_space;
    if (current_frame()->display_color_spaces.SupportsHDR() &&
        current_frame()->root_render_pass->content_color_usage ==
            gfx::ContentColorUsage::kHDR) {
      surface_candidate.hdr_metadata.extended_range.emplace();
      // TODO(crbug.com/40263227): Track the actual brightness of the
      // content. For now, assume that all HDR content is 1,000 nits.
      surface_candidate.hdr_metadata.extended_range->desired_headroom =
          gfx::HdrMetadataExtendedRange::kDefaultHdrHeadroom;
    }
    surface_candidate.is_opaque = !surface_plane.enable_blending;
    surface_candidate.opacity = surface_plane.opacity;
    surface_candidate.priority_hint = surface_plane.priority_hint;
    surface_candidate.rounded_corners = surface_plane.rounded_corners;
    surface_candidate.damage_rect =
        use_partial_swap_ ? gfx::RectF(swap_buffer_rect_)
                          : gfx::RectF(surface_plane.resource_size);
#if BUILDFLAG(IS_MAC)
    // Mac doesn't use the plane_z_order field and it needs to have primary
    // plane last in the list of overlays.
    auto insert_positon = current_frame()->overlay_list.end();
#else
    // Most platforms respect plane_z_order so the list order doesn't matter
    // but Ozone DRM needs the primary plane as the first overlay when overlay
    // testing.
    auto insert_positon = current_frame()->overlay_list.begin();
#endif
    current_frame()->overlay_list.insert(insert_positon, surface_candidate);

  } else {
    if (buffer_queue_) {
      // If there's no primary plane on these platforms it mean's we're
      // delegating to the system compositor, and don't need the buffers
      // anymore. On LaCrOS the primary plane buffers are immediately destroyed.
      // They'll be recreated when we need them again when GetCurrentBuffer() is
      // called. On Mac the primary plane buffers are marked as purgeable so the
      // OS can decide if they should be destroyed or not.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_WIN)
      buffer_queue_->DestroyBuffers();
#elif BUILDFLAG(IS_APPLE)
      buffer_queue_->SetBuffersPurgeable();
#endif
    }
  }

  ScheduleOverlays();
  debug_tint_modulate_count_++;
}

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_VULKAN) && \
    BUILDFLAG(USE_V4L2_CODEC)
// Simple scheme for de-allocating protected buffers: if we go one SwapBuffer
// cycle without needing a protected shared image, we can delete the protected
// buffer queue.
gpu::Mailbox SkiaRenderer::GetProtectedSharedImage(bool is_10bit) {
  is_protected_pool_idle_ = false;

  protected_buffer_queue_->Reshape(
      gfx::Size(kMaxProtectedContentWidth, kMaxProtectedContentHeight),
      gfx::ColorSpace::CreateSRGB(),
      (is_10bit &&
       base::FeatureList::IsEnabled(media::kEnableArmHwdrm10bitOverlays))
          ? SinglePlaneFormat::kBGRA_1010102
          : SinglePlaneFormat::kBGRA_8888);

  return protected_buffer_queue_->GetCurrentBuffer();
}

void SkiaRenderer::MaybeFreeProtectedPool() {
  if (is_protected_pool_idle_ && protected_buffer_queue_) {
    protected_buffer_queue_->DestroyBuffers();
    skia_output_surface_->CleanupImageProcessor();
  } else {
    is_protected_pool_idle_ = true;
  }
}
#endif

void SkiaRenderer::SwapBuffers(SwapFrameData swap_frame_data) {
  DCHECK(visible_);
  DCHECK(output_surface_->capabilities().supports_viewporter ||
         viewport_size_for_swap_buffers() == surface_size_for_swap_buffers());
  TRACE_EVENT0("viz,benchmark", "SkiaRenderer::SwapBuffers");

  OutputSurfaceFrame output_frame;
  output_frame.latency_info = std::move(swap_frame_data.latency_info);
  output_frame.choreographer_vsync_id = swap_frame_data.choreographer_vsync_id;
  output_frame.size = viewport_size_for_swap_buffers();
  output_frame.data.seq = swap_frame_data.seq;
  output_frame.data.swap_trace_id = swap_frame_data.swap_trace_id;
  if (use_partial_swap_) {
    swap_buffer_rect_.Intersect(gfx::Rect(surface_size_for_swap_buffers()));
    output_frame.sub_buffer_rect = swap_buffer_rect_;
  }
  if (delegated_ink_handler_ && !UsingSkiaForDelegatedInk()) {
    output_frame.delegated_ink_metadata =
        delegated_ink_handler_->TakeMetadata();
  }
  output_frame.data.display_hdr_headroom = swap_frame_data.display_hdr_headroom;
#if BUILDFLAG(IS_APPLE)
  output_frame.data.ca_layer_error_code = swap_frame_data.ca_layer_error_code;
#endif

  if (buffer_queue_) {
    gfx::Rect damage_rect = output_frame.sub_buffer_rect.value_or(
        gfx::Rect(surface_size_for_swap_buffers()));
    buffer_queue_->SwapBuffers(damage_rect);
  }

  skia_output_surface_->SwapBuffers(std::move(output_frame));
  swap_buffer_rect_ = gfx::Rect();

  FlushOutputSurface();

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
  // Delete render pass overlay backings from the previous frame that will not
  // be used again.
  for (auto& overlay : available_render_pass_overlay_backings_) {
    skia_output_surface_->DestroySharedImage(
        overlay.render_pass_backing.mailbox);
  }
  available_render_pass_overlay_backings_.clear();
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_VULKAN) && BUILDFLAG(IS_CHROMEOS) && \
    BUILDFLAG(USE_V4L2_CODEC)
  if (protected_buffer_queue_) {
    // Note that we still call BufferQueue::SwapBuffers() even when we suspect
    // our buffer queue is idle because there might still be in-flight frames
    // that need to be managed.
    protected_buffer_queue_->SwapBuffers(
        gfx::Rect(kMaxProtectedContentWidth, kMaxProtectedContentHeight));
  }

  MaybeFreeProtectedPool();
#endif
}

void SkiaRenderer::SwapBuffersSkipped() {
  gfx::Rect root_pass_damage_rect = gfx::Rect(surface_size_for_swap_buffers());
  if (use_partial_swap_)
    root_pass_damage_rect.Intersect(swap_buffer_rect_);

  if (overlay_processor_) {
    overlay_processor_->OnSwapBuffersComplete(gfx::SwapResult::SWAP_SKIPPED);
  }

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

  if (overlay_processor_) {
    overlay_processor_->OnSwapBuffersComplete(params.swap_response.result);
  }

  if (buffer_queue_) {
    if (params.swap_response.result ==
        gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS) {
      buffer_queue_->RecreateBuffers();
    }
    buffer_queue_->SwapBuffersComplete();
  }

#if BUILDFLAG(ENABLE_VULKAN) && BUILDFLAG(IS_CHROMEOS) && \
    BUILDFLAG(USE_V4L2_CODEC)
  if (protected_buffer_queue_) {
    protected_buffer_queue_->SwapBuffersComplete();
  }
#endif

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

#if BUILDFLAG(IS_APPLE)
  // On macOS, we don't want to release |committed_overlay_locks_| right away
  // because CoreAnimation can hold the overlay images for potentially several
  // frames. We depend on the output device to signal the return of overlays via
  // |DidReceiveReleasedOverlays| to know when it's safe to release the locks.
  for (auto lock_iter = committed_overlay_locks_.begin();
       lock_iter != read_fence_lock_iter; ++lock_iter) {
    awaiting_release_overlay_locks_.insert(std::move(*lock_iter));
  }
#endif  // BUILDFLAG(IS_APPLE)

  // Current pending locks should have been committed by the next time
  // SwapBuffers() is completed.
  {
    DisplayResourceProvider::ScopedBatchReturnResources returner(
        resource_provider_.get(), /*allow_access_to_gpu_thread=*/true);
    committed_overlay_locks_.clear();
  }
  if (delegated_ink_handler_ && UsingSkiaForDelegatedInk()) {
    delegated_ink_handler_->GetInkRenderer()->ReportPointsDrawn();
  }
  std::swap(committed_overlay_locks_, pending_overlay_locks_.front());
  pending_overlay_locks_.pop_front();
}

void SkiaRenderer::BuffersPresented() {
  read_lock_release_fence_overlay_locks_.pop_front();
}

void SkiaRenderer::DidReceiveReleasedOverlays(
    const std::vector<gpu::Mailbox>& released_overlays) {
#if BUILDFLAG(IS_APPLE)
  DisplayResourceProvider::ScopedBatchReturnResources returner(
      resource_provider_.get(), /*allow_access_to_gpu_thread=*/true);

  for (const auto& mailbox : released_overlays) {
    auto iter = awaiting_release_overlay_locks_.find(mailbox);
    if (iter == awaiting_release_overlay_locks_.end()) {
      // The released overlay should always be found as awaiting to be released.
      DLOG(FATAL) << "Got an unexpected mailbox";
      continue;
    }
    awaiting_release_overlay_locks_.erase(iter);
  }
#else
  // Only macOS has the requirement of polling the OS compositor to check if the
  // overlay images have been released.
  NOTREACHED_IN_MIGRATION();
#endif
}

void SkiaRenderer::EnsureScissorTestDisabled() {
  scissor_rect_.reset();
}

void SkiaRenderer::SetScissorTestRect(const gfx::Rect& scissor_rect) {
  scissor_rect_ = std::optional<gfx::Rect>(scissor_rect);
}

void SkiaRenderer::ClearCanvas(SkColor4f color) {
  if (!current_canvas_)
    return;

  if (scissor_rect_.has_value()) {
    // Limit the clear with the scissor rect.
    SkAutoCanvasRestore autoRestore(current_canvas_, true /* do_save */);
    current_canvas_->clipRect(gfx::RectToSkRect(scissor_rect_.value()));
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
    // TODO(crbug.com/40227119): add it back.
    ClearCanvas(SkColors::kBlue);
#endif
  }
}

bool SkiaRenderer::NeedsLayerForColorConversion(
    const AggregatedRenderPass* render_pass) {
  if (!base::FeatureList::IsEnabled(features::kColorConversionInRenderer)) {
    // Color conversion is already handled by a dedicated render pass if it was
    // needed.
    return false;
  }

  if (!render_pass->ShouldDrawWithBlending()) {
    // We don't need to be in a color space suitable for blending if we're not
    // doing any blending.
    return false;
  }

  const auto it = render_pass_backings_.find(render_pass->id);
  if (it == render_pass_backings_.end()) {
    // The render pass backing must be owned by the |render_pass_backings_|, the
    // output surface. If it is owned by the latter, then we know we're drawing
    // the root render pass.
    CHECK(!output_surface_->capabilities().renderer_allocates_images);
  }

  // If the color space of the render pass backing is suitable for blending, we
  // don't need to do any color conversion.
  const gfx::ColorSpace& pass_color_space =
      it != render_pass_backings_.end()
          ? it->second.color_space
          : RenderPassColorSpace(current_frame()->root_render_pass);
  if (pass_color_space.IsSuitableForBlending()) {
    return false;
  }

  CHECK(pass_color_space.IsHDR());
  return true;
}

gfx::ColorSpace SkiaRenderer::CurrentDrawLayerColorSpace() const {
  return hdr_color_conversion_layer_reset_
             ? gfx::ColorSpace::CreateExtendedSRGB()
             : RenderPassColorSpace(current_frame()->current_render_pass);
}

void SkiaRenderer::BeginDrawingRenderPass(
    const AggregatedRenderPass* render_pass,
    bool needs_clear,
    const gfx::Rect& render_pass_update_rect,
    const gfx::Size& viewport_size) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
               "SkiaRenderer::BeginDrawingRenderPass");

  // The root render pass will be either bound to the buffer allocated by the
  // SkiaOutputSurface, or if the renderer allocates images then the root render
  // pass buffer will be bound to the backing allocated in
  // AllocateRenderPassResourceIfNeeded().
  const bool is_root = render_pass == current_frame()->root_render_pass;
  if (is_root && !output_surface_->capabilities().renderer_allocates_images) {
    current_canvas_ = skia_output_surface_->BeginPaintCurrentFrame();
  } else {
    auto iter = render_pass_backings_.find(render_pass->id);
    // This function is called after AllocateRenderPassResourceIfNeeded, so
    // there should be backing ready.
    CHECK(render_pass_backings_.end() != iter);
    const RenderPassBacking& backing = iter->second;
    current_canvas_ = skia_output_surface_->BeginPaintRenderPass(
        render_pass->id, backing.size, backing.format, backing.alpha_type,
        backing.generate_mipmap ? skgpu::Mipmapped::kYes
                                : skgpu::Mipmapped::kNo,
        backing.scanout_dcomp_surface, RenderPassBackingSkColorSpace(backing),
        /*is_overlay=*/backing.is_scanout, backing.mailbox);
  }

  if (is_root && debug_settings_->show_overdraw_feedback) {
    current_canvas_ = skia_output_surface_->RecordOverdrawForCurrentPaint();
  }

  if (render_pass_update_rect == gfx::Rect(viewport_size)) {
    EnsureScissorTestDisabled();
  } else {
    SetScissorTestRect(render_pass_update_rect);
  }

  if (NeedsLayerForColorConversion(render_pass)) {
    CHECK(!hdr_color_conversion_layer_reset_.has_value());
    hdr_color_conversion_layer_reset_.emplace(current_canvas_.get(),
                                              /*do_save=*/true);

    const SkRect layer_bounds = gfx::RectToSkRect(render_pass_update_rect);
    if (scissor_rect_.has_value()) {
      // Set the clip rect so when we pop the layer, we only touch pixels within
      // the update rect.
      current_canvas_->clipRect(layer_bounds);
    }

    SkPaint no_blend;
    no_blend.setBlendMode(SkBlendMode::kSrc);
    const gfx::ColorSpace blend_color_space =
        gfx::ColorSpace::CreateExtendedSRGB();
    CHECK(blend_color_space.IsSuitableForBlending());
    sk_sp<const SkColorSpace> color_space = blend_color_space.ToSkColorSpace();
    current_canvas_->saveLayer(
        SkCanvas::SaveLayerRec(&layer_bounds, &no_blend,
                               /*backdrop=*/nullptr, color_space.get(),
                               SkCanvas::SaveLayerFlagsSet::kF16ColorType));
  }

  if (needs_clear) {
    ClearFramebuffer();
  }

  current_render_pass_update_rect_ = render_pass_update_rect;
}

void SkiaRenderer::DoDrawQuad(const DrawQuad* quad,
                              const gfx::QuadF* draw_region) {
  if (!current_canvas_)
    return;
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
               "SkiaRenderer::DoDrawQuad");
  DrawQuadParams params =
      CalculateDrawQuadParams(current_frame()->target_to_device_transform,
                              scissor_rect_, quad, draw_region);
  // The outer DrawQuad will never have RPDQ params
  DrawQuadInternal(quad, /* rpdq */ nullptr, &params);
}

void SkiaRenderer::DrawQuadInternal(const DrawQuad* quad,
                                    const DrawRPDQParams* rpdq_params,
                                    DrawQuadParams* params) {
  if (MustFlushBatchedQuads(quad, rpdq_params, *params)) {
    FlushBatchedQuads();
  }

  if (OverlayCandidate::RequiresOverlay(quad)) {
    // We cannot composite this quad properly, replace it with a fallback
    // solid color quad.
    if (!batched_quads_.empty()) {
      FlushBatchedQuads();
    }
#if DCHECK_IS_ON()
    DrawColoredQuad(SkColors::kMagenta, rpdq_params, params);
#else
    DrawColoredQuad(SkColors::kBlack, rpdq_params, params);
#endif
    return;
  }

  switch (quad->material) {
    case DrawQuad::Material::kAggregatedRenderPass:
      DrawRenderPassQuad(AggregatedRenderPassDrawQuad::MaterialCast(quad),
                         rpdq_params, params);
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
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
      break;
    case DrawQuad::Material::kInvalid:
      DrawUnsupportedQuad(quad, rpdq_params, params);
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void SkiaRenderer::PrepareCanvas(
    const std::optional<gfx::Rect>& scissor_rect,
    const std::optional<gfx::MaskFilterInfo>& mask_filter_info,
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
    const std::optional<gfx::MaskFilterInfo>& mask_filter_info) {
  if (!mask_filter_info || !mask_filter_info->HasGradientMask())
    return;

  const gfx::RectF rect = mask_filter_info->bounds();
  const std::optional<gfx::LinearGradient>& gradient_mask =
      mask_filter_info->gradient_mask();

  int16_t angle = gradient_mask->angle() % 360;
  if (angle < 0) angle += 360;

  SkPoint start_end[2];

  float rad_angle = base::DegToRad(static_cast<float>(angle));
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

  std::array<SkScalar, gfx::LinearGradient::kMaxStepSize> positions;
  std::array<SkColor, gfx::LinearGradient::kMaxStepSize> gradient_colors;

  size_t i = 0;
  for (; i < gradient_mask->step_count(); ++i) {
    positions[i] = gradient_mask->steps()[i].fraction;
    gradient_colors[i] = MaskColor(gradient_mask->steps()[i].alpha);
  }

  SkPoint::Offset(start_end, /*count=*/2, rect.x(), rect.y());
  sk_sp<SkShader> gradient = SkGradientShader::MakeLinear(
      start_end, gradient_colors.data(), positions.data(), /*count=*/i,
      SkTileMode::kClamp);
  current_canvas_->clipShader(std::move(gradient));
}

void SkiaRenderer::PrepareCanvasForRPDQ(const DrawRPDQParams& rpdq_params,
                                        DrawQuadParams* params) {
  // Clip before the saveLayer() so that Skia only filters the backdrop that is
  // necessary for the |backdrop_filter_bounds| (otherwise it will fill the
  // quad's SharedQuadState's |clip_rect|).
  rpdq_params.SetBackdropFilterClip(current_canvas_, params);

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

  SkRect bounds = rpdq_params.GetContentBounds(params);
  current_canvas_->saveLayer(SkCanvasPriv::ScaledBackdropLayer(
      &bounds, &layer_paint, rpdq_params.backdrop_filter.get(),
      rpdq_params.backdrop_filter_quality, 0));

  // If we have backdrop filtered content (and not transparent black like with
  // regular render passes), we have to clear out the parts of the layer that
  // shouldn't show the backdrop.
  rpdq_params.ClearOutsideBackdropBounds(current_canvas_, params);
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
    current_canvas_->clipShader(
        rpdq_params.mask_shader->GetOrCreateSkShader(this));
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
        gfx::RectFToSkRect(rpdq_params.bypass_geometry->clip_rect),
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
    current_canvas_->clipShader(
        rpdq_params.mask_shader->GetOrCreateSkShader(this));
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
        gfx::RectFToSkRect(rpdq_params.bypass_geometry->clip_rect),
        params->aa_flags != SkCanvas::kNone_QuadAAFlags);
  }
}

SkiaRenderer::DrawQuadParams SkiaRenderer::CalculateDrawQuadParams(
    const gfx::AxisTransform2d& target_to_device,
    const std::optional<gfx::Rect>& scissor_rect,
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
    const std::optional<gfx::Rect>& scissor_to_apply) {
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

  if (!Is2dScaleTranslateTransform(content_device_transform)) {
    return;
  }

  // State check: should not have a CompositorRenderPassDrawQuad if we got here.
  DCHECK_NE(quad->material, DrawQuad::Material::kCompositorRenderPass);
  if (const auto* quad_pass =
          quad->DynamicCast<AggregatedRenderPassDrawQuad>()) {
    // If the renderpass has filters, the filters may modify the effective
    // geometry beyond the quad's visible_rect, so it's not safe to pre-clip.
    // Note: no need to check against the backdrop filters, as they are always
    // restricted to the visible rect of a quad.
    auto pass_id = quad_pass->render_pass_id;
    if (const auto* filters = renderer->FiltersForPass(pass_id);
        filters && filters->HasFilterThatMovesPixels()) {
      return;
    }
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
  std::optional<gfx::RectF> local_scissor =
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
    const AggregatedRenderPass* pass,
    const RenderPassRequirements& requirements) {
  // If render pass bypassing is disabled for testing
  if (settings_->disable_render_pass_bypassing)
    return nullptr;

  // Only supports bypassing render passes with a single child quad and simple
  // content.
  if (pass->quad_list.size() != 1) {
    return nullptr;
  }

  // If it there are supposed to be mipmaps, the renderpass must exist
  if (pass->generate_mipmap)
    return nullptr;

    // Force passes whose backings can be directly scanned out from being a
    // bypass quad. This logic should mirror
    // |GetRenderPassBackingForDirectScanout|.
#if BUILDFLAG(IS_WIN)
  if (requirements.is_scanout) {
    return nullptr;
  }
#else
  // This platform doesn't support direct scanout, so we don't expect any
  // scanout render pass backings.
  CHECK(!requirements.is_scanout);
#endif

  const DrawQuad* quad = *pass->quad_list.BackToFrontBegin();
  // For simplicity in debug border and picture quad draw implementations, don't
  // bypass a render pass containing those. Their draw functions do not take a
  // DrawRPDQParams.
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

  if (const auto* render_pass_quad =
          quad->DynamicCast<AggregatedRenderPassDrawQuad>()) {
    if (render_pass_quad->mask_resource_id()) {
      return nullptr;
    }

    // Only allow merging render passes containing RenderPassDrawQuads if they
    // have 2D scale/translate transform. This allows merging clip rects into
    // a single intermediate coordinate space.
    if (!Is2dScaleTranslateTransform(
            render_pass_quad->shared_quad_state->quad_to_target_transform)) {
      return nullptr;
    }

    const auto nested_render_pass_id = render_pass_quad->render_pass_id;
    auto it = base::ranges::find_if(
        *current_frame()->render_passes_in_draw_order,
        [&nested_render_pass_id](const auto& render_pass) {
          return render_pass->id == nested_render_pass_id;
        });

    CHECK(it != current_frame()->render_passes_in_draw_order->end(),
          base::NotFatalUntil::M130);
    const auto& nested_render_pass = *it;
    if (!nested_render_pass->filters.IsEmpty() ||
        !nested_render_pass->backdrop_filters.IsEmpty()) {
      return nullptr;
    }
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
  SkMatrix bypass_to_rpdq = gfx::TransformToFlattenedSkMatrix(
      bypass_quad->shared_quad_state->quad_to_target_transform);

  if (params->draw_region) {
    SkMatrix rpdq_to_bypass;
    bool inverted = bypass_to_rpdq.invert(&rpdq_to_bypass);
    // Invertibility was a requirement for being bypassable.
    DCHECK(inverted);

    // The draw region was determined by the RPDQ's geometry, so map the
    // quadrilateral to the bypass'ed quad's coordinate space so that BSP
    // splitting is still respected.
    rpdq_to_bypass.mapPoints(params->draw_region->points.data(),
                             std::size(params->draw_region->points));
  }

  std::optional<gfx::RectF> bypassed_quad_clip_rect;
  if (rpdq_params->bypass_geometry) {
    // If BypassGeometry is already populated then this is part of a chain of
    // bypassed render passes. This merges any additional transform and clip
    // into the first bypassed render pass coordinate space. This is always
    // possible as RenderPassDrawQuads are only bypassed if they have 2D
    // scale/translation transform.
    auto& bypass_geometry = rpdq_params->bypass_geometry.value();
    gfx::Transform bypass_transform =
        gfx::SkMatrixToTransform(bypass_geometry.transform);
    DCHECK(Is2dScaleTranslateTransform(bypass_transform));

    // `bypass_geometry.clip_rect` is in the first bypassed render pass
    // coordinate space. The last CalculateBypassParams() updated
    // `params->visible_rect` so it is in the current bypassed render pass
    // coordinate space. Transform that into the first bypassed render pass
    // coordinate space and intersect with existing clip there. That way there
    // is a single intermediate clip entirely in the original bypassed render
    // pass coordinate space.
    gfx::RectF bypass_visible_rect =
        bypass_transform.MapRect(params->visible_rect);
    bypass_geometry.clip_rect.Intersect(bypass_visible_rect);

    if (bypass_quad->shared_quad_state->clip_rect) {
      // The bypass_quad clip_rect needs to be transformed from current bypassed
      // render pass coordinate space into the first bypassed render pass
      // coordinate space.
      bypassed_quad_clip_rect = bypass_transform.MapRect(
          gfx::RectF(*bypass_quad->shared_quad_state->clip_rect));
    }

    // Update transform so it maps from `bypass_quad` to the first bypassed
    // render pass coordinate space.
    bypass_geometry.transform.preConcat(bypass_to_rpdq);
  } else {
    // BypassGeometry holds the RenderPassDrawQuad visible_rect, which is in the
    // bypassed render pass coordinate space, along with the transform from
    // `bypass_quad` to bypassed render pass coordinate space.
    rpdq_params->bypass_geometry =
        DrawRPDQParams::BypassGeometry{bypass_to_rpdq, params->visible_rect};

    if (bypass_quad->shared_quad_state->clip_rect) {
      // The bypass_quad clip_rect is in the same coordinate space as the RPDQ +
      // bypassed render pass aka the same as bypass_geometry.
      bypassed_quad_clip_rect =
          gfx::RectF(*bypass_quad->shared_quad_state->clip_rect);
    }
  }

  if (bypassed_quad_clip_rect) {
    // If bypassed_quad clip_rect isn't empty then normally it would be added
    // to scissor_rect in SetScissorStateForQuad(). That never happens when the
    // render pass is bypassed so add it here.
    rpdq_params->bypass_geometry->clip_rect.Intersect(*bypassed_quad_clip_rect);
  }

  // Compute draw params for `bypass_quad` to update some of the original draw
  // params. Both transform and scissor_rect are already accounted for in
  // BypassGeometry so pass identify and empty for those.
  DrawQuadParams bypass_quad_params = CalculateDrawQuadParams(
      /*target_to_device=*/gfx::AxisTransform2d(),
      /*scissor_rect=*/std::nullopt, bypass_quad,
      /*draw_region=*/nullptr);

  // NOTE: params |content_device_transform| remains that of the RPDQ to prepare
  // the canvas' CTM to match what any image filters require. The above
  // BypassGeometry::transform is then applied when drawing so that these
  // updated coordinates are correctly transformed to device space.
  params->visible_rect = bypass_quad_params.visible_rect;
  params->vis_tex_coords = bypass_quad_params.vis_tex_coords;

  // Combine anti-aliasing policy (use AND so that any draw_region clipping
  // is preserved).
  params->aa_flags &= bypass_quad_params.aa_flags;

  // Blending will use the top-level RPDQ blend mode, but factor in the
  // content's opacity as well, since that would have normally been baked into
  // the RP's buffer.
  params->opacity *= bypass_quad_params.opacity;

  // Take the highest quality filter, since this single draw will reflect the
  // filtering decisions made both when drawing into the RP and when drawing the
  // RP results itself. The ord() lambda simulates this notion of "highest" when
  // we used to use FilterQuality.
  auto ord = [](const SkSamplingOptions& sampling) {
    if (sampling.useCubic) {
      return 3;
    } else if (sampling.mipmap != SkMipmapMode::kNone) {
      return 2;
    }
    return sampling.filter == SkFilterMode::kLinear ? 1 : 0;
  };

  if (ord(bypass_quad_params.sampling) > ord(params->sampling)) {
    params->sampling = bypass_quad_params.sampling;
  }

  // Rounded corner bounds are in device space, which gets tricky when bypassing
  // the device that the RP would have represented
  DCHECK(!bypass_quad_params.mask_filter_info.has_value());

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

  if (batched_cdt_matrices_.empty() || batched_cdt_matrices_.back() != m) {
    batched_cdt_matrices_.push_back(m);
  }
  int matrix_index = batched_cdt_matrices_.size() - 1;

  batched_quads_.push_back(MakeEntry(image, matrix_index, *params));
}

void SkiaRenderer::FlushBatchedQuads() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
               "SkiaRenderer::FlushBatchedQuads");

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
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
               "SkiaRenderer::DrawColoredQuad");
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
      params->draw_region ? params->draw_region->points.data() : nullptr;

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
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
               "SkiaRenderer::DrawSingleImage");

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
      params->draw_region ? params->draw_region->points.data() : nullptr;
  current_canvas_->experimental_DrawEdgeAAImageSet(
      &entry, 1, draw_region, bypass_transform, params->sampling, paint,
      constraint);
}

void SkiaRenderer::DrawPaintOpBuffer(
    const cc::PaintOpBuffer* buffer,
    const std::optional<SkColor4f>& clear_color,
    const TileDrawQuad* quad,
    const DrawQuadParams* params) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
               "SkiaRenderer::DrawPaintOpBuffer");
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
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
               "SkiaRenderer::DrawDebugBorderQuad");
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
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
               "SkiaRenderer::DrawPictureQuad");

  // If the layer is transparent or needs a non-SrcOver blend mode, saveLayer
  // must be used so that the display list is drawn into a transient image and
  // then blended as a single layer at the end.
  const bool needs_transparency =
      params->opacity < 1.f || params->blend_mode != SkBlendMode::kSrcOver;
  const bool disable_image_filtering = params->sampling == SkSamplingOptions();

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
  std::optional<skia::OpacityFilterCanvas> opacity_canvas;
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
  quad->display_item_list->Raster(raster_canvas, /*image_provider=*/nullptr,
                                  &quad->raster_inducing_scroll_offsets);
}

void SkiaRenderer::DrawSolidColorQuad(const SolidColorDrawQuad* quad,
                                      const DrawRPDQParams* rpdq_params,
                                      DrawQuadParams* params) {
  DrawColoredQuad(quad->color, rpdq_params, params);
}

void SkiaRenderer::DrawTextureQuad(const TextureDrawQuad* quad,
                                   const DrawRPDQParams* rpdq_params,
                                   DrawQuadParams* params) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
               "SkiaRenderer::DrawTextureQuad");

  // We need only RGB portion of the color space, YUV conversion handled in
  // skia.
  const gfx::ColorSpace src_color_space =
      resource_provider()
          ->GetColorSpace(quad->resource_id())
          .GetAsFullRangeRGB();
  const gfx::HDRMetadata& src_hdr_metadata =
      resource_provider()->GetHDRMetadata(quad->resource_id());
  const bool needs_color_conversion_filter =
      ((quad->is_video_frame && src_color_space.IsHDR()) ||
       src_color_space.IsToneMappedByDefault()) &&
      // Don't do color conversions for stream video.
      !quad->is_stream_video;

  sk_sp<SkColorSpace> override_color_space;
  if (needs_color_conversion_filter) {
    override_color_space = CurrentDrawLayerColorSpace().ToSkColorSpace();
  }

#if BUILDFLAG(IS_ANDROID)
  if (quad->is_stream_video) {
    // If overlay processor would override color space, override it here to to
    // avoid color changes during promotion.
    if (auto overlay_color_space =
            OverlayProcessorSurfaceControl::GetOverrideColorSpace()) {
      override_color_space = overlay_color_space->ToSkColorSpace();
    }
  }
#else
  // Only on android stream video can be composited.
  CHECK(!quad->is_stream_video);
#endif

  ScopedSkImageBuilder builder(
      this, quad->resource_id(), /*maybe_concurrent_reads=*/true,
      quad->premultiplied_alpha ? kPremul_SkAlphaType : kUnpremul_SkAlphaType,
      quad->y_flipped ? kBottomLeft_GrSurfaceOrigin : kTopLeft_GrSurfaceOrigin,
      override_color_space, false, quad->force_rgbx);
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
  // For video frames, `valid_texel_bounds` is VideoFrame::visible_rect which is
  // passed here via `uv_rect`.
  if (quad->is_video_frame) {
    valid_texel_bounds = uv_rect;
  }

  // There are three scenarios where a texture quad cannot be put into a batch:
  // 1. It needs to be blended with a constant background color.
  // 2. The quad contains video which might need special white level adjustment.
  const bool blend_background =
      quad->background_color != SkColors::kTransparent && !image->isOpaque();

  if (!blend_background && !needs_color_conversion_filter && !rpdq_params) {
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
    quad_alpha = 1.f;
  } else {
    // We will entirely handle the quad's opacity with the mask or color filter
    quad_alpha = params->opacity;
    params->opacity = 1.f;
  }

  // Auto-restore canvas state after applying clipShader and draw.
  SkAutoCanvasRestore acr(current_canvas_, /*do_save=*/true);

  if (needs_color_conversion_filter) {
    // Skia won't perform color conversion.
    const gfx::ColorSpace dst_color_space = CurrentDrawLayerColorSpace();
    DCHECK(SkColorSpace::Equals(image->colorSpace(),
                                dst_color_space.ToSkColorSpace().get()));
    sk_sp<SkColorFilter> color_filter = GetColorSpaceConversionFilter(
        src_color_space, std::nullopt, src_hdr_metadata, dst_color_space,
        quad->is_video_frame);
    paint.setColorFilter(color_filter->makeComposed(paint.refColorFilter()));
  }

  // From gl_renderer, the final src color will be
  // (textureColor + backgroundColor * (1 - textureAlpha))
  if (blend_background) {
    // Add a color filter that does DstOver blending between texture and the
    // background color. Then, modulate by quad's opacity *after* blending.
    // TODO(crbug.com/40219248) remove toSkColor and make all SkColor4f
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
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
               "SkiaRenderer::DrawTileDrawQuad");
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

  for (auto& overlay : current_frame()->overlay_list) {
    if (overlay.is_root_render_pass) {
      continue;
    }

#if BUILDFLAG(ENABLE_VULKAN) && BUILDFLAG(IS_CHROMEOS) && \
    BUILDFLAG(USE_V4L2_CODEC)
    if (overlay.needs_detiling) {
      if (!absl::holds_alternative<gfx::OverlayTransform>(overlay.transform)) {
        LOG(ERROR) << "Unsupported transform on tiled protected content.";
        continue;
      }

      locks.emplace_back(resource_provider(), overlay.resource_id);
      auto& lock = locks.back();

      bool is_10bit = overlay.format == MultiPlaneFormat::kP010;
      gpu::Mailbox detiled_image = GetProtectedSharedImage(is_10bit);
      skia_output_surface_->DetileOverlay(
          overlay.mailbox, overlay.resource_size_in_pixels, lock.sync_token(),
          detiled_image, overlay.display_rect, overlay.uv_rect,
          absl::get<gfx::OverlayTransform>(overlay.transform), is_10bit);
      overlay.uv_rect = gfx::RectF(
          static_cast<float>(overlay.display_rect.width()) /
              static_cast<float>(kMaxProtectedContentWidth),
          static_cast<float>(overlay.display_rect.height() /
                             static_cast<float>(kMaxProtectedContentHeight)));
      overlay.mailbox = detiled_image;
      overlay.format = (is_10bit && base::FeatureList::IsEnabled(
                                        media::kEnableArmHwdrm10bitOverlays))
                           ? SinglePlaneFormat::kBGRA_1010102
                           : SinglePlaneFormat::kBGRA_8888;
      overlay.transform = gfx::OVERLAY_TRANSFORM_NONE;

      continue;
    }
#endif

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
    if (overlay.rpdq) {
      // Try and use the render pass backing image directly as an overlay.
      if (auto backing = GetRenderPassBackingForDirectScanout(
              overlay.rpdq->render_pass_id);
          backing) {
        DBG_LOG("delegated.overlay.log",
                "Pass %" PRIu64 ": RPDQ overlay can scanout directly",
                overlay.rpdq->render_pass_id.value());

        // SkiaRenderer might've allocated a larger backing than our render
        // pass' requested size.
        overlay.uv_rect =
            gfx::MapRect(overlay.rpdq->tex_coord_rect,
                         gfx::RectF(backing->size), gfx::RectF(1, 1));

        if (overlay.rpdq->visible_rect != overlay.rpdq->rect) {
          // The overlay's |display_rect| is the quad's |visible_rect|, so we
          // need to adjust our |uv_rect| to account for that.
          overlay.uv_rect =
              gfx::MapRect(gfx::RectF(overlay.rpdq->visible_rect),
                           gfx::RectF(overlay.rpdq->rect), overlay.uv_rect);
        }

        overlay.mailbox = backing->mailbox;
        overlay.resource_size_in_pixels = backing->size;

        // We do not add the overlay mailbox to |locks| since we're using a
        // backing from |render_pass_backings_| directly. We can expect the
        // synchronization to be handled externally.
      } else {
        PrepareRenderPassOverlay(&overlay);

        if (!overlay.mailbox.IsZero()) {
          locks.emplace_back(this, overlay.mailbox);
        }
      }

      // The overlay will be sent to GPU the thread, so set rpdq to nullptr to
      // avoid being accessed on the GPU thread.
      overlay.rpdq = nullptr;

      continue;
    }
#else
    DCHECK(!overlay.rpdq);
#endif

    if (overlay.is_solid_color) {
      DCHECK(overlay.color);
      DCHECK(!overlay.resource_id);
      // All other platforms must support solid color overlays
      DCHECK(output_surface_->capabilities()
                 .supports_non_backed_solid_color_overlays ||
             output_surface_->capabilities().supports_single_pixel_buffer);
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

  DCHECK(!current_gpu_commands_completed_fence_->was_set());
  DCHECK(!current_release_fence_->was_set());

  skia_output_surface_->ScheduleOverlays(
      std::move(current_frame()->overlay_list), std::move(sync_tokens));
}

sk_sp<SkColorFilter> SkiaRenderer::GetColorSpaceConversionFilter(
    const gfx::ColorSpace& src,
    std::optional<uint32_t> src_bit_depth,
    std::optional<gfx::HDRMetadata> src_hdr_metadata,
    const gfx::ColorSpace& dst,
    bool is_video_frame) {
  // Use the current SDR slider white level for PQ HDR videos on
  // Windows, so that they look similar when rendered by the
  // compositor and when rendered as an overlay (HDR10 MPO).
  // https://crbug.com/1492817
  auto hdr_metadata = src_hdr_metadata;
  if (is_video_frame &&
      src.GetTransferID() == gfx::ColorSpace::TransferID::PQ &&
      base::FeatureList::IsEnabled(features::kUseDisplaySDRMaxLuminanceNits)) {
    hdr_metadata =
        gfx::HDRMetadata::PopulateUnspecifiedWithDefaults(src_hdr_metadata);
    hdr_metadata->ndwl = gfx::HdrMetadataNdwl(
        current_frame()->display_color_spaces.GetSDRMaxLuminanceNits());
  }
  return color_filter_cache_.Get(
      src, dst, src_bit_depth, hdr_metadata,
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
    const gfx::AxisTransform2d& target_to_device,
    const AggregatedRenderPassDrawQuad* quad,
    const DrawQuadParams* params) {
  DrawRPDQParams rpdq_params(params->visible_rect);

  if (!quad->mask_resource_id().is_null()) {
    // Scale normalized uv rect into absolute texel coordinates.
    SkRect mask_rect = gfx::RectFToSkRect(
        gfx::ScaleRect(quad->mask_uv_rect, quad->mask_texture_size.width(),
                       quad->mask_texture_size.height()));
    SkMatrix mask_to_quad_matrix =
        SkMatrix::RectToRect(mask_rect, gfx::RectToSkRect(quad->rect));
    rpdq_params.mask_shader.emplace(quad->mask_resource_id(),
                                    mask_to_quad_matrix);
  }

  const cc::FilterOperations* filters = FiltersForPass(quad->render_pass_id);
  const cc::FilterOperations* backdrop_filters =
      BackdropFiltersForPass(quad->render_pass_id);
  // Early out if there are no filters to convert to SkImageFilters
  if (!filters && !backdrop_filters) {
    return rpdq_params;
  }

  // Calculate local matrix that's shared by filters and backdrop_filters. This
  // local matrix represents the UI display scale that's already been applied to
  // the DrawQuads but not any geometric properties of the filters.
  SkMatrix local_matrix;
  local_matrix.setTranslate(quad->filters_origin.x(), quad->filters_origin.y());
  local_matrix.postScale(quad->filters_scale.x(), quad->filters_scale.y());

  auto to_sk_image_filter =
      [](sk_sp<cc::PaintFilter> paint_filter,
         const SkMatrix& local_matrix) -> sk_sp<SkImageFilter> {
    if (paint_filter && paint_filter->cached_sk_filter_) {
      return paint_filter->cached_sk_filter_->makeWithLocalMatrix(local_matrix);
    } else {
      return nullptr;
    }
  };

  // Convert CC image filters into a SkImageFilter root node
  if (filters) {
    DCHECK(!filters->IsEmpty());
    auto paint_filter = cc::RenderSurfaceFilters::BuildImageFilter(*filters);
    rpdq_params.image_filter =
        to_sk_image_filter(std::move(paint_filter), local_matrix);

    if (rpdq_params.image_filter) {
      // Update the filter bounds to account for how the image filters
      // grow or move the area touched by the base quad.
      rpdq_params.filter_bounds = rpdq_params.image_filter->computeFastBounds(
          rpdq_params.filter_bounds);

      // Attempt to simplify the image filter to a color filter, which enables
      // the RPDQ effects to be applied more efficiently.
      SkColorFilter* color_filter_ptr = nullptr;
      if (rpdq_params.image_filter->asAColorFilter(&color_filter_ptr)) {
        // asAColorFilter already ref'ed the filter when true is returned,
        // reset() does not add a ref itself, so everything is okay.
        rpdq_params.color_filter.reset(color_filter_ptr);
      }
    }
  }

  // Convert CC image filters for the backdrop into a SkImageFilter root node
  if (backdrop_filters) {
    DCHECK(!backdrop_filters->IsEmpty());
    rpdq_params.backdrop_filter_quality = quad->backdrop_filter_quality;

    // quad->rect represents the layer's bounds *after* any display scale has
    // been applied to it. The ZOOM FilterOperation uses the layer's bounds as
    // its "lens" bounds. All image filters operate with a local matrix to
    // match the display scale. We must undo the local matrix's effect on
    // quad->rect to get the input bounds for ZOOM. Otherwise its lens would be
    // doubly-scaled while none of the other filter operations would align.
    SkMatrix inv_local_matrix;
    if (local_matrix.invert(&inv_local_matrix)) {
      SkIRect filter_rect =
          inv_local_matrix.mapRect(gfx::RectToSkRect(quad->rect)).roundOut();
      auto bg_paint_filter = cc::RenderSurfaceFilters::BuildImageFilter(
          *backdrop_filters, gfx::SkIRectToRect(filter_rect));

      rpdq_params.backdrop_filter =
          to_sk_image_filter(std::move(bg_paint_filter), local_matrix);
    }
  }

  // Determine the clipping to apply to the backdrop filter. Skia normally
  // fills layers with the backdrop content, whereas viz wants the backdrop
  // content restricted to the intersection of the DrawQuad and any defined
  // |backdrop_filter_bounds|.
  if (rpdq_params.backdrop_filter) {
    SkRect backdrop_rect = gfx::RectFToSkRect(params->visible_rect);
    // Pass bounds do not match the display scale; they will be scaled and
    // converted into an SkRRect in |backdrop_filter_bounds| if defined.
    std::optional<gfx::RRectF> pass_bounds =
        BackdropFilterBoundsForPass(quad->render_pass_id);
    std::optional<SkRRect> backdrop_filter_bounds;
    if (pass_bounds) {
      // Scale by the filter's scale, but don't apply filter origin
      SkRRect result;
      if (!SkRRect(*pass_bounds).transform(local_matrix, &result) ||
          !backdrop_rect.intersect(result.rect())) {
        // No visible backdrop filter
        rpdq_params.backdrop_filter = nullptr;
        return rpdq_params;
      } else {
        backdrop_filter_bounds = result;
      }

      if (backdrop_filter_bounds->contains(rpdq_params.filter_bounds)) {
        // The backdrop filter bounds are a no-op since the quad rect or region
        // fully limits the backdrop filter.
        backdrop_filter_bounds.reset();
      } else {
        // The backdrop filter bounds might have an effect, but a simple case to
        // check for is if the backdrop rounded corners are identical to the
        // quad's rounded corner mask info. In that case, the prior contains()
        // check would be false, but we can still discard these bounds since the
        // final mask clip will achieve the same visual effect.
        if (params->mask_filter_info) {
          SkMatrix m = gfx::TransformToFlattenedSkMatrix(
              params->content_device_transform);
          if (backdrop_filter_bounds->transform(m, &result) &&
              SkRRect(params->mask_filter_info->rounded_corner_bounds()) ==
                  result) {
            backdrop_filter_bounds.reset();
          }
        }
      }
    }

    // Besides ensuring the output of the backdrop filter doesn't go beyond its
    // bounds, it should not read pixels outside of its bounds to prevent color
    // bleeding. If it's a pixel-moving filter, we compose a kMirror-tiling Crop
    // image filter to enforce this requirement. Mirror tiling avoids jarring
    // discontinuities and flickering when content moves in and out of the
    // background. See https://github.com/w3c/fxtf-drafts/issues/374.
    // NOTE: The above comment refers to the intended ideal behavior. Originally
    // the edge mode was kClamp and a feature controls the active mode.
    SkIRect sk_crop_rect = backdrop_rect.roundOut();
    SkIRect sk_src_rect = rpdq_params.backdrop_filter->filterBounds(
        sk_crop_rect, SkMatrix::I(), SkImageFilter::kReverse_MapDirection,
        /*inputRect=*/nullptr);
    if (!sk_crop_rect.contains(sk_src_rect)) {
      SkTileMode sk_tile_mode =
          base::FeatureList::IsEnabled(features::kBackdropFilterMirrorEdgeMode)
              ? SkTileMode::kMirror
              : SkTileMode::kClamp;
      rpdq_params.backdrop_filter = SkImageFilters::Compose(
          /*outer=*/std::move(rpdq_params.backdrop_filter),
          /*inner=*/SkImageFilters::Crop(backdrop_rect, sk_tile_mode, nullptr));
    }

    // Update |filter_bounds| to include content produced by the backdrop. Under
    // most circumstances this will be a no-op since content is restricted to
    // underneath the RPDQ's draw region, but if a backdrop filter is combined
    // with some pixel-moving filters, that may not remain the case and this
    // ensures |filter_bounds| will contain all possible output.
    rpdq_params.filter_bounds.join(backdrop_rect);
    rpdq_params.backdrop_filter_bounds = backdrop_filter_bounds;
  }

  return rpdq_params;
}

void SkiaRenderer::DrawRenderPassQuad(
    const AggregatedRenderPassDrawQuad* quad,
    const DrawRPDQParams* bypassed_rpdq_params,
    DrawQuadParams* params) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
               "SkiaRenderer::DrawRenderPassQuad");
  DrawRPDQParams rpdq_params =
      bypassed_rpdq_params
          ? *bypassed_rpdq_params
          : CalculateRPDQParams(current_frame()->target_to_device_transform,
                                quad, params);

  // |filter_bounds| is the content space bounds that includes any filtered
  // extents. If empty, the draw can be skipped.
  if (rpdq_params.filter_bounds.isEmpty()) {
    return;
  }

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
  if (iter == render_pass_backings_.end()) {
    if (base::FeatureList::IsEnabled(
            kDumpWithoutCrashingOnMissingRenderPassBacking)) {
      // This can happen if we previously skipped drawing a render pass (and
      // allocating its backing) due to an empty update rect.
      LOG(ERROR) << "Could not find render pass id # " << quad->render_pass_id
                 << " in the render pass overlay backings";

      SCOPED_CRASH_KEY_STRING32(
          "missing rp backing", "0-seen before?",
          base::NumberToString(
              seen_render_pass_ids_.contains(quad->render_pass_id)));

      // This is derived from |DirectRenderer::ShouldSkipQuad|.
      gfx::Rect target_rect = quad->visible_rect;
      SCOPED_CRASH_KEY_STRING32("missing rp backing", "1-visible rect",
                                target_rect.ToString());
      auto filter_it = render_pass_filters_.find(quad->render_pass_id);
      if (filter_it != render_pass_filters_.end()) {
        target_rect =
            filter_it->second->ExpandRectForPixelMovement(target_rect);
      }
      SCOPED_CRASH_KEY_STRING32("missing rp backing", "2-filter expansion",
                                filter_it != render_pass_filters_.end()
                                    ? target_rect.ToString()
                                    : "no filter expansion");

      const gfx::QuadF target_quad =
          quad->shared_quad_state->quad_to_target_transform.MapQuad(
              gfx::QuadF(gfx::RectF(target_rect)));
      SCOPED_CRASH_KEY_STRING256("missing rp backing", "3-rpdq in draw",
                                 target_quad.IsRectilinear()
                                     ? target_quad.BoundingBox().ToString()
                                     : target_quad.ToString());

      gfx::Rect draw_rect_in_draw_space = OutputSurfaceRectInDrawSpace();
      SCOPED_CRASH_KEY_STRING32("missing rp backing", "4-output surface",
                                draw_rect_in_draw_space.ToString());
      if (scissor_rect_) {
        draw_rect_in_draw_space = scissor_rect_.value();
        draw_rect_in_draw_space.Offset(
            current_frame()
                ->current_render_pass->output_rect.OffsetFromOrigin());
      }
      SCOPED_CRASH_KEY_STRING32(
          "missing rp backing", "5-with scissor?",
          scissor_rect_ ? draw_rect_in_draw_space.ToString() : "no scissor");

      if (quad->shared_quad_state->clip_rect) {
        draw_rect_in_draw_space.Intersect(*quad->shared_quad_state->clip_rect);
      }
      SCOPED_CRASH_KEY_STRING32("missing rp backing", "6-with quad clip?",
                                quad->shared_quad_state->clip_rect
                                    ? draw_rect_in_draw_space.ToString()
                                    : "no quad clip");

      const bool intersects =
          target_quad.IntersectsRect(gfx::RectF(draw_rect_in_draw_space));
      SCOPED_CRASH_KEY_STRING32("missing rp backing", "7-intersects?",
                                base::NumberToString(intersects));

      SCOPED_CRASH_KEY_STRING32(
          "missing rp backing", "8-quad-pass-id",
          base::NumberToString(quad->render_pass_id.value()));

      std::vector<std::string> pass_ids;
      for (const auto& pass : *current_frame()->render_passes_in_draw_order) {
        pass_ids.push_back(base::NumberToString(pass->id.value()));
      }
      SCOPED_CRASH_KEY_STRING256("missing rp backing", "9-frame-pass-ids",
                                 base::JoinString(pass_ids, ","));

      pass_ids.clear();
      for (const auto& [id, pass] : render_pass_backings_) {
        pass_ids.push_back(base::NumberToString(id.value()));
      }
      SCOPED_CRASH_KEY_STRING256("missing rp backing", "10-backing-pass-ids",
                                 base::JoinString(pass_ids, ","));

      auto it =
          base::ranges::find(*current_frame()->render_passes_in_draw_order,
                             quad->render_pass_id, &AggregatedRenderPass::id);
      SCOPED_CRASH_KEY_STRING256(
          "missing rp backing", "11-rp transform",
          it != current_frame()->render_passes_in_draw_order->end()
              ? it->get()->transform_to_root_target.ToString()
              : "missing pass in frame");

      SCOPED_CRASH_KEY_STRING256(
          "missing rp backing", "12-rpdq transform",
          quad->shared_quad_state->quad_to_target_transform.ToString());

      // Collect a dump so we can investigate the root cause, but fallback to a
      // solid color to avoid disrupting the user.
      base::debug::DumpWithoutCrashing();
    }

    // The fallback is a solid color quad, which do not support batching.
    if (!batched_quads_.empty()) {
      FlushBatchedQuads();
    }
#if DCHECK_IS_ON()
    DrawColoredQuad(SkColors::kRed, &rpdq_params, params);
#else
    DrawColoredQuad(SkColors::kWhite, &rpdq_params, params);
#endif
    return;
  }
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
      !rpdq_params.mask_shader && !rpdq_params.bypass_geometry) {
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
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
               "SkiaRenderer::CopyDrawnRenderPass");

  // Root framebuffer uses a zero-mailbox in SkiaOutputSurface.
  gpu::Mailbox mailbox;
  const auto* const render_pass = current_frame()->current_render_pass.get();
  AggregatedRenderPassId render_pass_id = render_pass->id;
  auto it = render_pass_backings_.find(render_pass_id);
  if (it != render_pass_backings_.end()) {
    mailbox = it->second.mailbox;
  }

  skia_output_surface_->CopyOutput(geometry, RenderPassColorSpace(render_pass),
                                   std::move(request), mailbox);
}

void SkiaRenderer::DidChangeVisibility() {
  if (visible_)
    output_surface_->EnsureBackbuffer();
  else
    output_surface_->DiscardBackbuffer();
}

void SkiaRenderer::FinishDrawingRenderPass() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
               "SkiaRenderer::FinishDrawingRenderPass");
  if (!current_canvas_)
    return;

  if (!batched_quads_.empty())
    FlushBatchedQuads();

  bool is_root_render_pass =
      current_frame()->current_render_pass == current_frame()->root_render_pass;

  // Drawing the delegated ink trail must happen after the final
  // FlushBatchedQuads() call so that the trail can always be on top of
  // everything else that has already been drawn on the page.
  if (UsingSkiaForDelegatedInk()) {
    if (const auto pass_id = delegated_ink_handler_->GetInkRenderer()
                                 ->GetLatestMetadataRenderPassId();
        current_frame()->current_render_pass->id == pass_id) {
      gfx::Transform root_target_to_render_pass_draw_transform;
      if (current_frame()
              ->current_render_pass->transform_to_root_target.GetInverse(
                  &root_target_to_render_pass_draw_transform)) {
        DrawDelegatedInkTrail(root_target_to_render_pass_draw_transform);
      }
    }
  }

  // Pops a layer that is pushed at the start of |BeginDrawingRenderPass|. This
  // applies color space conversion for HDR passes, if present.
  hdr_color_conversion_layer_reset_.reset();

  current_canvas_ = nullptr;
  // Non-root render passes that are scheduled as overlays will be painted in
  // PrepareRenderPassOverlay().
  bool is_overlay =
      skia_output_surface_->capabilities().renderer_allocates_images &&
      is_root_render_pass;
  EndPaint(current_render_pass_update_rect_, /*failed=*/false, is_overlay);

  // Defer flushing drawing task for root render pass, to avoid extra
  // MakeCurrent() call. It is expensive on GL.
  // TODO(crbug.com/40154045): Consider deferring drawing tasks for
  // all render passes.
  if (is_root_render_pass)
    return;

  FlushOutputSurface();
}

void SkiaRenderer::UpdateRenderPassTextures(
    const AggregatedRenderPassList& render_passes_in_draw_order,
    const base::flat_map<AggregatedRenderPassId, RenderPassRequirements>&
        render_passes_in_frame) {
  const auto& root_pass_id = render_passes_in_draw_order.back()->id;
  std::vector<AggregatedRenderPassId> passes_to_delete;
  for (const auto& [backing_id, backing] : render_pass_backings_) {
    // Buffer queue's root manages the root pass backing and its bookkeeping
    // separately from other render pass backings.
    if (buffer_queue_) {
      // If a root backing exists but its id does not match the current root
      // render pass id, then it must be an old backing that should be deleted.
      // Otherwise we should not delete a root backing in case it is scheduled
      // this frame but not drawn (e.g. in the case of overlay-only damage).
      if (backing.is_root) {
        if (backing_id != root_pass_id) {
          passes_to_delete.push_back(backing_id);
        }
        continue;
      }
    }

    auto render_pass_it = render_passes_in_frame.find(backing_id);
    if (render_pass_it == render_passes_in_frame.end()) {
      passes_to_delete.push_back(backing_id);
      DBG_LOG("renderer.skia.render_pass_backings",
              "render_pass %" PRIu64 " is no longer in frame",
              backing_id.value());
      continue;
    }

    const RenderPassRequirements& requirements = render_pass_it->second;
    const bool size_is_exact_match = backing.size == requirements.size;
    const bool size_is_sufficient =
        backing.size.width() >= requirements.size.width() &&
        backing.size.height() >= requirements.size.height();
    bool size_appropriate =
        backing.is_root ? size_is_exact_match : size_is_sufficient;
    bool mipmap_appropriate =
        !requirements.generate_mipmap || backing.generate_mipmap;
    bool no_change_in_format = requirements.format == backing.format;
    bool no_change_in_alpha_type =
        requirements.alpha_type == backing.alpha_type;
    bool no_change_in_color_space =
        requirements.color_space == backing.color_space;
    bool scanout_appropriate =
        requirements.is_scanout == backing.is_scanout &&
        requirements.scanout_dcomp_surface == backing.scanout_dcomp_surface;

    if (!size_appropriate || !mipmap_appropriate || !no_change_in_format ||
        !no_change_in_alpha_type || !no_change_in_color_space ||
        !scanout_appropriate) {
      passes_to_delete.push_back(backing_id);
      DBG_LOG("renderer.skia.render_pass_backings",
              "render_pass %" PRIu64
              " allocation part not appropriate:%s%s%s%s%s%s",
              backing_id.value(), !size_appropriate ? " size" : "",
              !mipmap_appropriate ? " mipmap" : "",
              !no_change_in_format ? " format" : "",
              !no_change_in_alpha_type ? " alpha_type" : "",
              !no_change_in_color_space ? " color_space" : "",
              !scanout_appropriate ? " scanout" : "");
    }
  }

  // Delete RenderPass backings from the previous frame that will not be used
  // again.
  for (size_t i = 0; i < passes_to_delete.size(); ++i) {
    auto it = render_pass_backings_.find(passes_to_delete[i]);
    auto& backing = it->second;
    // Root render pass backings managed by |buffer_queue_| are not managed by
    // DisplayResourceProvider, so we should not destroy them here. This
    // reallocation is done in Reshape before drawing the frame
    if (!(buffer_queue_ && backing.is_root)) {
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
  const bool is_root = render_pass_id == current_frame()->root_render_pass->id;

  // Root render pass backings managed by |buffer_queue_| are not managed by
  // DisplayResourceProvider, so we should not allocate them here.
  if (buffer_queue_ && is_root) {
    auto& root_pass_backing = render_pass_backings_[render_pass_id];
    root_pass_backing.is_root = true;
    root_pass_backing.mailbox = buffer_queue_->GetCurrentBuffer();
    root_pass_backing.generate_mipmap = requirements.generate_mipmap;
    root_pass_backing.size = requirements.size;
    root_pass_backing.format = requirements.format;
    root_pass_backing.alpha_type = requirements.alpha_type;
    root_pass_backing.color_space = requirements.color_space;
    root_pass_backing.is_scanout = true;
    root_pass_backing.scanout_dcomp_surface = false;
    return;
  }

  auto it = render_pass_backings_.find(render_pass_id);
  if (it != render_pass_backings_.end()) {
    DCHECK(gfx::Rect(it->second.size).Contains(gfx::Rect(requirements.size)));
    // A root backing should not be used for other render passes. If the root
    // pass id has changed, then it's old backing should have been deleted
    // already in UpdateRenderPassTextures().
    DCHECK(!(buffer_queue_ && it->second.is_root));
    return;
  }

  gpu::SharedImageUsageSet usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                   gpu::SHARED_IMAGE_USAGE_DISPLAY_WRITE;
  if (requirements.generate_mipmap) {
    DCHECK(!requirements.is_scanout);
    usage |= gpu::SHARED_IMAGE_USAGE_MIPMAP;
  }
  if (requirements.is_scanout) {
    usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;

#if BUILDFLAG(IS_WIN)
    // DComp surfaces do not support RGB10A2 so we must fall back to swap
    // chains. If this happens with video overlays, this can result in the video
    // overlay and its parent surface having unsynchronized updates.
    //
    // TODO(tangm): We should clean this up by either avoiding HDR or using
    //              RGBAF16 surfaces in this case.
    const bool dcomp_surface_unsupported_format =
        requirements.format == SinglePlaneFormat::kRGBA_1010102;

    if (requirements.scanout_dcomp_surface &&
        !dcomp_surface_unsupported_format) {
      usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT_DCOMP_SURFACE;

      // DComp surfaces are write-only, viz cannot sample them.
      usage.RemoveAll(gpu::SHARED_IMAGE_USAGE_DISPLAY_READ);
    } else {
      usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT_DXGI_SWAP_CHAIN;
    }
#else
    DCHECK(!requirements.scanout_dcomp_surface);
#endif
  } else {
    DCHECK(!requirements.scanout_dcomp_surface);
  }

  auto mailbox = skia_output_surface_->CreateSharedImage(
      requirements.format, requirements.size, requirements.color_space,
      requirements.alpha_type, usage, "RenderPassBacking",
      gpu::kNullSurfaceHandle);

  VizDebuggerLog::DebugLogNewRenderPassBacking(render_pass_id, requirements);

  render_pass_backings_.emplace(
      render_pass_id,
      RenderPassBacking(requirements.size, requirements.generate_mipmap,
                        requirements.color_space, requirements.alpha_type,
                        requirements.format, mailbox, is_root,
                        requirements.is_scanout,
                        requirements.scanout_dcomp_surface));
  if (base::FeatureList::IsEnabled(
          kDumpWithoutCrashingOnMissingRenderPassBacking)) {
    seen_render_pass_ids_.insert(render_pass_id);
  }
}

void SkiaRenderer::FlushOutputSurface() {
  auto sync_token = skia_output_surface_->Flush();
  lock_set_for_external_use_.UnlockResources(sync_token);
}

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
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

std::optional<SkiaRenderer::RenderPassBacking>
SkiaRenderer::GetRenderPassBackingForDirectScanout(
    const AggregatedRenderPassId& render_pass_id) const {
#if BUILDFLAG(IS_WIN)
  if (auto backing_it = render_pass_backings_.find(render_pass_id);
      backing_it != render_pass_backings_.end()) {
    if (backing_it->second.is_scanout) {
      if (DCHECK_IS_ON()) {
        auto pass_it =
            base::ranges::find(*current_frame()->render_passes_in_draw_order,
                               backing_it->first, &AggregatedRenderPass::id);
        CHECK(pass_it != current_frame()->render_passes_in_draw_order->end());

        DCHECK(!pass_it->get()->generate_mipmap);
        DCHECK(pass_it->get()->filters.IsEmpty());
        DCHECK(pass_it->get()->backdrop_filters.IsEmpty());
        DCHECK(!(pass_it->get()->will_backing_be_read_by_viz &&
                 backing_it->second.scanout_dcomp_surface));
      }

      return std::make_optional(backing_it->second);
    }
  }
#else
  // Non-Win backends need BufferQueue support on render pass backings. Any new
  // implementation should also modify |CanPassBeDrawnDirectly| to avoid the
  // bypass quad case for direct scanout backings.
#endif

  return std::nullopt;
}

SkiaRenderer::RenderPassOverlayParams*
SkiaRenderer::GetOrCreateRenderPassOverlayBacking(
    AggregatedRenderPassId render_pass_id,
    const AggregatedRenderPassDrawQuad* rpdq,
    SharedImageFormat buffer_format,
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
    auto kOverlayUsage = gpu::SHARED_IMAGE_USAGE_SCANOUT |
                         gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                         gpu::SHARED_IMAGE_USAGE_DISPLAY_WRITE;

    auto mailbox = skia_output_surface_->CreateSharedImage(
        buffer_format, buffer_size, color_space, RenderPassAlphaType::kPremul,
        kOverlayUsage, "RenderPassOverlay", gpu::kNullSurfaceHandle);
    overlay_params.render_pass_backing = {
        buffer_size,
        /*generate_mipmap=*/false,
        color_space,
        RenderPassAlphaType::kPremul,
        buffer_format,
        mailbox,
        /*is_root=*/false,
        /*is_scanout=*/true,
        /*scanout_dcomp_surface=*/false,
    };
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

  auto* const quad = overlay->rpdq.get();

  // The |current_render_pass| could be used for calculating destination
  // color space or clipping rect for backdrop filters. However
  // the |current_render_pass| is nullptr during ScheduleOverlays(), since all
  // overlay quads should be in the |root_render_pass|, before they are promoted
  // to overlays, so set the |root_render_pass| to the |current_render_pass|.
  base::AutoReset<raw_ptr<const AggregatedRenderPass>>
      auto_reset_current_render_pass(&current_frame()->current_render_pass,
                                     current_frame()->root_render_pass);

  auto* shared_quad_state =
      const_cast<SharedQuadState*>(quad->shared_quad_state);

  std::optional<gfx::Transform> quad_to_target_transform_inverse;
  if (shared_quad_state->quad_to_target_transform.IsInvertible()) {
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

  // The |clip_rect| is in the target coordinate space with all transforms
  // (translation, scaling, rotation, etc), so remove them since we are
  // later updating the quad-to-target transform to be the identity. These
  // properties must stay in sync with the transform in case they are used
  // in Calculate[RPDQ|DrawQuad]Params(), and so we can restrict the overlay
  // backing size to just what is visible.
  std::optional<base::AutoReset<std::optional<gfx::Rect>>> auto_reset_clip_rect;
  if (quad_to_target_transform_inverse) {
    // |output_rect| is also in the original target space and is the default
    // clip that we should include since we have to manually apply that as
    // well for overlay sizing.
    gfx::RectF clip_rect(quad->shared_quad_state->clip_rect.value_or(
        current_frame()->current_render_pass->output_rect));
    // NOTE: If the quad to target transform has rotation, skew, or perspective,
    // the modified clip rect might be expanded to ensure the quad image covers
    // the original area when transformed by the overlay.
    clip_rect = quad_to_target_transform_inverse->MapRect(clip_rect);
    auto_reset_clip_rect.emplace(&shared_quad_state->clip_rect,
                                 gfx::ToEnclosedRect(clip_rect));
  } else {
    // If we can't position the clip rect into render pass space, we shouldn't
    // use it when rendering.
    auto_reset_clip_rect.emplace(&shared_quad_state->clip_rect, std::nullopt);
  }

  // The |mask_filter_info| is in the device coordinate and with all transforms
  // (translation, scaling, rotation, etc), so remove them.
  std::optional<base::AutoReset<gfx::MaskFilterInfo>> auto_reset_mask_info;
  if (!shared_quad_state->mask_filter_info.IsEmpty()) {
    if (quad_to_target_transform_inverse) {
      // Instantiate the auto reset with the original value so that
      // ApplyTransform can modify it in place, but it will still be reset to
      // the original value.
      auto_reset_mask_info.emplace(&shared_quad_state->mask_filter_info,
                                   shared_quad_state->mask_filter_info);
      // NOTE: ApplyTransform only supports scale+translate transformations, if
      // the quad has rotation, skew, or perspective, the mask filter will be
      // discarded automatically since it can't be represented in the quad's
      // local coordinate space.
      shared_quad_state->mask_filter_info.ApplyTransform(
          *quad_to_target_transform_inverse);
    } else {
      // If we can't position the mask info into the render pass space, we
      // shouldn't use it when rendering.
      auto_reset_mask_info.emplace(&shared_quad_state->mask_filter_info,
                                   gfx::MaskFilterInfo());
    }
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
    params = CalculateDrawQuadParams(target_to_device,
                                     /*scissor_rect=*/std::nullopt, quad,
                                     /*draw_region=*/nullptr);
    rpdq_params = CalculateRPDQParams(target_to_device, quad, &params);
  }

  gfx::Rect filter_bounds =
      gfx::SkIRectToRect(rpdq_params.filter_bounds.roundOut());
  // Apply the clip that is normally handled indirectly via SkCanvas::clipRect.
  // Since |shared_quad_state->quad_to_target_transform| and |clip_rect| were
  // modified above, it is in the same coordinate space as |filter_bounds| now.
  if (shared_quad_state->clip_rect) {
    filter_bounds.Intersect(*shared_quad_state->clip_rect);
  }
  // If empty, the draw can be skipped
  if (filter_bounds.IsEmpty()) {
    return;
  }

  SharedImageFormat si_format;
  gfx::ColorSpace color_space;

  RenderPassBacking* src_quad_backing = nullptr;
  auto bypass = render_pass_bypass_quads_.find(quad->render_pass_id);
  BypassMode bypass_mode = BypassMode::kSkip;
  // When Render Pass has a single quad inside we would draw that directly.
  if (bypass != render_pass_bypass_quads_.end()) {
    bypass_mode = CalculateBypassParams(bypass->second, &rpdq_params, &params);
    if (bypass_mode == BypassMode::kSkip) {
      return;
    }

    // For bypassed render pass, we use the same format and color space for the
    // framebuffer.
    si_format = reshape_si_format();
    color_space = reshape_color_space();
  } else {
    // A real render pass that was turned into an image
    auto it = render_pass_backings_.find(quad->render_pass_id);
    CHECK(render_pass_backings_.end() != it, base::NotFatalUntil::M130);
    // This function is called after AllocateRenderPassResourceIfNeeded, so
    // there should be backing ready.
    src_quad_backing = &it->second;
    si_format = src_quad_backing->format;
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
        quad->render_pass_id, quad, si_format, color_space, buffer_size);
  }
  DCHECK(overlay_params);
  UMA_HISTOGRAM_BOOLEAN(
      "Compositing.SkiaRenderer.SkipOverlayRenderPassDrawQuad",
      can_skip_render_pass);

  const RenderPassBacking& dst_overlay_backing =
      overlay_params->render_pass_backing;
  overlay->mailbox = dst_overlay_backing.mailbox;
  overlay->resource_size_in_pixels = dst_overlay_backing.size;

  if (can_skip_render_pass) {
    int pixel_size = quad->rect.width() * quad->rect.height();
    UMA_HISTOGRAM_COUNTS_10M(
        "Compositing.SkiaRenderer.SkippedOverlayRenderPassDrawQuadSize",
        pixel_size);
  } else {
    current_canvas_ = skia_output_surface_->BeginPaintRenderPass(
        quad->render_pass_id, dst_overlay_backing.size,
        dst_overlay_backing.format, dst_overlay_backing.alpha_type,
        skgpu::Mipmapped::kNo, dst_overlay_backing.scanout_dcomp_surface,
        RenderPassBackingSkColorSpace(dst_overlay_backing),
        /*is_overlay=*/true, overlay->mailbox);
    if (!current_canvas_) {
      DLOG(ERROR)
          << "BeginPaintRenderPass() in PrepareRenderPassOverlay() failed.";
      return;
    }

    current_canvas_->clear(SK_ColorTRANSPARENT);

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
        NOTREACHED_IN_MIGRATION();
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
        EndPaint(gfx::Rect(dst_overlay_backing.size), /*failed=*/true,
                 /*is_overlay=*/true);
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
    EndPaint(gfx::Rect(dst_overlay_backing.size), /*failed=*/false,
             /*is_overlay=*/true);
  }

#if BUILDFLAG(IS_APPLE)
  // Adjust |bounds_rect| to contain the whole buffer and at the right location.
  overlay->display_rect.set_origin(gfx::PointF(filter_bounds.origin()));
  overlay->display_rect.set_size(gfx::SizeF(buffer_size));
#else   // BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
  // TODO(fangzhoug): Merge Ozone and Apple code paths of delegated compositing.

  // Set |uv_rect| to reflect rounding up from |filter_bounds| to |buffer_size|.
  overlay->uv_rect = gfx::RectF(filter_bounds.size());
  overlay->uv_rect.InvScale(buffer_size.width(), buffer_size.height());

  if (absl::holds_alternative<gfx::OverlayTransform>(overlay->transform)) {
    // When using an OverlayTransform, the transform should be baked into the
    // display_rect.
    overlay->display_rect =
        quad->shared_quad_state->quad_to_target_transform.MapRect(
            gfx::RectF(filter_bounds));
    // Apply all clipping because we can't always delegate quads that extend
    // beyond window bounds in Lacros.
    gfx::Rect apply_clip = gfx::Rect(current_frame()->device_viewport_size);
    if (overlay->clip_rect.has_value()) {
      apply_clip.Intersect(overlay->clip_rect.value());
    }
    OverlayCandidate::ApplyClip(*overlay, gfx::RectF(apply_clip));
    overlay->clip_rect = std::nullopt;
  } else {
    overlay->display_rect = gfx::RectF(filter_bounds);
  }

  // Fill in |format| and |color_space| information based on selected backing.
  overlay->color_space = color_space;
  overlay->format = si_format;
#endif  // BUILDFLAG(IS_APPLE)
}
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)

void SkiaRenderer::EndPaint(const gfx::Rect& update_rect,
                            bool failed,
                            bool is_overlay) {
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
  skia_output_surface_->EndPaint(std::move(on_finished_callback),
                                 std::move(on_return_release_fence_cb),
                                 update_rect, is_overlay);
}

bool SkiaRenderer::IsRenderPassResourceAllocated(
    const AggregatedRenderPassId& render_pass_id) const {
  auto it = render_pass_backings_.find(render_pass_id);
  return it != render_pass_backings_.end();
}

gfx::Size SkiaRenderer::GetRenderPassBackingPixelSize(
    const AggregatedRenderPassId& render_pass_id) {
  auto it = render_pass_backings_.find(render_pass_id);
  CHECK(it != render_pass_backings_.end(), base::NotFatalUntil::M130);
  return it->second.size;
}

gfx::Rect SkiaRenderer::GetRenderPassBackingDrawnRect(
    const AggregatedRenderPassId& render_pass_id) const {
  if (auto it = render_pass_backings_.find(render_pass_id);
      it != render_pass_backings_.end()) {
    return it->second.drawn_rect;
  } else {
    // DirectRenderer can call this before it has allocated a render pass
    // backing if this is the first contiguous frame we're seeing
    // |render_pass_id|. This can happen because it calculates the render pass
    // scissor rect before it actually allocates the backing.
    return gfx::Rect();
  }
}

void SkiaRenderer::SetRenderPassBackingDrawnRect(
    const AggregatedRenderPassId& render_pass_id,
    const gfx::Rect& drawn_rect) {
  auto it = render_pass_backings_.find(render_pass_id);
  CHECK(it != render_pass_backings_.end());
  it->second.drawn_rect = drawn_rect;
  return;
}

void SkiaRenderer::SetDelegatedInkPointRendererSkiaForTest(
    std::unique_ptr<DelegatedInkPointRendererSkia> renderer) {
  DCHECK(!delegated_ink_handler_);
  delegated_ink_handler_ = std::make_unique<DelegatedInkHandler>(
      output_surface_->capabilities().supports_delegated_ink);
  delegated_ink_handler_->SetDelegatedInkPointRendererForTest(
      std::move(renderer));
}

void SkiaRenderer::DrawDelegatedInkTrail(
    const gfx::Transform& root_target_to_render_pass_transform) {
  if (!delegated_ink_handler_ || !delegated_ink_handler_->GetInkRenderer())
    return;

  delegated_ink_handler_->GetInkRenderer()->DrawDelegatedInkTrail(
      current_canvas_, root_target_to_render_pass_transform);
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

bool SkiaRenderer::SupportsBGRA() const {
  return skia_output_surface_->SupportsBGRA();
}

void SkiaRenderer::SetDelegatedInkMetadata(
    std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
  if (!delegated_ink_handler_) {
    delegated_ink_handler_ = std::make_unique<DelegatedInkHandler>(
        output_surface_->capabilities().supports_delegated_ink);
  }
  delegated_ink_handler_->SetDelegatedInkMetadata(std::move(metadata));
  if (!UsingSkiaForDelegatedInk()) {
    overlay_processor_->SetFrameHasDelegatedInk();
  }
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
  CHECK(buffer_queue_);
  buffer_queue_->EnsureMinNumberOfBuffers(n);
}

gpu::Mailbox SkiaRenderer::GetPrimaryPlaneOverlayTestingMailbox() {
#if BUILDFLAG(IS_WIN)
  // Windows dcomp uses a swap chain for primary plane instead of BufferQueue.
  return gpu::Mailbox();
#else
  // For the purpose of testing the overlay configuration, the mailbox for ANY
  // buffer from BufferQueue is good enough because they're all created with
  // identical properties.
  // At the time we're testing overlays we don't yet know which mailbox will be
  // presented this frame so we'll just use the last swapped buffer. (We might
  // present a new frame's mailbox, or if we empty-swap we'll present the
  // previous frame's mailbox.)
  CHECK(buffer_queue_);
  return buffer_queue_->GetLastSwappedBuffer();
#endif
}

#if BUILDFLAG(IS_OZONE)

DBG_FLAG_FBOOL("delegated.overlay.background_candidate.colored",
               toggle_background_overlay_color)  // False by default.

void SkiaRenderer::MaybeScheduleBackgroundImage(
    OverlayProcessorInterface::CandidateList& overlay_list) {
  if (!output_surface_->capabilities().needs_background_image) {
    return;
  }

  DCHECK(output_surface_->capabilities()
             .supports_non_backed_solid_color_overlays ||
         output_surface_->capabilities().supports_single_pixel_buffer);

  OverlayCandidate background_candidate;
  background_candidate.color_space = reshape_color_space();
  background_candidate.display_rect =
      gfx::RectF(gfx::SizeF(viewport_size_for_swap_buffers()));
  background_candidate.color = toggle_background_overlay_color()
                                   ? SkColors::kRed
                                   : SkColors::kTransparent;
  background_candidate.plane_z_order = INT32_MIN;

  // Mark the candidate as transparent. Otherwise it can cause visual
  // artifacts, especially if the contents of viewport are opaque but have
  // efects that makes portions of the content as transparent (like rounded
  // corners). See b/334959107.
  background_candidate.opacity =
      toggle_background_overlay_color() ? 1.0f : 0.0f;

  // ScheduleOverlays() will convert this to a buffer-backed solid color overlay
  // if necessary.
  background_candidate.is_solid_color = true;
  if (overlay_processor_) {
    background_candidate.damage_rect =
        overlay_processor_->GetUnassignedDamage();
    DBG_DRAW_RECT("damage_not_assigned", background_candidate.damage_rect);
  }

  overlay_list.push_back(background_candidate);
}

#endif  // BUILDFLAG(IS_OZONE)

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
SkiaRenderer::ScopedInFlightRenderPassOverlayBackingRef::
    ScopedInFlightRenderPassOverlayBackingRef(SkiaRenderer* renderer,
                                              const gpu::Mailbox& mailbox)
    : renderer_(renderer), mailbox_(mailbox) {
  CHECK(renderer_);
  CHECK(!mailbox_.IsZero());

  auto it =
      base::ranges::find(renderer_->in_flight_render_pass_overlay_backings_,
                         mailbox_, [](const RenderPassOverlayParams& overlay) {
                           return overlay.render_pass_backing.mailbox;
                         });
  CHECK(it != renderer_->in_flight_render_pass_overlay_backings_.end());

  it->ref_count++;
}

void SkiaRenderer::ScopedInFlightRenderPassOverlayBackingRef::Reset() {
  if (!renderer_ && mailbox_.IsZero()) {
    return;
  }

  auto it =
      base::ranges::find(renderer_->in_flight_render_pass_overlay_backings_,
                         mailbox_, [](const RenderPassOverlayParams& overlay) {
                           return overlay.render_pass_backing.mailbox;
                         });
  CHECK(it != renderer_->in_flight_render_pass_overlay_backings_.end());

  // Render pass overlay backings can be reused across multiple frames so we
  // only want to mark them as available when we're releasing lock holding the
  // last reference.
  CHECK_GT(it->ref_count, 0);
  it->ref_count--;
  if (it->ref_count == 0) {
    renderer_->available_render_pass_overlay_backings_.push_back(*it);
    renderer_->in_flight_render_pass_overlay_backings_.erase(it);
  }
}

SkiaRenderer::ScopedInFlightRenderPassOverlayBackingRef::
    ~ScopedInFlightRenderPassOverlayBackingRef() {
  Reset();
}

SkiaRenderer::ScopedInFlightRenderPassOverlayBackingRef::
    ScopedInFlightRenderPassOverlayBackingRef(
        SkiaRenderer::ScopedInFlightRenderPassOverlayBackingRef&& other) {
  *this = std::move(other);
}

SkiaRenderer::ScopedInFlightRenderPassOverlayBackingRef&
SkiaRenderer::ScopedInFlightRenderPassOverlayBackingRef::
    ScopedInFlightRenderPassOverlayBackingRef::operator=(
        SkiaRenderer::ScopedInFlightRenderPassOverlayBackingRef&& other) {
  Reset();

  // This is an RAII type so we depend on the move operators to clear the
  // |other| binding so we don't to unintentional work in the dtor. We need to
  // manually implement the move operators since neither |raw_ptr| nor
  // |gpu::Mailbox| guarantees a move operation that default initializes the
  // original binding.
  renderer_ = other.renderer_;
  mailbox_ = other.mailbox_;
  other.renderer_ = nullptr;
  other.mailbox_ = gpu::Mailbox();
  return *this;
}
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)

SkiaRenderer::OverlayLock::OverlayLock(
    DisplayResourceProvider* resource_provider,
    ResourceId resource_id) {
  resource_lock.emplace(resource_provider, resource_id);
}

SkiaRenderer::OverlayLock::~OverlayLock() = default;

SkiaRenderer::OverlayLock::OverlayLock(SkiaRenderer::OverlayLock&& other) {
  resource_lock = std::move(other.resource_lock);

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
  render_pass_lock = std::move(other.render_pass_lock);
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
}

SkiaRenderer::OverlayLock& SkiaRenderer::OverlayLock::OverlayLock::operator=(
    SkiaRenderer::OverlayLock&& other) {
  resource_lock = std::move(other.resource_lock);

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
  render_pass_lock = std::move(other.render_pass_lock);
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)

  return *this;
}

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
SkiaRenderer::OverlayLock::OverlayLock(SkiaRenderer* renderer,
                                       const gpu::Mailbox& mailbox) {
  render_pass_lock.emplace(renderer, mailbox);
}
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_APPLE)
std::size_t SkiaRenderer::OverlayLockHash::operator()(
    const OverlayLock& o) const {
  return std::hash<gpu::Mailbox>{}(o.mailbox());
}

std::size_t SkiaRenderer::OverlayLockHash::operator()(
    const gpu::Mailbox& m) const {
  return std::hash<gpu::Mailbox>{}(m);
}

bool SkiaRenderer::OverlayLockKeyEqual::operator()(
    const OverlayLock& lhs,
    const OverlayLock& rhs) const {
  return lhs.mailbox() == rhs.mailbox();
}

bool SkiaRenderer::OverlayLockKeyEqual::operator()(
    const OverlayLock& lhs,
    const gpu::Mailbox& rhs) const {
  return lhs.mailbox() == rhs;
}
#endif

}  // namespace viz
