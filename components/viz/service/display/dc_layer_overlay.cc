// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/dc_layer_overlay.h"

#include <limits>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "components/viz/common/overlay_state/win/overlay_state_service.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/common/viz_utils.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/base/media_switches.h"
#include "media/base/win/mf_feature_checks.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/gl_switches.h"

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
  DC_LAYER_FAILED_HDR_TONE_MAPPING = 17,
  DC_LAYER_FAILED_YUV_VIDEO_QUAD_NO_HDR_METADATA = 18,
  DC_LAYER_FAILED_YUV_VIDEO_QUAD_HLG = 19,
  kMaxValue = DC_LAYER_FAILED_YUV_VIDEO_QUAD_HLG,
};

DCLayerResult ValidateYUVOverlay(
    const gfx::ProtectedVideoType& protected_video_type,
    const gfx::ColorSpace& video_color_space,
    const absl::optional<gfx::HDRMetadata>& hdr_metadata,
    bool has_overlay_support,
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

  // HLG shouldn't have the hdr metadata, but we don't want to promote it to
  // overlay, as VideoProcessor doesn't support HLG tone mapping well between
  // different gpu vendors, see: https://crbug.com/1144260#c6.
  // Some HLG streams may carry hdr metadata, see: https://crbug.com/1429172.
  if (video_color_space.GetTransferID() == gfx::ColorSpace::TransferID::HLG) {
    return DC_LAYER_FAILED_YUV_VIDEO_QUAD_HLG;
  }

  // Otherwise, it could be a parser bug like https://crbug.com/1362288 if the
  // hdr metadata is still missing. We shouldn't promote too for that case.
  if (video_color_space.IsHDR() && !hdr_metadata.has_value()) {
    return DC_LAYER_FAILED_YUV_VIDEO_QUAD_NO_HDR_METADATA;
  }

  return DC_LAYER_SUCCESS;
}

DCLayerResult ValidateYUVQuad(
    const YUVVideoDrawQuad* quad,
    const std::vector<gfx::Rect>& backdrop_filter_rects,
    bool has_overlay_support,
    int allowed_yuv_overlay_count,
    int processed_yuv_overlay_count,
    DisplayResourceProvider* resource_provider) {
  // Note: Do not override this value based on base::Feature values. It is the
  // result after the GPU blocklist has been consulted.
  if (!has_overlay_support)
    return DC_LAYER_FAILED_UNSUPPORTED_QUAD;

  // Check that resources are overlay compatible first so that subsequent
  // assumptions are valid.
  for (const auto& resource : quad->resources) {
    if (!resource_provider->IsOverlayCandidate(resource))
      return DC_LAYER_FAILED_TEXTURE_NOT_CANDIDATE;
  }

  // Hardware protected video must use Direct Composition Overlay
  if (quad->protected_video_type == gfx::ProtectedVideoType::kHardwareProtected)
    return DC_LAYER_SUCCESS;

  if (quad->shared_quad_state->blend_mode != SkBlendMode::kSrcOver)
    return DC_LAYER_FAILED_QUAD_BLEND_MODE;

  if (!quad->shared_quad_state->quad_to_target_transform
           .Preserves2dAxisAlignment()) {
    return DC_LAYER_FAILED_COMPLEX_TRANSFORM;
  }

  if (processed_yuv_overlay_count >= allowed_yuv_overlay_count)
    return DC_LAYER_FAILED_TOO_MANY_OVERLAYS;

  auto quad_target_rect = ClippedQuadRectangle(quad);
  for (const auto& filter_target_rect : backdrop_filter_rects) {
    if (filter_target_rect.Intersects(quad_target_rect))
      return DC_LAYER_FAILED_BACKDROP_FILTERS;
  }

  // HLG shouldn't have the hdr metadata, but we don't want to promote it to
  // overlay, as VideoProcessor doesn't support HLG tone mapping well between
  // different gpu vendors, see: https://crbug.com/1144260#c6.
  // Some HLG streams may carry hdr metadata, see: https://crbug.com/1429172.
  if (quad->video_color_space.GetTransferID() ==
      gfx::ColorSpace::TransferID::HLG) {
    return DC_LAYER_FAILED_YUV_VIDEO_QUAD_HLG;
  }

  // Otherwise, it could be a parser bug like https://crbug.com/1362288 if the
  // hdr metadata is still missing. We shouldn't promote too for that case.
  if (quad->video_color_space.IsHDR() && !quad->hdr_metadata.has_value()) {
    return DC_LAYER_FAILED_YUV_VIDEO_QUAD_NO_HDR_METADATA;
  }

  return DC_LAYER_SUCCESS;
}

void FromYUVQuad(const YUVVideoDrawQuad* quad,
                 const gfx::Transform& transform_to_root_target,
                 OverlayCandidate* dc_layer) {
  // Direct composition path only supports a single NV12 buffer.
  DCHECK(quad->y_plane_resource_id() && quad->u_plane_resource_id());
  DCHECK_EQ(quad->u_plane_resource_id(), quad->v_plane_resource_id());
  dc_layer->resource_id = quad->y_plane_resource_id();

  dc_layer->plane_z_order = 1;
  dc_layer->display_rect = gfx::RectF(quad->rect);
  dc_layer->resource_size_in_pixels = quad->ya_tex_size();
  dc_layer->uv_rect =
      gfx::ScaleRect(quad->ya_tex_coord_rect(),
                     1.f / dc_layer->resource_size_in_pixels.width(),
                     1.f / dc_layer->resource_size_in_pixels.height());

  // Quad rect is in quad content space so both quad to target, and target to
  // root transforms must be applied to it.
  gfx::Transform quad_to_root_transform(
      quad->shared_quad_state->quad_to_target_transform);
  quad_to_root_transform.PostConcat(transform_to_root_target);
  // Flatten transform to 2D since DirectComposition doesn't support 3D
  // transforms.  This only applies when non axis aligned overlays are enabled.
  quad_to_root_transform.Flatten();
  dc_layer->transform = quad_to_root_transform;

  if (quad->shared_quad_state->clip_rect) {
    // Clip rect is in quad target space, and must be transformed to root target
    // space.
    dc_layer->clip_rect =
        transform_to_root_target.MapRect(*quad->shared_quad_state->clip_rect);
  }
  dc_layer->color_space = quad->video_color_space;
  dc_layer->protected_video_type = quad->protected_video_type;
  dc_layer->hdr_metadata = quad->hdr_metadata.value_or(gfx::HDRMetadata());
}

DCLayerResult ValidateTextureQuad(
    const TextureDrawQuad* quad,
    const std::vector<gfx::Rect>& backdrop_filter_rects,
    bool has_overlay_support,
    int allowed_yuv_overlay_count,
    int processed_yuv_overlay_count,
    DisplayResourceProvider* resource_provider) {
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

  auto si_format = resource_provider->GetSharedImageFormat(quad->resource_id());
  if (media::IsMultiPlaneFormatForHardwareVideoEnabled() &&
      si_format.is_multi_plane()) {
    auto color_space =
        resource_provider->GetOverlayColorSpace(quad->resource_id());
    auto result = ValidateYUVOverlay(quad->protected_video_type, color_space,
                                     quad->hdr_metadata, has_overlay_support,
                                     allowed_yuv_overlay_count,
                                     processed_yuv_overlay_count);
    return result;
  }

  return DC_LAYER_SUCCESS;
}

void FromTextureQuad(const TextureDrawQuad* quad,
                     const gfx::Transform& transform_to_root_target,
                     DisplayResourceProvider* resource_provider,
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

  dc_layer->color_space =
      resource_provider->GetOverlayColorSpace(quad->resource_id());
  // Both color space and protected_video_type are hard-coded for stream video.
  // TODO(crbug.com/1384544): Consider using quad->protected_video_type.
  if (quad->is_stream_video) {
    dc_layer->color_space = gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                                            gfx::ColorSpace::TransferID::BT709);
    dc_layer->protected_video_type =
        gfx::ProtectedVideoType::kHardwareProtected;
  }
}

bool IsProtectedVideo(const QuadList::ConstIterator& it) {
  if (it->material == DrawQuad::Material::kYuvVideoContent) {
    const auto* yuv_quad = YUVVideoDrawQuad::MaterialCast(*it);
    return yuv_quad->protected_video_type ==
               gfx::ProtectedVideoType::kHardwareProtected ||
           yuv_quad->protected_video_type ==
               gfx::ProtectedVideoType::kSoftwareProtected;
  } else if (it->material == DrawQuad::Material::kTextureContent &&
             TextureDrawQuad::MaterialCast(*it)->is_stream_video) {
    return true;
  } else {
    return false;
  }
}

DCLayerResult IsUnderlayAllowed(const QuadList::Iterator& it) {
  if (it->ShouldDrawWithBlending() &&
      !it->shared_quad_state->mask_filter_info.HasRoundedCorners()) {
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
        auto* filters = render_pass_it->second;
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
    const gfx::Rect& quad_rect_in_root_space) {
  if (!shared_quad_state->overlay_damage_index.has_value())
    return !quad_rect_in_root_space.IsEmpty();

  size_t overlay_damage_index = shared_quad_state->overlay_damage_index.value();
  if (overlay_damage_index >= surface_damage_rect_list.size()) {
    DCHECK(false);
  }

  // Damage rects in surface_damage_rect_list are arranged from top to bottom.
  // surface_damage_rect_list[0] is the one on the very top.
  // surface_damage_rect_list[overlay_damage_index] is the damage rect of
  // this overlay surface.
  gfx::Rect occluding_damage_rect;
  for (size_t i = 0; i < overlay_damage_index; ++i) {
    occluding_damage_rect.Union(surface_damage_rect_list[i]);
  }
  occluding_damage_rect.Intersect(quad_rect_in_root_space);

  return !occluding_damage_rect.IsEmpty();
}

bool IsPossibleFullScreenLetterboxing(const QuadList::Iterator& it,
                                      QuadList::ConstIterator quad_list_end,
                                      const gfx::RectF& display_rect) {
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
      return (beneath_rect.origin() == display_rect.origin() &&
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

void RecordDCLayerResult(DCLayerResult result, QuadList::ConstIterator it) {
  // Skip recording unsupported quads since that'd dwarf the data we care about.
  if (result == DC_LAYER_FAILED_UNSUPPORTED_QUAD)
    return;

  switch (it->material) {
    case DrawQuad::Material::kYuvVideoContent:
      RecordVideoDCLayerResult(
          result, YUVVideoDrawQuad::MaterialCast(*it)->protected_video_type);
      break;
    case DrawQuad::Material::kTextureContent: {
      if (TextureDrawQuad::MaterialCast(*it)->is_stream_video) {
        UMA_HISTOGRAM_ENUMERATION(
            "GPU.DirectComposition.DCLayerResult.StreamVideo", result);
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
void RecordOverlayHistograms(OverlayCandidateList* dc_layer_overlays,
                             bool has_occluding_surface_damage,
                             const gfx::Rect* damage_rect) {
  // If an underlay is found, we record the damage rect of this frame as an
  // underlay.
  bool is_overlay = true;
  for (const auto& dc_layer : *dc_layer_overlays) {
    if (dc_layer.plane_z_order != 1) {
      is_overlay = false;
      break;
    }
  }

  OverlayProcessorInterface::RecordOverlayDamageRectHistograms(
      is_overlay, has_occluding_surface_damage, damage_rect->IsEmpty());
}

QuadList::Iterator FindAnOverlayCandidate(QuadList& quad_list) {
  for (auto it = quad_list.begin(); it != quad_list.end(); ++it) {
    if (it->material == DrawQuad::Material::kYuvVideoContent ||
        it->material == DrawQuad::Material::kTextureContent)
      return it;
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
        (quad_it->material == DrawQuad::Material::kYuvVideoContent ||
         quad_it->material == DrawQuad::Material::kTextureContent))
      it = quad_it;
  }
  return it;
}

bool IsClearVideoQuad(const QuadList::ConstIterator& it) {
  return it->material == DrawQuad::Material::kYuvVideoContent &&
         !IsProtectedVideo(it);
}

}  // namespace

DCLayerOverlayProcessor::DCLayerOverlayProcessor(
    int allowed_yuv_overlay_count,
    bool skip_initialization_for_testing)
    : has_overlay_support_(skip_initialization_for_testing),
      allowed_yuv_overlay_count_(allowed_yuv_overlay_count),
      no_undamaged_overlay_promotion_(base::FeatureList::IsEnabled(
          features::kNoUndamagedOverlayPromotion)) {
  if (!skip_initialization_for_testing) {
    UpdateHasHwOverlaySupport();
    UpdateSystemHDRStatus();
    gl::DirectCompositionOverlayCapsMonitor::GetInstance()->AddObserver(this);
  }
  allow_promotion_hinting_ = media::SupportMediaFoundationClearPlayback();
}

DCLayerOverlayProcessor::~DCLayerOverlayProcessor() {
  gl::DirectCompositionOverlayCapsMonitor::GetInstance()->RemoveObserver(this);
}

void DCLayerOverlayProcessor::UpdateHasHwOverlaySupport() {
  has_overlay_support_ = gl::DirectCompositionOverlaysSupported();
}

void DCLayerOverlayProcessor::UpdateSystemHDRStatus() {
  bool hdr_enabled = false;
  auto dxgi_info = gl::GetDirectCompositionHDRMonitorDXGIInfo();
  for (const auto& output_desc : dxgi_info->output_descs)
    hdr_enabled |= output_desc->hdr_enabled;
  system_hdr_enabled_ = hdr_enabled;
}

// Called on the Viz Compositor thread.
void DCLayerOverlayProcessor::OnOverlayCapsChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  UpdateHasHwOverlaySupport();
  UpdateSystemHDRStatus();
}

void DCLayerOverlayProcessor::ClearOverlayState() {
  previous_frame_overlay_rects_.clear();
  previous_frame_underlay_is_opaque_ = true;
}

gfx::Rect DCLayerOverlayProcessor::PreviousFrameOverlayDamageContribution() {
  gfx::Rect rects_union;
  for (const auto& overlay : previous_frame_overlay_rects_)
    rects_union.Union(overlay.rect);
  return rects_union;
}

void DCLayerOverlayProcessor::RemoveOverlayDamageRect(
    const QuadList::Iterator& it) {
  // This is done by setting the overlay surface damage rect in the
  // |surface_damage_rect_list_| to zero.
  if (it->shared_quad_state->overlay_damage_index.has_value()) {
    size_t overlay_damage_index =
        it->shared_quad_state->overlay_damage_index.value();
    if (overlay_damage_index >= surface_damage_rect_list_.size())
      DCHECK(false);
    else
      damages_to_be_removed_.push_back(overlay_damage_index);
  }
}

// This is called at the end of Process(). The goal is to get an empty root
// damage rect if the overlays are the only damages in the frame.
void DCLayerOverlayProcessor::UpdateRootDamageRect(
    const gfx::RectF& display_rect,
    gfx::Rect* damage_rect) {
  // Check whether the overlay rect union from the previous frame should be
  // added to the current frame and whether the overlay damages can be removed
  // from the current damage rect.

  bool should_add_previous_frame_overlay_damage = true;
  size_t current_frame_overlay_count = current_frame_overlay_rects_.size();
  if (current_frame_overlay_count > 0 &&
      current_frame_overlay_count == previous_frame_overlay_rects_.size() &&
      display_rect == previous_display_rect_) {
    bool same_overlays = true;
    for (size_t i = 0; i < current_frame_overlay_count; ++i) {
      if (previous_frame_overlay_rects_[i] != current_frame_overlay_rects_[i]) {
        same_overlays = false;
        break;
      }
    }

    if (same_overlays) {
      // No need to add back the overlay rect union from the previous frame
      // if no changes in overlays.
      should_add_previous_frame_overlay_damage = false;

      // The final root damage rect is computed by add up all surface damages
      // except for the overlay surface damages and the damages right below
      // the overlays.
      gfx::Rect root_damage_rect;
      size_t surface_index = 0;
      for (auto surface_damage_rect : surface_damage_rect_list_) {
        // We only support at most two overlays. The size of
        // damages_to_be_removed_ will not be bigger than 2. We should
        // revisit this damages_to_be_removed_ for-loop if we try to support
        // many overlays.
        // See capabilities.supports_two_yuv_hardware_overlays.
        for (const auto index_to_be_removed : damages_to_be_removed_) {
          // The overlay damages and the damages right below them will not be
          // added to the root damage rect.
          if (surface_index == index_to_be_removed) {
            // This is the overlay surface.
            surface_damage_rect = gfx::Rect();
            break;
          } else if (surface_index > index_to_be_removed) {
            // This is the surface below the overlays.
            surface_damage_rect.Subtract(
                surface_damage_rect_list_[index_to_be_removed]);
          }
        }
        root_damage_rect.Union(surface_damage_rect);
        ++surface_index;
      }

      *damage_rect = root_damage_rect;
    }
  }

  if (should_add_previous_frame_overlay_damage) {
    damage_rect->Union(PreviousFrameOverlayDamageContribution());
  }
  damage_rect->Intersect(gfx::ToEnclosingRect(display_rect));
  damages_to_be_removed_.clear();
}

bool DCLayerOverlayProcessor::IsPreviousFrameUnderlayRect(
    const gfx::Rect& quad_rectangle,
    size_t index) {
  if (index >= previous_frame_overlay_rects_.size()) {
    return false;
  } else {
    // Although we can loop through the list to find out if there is an
    // underlay with the same size from the previous frame, checking
    // _rectx_[index] is the quickest way to do it. If we cannot find a match
    // with the same index, there is probably a change in the number of
    // overlays or layout. Then we won't be able to get a zero root damage
    // rect in this case. Looping through the list won't give better power.
    return (previous_frame_overlay_rects_[index].rect == quad_rectangle) &&
           (previous_frame_overlay_rects_[index].is_overlay == false);
  }
}

void DCLayerOverlayProcessor::RemoveClearVideoQuadCandidatesIfMoving(
    const QuadList* quad_list,
    std::vector<QuadList::Iterator>& candidates) {
  // The number of frames all overlay candidates need to be stable before we
  // allow overlays again. This number was chosen experimentally.
  constexpr int kFramesOfStabilityForOverlayPromotion = 5;

  std::vector<gfx::Rect> current_frame_overlay_candidate_rects;
  current_frame_overlay_candidate_rects.reserve(candidates.size());

  for (const auto& candidate_it : candidates) {
    if (IsClearVideoQuad(candidate_it)) {
      gfx::Rect quad_rectangle_in_target_space =
          ClippedQuadRectangle(*candidate_it);
      current_frame_overlay_candidate_rects.push_back(
          quad_rectangle_in_target_space);
    }
  }

  if (previous_frame_overlay_candidate_rects_ !=
      current_frame_overlay_candidate_rects) {
    frames_since_last_overlay_candidate_rects_change_ = 0;

    std::swap(previous_frame_overlay_candidate_rects_,
              current_frame_overlay_candidate_rects);
  } else {
    frames_since_last_overlay_candidate_rects_change_++;
  }

  if (frames_since_last_overlay_candidate_rects_change_ <=
      kFramesOfStabilityForOverlayPromotion) {
    // Remove all video quad candidates if any of them moved recently
    auto candidate_it = candidates.begin();
    while (candidate_it != candidates.end()) {
      if (IsClearVideoQuad(*candidate_it)) {
        RecordDCLayerResult(DC_LAYER_FAILED_YUV_VIDEO_QUAD_MOVED,
                            *candidate_it);
        candidate_it = candidates.erase(candidate_it);
      } else {
        candidate_it++;
      }
    }
  }
}

void DCLayerOverlayProcessor::Process(
    DisplayResourceProvider* resource_provider,
    const gfx::RectF& display_rect,
    const FilterOperationsMap& render_pass_filters,
    const FilterOperationsMap& render_pass_backdrop_filters,
    AggregatedRenderPass* render_pass,
    gfx::Rect* damage_rect,
    SurfaceDamageRectList surface_damage_rect_list,
    OverlayCandidateList* dc_layer_overlays,
    bool is_video_capture_enabled,
    bool is_page_fullscreen_mode) {
  bool this_frame_has_occluding_damage_rect = false;
  processed_yuv_overlay_count_ = 0;
  surface_damage_rect_list_ = std::move(surface_damage_rect_list);

  // Output rects of child render passes that have backdrop filters in target
  // space. These rects are used to determine if the overlay rect could be read
  // by backdrop filters.
  std::vector<gfx::Rect> backdrop_filter_rects;

  // Skip overlay for copy request, video capture or HDR P010 format.
  if (ShouldSkipOverlay(render_pass, is_video_capture_enabled)) {
    // Update damage rect before calling ClearOverlayState, otherwise
    // previous_frame_overlay_rect_union will be empty.
    damage_rect->Union(PreviousFrameOverlayDamageContribution());
    ClearOverlayState();

    return;
  }

  std::vector<QuadList::Iterator> candidates;
  QuadList* quad_list = &render_pass->quad_list;

  // Used for whether overlay should be skipped
  int yuv_quads_in_quad_list = 0;
  int damaged_yuv_quads_in_quad_list = 0;
  bool has_protected_video_or_texture_overlays = false;

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

    gpu::Mailbox promotion_hint_mailbox;
    DCLayerResult result;
    bool is_yuv_overlay = false;
    switch (it->material) {
      case DrawQuad::Material::kYuvVideoContent:
        result = ValidateYUVQuad(
            YUVVideoDrawQuad::MaterialCast(*it), backdrop_filter_rects,
            has_overlay_support_, allowed_yuv_overlay_count_,
            processed_yuv_overlay_count_, resource_provider);
        is_yuv_overlay = true;
        break;
      case DrawQuad::Material::kTextureContent: {
        const TextureDrawQuad* tex_quad = TextureDrawQuad::MaterialCast(*it);

        if (tex_quad->is_stream_video) {
          // Stream video quads contain Media Foundation dcomp surface which is
          // always presented as overlay.
          result = DC_LAYER_SUCCESS;
        } else {
          result = ValidateTextureQuad(
              tex_quad, backdrop_filter_rects, has_overlay_support_,
              allowed_yuv_overlay_count_, processed_yuv_overlay_count_,
              resource_provider);
        }
        auto si_format =
            resource_provider->GetSharedImageFormat(tex_quad->resource_id());
        if (media::IsMultiPlaneFormatForHardwareVideoEnabled() &&
            si_format.is_multi_plane()) {
          is_yuv_overlay = true;
        }

        if (allow_promotion_hinting_) {
          // If this quad has marked itself as wanting promotion hints then get
          // the associated mailbox.
          ResourceId id = tex_quad->resource_id();
          if (resource_provider->DoesResourceWantPromotionHint(id)) {
            promotion_hint_mailbox = resource_provider->GetMailbox(id);
          }
        }
      } break;
      default:
        result = DC_LAYER_FAILED_UNSUPPORTED_QUAD;
    }

    if (is_yuv_overlay) {
      yuv_quads_in_quad_list++;
      if (no_undamaged_overlay_promotion_) {
        if (it->shared_quad_state->overlay_damage_index.has_value() &&
            !surface_damage_rect_list_[it->shared_quad_state
                                           ->overlay_damage_index.value()]
                 .IsEmpty()) {
          damaged_yuv_quads_in_quad_list++;
          if (result == DC_LAYER_SUCCESS) {
            processed_yuv_overlay_count_++;
          }
        }
      } else {
        if (result == DC_LAYER_SUCCESS) {
          processed_yuv_overlay_count_++;
        }
      }
    }

    if (!promotion_hint_mailbox.IsZero()) {
      DCHECK(allow_promotion_hinting_);
      bool promoted = result == DC_LAYER_SUCCESS;
      auto* overlay_state_service = OverlayStateService::GetInstance();
      // The OverlayStateService should always be initialized by GpuServiceImpl
      // at creation - DCHECK here just to assert there aren't any corner cases
      // where this isn't true.
      DCHECK(overlay_state_service->IsInitialized());
      overlay_state_service->SetPromotionHint(promotion_hint_mailbox, promoted);
    }

    if (result != DC_LAYER_SUCCESS) {
      RecordDCLayerResult(result, it);
      continue;
    }

    const bool is_protected_video = IsProtectedVideo(it);
    const bool is_texture_quad =
        it->material == DrawQuad::Material::kTextureContent;
    if (is_protected_video || is_texture_quad)
      has_protected_video_or_texture_overlays = true;

    candidates.push_back(it);
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
  bool reject_overlays = false;
  if (yuv_quads_in_quad_list > 1 && !has_protected_video_or_texture_overlays) {
    if (no_undamaged_overlay_promotion_) {
      if (damaged_yuv_quads_in_quad_list == processed_yuv_overlay_count_) {
        frames_since_last_qualified_multi_overlays_++;
      } else {
        frames_since_last_qualified_multi_overlays_ = 0;
      }
      reject_overlays = frames_since_last_qualified_multi_overlays_ <=
                        kDCLayerFramesDelayedBeforeOverlay;
    } else {
      if (yuv_quads_in_quad_list != processed_yuv_overlay_count_)
        reject_overlays = true;
    }
  }

  // A YUV quad might be rejected later due to not allowed as an underlay.
  // Recount the YUV overlays when they are added to the overlay list
  // successfully.
  processed_yuv_overlay_count_ = 0;

  if (base::FeatureList::IsEnabled(features::kDisableVideoOverlayIfMoving)) {
    RemoveClearVideoQuadCandidatesIfMoving(quad_list, candidates);
  }

  // Copy the overlay quad info to dc_layer_overlays and replace/delete overlay
  // quads in quad_list.
  for (auto& it : candidates) {
    if (reject_overlays) {
      RecordDCLayerResult(DC_LAYER_FAILED_TOO_MANY_OVERLAYS, it);
      continue;
    }

    // Do not promote undamaged video to overlays.
    bool undamaged =
        it->shared_quad_state->overlay_damage_index.has_value() &&
        surface_damage_rect_list_[it->shared_quad_state->overlay_damage_index
                                      .value()]
            .IsEmpty();

    if (yuv_quads_in_quad_list > allowed_yuv_overlay_count_ &&
        !has_protected_video_or_texture_overlays && undamaged &&
        no_undamaged_overlay_promotion_ &&
        it->material == DrawQuad::Material::kYuvVideoContent) {
      RecordDCLayerResult(DC_LAYER_FAILED_NOT_DAMAGED, it);
      continue;
    }

    gfx::Rect quad_rectangle_in_target_space = ClippedQuadRectangle(*it);

    // Quad is considered an "overlay" if it has no occluders.
    bool is_overlay = !IsOccluded(gfx::RectF(quad_rectangle_in_target_space),
                                  quad_list->begin(), it, render_pass_filters);

    // Protected video is always put in an overlay, but texture quads can be
    // skipped if they're not underlay compatible.
    const bool requires_overlay = IsProtectedVideo(it);

    // TODO(magchen@): Since we reject underlays here, the max number of YUV
    // overlays we can promote might not be accurate. We should allow all YUV
    // quads to be put into candidate_index_list, but only
    // |allowed_yuv_overlay_count_| YUV quads should be promoted to
    // overlays/underlays from that list.

    // Skip quad if it's an underlay and underlays are not allowed.
    if (!is_overlay && !requires_overlay) {
      DCLayerResult result = IsUnderlayAllowed(it);

      if (result != DC_LAYER_SUCCESS) {
        RecordDCLayerResult(result, it);
        continue;
      }
    }

    gfx::Rect quad_rectangle_in_root_space =
        cc::MathUtil::MapEnclosingClippedRect(
            render_pass->transform_to_root_target,
            quad_rectangle_in_target_space);

    // Used by a histogram.
    this_frame_has_occluding_damage_rect =
        !is_overlay &&
        HasOccludingDamageRect(it->shared_quad_state, surface_damage_rect_list_,
                               quad_rectangle_in_root_space);

    UpdateDCLayerOverlays(resource_provider, display_rect, render_pass, it,
                          quad_rectangle_in_root_space, is_overlay, damage_rect,
                          dc_layer_overlays, is_page_fullscreen_mode);
  }

  // Update previous frame state after processing root pass. If there is no
  // overlay in this frame, previous_frame_overlay_rect_union will be added
  // to the damage_rect here for GL composition because the overlay image from
  // the previous frame is missing in the GL composition path. If any overlay is
  // found in this frame, the previous overlay rects would have been handled
  // above and previous_frame_overlay_rect_union becomes empty.
  UpdateRootDamageRect(display_rect, damage_rect);

  std::swap(previous_frame_overlay_rects_, current_frame_overlay_rects_);
  current_frame_overlay_rects_.clear();
  previous_display_rect_ = display_rect;

  if (processed_yuv_overlay_count_ > 0) {
    base::UmaHistogramExactLinear(
        "GPU.DirectComposition.DCLayer.YUVOverlayCount",
        /*sample=*/processed_yuv_overlay_count_,
        /*value_max=*/10);

    RecordOverlayHistograms(dc_layer_overlays,
                            this_frame_has_occluding_damage_rect, damage_rect);
  }
}

bool DCLayerOverlayProcessor::ShouldSkipOverlay(
    AggregatedRenderPass* render_pass,
    bool is_video_capture_enabled) {
  QuadList* quad_list = &render_pass->quad_list;

  // Skip overlay processing if we have copy request or video capture is
  // enabled. When video capture is enabled, some frames might not have copy
  // request.
  if (!render_pass->copy_requests.empty() || is_video_capture_enabled) {
    // Find a valid overlay candidate from quad_list.
    QuadList::Iterator it = FindAnOverlayCandidate(*quad_list);
    if (it != quad_list->end()) {
      is_video_capture_enabled
          ? RecordDCLayerResult(DC_LAYER_FAILED_VIDEO_CAPTURE_ENABLED, it)
          : RecordDCLayerResult(DC_LAYER_FAILED_COPY_REQUESTS, it);
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
        RecordDCLayerResult(DC_LAYER_FAILED_OUTPUT_HDR, it);
        return true;
      }
      // Skip overlay processing if output colorspace is HDR and system HDR is
      // not enabled. Since we always want to use Viz do HDR tone mapping, to
      // avoid a visual difference between Viz and video processor, do not allow
      // overlay.
      if (!system_hdr_enabled_) {
        RecordDCLayerResult(DC_LAYER_FAILED_HDR_TONE_MAPPING, it);
        return true;
      }
    }
  }

  return false;
}

void DCLayerOverlayProcessor::UpdateDCLayerOverlays(
    DisplayResourceProvider* resource_provider,
    const gfx::RectF& display_rect,
    AggregatedRenderPass* render_pass,
    const QuadList::Iterator& it,
    const gfx::Rect& quad_rectangle_in_root_space,
    bool is_overlay,
    gfx::Rect* damage_rect,
    OverlayCandidateList* dc_layer_overlays,
    bool is_page_fullscreen_mode) {
  // Record the result first before ProcessForOverlay().
  RecordDCLayerResult(DC_LAYER_SUCCESS, it);

  OverlayCandidate dc_layer;
  dc_layer.possible_video_fullscreen_letterboxing =
      is_page_fullscreen_mode
          ? IsPossibleFullScreenLetterboxing(it, render_pass->quad_list.end(),
                                             display_rect)
          : false;
  switch (it->material) {
    case DrawQuad::Material::kYuvVideoContent:
      FromYUVQuad(YUVVideoDrawQuad::MaterialCast(*it),
                  render_pass->transform_to_root_target, &dc_layer);
      processed_yuv_overlay_count_++;
      break;
    case DrawQuad::Material::kTextureContent: {
      const TextureDrawQuad* tex_quad = TextureDrawQuad::MaterialCast(*it);
      FromTextureQuad(tex_quad, render_pass->transform_to_root_target,
                      resource_provider, &dc_layer);
      auto si_format =
          resource_provider->GetSharedImageFormat(tex_quad->resource_id());
      if (media::IsMultiPlaneFormatForHardwareVideoEnabled() &&
          si_format.is_multi_plane()) {
        processed_yuv_overlay_count_++;
      }
    } break;
    default:
      NOTREACHED();
  }

  // Underlays are less efficient, so attempt regular overlays first. We can
  // only check for occlusion within a render pass.
  if (is_overlay) {
    ProcessForOverlay(display_rect, render_pass, it);
  } else {
    ProcessForUnderlay(display_rect, render_pass, quad_rectangle_in_root_space,
                       it, dc_layer_overlays->size(), damage_rect, &dc_layer);
  }

  current_frame_overlay_rects_.push_back(
      {quad_rectangle_in_root_space, is_overlay});
  dc_layer_overlays->push_back(dc_layer);

  // Recorded for each overlay.
  UMA_HISTOGRAM_BOOLEAN("GPU.DirectComposition.IsUnderlay", !is_overlay);
}

void DCLayerOverlayProcessor::ProcessForOverlay(
    const gfx::RectF& display_rect,
    AggregatedRenderPass* render_pass,
    const QuadList::Iterator& it) {
  // The quad is on top, so promote it to an overlay and remove all damage
  // underneath it.
  const bool display_rect_changed = (display_rect != previous_display_rect_);
  const bool is_axis_aligned = it->shared_quad_state->quad_to_target_transform
                                   .Preserves2dAxisAlignment();
  const bool needs_blending = it->ShouldDrawWithBlending();

  if (is_axis_aligned && !display_rect_changed && !needs_blending) {
    RemoveOverlayDamageRect(it);
  }

  // Overlay quads should not be drawn. Removing the quads from the quad list
  // creates extra complexity since we would be traversing the list while
  // removing them. Instead, we can just make the visible rect empty, which
  // would then be skipped by DirectRenderer::ShouldSkipQuad.
  it->visible_rect = gfx::Rect();
}

void DCLayerOverlayProcessor::ProcessForUnderlay(
    const gfx::RectF& display_rect,
    AggregatedRenderPass* render_pass,
    const gfx::Rect& quad_rectangle,
    const QuadList::Iterator& it,
    size_t processed_overlay_count,
    gfx::Rect* damage_rect,
    OverlayCandidate* dc_layer) {
  // Assign decreasing z-order so that underlays processed earlier, and hence
  // which are above the subsequent underlays, are placed above in the direct
  // composition visual tree.
  dc_layer->plane_z_order = -1 - processed_overlay_count;

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

  const bool display_rect_unchanged = (display_rect == previous_display_rect_);
  const bool underlay_rect_unchanged =
      IsPreviousFrameUnderlayRect(quad_rectangle, processed_overlay_count);
  const bool is_axis_aligned = it->shared_quad_state->quad_to_target_transform
                                   .Preserves2dAxisAlignment();
  bool opacity_unchanged = (is_opaque == previous_frame_underlay_is_opaque_);
  previous_frame_underlay_is_opaque_ = is_opaque;

  if (is_axis_aligned && opacity_unchanged && underlay_rect_unchanged &&
      display_rect_unchanged) {
    // If this underlay rect is the same as for last frame, Remove its area
    // from the damage of the main surface, as the cleared area was already
    // cleared last frame.

    // If none of the quads on top give any damage, we can skip compositing
    // these quads. The output root damage rect might be empty after we remove
    // the damage from the video quad. We can save power if the root damage
    // rect is empty.
    RemoveOverlayDamageRect(it);
  } else {
    // Entire replacement quad must be redrawn.
    damage_rect->Union(quad_rectangle);
    surface_damage_rect_list_.push_back(quad_rectangle);
  }
}

}  // namespace viz
