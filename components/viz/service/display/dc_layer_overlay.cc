// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/dc_layer_overlay.h"

#include <limits>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "components/viz/common/overlay_state/win/overlay_state_service.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/viz_utils.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/base/media_switches.h"
#include "media/base/win/mf_feature_checks.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/video_types.h"

namespace viz {

namespace {

// This is the number of frames we should wait before actual overlay promotion
// under multi-video cases.
constexpr int kDCLayerFramesDelayedBeforeOverlay = 5;

// This is used for a histogram to determine why overlays are or aren't used,
// so don't remove entries and make sure to update enums.xml if it changes.
enum DCLayerResult {
  DC_LAYER_SUCCESS = 0,
  DC_LAYER_FAILED_UNSUPPORTED_QUAD = 1,  // not recorded
  DC_LAYER_FAILED_QUAD_BLEND_MODE = 2,
  DC_LAYER_FAILED_TEXTURE_NOT_CANDIDATE = 3,
  DC_LAYER_FAILED_OCCLUDED [[deprecated]] = 4,
  DC_LAYER_FAILED_COMPLEX_TRANSFORM = 5,
  DC_LAYER_FAILED_TRANSPARENT = 6,
  DC_LAYER_FAILED_NON_ROOT [[deprecated]] = 7,
  DC_LAYER_FAILED_TOO_MANY_OVERLAYS = 8,
  DC_LAYER_FAILED_NO_HW_OVERLAY_SUPPORT [[deprecated]] = 9,
  DC_LAYER_FAILED_ROUNDED_CORNERS [[deprecated]] = 10,
  DC_LAYER_FAILED_BACKDROP_FILTERS = 11,
  DC_LAYER_FAILED_COPY_REQUESTS = 12,
  DC_LAYER_FAILED_VIDEO_CAPTURE_ENABLED = 13,
  DC_LAYER_FAILED_OUTPUT_HDR = 14,
  DC_LAYER_FAILED_NOT_DAMAGED = 15,
  DC_LAYER_FAILED_YUV_VIDEO_QUAD_MOVED = 16,
  DC_LAYER_FAILED_YUV_VIDEO_QUAD_HDR_TONE_MAPPING = 17,
  DC_LAYER_FAILED_YUV_VIDEO_QUAD_NO_HDR_METADATA = 18,
  DC_LAYER_FAILED_YUV_VIDEO_QUAD_HLG = 19,
  DC_LAYER_FAILED_YUV_VIDEO_QUAD_NO_P010_VIDEO_PROCESSOR_SUPPORT = 20,
  DC_LAYER_FAILED_YUV_VIDEO_QUAD_HDR_NON_FULLSCREEN [[deprecated]] = 21,
  DC_LAYER_FAILED_YUV_VIDEO_QUAD_HDR_NON_P010 = 22,
  DC_LAYER_FAILED_YUV_VIDEO_QUAD_UNSUPPORTED_COLORSPACE = 23,
  kMaxValue = DC_LAYER_FAILED_YUV_VIDEO_QUAD_UNSUPPORTED_COLORSPACE,
};

bool IsCompatibleHDRMetadata(
    const std::optional<gfx::HDRMetadata>& hdr_metadata) {
  return hdr_metadata &&
         ((hdr_metadata->smpte_st_2086 &&
           hdr_metadata->smpte_st_2086->IsValid()) ||
          (hdr_metadata->cta_861_3 && hdr_metadata->cta_861_3->IsValid()));
}

DCLayerResult ValidateYUVOverlay(
    const gfx::ProtectedVideoType& protected_video_type,
    const gfx::ColorSpace& video_color_space,
    const SharedImageFormat si_format,
    const std::optional<gfx::HDRMetadata>& hdr_metadata,
    bool has_overlay_support,
    bool has_p010_video_processor_support,
    int allowed_yuv_overlay_count,
    int processed_yuv_overlay_count) {
  // Note: Do not override this value based on base::Feature values. It is the
  // result after the GPU blocklist has been consulted.
  if (!has_overlay_support) {
    return DC_LAYER_FAILED_UNSUPPORTED_QUAD;
  }

  // Hardware protected video must use Direct Composition Overlay
  if (protected_video_type == gfx::ProtectedVideoType::kHardwareProtected) {
    return DC_LAYER_SUCCESS;
  }

  if (processed_yuv_overlay_count >= allowed_yuv_overlay_count) {
    return DC_LAYER_FAILED_TOO_MANY_OVERLAYS;
  }

  // For YUV color spaces that VP couldn't handle, stop promote overlay.
  if ((video_color_space.GetMatrixID() != gfx::ColorSpace::MatrixID::RGB) &&
      !gfx::ColorSpaceWin::CanConvertToDXGIColorSpace(video_color_space)) {
    return DC_LAYER_FAILED_YUV_VIDEO_QUAD_UNSUPPORTED_COLORSPACE;
  }

  // HLG shouldn't have the hdr metadata, but we don't want to promote it to
  // overlay, as VideoProcessor doesn't support HLG tone mapping well between
  // different gpu vendors, see: https://crbug.com/1144260#c6.
  // Some HLG streams may carry hdr metadata, see: https://crbug.com/1429172.
  if (video_color_space.GetTransferID() == gfx::ColorSpace::TransferID::HLG) {
    return DC_LAYER_FAILED_YUV_VIDEO_QUAD_HLG;
  }

  if (video_color_space.IsHDR()) {
    // Otherwise, it could be a parser bug like https://crbug.com/1362288 if the
    // hdr metadata is still missing. Missing `smpte_st_2086` or `cta_861_3`
    // could always causes intel driver crash when in HDR overlay mode, and
    // technically as long as one of the `smpte_st_2086` or `cta_861_3` exists
    // could solve the crash issue.
    if (!IsCompatibleHDRMetadata(hdr_metadata)) {
      return DC_LAYER_FAILED_YUV_VIDEO_QUAD_NO_HDR_METADATA;
    }

    // Do not promote hdr overlay if buffer is not in 10bit P010 format. as this
    // may cause blue output result if content is NV12 8bit HDR10.
    if (si_format != MultiPlaneFormat::kP010) {
      return DC_LAYER_FAILED_YUV_VIDEO_QUAD_HDR_NON_P010;
    }
  }

  // Only promote overlay for 10bit+ contents when video processor can
  // handle P010 contents, otherwise disable overlay.
  if (si_format == MultiPlaneFormat::kP010 &&
      !has_p010_video_processor_support) {
    return DC_LAYER_FAILED_YUV_VIDEO_QUAD_NO_P010_VIDEO_PROCESSOR_SUPPORT;
  }

  return DC_LAYER_SUCCESS;
}

DCLayerResult ValidateTextureQuad(
    const TextureDrawQuad* quad,
    const std::vector<gfx::Rect>& backdrop_filter_rects,
    bool has_overlay_support,
    bool has_p010_video_processor_support,
    int allowed_yuv_overlay_count,
    int processed_yuv_overlay_count,
    const DisplayResourceProvider* resource_provider) {
  // Check that resources are overlay compatible first so that subsequent
  // assumptions are valid.
  for (const auto& resource : quad->resources) {
    if (!resource_provider->IsOverlayCandidate(resource))
      return DC_LAYER_FAILED_TEXTURE_NOT_CANDIDATE;
  }

  if (quad->shared_quad_state->blend_mode != SkBlendMode::kSrcOver)
    return DC_LAYER_FAILED_QUAD_BLEND_MODE;

  if (!quad->shared_quad_state->quad_to_target_transform
           .Preserves2dAxisAlignment()) {
    return DC_LAYER_FAILED_COMPLEX_TRANSFORM;
  }

  auto quad_target_rect = ClippedQuadRectangle(quad);
  for (const auto& filter_target_rect : backdrop_filter_rects) {
    if (filter_target_rect.Intersects(quad_target_rect))
      return DC_LAYER_FAILED_BACKDROP_FILTERS;
  }

  if (quad->is_video_frame) {
    const auto& color_space =
        resource_provider->GetColorSpace(quad->resource_id());
    const auto& hdr_metadata =
        resource_provider->GetHDRMetadata(quad->resource_id());
    auto si_format =
        resource_provider->GetSharedImageFormat(quad->resource_id());
    auto result = ValidateYUVOverlay(
        quad->protected_video_type, color_space, si_format, hdr_metadata,
        has_overlay_support, has_p010_video_processor_support,
        allowed_yuv_overlay_count, processed_yuv_overlay_count);
    return result;
  }

  return DC_LAYER_SUCCESS;
}

void FromTextureQuad(const TextureDrawQuad* quad,
                     const gfx::Transform& transform_to_root_target,
                     const DisplayResourceProvider* resource_provider,
                     OverlayCandidate* dc_layer) {
  dc_layer->resource_id = quad->resource_id();
  dc_layer->plane_z_order = 1;
  dc_layer->resource_size_in_pixels = quad->resource_size_in_pixels();
  dc_layer->uv_rect =
      gfx::BoundingRect(quad->uv_top_left, quad->uv_bottom_right);
  dc_layer->display_rect = gfx::RectF(quad->rect);
  // Quad rect is in quad content space so both quad to target, and target to
  // root transforms must be applied to it.
  gfx::Transform quad_to_root_transform;
  if (quad->y_flipped) {
    quad_to_root_transform.Scale(1.0, -1.0);
    quad_to_root_transform.PostTranslate(
        0.0, dc_layer->resource_size_in_pixels.height());
  }
  quad_to_root_transform.PostConcat(
      quad->shared_quad_state->quad_to_target_transform);
  quad_to_root_transform.PostConcat(transform_to_root_target);
  // Flatten transform to 2D since DirectComposition doesn't support 3D
  // transforms.  This only applies when non axis aligned overlays are enabled.
  quad_to_root_transform.Flatten();
  dc_layer->transform = quad_to_root_transform;

  if (quad->shared_quad_state->clip_rect) {
    // Clip rect is in quad target space, and must be transformed to root target
    // space.
    dc_layer->clip_rect = transform_to_root_target.MapRect(
        quad->shared_quad_state->clip_rect.value_or(gfx::Rect()));
  }

  dc_layer->color_space = resource_provider->GetColorSpace(quad->resource_id());
  dc_layer->hdr_metadata =
      resource_provider->GetHDRMetadata(quad->resource_id());

  dc_layer->protected_video_type = quad->protected_video_type;
  // Both color space and protected_video_type are hard-coded for stream video.
  // TODO(crbug.com/40878556): Consider using quad->protected_video_type.
  if (quad->is_stream_video) {
    dc_layer->color_space = gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                                            gfx::ColorSpace::TransferID::BT709);
    dc_layer->protected_video_type =
        gfx::ProtectedVideoType::kHardwareProtected;
  }
}

DCLayerResult IsUnderlayAllowed(const DrawQuad* quad) {
  if (quad->ShouldDrawWithBlending() &&
      !quad->shared_quad_state->mask_filter_info.HasRoundedCorners()) {
    return DC_LAYER_FAILED_TRANSPARENT;
  }

  return DC_LAYER_SUCCESS;
}

// Any occluding quads in the quad list on top of the overlay/underlay
bool IsOccluded(
    const gfx::RectF& target_quad,
    QuadList::ConstIterator quad_list_begin,
    QuadList::ConstIterator quad_list_end,
    const DCLayerOverlayProcessor::FilterOperationsMap& render_pass_filters) {
  // If the current quad |quad_list_end| has rounded corners, force it
  // to underlay mode.
  if (quad_list_end->shared_quad_state->mask_filter_info.HasRoundedCorners()) {
    return true;
  }

  for (auto overlap_iter = quad_list_begin; overlap_iter != quad_list_end;
       ++overlap_iter) {
    float opacity = overlap_iter->shared_quad_state->opacity;
    if (opacity < std::numeric_limits<float>::epsilon())
      continue;

    const DrawQuad* quad = *overlap_iter;
    gfx::RectF overlap_rect;
    // Expand the overlap_rect for the render pass draw quad with pixel moving
    // foreground filters.
    bool has_pixel_moving_filter = false;
    if (!render_pass_filters.empty() &&
        quad->material == DrawQuad::Material::kAggregatedRenderPass) {
      const auto* rpdq = AggregatedRenderPassDrawQuad::MaterialCast(quad);
      auto render_pass_it = render_pass_filters.find(rpdq->render_pass_id);
      if (render_pass_it != render_pass_filters.end()) {
        auto* filters = render_pass_it->second.get();
        overlap_rect = gfx::RectF(
            GetExpandedRectWithPixelMovingForegroundFilter(*rpdq, *filters));
        has_pixel_moving_filter = true;
      }
    }

    if (!has_pixel_moving_filter)
      overlap_rect = ClippedQuadRectangleF(quad);

    if (quad->material == DrawQuad::Material::kSolidColor) {
      SkColor4f color = SolidColorDrawQuad::MaterialCast(quad)->color;
      float alpha = color.fA * opacity;
      if (quad->ShouldDrawWithBlending() &&
          alpha < std::numeric_limits<float>::epsilon())
        continue;
    }
    if (overlap_rect.Intersects(target_quad))
      return true;
  }
  return false;
}

bool HasOccludingDamageRect(
    const SharedQuadState* shared_quad_state,
    const SurfaceDamageRectList& surface_damage_rect_list,
    const gfx::Rect& quad_rect_in_target_space) {
  if (!shared_quad_state->overlay_damage_index.has_value())
    return !quad_rect_in_target_space.IsEmpty();

  size_t overlay_damage_index = shared_quad_state->overlay_damage_index.value();
  CHECK_LT(overlay_damage_index, surface_damage_rect_list.size());

  // Damage rects in surface_damage_rect_list are arranged from top to bottom.
  // surface_damage_rect_list[0] is the one on the very top.
  // surface_damage_rect_list[overlay_damage_index] is the damage rect of
  // this overlay surface.
  gfx::Rect occluding_damage_rect = gfx::UnionRects(
      base::make_span(surface_damage_rect_list).first(overlay_damage_index));
  occluding_damage_rect.Intersect(quad_rect_in_target_space);

  return !occluding_damage_rect.IsEmpty();
}

bool IsPossibleFullScreenLetterboxing(const QuadList::ConstIterator& it,
                                      QuadList::ConstIterator quad_list_end,
                                      const gfx::Rect& display_rect) {
  // Two cases are considered as possible fullscreen letterboxing:
  // 1. If the quad beneath the overlay quad is DrawQuad::Material::kSolidColor
  // with black, and it touches two sides of the screen, while starting at
  // display origin (0, 0).
  // 2. If the quad beneath the overlay quad is
  // DrawQuad::Material::kTiledContent, and it touches two sides of the screen,
  // while starting at display origin (0, 0).
  // For YouTube with F11 page fullscreen mode, the kTiledContent beneath the
  // overlay does not touch the right edge due to the existing of a scrolling
  // bar.
  auto beneath_overlay_it = it;
  beneath_overlay_it++;

  if (beneath_overlay_it != quad_list_end) {
    if (beneath_overlay_it->material == DrawQuad::Material::kTiledContent ||
        (beneath_overlay_it->material == DrawQuad::Material::kSolidColor &&
         SolidColorDrawQuad::MaterialCast(*beneath_overlay_it)->color ==
             SkColors::kBlack)) {
      gfx::RectF beneath_rect = ClippedQuadRectangleF(*beneath_overlay_it);
      return (beneath_rect.origin() == gfx::PointF(display_rect.origin()) &&
              (beneath_rect.width() == display_rect.width() ||
               beneath_rect.height() == display_rect.height()));
    }
  }

  return false;
}

void RecordVideoDCLayerResult(DCLayerResult result,
                              gfx::ProtectedVideoType protected_video_type) {
  switch (protected_video_type) {
    case gfx::ProtectedVideoType::kClear:
      UMA_HISTOGRAM_ENUMERATION(
          "GPU.DirectComposition.DCLayerResult.Video.Clear", result);
      break;
    case gfx::ProtectedVideoType::kSoftwareProtected:
      UMA_HISTOGRAM_ENUMERATION(
          "GPU.DirectComposition.DCLayerResult.Video.SoftwareProtected",
          result);
      break;
    case gfx::ProtectedVideoType::kHardwareProtected:
      UMA_HISTOGRAM_ENUMERATION(
          "GPU.DirectComposition.DCLayerResult.Video.HardwareProtected",
          result);
      break;
  }
}

void RecordDCLayerResult(DCLayerResult result, const DrawQuad* quad) {
  // Skip recording unsupported quads since that'd dwarf the data we care about.
  if (result == DC_LAYER_FAILED_UNSUPPORTED_QUAD)
    return;

  switch (quad->material) {
    case DrawQuad::Material::kTextureContent: {
      auto* tex_quad = TextureDrawQuad::MaterialCast(quad);
      if (tex_quad->is_stream_video) {
        UMA_HISTOGRAM_ENUMERATION(
            "GPU.DirectComposition.DCLayerResult.StreamVideo", result);
      } else if (tex_quad->is_video_frame) {
        RecordVideoDCLayerResult(result, tex_quad->protected_video_type);
      } else {
        UMA_HISTOGRAM_ENUMERATION("GPU.DirectComposition.DCLayerResult.Texture",
                                  result);
      }
      break;
    }
    default:
      break;
  }
}

// This function records the damage rect rect of the current frame.
void RecordOverlayHistograms(
    const DCLayerOverlayProcessor::RenderPassOverlayDataMap&
        render_pass_overlay_data_map,
    bool has_occluding_surface_damage) {
  // If an underlay is found, we record the damage rect of this frame as an
  // underlay.
  bool is_overlay = true;
  for (auto& [render_pass, overlay_data] : render_pass_overlay_data_map) {
    is_overlay = base::ranges::all_of(
        overlay_data.promoted_overlays,
        [](const auto& dc_layer) { return dc_layer.plane_z_order > 0; });
    if (!is_overlay) {
      break;
    }
  }

  bool damage_rects_empty = base::ranges::all_of(
      render_pass_overlay_data_map,
      [](const auto& data) { return data.second.damage_rect.IsEmpty(); });

  OverlayProcessorInterface::RecordOverlayDamageRectHistograms(
      is_overlay, has_occluding_surface_damage, damage_rects_empty);
}

QuadList::Iterator FindAnOverlayCandidate(QuadList& quad_list) {
  for (auto it = quad_list.begin(); it != quad_list.end(); ++it) {
    if (it->material == DrawQuad::Material::kTextureContent) {
      return it;
    }
  }
  return quad_list.end();
}

QuadList::Iterator FindAnOverlayCandidateExcludingMediaFoundationVideoContent(
    QuadList& quad_list) {
  QuadList::Iterator it = quad_list.end();
  for (auto quad_it = quad_list.begin(); quad_it != quad_list.end();
       ++quad_it) {
    if (quad_it->material == DrawQuad::Material::kTextureContent &&
        TextureDrawQuad::MaterialCast(*quad_it)->is_stream_video) {
      return quad_list.end();
    }
    if (it == quad_list.end() &&
        (quad_it->material == DrawQuad::Material::kTextureContent)) {
      it = quad_it;
    }
  }
  return it;
}

bool IsVideoQuad(const DrawQuad* quad) {
  return quad->material == DrawQuad::Material::kTextureContent &&
         TextureDrawQuad::MaterialCast(quad)->is_video_frame;
}

gfx::ProtectedVideoType GetProtectedVideoType(const DrawQuad* quad) {
  if (quad->material == DrawQuad::Material::kTextureContent) {
    return TextureDrawQuad::MaterialCast(quad)->protected_video_type;
  } else {
    return gfx::ProtectedVideoType::kClear;
  }
}

bool IsOverlayRequiredForQuad(const DrawQuad* quad) {
  // Hardware protected video always requires overlays, and for software
  // protected video we prefer it for the protection benefits of overlays.
  if (GetProtectedVideoType(quad) != gfx::ProtectedVideoType::kClear) {
    return true;
  }
  // As do stream video textures e.g. when MediaFoundationRenderer is used for
  // clear video with direct composition.
  return quad->material == DrawQuad::Material::kTextureContent &&
         TextureDrawQuad::MaterialCast(quad)->is_stream_video;
}

// A bit of a misnomer, but these are all the "standard" no overlay required
// (which implies) clear video quads.
bool IsClearVideoQuad(const DrawQuad* quad) {
  return IsVideoQuad(quad) && !IsOverlayRequiredForQuad(quad);
}

bool AllowRemoveClearVideoQuadCandidatesWhenMoving(
    const DisplayResourceProvider* resource_provider,
    const DrawQuad* quad,
    bool force_overlay_for_auto_hdr) {
  if (!IsClearVideoQuad(quad)) {
    return false;
  }
  // Do not allow remove clear video quad candidates for HDR videos or SDR to
  // HDR videos, since there will always be a huge visual difference between
  // compositor tone-mapping (by Chrome) and MPO tone-mapping (by Driver).
  switch (quad->material) {
    case DrawQuad::Material::kTextureContent: {
      const TextureDrawQuad* texture_quad = TextureDrawQuad::MaterialCast(quad);
      return !(resource_provider->GetColorSpace(texture_quad->resource_id())
                   .IsHDR() ||
               force_overlay_for_auto_hdr);
    }
    default:
      NOTREACHED();
  }
}

// This is the damage contribution due to previous frame's overlays which can
// be empty.
gfx::Rect PreviousFrameOverlayDamageContribution(
    const std::vector<DCLayerOverlayProcessor::OverlayRect>&
        previous_frame_overlay_rects) {
  gfx::Rect rects_union;
  for (const auto& overlay : previous_frame_overlay_rects) {
    rects_union.Union(overlay.rect);
  }
  return rects_union;
}

bool IsPreviousFrameUnderlayRect(
    const std::vector<DCLayerOverlayProcessor::OverlayRect>&
        previous_frame_overlay_rects,
    const gfx::Rect& quad_rect,
    size_t index) {
  if (index >= previous_frame_overlay_rects.size()) {
    return false;
  } else {
    // Although we can loop through the list to find out if there is an
    // underlay with the same size from the previous frame, checking
    // previous_frame_overlay_rects[index] is the quickest way to do it. If we
    // cannot find a match with the same index, there is probably a change in
    // the number of overlays or layout. Then we won't be able to get a zero
    // damage rect in this case. Looping through the list won't give better
    // power.
    return (previous_frame_overlay_rects[index].rect == quad_rect) &&
           (previous_frame_overlay_rects[index].is_overlay == false);
  }
}

// Return value of |ValidateDrawQuad|.
struct ValidateDrawQuadResult {
  DCLayerResult code = DC_LAYER_FAILED_UNSUPPORTED_QUAD;
  bool is_yuv_overlay = false;
  gpu::Mailbox promotion_hint_mailbox;
};

ValidateDrawQuadResult ValidateDrawQuad(
    const DisplayResourceProvider* resource_provider,
    const QuadList::ConstIterator& it,
    const std::vector<gfx::Rect>& backdrop_filter_rects,
    const bool has_overlay_support,
    const bool has_p010_video_processor_support,
    const int allowed_yuv_overlay_count,
    const int processed_yuv_overlay_count,
    const bool allow_promotion_hinting) {
  ValidateDrawQuadResult result;
  switch (it->material) {
    case DrawQuad::Material::kTextureContent: {
      const TextureDrawQuad* tex_quad = TextureDrawQuad::MaterialCast(*it);

      if (tex_quad->is_stream_video) {
        // Stream video quads contain Media Foundation dcomp surface which is
        // always presented as overlay.
        result.code = DC_LAYER_SUCCESS;
      } else {
        result.code = ValidateTextureQuad(
            tex_quad, backdrop_filter_rects, has_overlay_support,
            has_p010_video_processor_support, allowed_yuv_overlay_count,
            processed_yuv_overlay_count, resource_provider);
      }

      result.is_yuv_overlay = tex_quad->is_video_frame;

      if (allow_promotion_hinting) {
        // If this quad has marked itself as wanting promotion hints then get
        // the associated mailbox.
        ResourceId id = tex_quad->resource_id();
        if (resource_provider->DoesResourceWantPromotionHint(id)) {
          result.promotion_hint_mailbox = resource_provider->GetMailbox(id);
        }
      }
    } break;

    default:
      result.code = DC_LAYER_FAILED_UNSUPPORTED_QUAD;
      break;
  }

  return result;
}

void FromDrawQuad(const DisplayResourceProvider* resource_provider,
                  const AggregatedRenderPass* render_pass,
                  bool is_page_fullscreen_mode,
                  const QuadList::ConstIterator& it,
                  int& processed_yuv_overlay_count,
                  OverlayCandidate& dc_layer) {
  dc_layer.possible_video_fullscreen_letterboxing =
      is_page_fullscreen_mode
          ? IsPossibleFullScreenLetterboxing(it, render_pass->quad_list.end(),
                                             render_pass->output_rect)
          : false;
  switch (it->material) {
    case DrawQuad::Material::kTextureContent: {
      const TextureDrawQuad* tex_quad = TextureDrawQuad::MaterialCast(*it);
      FromTextureQuad(tex_quad, render_pass->transform_to_root_target,
                      resource_provider, &dc_layer);
      if (tex_quad->is_video_frame) {
        processed_yuv_overlay_count++;
      }
    } break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace

std::optional<OverlayCandidate> DCLayerOverlayProcessor::FromTextureOrYuvQuad(
    const DisplayResourceProvider* resource_provider,
    const AggregatedRenderPass* render_pass,
    const QuadList::ConstIterator& it,
    bool is_page_fullscreen_mode) const {
  // Backdrop filter occlusion is checked in |OverlayProcessorWin| via
  // |OverlayCandidate::IsOccludedByFilteredQuad|, so we don't need to populate
  // this vector.
  const std::vector<gfx::Rect> backdrop_filter_rects;

  ValidateDrawQuadResult result = ValidateDrawQuad(
      resource_provider, it, backdrop_filter_rects, true,
      has_p010_video_processor_support_, INT_MAX, INT_MIN, false);

  if (result.code != DC_LAYER_SUCCESS) {
    RecordDCLayerResult(result.code, *it);
    return std::nullopt;
  }

  OverlayCandidate candidate;
  int ignore_processed_yuv_overlay_count = 0;
  FromDrawQuad(resource_provider, render_pass, is_page_fullscreen_mode, it,
               ignore_processed_yuv_overlay_count, candidate);

  // Once we've promoted the video as normal, add extra properties required for
  // delegated compositing.

  if (it->shared_quad_state->mask_filter_info.HasRoundedCorners()) {
    gfx::MaskFilterInfo mask_filter_info =
        it->shared_quad_state->mask_filter_info;
    mask_filter_info.ApplyTransform(render_pass->transform_to_root_target);
    candidate.rounded_corners = mask_filter_info.rounded_corner_bounds();
  }

  candidate.opacity = it->shared_quad_state->opacity;

  // We don't expect quads promoted by |DCLayerOverlayProcessor| to have a
  // differing |visible_rect|, but we handle it here just in case.
  if (it->visible_rect != it->rect) {
    // |OverlayCandidate| does not support clipping a candidate via
    // |visible_rect|, but we can get the same effect by clipping its buffer via
    // |uv_rect| and resizing its |display_rect|. This is similar to how
    // |OverlayCandidateFactory| handles |visible_rect|.
    candidate.uv_rect = gfx::MapRect(gfx::RectF(it->visible_rect),
                                     gfx::RectF(it->rect), candidate.uv_rect);
    candidate.display_rect = gfx::RectF(it->visible_rect);
  }

  return candidate;
}

DCLayerOverlayProcessor::DCLayerOverlayProcessor(
    int allowed_yuv_overlay_count,
    bool disable_video_overlay_if_moving,
    bool skip_initialization_for_testing)
    : has_overlay_support_(skip_initialization_for_testing),
      allowed_yuv_overlay_count_(allowed_yuv_overlay_count),
      is_on_battery_power_(
          base::PowerMonitor::GetInstance()
              ->AddPowerStateObserverAndReturnOnBatteryState(this)),
      no_undamaged_overlay_promotion_(
          base::FeatureList::IsEnabled(features::kNoUndamagedOverlayPromotion)),
      disable_video_overlay_if_moving_(disable_video_overlay_if_moving) {
  if (!skip_initialization_for_testing) {
    UpdateHasHwOverlaySupport();
    UpdateSystemHDRStatus();
    UpdateP010VideoProcessorSupport();
    UpdateAutoHDRVideoProcessorSupport();
    gl::DirectCompositionOverlayCapsMonitor::GetInstance()->AddObserver(this);
  }
  allow_promotion_hinting_ = media::SupportMediaFoundationClearPlayback();
}

DCLayerOverlayProcessor::~DCLayerOverlayProcessor() {
  gl::DirectCompositionOverlayCapsMonitor::GetInstance()->RemoveObserver(this);
  base::PowerMonitor::GetInstance()->RemovePowerStateObserver(this);
}

void DCLayerOverlayProcessor::UpdateHasHwOverlaySupport() {
  has_overlay_support_ = gl::DirectCompositionOverlaysSupported();
}

void DCLayerOverlayProcessor::UpdateSystemHDRStatus() {
  bool hdr_enabled_on_any_display = false;
  bool hdr_disabled_on_any_display = false;
  auto dxgi_info = gl::GetDirectCompositionHDRMonitorDXGIInfo();
  for (const auto& output_desc : dxgi_info->output_descs) {
    hdr_enabled_on_any_display |= output_desc->hdr_enabled;
    hdr_disabled_on_any_display |= !output_desc->hdr_enabled;
  }
  system_hdr_enabled_on_any_display_ = hdr_enabled_on_any_display;
  // If there is no monitor connected, treat it as if there is one SDR monitor.
  system_hdr_disabled_on_any_display_ =
      dxgi_info->output_descs.size() > 0 ? hdr_disabled_on_any_display : true;
}

void DCLayerOverlayProcessor::UpdateP010VideoProcessorSupport() {
  has_p010_video_processor_support_ =
      gl::CheckVideoProcessorFormatSupport(DXGI_FORMAT_P010);
}

void DCLayerOverlayProcessor::UpdateAutoHDRVideoProcessorSupport() {
  has_auto_hdr_video_processor_support_ = gl::VideoProcessorAutoHDRSupported();
}

void DCLayerOverlayProcessor::OnBatteryPowerStatusChange(
    base::PowerStateObserver::BatteryPowerStatus battery_power_status) {
  is_on_battery_power_ =
      (battery_power_status ==
       base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);
}

// Called on the Viz Compositor thread.
void DCLayerOverlayProcessor::OnOverlayCapsChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  UpdateHasHwOverlaySupport();
  UpdateSystemHDRStatus();
  UpdateP010VideoProcessorSupport();
  UpdateAutoHDRVideoProcessorSupport();
}

void DCLayerOverlayProcessor::RemoveOverlayDamageRect(
    const QuadList::Iterator& it,
    RenderPassCurrentFrameState& render_pass_state) const {
  // This is done by setting the overlay surface damage rect in the
  // |surface_damage_rect_list| to zero.
  if (it->shared_quad_state->overlay_damage_index.has_value()) {
    size_t overlay_damage_index =
        it->shared_quad_state->overlay_damage_index.value();
    CHECK_LT(overlay_damage_index,
             render_pass_state.surface_damage_rect_list.size());
    render_pass_state.damages_to_be_removed.push_back(overlay_damage_index);
  }
}

// This is called at the end of Process(). The goal is to get an empty damage
// rect if the overlays are the only damages in the frame.
void DCLayerOverlayProcessor::UpdateDamageRect(
    AggregatedRenderPass* render_pass,
    const RenderPassPreviousFrameState& previous_frame_state,
    RenderPassOverlayData& overlay_data,
    RenderPassCurrentFrameState& current_frame_state) const {
  // Check whether the overlay rect union from the previous frame should be
  // added to the current frame and whether the overlay damages can be removed
  // from the current damage rect.

  const std::vector<OverlayRect>& previous_frame_overlay_rects =
      previous_frame_state.overlay_rects;

  bool should_add_previous_frame_overlay_damage = true;
  if (!current_frame_state.overlay_rects.empty() &&
      current_frame_state.overlay_rects == previous_frame_overlay_rects &&
      render_pass->output_rect == previous_frame_state.display_rect) {
    // No need to add back the overlay rect union from the previous frame
    // if no changes in overlays.
    should_add_previous_frame_overlay_damage = false;

    // Only perform this optimization if the transform is axis aligned.
    // Transforms that are not axis aligned make the original rect larger when
    // the transformation is applied. Since we transform the damage rects
    // between root space and render pass space (SurfaceAggregator converts
    // to root space and Process() converts to render pass space), the damage
    // rects will be larger than the original rects. This would result in
    // subtracting a larger damage than the overlay itself.
    if (render_pass->transform_to_root_target.Preserves2dAxisAlignment()) {
      // The final damage rect is computed by add up all surface damages except
      // for the overlay surface damages and the damages right below the
      // overlays.
      gfx::Rect final_damage_rect;
      size_t surface_index = 0;
      for (auto surface_damage_rect :
           current_frame_state.surface_damage_rect_list) {
        // We only support at most two overlays. The size of
        // damages_to_be_removed will not be bigger than 2. We should revisit
        // this damages_to_be_removed for-loop if we try to support many
        // overlays. See capabilities.allowed_yuv_overlay_count.
        for (const auto index_to_be_removed :
             current_frame_state.damages_to_be_removed) {
          // The overlay damages and the damages right below them will not be
          // added to the damage rect.
          if (surface_index == index_to_be_removed) {
            // This is the overlay surface.
            surface_damage_rect = gfx::Rect();
            break;
          } else if (surface_index > index_to_be_removed) {
            // This is the surface below the overlays.
            surface_damage_rect.Subtract(
                current_frame_state
                    .surface_damage_rect_list[index_to_be_removed]);
          }
        }
        final_damage_rect.Union(surface_damage_rect);
        ++surface_index;
      }

      overlay_data.damage_rect = final_damage_rect;
    }
  }

  if (should_add_previous_frame_overlay_damage) {
    overlay_data.damage_rect.Union(
        PreviousFrameOverlayDamageContribution(previous_frame_overlay_rects));
  }
  overlay_data.damage_rect.Intersect(render_pass->output_rect);
  current_frame_state.damages_to_be_removed.clear();
}

void DCLayerOverlayProcessor::RemoveClearVideoQuadCandidatesIfMoving(
    const DisplayResourceProvider* resource_provider,
    RenderPassOverlayDataMap& render_pass_overlay_data_map,
    RenderPassCurrentFrameStateMap& render_pass_state_map) {
  // The number of frames all overlay candidates need to be stable before we
  // allow overlays again. This number was chosen experimentally.
  constexpr int kFramesOfStabilityForOverlayPromotion = 5;

  std::vector<gfx::Rect> current_overlay_candidate_rects;

  for (auto& [render_pass, overlay_data] : render_pass_overlay_data_map) {
    std::vector<QuadList::Iterator>& candidates =
        render_pass_state_map[render_pass].candidates;
    current_overlay_candidate_rects.reserve(
        current_overlay_candidate_rects.size() + candidates.size());
    for (auto candidate_it : candidates) {
      if (AllowRemoveClearVideoQuadCandidatesWhenMoving(
              resource_provider, *candidate_it, force_overlay_for_auto_hdr())) {
        gfx::Rect quad_rect_in_target_space =
            ClippedQuadRectangle(*candidate_it);
        gfx::Rect quad_rect_in_root_space =
            cc::MathUtil::MapEnclosingClippedRect(
                render_pass->transform_to_root_target,
                quad_rect_in_target_space);
        current_overlay_candidate_rects.push_back(quad_rect_in_root_space);
      }
    }
  }

  if (previous_frame_overlay_candidate_rects_ !=
      current_overlay_candidate_rects) {
    frames_since_last_overlay_candidate_rects_change_ = 0;
    std::swap(previous_frame_overlay_candidate_rects_,
              current_overlay_candidate_rects);
  } else {
    frames_since_last_overlay_candidate_rects_change_++;
  }

  if (frames_since_last_overlay_candidate_rects_change_ <=
      kFramesOfStabilityForOverlayPromotion) {
    // Remove all video quad candidates if any of them moved recently
    for (auto& [render_pass, overlay_data] : render_pass_overlay_data_map) {
      std::vector<QuadList::Iterator>& candidates =
          render_pass_state_map[render_pass].candidates;

      auto candidate_it = candidates.begin();
      while (candidate_it != candidates.end()) {
        if (AllowRemoveClearVideoQuadCandidatesWhenMoving(
                resource_provider, **candidate_it,
                force_overlay_for_auto_hdr())) {
          RecordDCLayerResult(DC_LAYER_FAILED_YUV_VIDEO_QUAD_MOVED,
                              **candidate_it);
          candidate_it = candidates.erase(candidate_it);
        } else {
          candidate_it++;
        }
      }
    }
  }
}

void DCLayerOverlayProcessor::CollectCandidates(
    const DisplayResourceProvider* resource_provider,
    AggregatedRenderPass* render_pass,
    const FilterOperationsMap& render_pass_backdrop_filters,
    RenderPassOverlayData& overlay_data,
    RenderPassCurrentFrameState& render_pass_state,
    GlobalOverlayState& global_overlay_state) {
  // Output rects of child render passes that have backdrop filters in target
  // space. These rects are used to determine if the overlay rect could be read
  // by backdrop filters.
  std::vector<gfx::Rect> backdrop_filter_rects;

  // Skip overlay for copy request, video capture or HDR P010 format.
  if (ShouldSkipOverlay(render_pass)) {
    auto it = previous_frame_render_pass_states_.find(render_pass->id);
    if (it != previous_frame_render_pass_states_.end()) {
      // Add any overlay damage from the previous frame. Since we're not
      // promoting overlays this frame, damages that may have been removed in
      // the previous frame's UpdateDamageRect() now needs to be accounted
      // for.
      overlay_data.damage_rect.Union(
          PreviousFrameOverlayDamageContribution(it->second.overlay_rects));
      previous_frame_render_pass_states_.erase(it);
    }
    return;
  }

  QuadList* quad_list = &render_pass->quad_list;
  for (auto it = quad_list->begin(); it != quad_list->end(); ++it) {
    if (it->material == DrawQuad::Material::kAggregatedRenderPass) {
      const auto* rpdq = AggregatedRenderPassDrawQuad::MaterialCast(*it);
      auto render_pass_it =
          render_pass_backdrop_filters.find(rpdq->render_pass_id);
      if (render_pass_it != render_pass_backdrop_filters.end()) {
        backdrop_filter_rects.push_back(ClippedQuadRectangle(rpdq));
      }
      continue;
    }

    ValidateDrawQuadResult result = ValidateDrawQuad(
        resource_provider, it, backdrop_filter_rects, has_overlay_support_,
        has_p010_video_processor_support_, allowed_yuv_overlay_count_,
        global_overlay_state.processed_yuv_overlay_count,
        allow_promotion_hinting_);

    if (result.is_yuv_overlay) {
      global_overlay_state.yuv_quads++;
      if (no_undamaged_overlay_promotion_) {
        if (it->shared_quad_state->overlay_damage_index.has_value() &&
            !render_pass_state
                 .surface_damage_rect_list[it->shared_quad_state
                                               ->overlay_damage_index.value()]
                 .IsEmpty()) {
          global_overlay_state.damaged_yuv_quads++;
          if (result.code == DC_LAYER_SUCCESS) {
            global_overlay_state.processed_yuv_overlay_count++;
          }
        }
      } else {
        if (result.code == DC_LAYER_SUCCESS) {
          global_overlay_state.processed_yuv_overlay_count++;
        }
      }
    }

    if (!result.promotion_hint_mailbox.IsZero()) {
      DCHECK(allow_promotion_hinting_);
      bool promoted = result.code == DC_LAYER_SUCCESS;
      auto* overlay_state_service = OverlayStateService::GetInstance();
      // The OverlayStateService should always be initialized by GpuServiceImpl
      // at creation - DCHECK here just to assert there aren't any corner cases
      // where this isn't true.
      DCHECK(overlay_state_service->IsInitialized());
      overlay_state_service->SetPromotionHint(result.promotion_hint_mailbox,
                                              promoted);
    }

    if (result.code != DC_LAYER_SUCCESS) {
      RecordDCLayerResult(result.code, *it);
      continue;
    }

    if (!IsClearVideoQuad(*it)) {
      global_overlay_state.has_non_clear_video_overlays = true;
    }

    render_pass_state.candidates.push_back(it);
  }
}

void DCLayerOverlayProcessor::PromoteCandidates(
    const DisplayResourceProvider* resource_provider,
    AggregatedRenderPass* render_pass,
    const FilterOperationsMap& render_pass_filters,
    const RenderPassPreviousFrameState& previous_frame_state,
    bool is_page_fullscreen_mode,
    RenderPassOverlayData& overlay_data,
    RenderPassCurrentFrameState& current_frame_state,
    GlobalOverlayState& global_overlay_state) {
  QuadList* quad_list = &render_pass->quad_list;

  // Copy the overlay quad info to dc_layer_overlays and replace/delete overlay
  // quads in quad_list.
  for (auto& it : current_frame_state.candidates) {
    if (global_overlay_state.reject_overlays) {
      RecordDCLayerResult(DC_LAYER_FAILED_TOO_MANY_OVERLAYS, *it);
      continue;
    }

    // Do not promote undamaged video to overlays.
    bool undamaged =
        it->shared_quad_state->overlay_damage_index.has_value() &&
        current_frame_state
            .surface_damage_rect_list[it->shared_quad_state
                                          ->overlay_damage_index.value()]
            .IsEmpty();

    if (global_overlay_state.yuv_quads > allowed_yuv_overlay_count_ &&
        !global_overlay_state.has_non_clear_video_overlays && undamaged &&
        no_undamaged_overlay_promotion_ && IsVideoQuad(*it)) {
      RecordDCLayerResult(DC_LAYER_FAILED_NOT_DAMAGED, *it);
      continue;
    }

    gfx::Rect quad_rect_in_target_space = ClippedQuadRectangle(*it);

    // Quad is considered an "overlay" if it has no occluders.
    bool is_overlay = !IsOccluded(gfx::RectF(quad_rect_in_target_space),
                                  quad_list->begin(), it, render_pass_filters);

    // Protected video is always put in an overlay, but texture quads can be
    // skipped if they're not underlay compatible.
    const bool requires_overlay = IsOverlayRequiredForQuad(*it);

    // TODO(magchen@): Since we reject underlays here, the max number of YUV
    // overlays we can promote might not be accurate. We should allow all YUV
    // quads to be put into candidate_index_list, but only
    // |allowed_yuv_overlay_count_| YUV quads should be promoted to
    // overlays/underlays from that list.

    // Skip quad if it's an underlay and underlays are not allowed.
    if (!is_overlay && !requires_overlay) {
      DCLayerResult result = IsUnderlayAllowed(*it);

      if (result != DC_LAYER_SUCCESS) {
        RecordDCLayerResult(result, *it);
        continue;
      }
    }

    // Used by a histogram.
    global_overlay_state.has_occluding_damage_rect =
        global_overlay_state.has_occluding_damage_rect ||
        (!is_overlay &&
         HasOccludingDamageRect(it->shared_quad_state,
                                current_frame_state.surface_damage_rect_list,
                                quad_rect_in_target_space));

    UpdateDCLayerOverlays(resource_provider, render_pass, it,
                          quad_rect_in_target_space, previous_frame_state,
                          is_overlay, is_page_fullscreen_mode, overlay_data,
                          current_frame_state, global_overlay_state);
  }

  // Update previous frame state after processing render pass. If there is no
  // overlay in this frame, previous_frame_overlay_rect_union will be added
  // to the damage_rect here for GL composition because the overlay image from
  // the previous frame is missing in the GL composition path. If any overlay is
  // found in this frame, the previous overlay rects would have been handled
  // above and previous_frame_overlay_rect_union becomes empty.
  UpdateDamageRect(render_pass, previous_frame_state, overlay_data,
                   current_frame_state);

  RenderPassPreviousFrameState& previous_frame_render_pass_state =
      previous_frame_render_pass_states_.at(render_pass->id);
  std::swap(previous_frame_render_pass_state.overlay_rects,
            current_frame_state.overlay_rects);
  previous_frame_render_pass_state.display_rect = render_pass->output_rect;
}

void DCLayerOverlayProcessor::Process(
    const DisplayResourceProvider* resource_provider,
    const FilterOperationsMap& render_pass_filters,
    const FilterOperationsMap& render_pass_backdrop_filters,
    const SurfaceDamageRectList& surface_damage_rect_list_in_root_space,
    bool is_page_fullscreen_mode,
    RenderPassOverlayDataMap& render_pass_overlay_data_map) {
  GlobalOverlayState global_overlay_state;
  RenderPassCurrentFrameStateMap render_pass_state_map;
  render_pass_state_map.reserve(render_pass_overlay_data_map.size());

  for (auto& [render_pass, overlay_data] : render_pass_overlay_data_map) {
    if (!render_pass->transform_to_root_target.IsInvertible()) {
      // We can skip render passes that do not have an invertible transform
      // since it isn't visible.
      continue;
    }

    RenderPassCurrentFrameState& current_frame_state =
        render_pass_state_map[render_pass];
    // Convert the surface damage rects from root space to render pass space.
    // |surface_damage_rect_list_in_root_space| contains surface damages for all
    // surfaces in the frame across all render passes. We only need the surface
    // damage rects for the current render pass, but since we don't expect this
    // list to be large, this keeps the entire list for the simplicity.
    current_frame_state.surface_damage_rect_list =
        surface_damage_rect_list_in_root_space;
    for (auto& rect : current_frame_state.surface_damage_rect_list) {
      rect = render_pass->transform_to_root_target.InverseMapRect(rect).value();
      rect.Intersect(render_pass->output_rect);
    }

    CollectCandidates(resource_provider, render_pass,
                      render_pass_backdrop_filters, overlay_data,
                      current_frame_state, global_overlay_state);
  }

  // We might not save power if there are more than one videos and only part of
  // them are promoted to overlay. Skip overlays for this frame unless there are
  // protected video or texture overlays.
  // In case of videos being paused or not started yet, we will allow multiple
  // overlays if the number of damaged overlays doesn't exceed
  // |allowed_yuv_overlay_count|. However, videos are not always damaged in
  // every frame during video playback. To prevent overlay promotion from being
  // switched between on and off, we wait for
  // |kDCLayerFramesDelayedBeforeOverlay| frames before allowing multiple
  // overlays
  if (global_overlay_state.yuv_quads > 1 &&
      !global_overlay_state.has_non_clear_video_overlays) {
    if (no_undamaged_overlay_promotion_) {
      if (global_overlay_state.damaged_yuv_quads ==
          global_overlay_state.processed_yuv_overlay_count) {
        frames_since_last_qualified_multi_overlays_++;
      } else {
        frames_since_last_qualified_multi_overlays_ = 0;
      }
      global_overlay_state.reject_overlays =
          frames_since_last_qualified_multi_overlays_ <=
          kDCLayerFramesDelayedBeforeOverlay;
    } else {
      if (global_overlay_state.yuv_quads !=
          global_overlay_state.processed_yuv_overlay_count) {
        global_overlay_state.reject_overlays = true;
      }
    }
  }

  // A YUV quad might be rejected later due to not allowed as an underlay.
  // Recount the YUV overlays when they are added to the overlay list
  // successfully.
  global_overlay_state.processed_yuv_overlay_count = 0;
  if (disable_video_overlay_if_moving_) {
    RemoveClearVideoQuadCandidatesIfMoving(
        resource_provider, render_pass_overlay_data_map, render_pass_state_map);
  }

  // Swap the entire map into a local variable. For the rest of this function,
  // information about the current frame is populated into the member variable,
  // while the local variable is used to access information about the previous
  // frame. Clearing the member variable allows us to remove render passes that
  // don't exist in the current frame.
  base::flat_map<AggregatedRenderPassId, RenderPassPreviousFrameState>
      previous_frame_render_pass_states;
  std::swap(previous_frame_render_pass_states_,
            previous_frame_render_pass_states);

  for (auto& [render_pass, overlay_data] : render_pass_overlay_data_map) {
    // Create an entry for the current frame. This is the only place where an
    // entry is created in this map.
    previous_frame_render_pass_states_.emplace(render_pass->id,
                                               RenderPassPreviousFrameState());

    PromoteCandidates(resource_provider, render_pass, render_pass_filters,
                      previous_frame_render_pass_states[render_pass->id],
                      is_page_fullscreen_mode, overlay_data,
                      render_pass_state_map[render_pass], global_overlay_state);
  }

  if (global_overlay_state.processed_yuv_overlay_count > 0) {
    base::UmaHistogramExactLinear(
        "GPU.DirectComposition.DCLayer.YUVOverlayCount",
        /*sample=*/global_overlay_state.processed_yuv_overlay_count,
        /*exclusive_max=*/10);

    RecordOverlayHistograms(render_pass_overlay_data_map,
                            global_overlay_state.has_occluding_damage_rect);
  }
}

bool DCLayerOverlayProcessor::ShouldSkipOverlay(
    AggregatedRenderPass* render_pass) const {
  QuadList* quad_list = &render_pass->quad_list;

  // Skip overlay processing if we have copy request or video capture is
  // enabled. When video capture is enabled, some frames might not have copy
  // request.
  if (render_pass->HasCapture()) {
    // Find a valid overlay candidate from quad_list.
    QuadList::Iterator it = FindAnOverlayCandidate(*quad_list);
    if (it != quad_list->end()) {
      render_pass->video_capture_enabled
          ? RecordDCLayerResult(DC_LAYER_FAILED_VIDEO_CAPTURE_ENABLED, *it)
          : RecordDCLayerResult(DC_LAYER_FAILED_COPY_REQUESTS, *it);
    }
    return true;
  }

  if (render_pass->content_color_usage == gfx::ContentColorUsage::kHDR) {
    // Media Foundation always uses overlays to render video, so do not skip.
    QuadList::Iterator it =
        FindAnOverlayCandidateExcludingMediaFoundationVideoContent(*quad_list);
    if (it != quad_list->end()) {
      // Skip overlay processing if output colorspace is HDR and rgb10a2 overlay
      // is not supported. Since most of overlay only supports NV12 and YUY2
      // now, HDR content (usually P010 format) cannot output through overlay
      // without format degrading. In some Intel's platforms (Icelake or above),
      // Overlay can play HDR content by supporting RGB10 format. Let overlay
      // deal with HDR content in this situation.
      bool supports_rgb10a2_overlay =
          gl::GetDirectCompositionOverlaySupportFlags(
              DXGI_FORMAT_R10G10B10A2_UNORM) != 0;
      if (!supports_rgb10a2_overlay) {
        RecordDCLayerResult(DC_LAYER_FAILED_OUTPUT_HDR, *it);
        return true;
      }
      // Skip overlay processing if output colorspace is HDR and any
      // non-HDR-enabled display exists. Technically we should use HWND detect
      // if HDR is enabled on the current display or not, if it is enabled
      // then promote overlay, otherwise not, but since currently we can't
      // retrieve HWND in DCLayerOverlayProcessor, in case of very bad
      // tone-mapping result by video processor on non-HDR-enabled display, we
      // tend to be strict about the overlay promotion and always let Viz do HDR
      // tone mapping to avoid a visual difference between Viz and video
      // processor.
      if (system_hdr_disabled_on_any_display_) {
        RecordDCLayerResult(DC_LAYER_FAILED_YUV_VIDEO_QUAD_HDR_TONE_MAPPING,
                            *it);
        return true;
      }
    }
  }

  return false;
}

void DCLayerOverlayProcessor::UpdateDCLayerOverlays(
    const DisplayResourceProvider* resource_provider,
    AggregatedRenderPass* render_pass,
    const QuadList::Iterator& it,
    const gfx::Rect& quad_rect_in_target_space,
    const RenderPassPreviousFrameState& previous_frame_state,
    bool is_overlay,
    bool is_page_fullscreen_mode,
    RenderPassOverlayData& overlay_data,
    RenderPassCurrentFrameState& current_frame_state,
    GlobalOverlayState& global_overlay_state) {
  // Record the result first before ProcessForOverlay().
  RecordDCLayerResult(DC_LAYER_SUCCESS, *it);

  OverlayCandidate dc_layer;
  FromDrawQuad(resource_provider, render_pass, is_page_fullscreen_mode, it,
               global_overlay_state.processed_yuv_overlay_count, dc_layer);

  // Underlays are less efficient, so attempt regular overlays first. We can
  // only check for occlusion within a render pass.
  if (is_overlay) {
    ProcessForOverlay(render_pass, it, previous_frame_state,
                      current_frame_state);
  } else {
    ProcessForUnderlay(render_pass, it, quad_rect_in_target_space,
                       previous_frame_state, global_overlay_state, overlay_data,
                       current_frame_state, dc_layer);
  }

  current_frame_state.overlay_rects.push_back(
      {quad_rect_in_target_space, is_overlay});

  overlay_data.promoted_overlays.push_back(dc_layer);

  // Recorded for each overlay.
  UMA_HISTOGRAM_BOOLEAN("GPU.DirectComposition.IsUnderlay", !is_overlay);
}

void DCLayerOverlayProcessor::ProcessForOverlay(
    AggregatedRenderPass* render_pass,
    const QuadList::Iterator& it,
    const RenderPassPreviousFrameState& previous_frame_state,
    RenderPassCurrentFrameState& current_frame_state) const {
  // The quad is on top, so promote it to an overlay and remove all damage
  // underneath it.
  const bool display_rect_changed =
      render_pass->output_rect != previous_frame_state.display_rect;
  const bool is_axis_aligned = it->shared_quad_state->quad_to_target_transform
                                   .Preserves2dAxisAlignment();
  const bool needs_blending = it->ShouldDrawWithBlending();

  if (is_axis_aligned && !display_rect_changed && !needs_blending) {
    RemoveOverlayDamageRect(it, current_frame_state);
  }

  // Overlay quads should not be drawn. Removing the quads from the quad list
  // creates extra complexity since we would be traversing the list while
  // removing them. Instead, we can just make the visible rect empty, which
  // would then be skipped by DirectRenderer::ShouldSkipQuad.
  it->visible_rect = gfx::Rect();
}

void DCLayerOverlayProcessor::ProcessForUnderlay(
    AggregatedRenderPass* render_pass,
    const QuadList::Iterator& it,
    const gfx::Rect& quad_rect_in_target_space,
    const RenderPassPreviousFrameState& previous_frame_state,
    const GlobalOverlayState& global_overlay_state,
    RenderPassOverlayData& overlay_data,
    RenderPassCurrentFrameState& current_frame_state,
    OverlayCandidate& dc_layer) {
  // Assign decreasing z-order so that underlays processed earlier, and hence
  // which are above the subsequent underlays, are placed above in the direct
  // composition visual tree. The z-orders are assigned relative to other
  // underlays in its render pass, not relative to the total number of underlays
  // across all render passes.
  dc_layer.plane_z_order = -1 - overlay_data.promoted_overlays.size();

  // If the video is translucent and uses SrcOver blend mode, we can achieve the
  // same result as compositing with video on top if we replace video quad with
  // a solid color quad with DstOut blend mode, and rely on SrcOver blending
  // of the root surface with video on bottom. Essentially,
  //
  // SrcOver_quad(V, B, V_alpha) = SrcOver_premul(DstOut(BLACK, B, V_alpha), V)
  // where
  //    V is the video quad
  //    B is the background
  //    SrcOver_quad uses opacity of source quad (V_alpha)
  //    SrcOver_premul uses alpha channel and assumes premultipled alpha
  //
  // This also applies to quads with a mask filter for rounded corners.
  bool is_opaque = false;

  if (it->ShouldDrawWithBlending() &&
      it->shared_quad_state->blend_mode == SkBlendMode::kSrcOver) {
    render_pass->ReplaceExistingQuadWithSolidColor(it, SkColors::kBlack,
                                                   SkBlendMode::kDstOut);
  } else {
    // When the opacity == 1.0, drawing with transparent will be done without
    // blending and will have the proper effect of completely clearing the
    // layer.
    render_pass->ReplaceExistingQuadWithSolidColor(it, SkColors::kTransparent,
                                                   SkBlendMode::kSrcOver);
    is_opaque = true;
  }

  const bool display_rect_unchanged =
      render_pass->output_rect == previous_frame_state.display_rect;
  const bool underlay_rect_unchanged = IsPreviousFrameUnderlayRect(
      previous_frame_state.overlay_rects, quad_rect_in_target_space,
      overlay_data.promoted_overlays.size());
  const bool is_axis_aligned = it->shared_quad_state->quad_to_target_transform
                                   .Preserves2dAxisAlignment();
  bool opacity_unchanged =
      (is_opaque == previous_frame_state.underlay_is_opaque);
  previous_frame_render_pass_states_.at(render_pass->id).underlay_is_opaque =
      is_opaque;

  if (is_axis_aligned && opacity_unchanged && underlay_rect_unchanged &&
      display_rect_unchanged) {
    // If this underlay rect is the same as for last frame, Remove its area
    // from the damage of the main surface, as the cleared area was already
    // cleared last frame.

    // If none of the quads on top give any damage, we can skip compositing
    // these quads. The output damage rect might be empty after we remove the
    // the damage from the video quad. We can save power if the damage rect is
    // empty.
    RemoveOverlayDamageRect(it, current_frame_state);
  } else {
    // Entire replacement quad must be redrawn.
    overlay_data.damage_rect.Union(quad_rect_in_target_space);
    current_frame_state.surface_damage_rect_list.push_back(
        quad_rect_in_target_space);
  }
}

DCLayerOverlayProcessor::RenderPassOverlayData::RenderPassOverlayData() =
    default;
DCLayerOverlayProcessor::RenderPassOverlayData::~RenderPassOverlayData() =
    default;
DCLayerOverlayProcessor::RenderPassOverlayData::RenderPassOverlayData(
    RenderPassOverlayData&&) = default;
DCLayerOverlayProcessor::RenderPassOverlayData&
DCLayerOverlayProcessor::RenderPassOverlayData::operator=(
    RenderPassOverlayData&&) = default;

DCLayerOverlayProcessor::RenderPassPreviousFrameState::
    RenderPassPreviousFrameState() = default;
DCLayerOverlayProcessor::RenderPassPreviousFrameState::
    ~RenderPassPreviousFrameState() = default;
DCLayerOverlayProcessor::RenderPassPreviousFrameState::
    RenderPassPreviousFrameState(RenderPassPreviousFrameState&&) = default;
DCLayerOverlayProcessor::RenderPassPreviousFrameState&
DCLayerOverlayProcessor::RenderPassPreviousFrameState::operator=(
    RenderPassPreviousFrameState&&) = default;

DCLayerOverlayProcessor::RenderPassCurrentFrameState::
    RenderPassCurrentFrameState() = default;
DCLayerOverlayProcessor::RenderPassCurrentFrameState::
    ~RenderPassCurrentFrameState() = default;
DCLayerOverlayProcessor::RenderPassCurrentFrameState::
    RenderPassCurrentFrameState(RenderPassCurrentFrameState&&) = default;
DCLayerOverlayProcessor::RenderPassCurrentFrameState&
DCLayerOverlayProcessor::RenderPassCurrentFrameState::operator=(
    RenderPassCurrentFrameState&&) = default;
}  // namespace viz
