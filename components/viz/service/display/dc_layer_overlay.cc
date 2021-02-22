// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/dc_layer_overlay.h"

#include <limits>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gpu_switching_manager.h"

namespace viz {

namespace {

constexpr int kDCLayerDebugBorderWidth = 4;
constexpr gfx::Insets kDCLayerDebugBorderInsets = gfx::Insets(-2);

// This is used for a histogram to determine why overlays are or aren't used,
// so don't remove entries and make sure to update enums.xml if it changes.
enum DCLayerResult {
  DC_LAYER_SUCCESS,
  DC_LAYER_FAILED_UNSUPPORTED_QUAD,  // not recorded
  DC_LAYER_FAILED_QUAD_BLEND_MODE,
  DC_LAYER_FAILED_TEXTURE_NOT_CANDIDATE,
  DC_LAYER_FAILED_OCCLUDED,
  DC_LAYER_FAILED_COMPLEX_TRANSFORM,
  DC_LAYER_FAILED_TRANSPARENT,
  DC_LAYER_FAILED_NON_ROOT,
  DC_LAYER_FAILED_TOO_MANY_OVERLAYS,
  DC_LAYER_FAILED_NO_HW_OVERLAY_SUPPORT,  // deprecated
  DC_LAYER_FAILED_ROUNDED_CORNERS,
  DC_LAYER_FAILED_BACKDROP_FILTERS,
  kMaxValue = DC_LAYER_FAILED_BACKDROP_FILTERS,
};

enum : size_t {
  kTextureResourceIndex = 0,
  kYPlaneResourceIndex = 0,
  kUVPlaneResourceIndex = 1,
};

// This returns the smallest rectangle in target space that contains the quad.
gfx::RectF ClippedQuadRectangle(const DrawQuad* quad) {
  gfx::RectF quad_rect = cc::MathUtil::MapClippedRect(
      quad->shared_quad_state->quad_to_target_transform,
      gfx::RectF(quad->rect));
  if (quad->shared_quad_state->is_clipped)
    quad_rect.Intersect(gfx::RectF(quad->shared_quad_state->clip_rect));
  return quad_rect;
}

gfx::RectF GetExpandedRectWithPixelMovingFilter(
    const AggregatedRenderPassDrawQuad* rpdq,
    float max_pixel_movement) {
  const SharedQuadState* shared_quad_state = rpdq->shared_quad_state;
  gfx::RectF expanded_rect(rpdq->rect);
  expanded_rect.Inset(-max_pixel_movement, -max_pixel_movement);

  // expanded_rect in the target space
  return cc::MathUtil::MapClippedRect(
      shared_quad_state->quad_to_target_transform, expanded_rect);
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

  // Rounded corner on overlays are not supported.
  if (quad->shared_quad_state->mask_filter_info.HasRoundedCorners())
    return DC_LAYER_FAILED_ROUNDED_CORNERS;

  auto quad_target_rect = gfx::ToEnclosingRect(ClippedQuadRectangle(quad));
  for (const auto& filter_target_rect : backdrop_filter_rects) {
    if (filter_target_rect.Intersects(quad_target_rect))
      return DC_LAYER_FAILED_BACKDROP_FILTERS;
  }

  return DC_LAYER_SUCCESS;
}

void FromYUVQuad(const YUVVideoDrawQuad* quad,
                 const gfx::Transform& transform_to_root_target,
                 DCLayerOverlay* dc_layer) {
  // Direct composition path only supports single NV12 buffer, or two buffers
  // one each for Y and UV planes.
  DCHECK(quad->y_plane_resource_id() && quad->u_plane_resource_id());
  DCHECK_EQ(quad->u_plane_resource_id(), quad->v_plane_resource_id());
  dc_layer->resources[kYPlaneResourceIndex] = quad->y_plane_resource_id();
  dc_layer->resources[kUVPlaneResourceIndex] = quad->u_plane_resource_id();

  dc_layer->z_order = 1;
  dc_layer->content_rect = gfx::ToNearestRect(quad->ya_tex_coord_rect);
  dc_layer->quad_rect = quad->rect;
  // Quad rect is in quad content space so both quad to target, and target to
  // root transforms must be applied to it.
  gfx::Transform quad_to_root_transform(
      quad->shared_quad_state->quad_to_target_transform);
  quad_to_root_transform.ConcatTransform(transform_to_root_target);
  // Flatten transform to 2D since DirectComposition doesn't support 3D
  // transforms.  This only applies when non axis aligned overlays are enabled.
  quad_to_root_transform.FlattenTo2d();
  dc_layer->transform = quad_to_root_transform;

  dc_layer->is_clipped = quad->shared_quad_state->is_clipped;
  if (dc_layer->is_clipped) {
    // Clip rect is in quad target space, and must be transformed to root target
    // space.
    gfx::RectF clip_rect = gfx::RectF(quad->shared_quad_state->clip_rect);
    transform_to_root_target.TransformRect(&clip_rect);
    dc_layer->clip_rect = gfx::ToEnclosingRect(clip_rect);
  }
  dc_layer->color_space = quad->video_color_space;
  dc_layer->protected_video_type = quad->protected_video_type;
  dc_layer->hdr_metadata = quad->hdr_metadata;
}

DCLayerResult ValidateTextureQuad(
    const TextureDrawQuad* quad,
    const std::vector<gfx::Rect>& backdrop_filter_rects,
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

  // Rounded corner on overlays are not supported.
  if (quad->shared_quad_state->mask_filter_info.HasRoundedCorners())
    return DC_LAYER_FAILED_ROUNDED_CORNERS;

  auto quad_target_rect = gfx::ToEnclosingRect(ClippedQuadRectangle(quad));
  for (const auto& filter_target_rect : backdrop_filter_rects) {
    if (filter_target_rect.Intersects(quad_target_rect))
      return DC_LAYER_FAILED_BACKDROP_FILTERS;
  }

  return DC_LAYER_SUCCESS;
}

void FromTextureQuad(const TextureDrawQuad* quad,
                     const gfx::Transform& transform_to_root_target,
                     DCLayerOverlay* dc_layer) {
  dc_layer->resources[kTextureResourceIndex] = quad->resource_id();
  dc_layer->z_order = 1;
  dc_layer->content_rect = gfx::Rect(quad->resource_size_in_pixels());
  dc_layer->quad_rect = quad->rect;
  // Quad rect is in quad content space so both quad to target, and target to
  // root transforms must be applied to it.
  gfx::Transform quad_to_root_transform;
  if (quad->y_flipped) {
    quad_to_root_transform.Scale(1.0, -1.0);
    quad_to_root_transform.PostTranslate(0.0, dc_layer->content_rect.height());
  }
  quad_to_root_transform.ConcatTransform(
      quad->shared_quad_state->quad_to_target_transform);
  quad_to_root_transform.ConcatTransform(transform_to_root_target);
  // Flatten transform to 2D since DirectComposition doesn't support 3D
  // transforms.  This only applies when non axis aligned overlays are enabled.
  quad_to_root_transform.FlattenTo2d();
  dc_layer->transform = quad_to_root_transform;

  dc_layer->is_clipped = quad->shared_quad_state->is_clipped;
  if (dc_layer->is_clipped) {
    // Clip rect is in quad target space, and must be transformed to root target
    // space.
    gfx::RectF clip_rect = gfx::RectF(quad->shared_quad_state->clip_rect);
    transform_to_root_target.TransformRect(&clip_rect);
    dc_layer->clip_rect = gfx::ToEnclosingRect(clip_rect);
  }
  dc_layer->color_space = gfx::ColorSpace::CreateSRGB();
}

bool IsProtectedVideo(const QuadList::Iterator& it) {
  if (it->material == DrawQuad::Material::kYuvVideoContent) {
    const auto* yuv_quad = YUVVideoDrawQuad::MaterialCast(*it);
    return yuv_quad->protected_video_type ==
               gfx::ProtectedVideoType::kHardwareProtected ||
           yuv_quad->protected_video_type ==
               gfx::ProtectedVideoType::kSoftwareProtected;
  }
  return false;
}

DCLayerResult IsUnderlayAllowed(const QuadList::Iterator& it) {
  if (it->ShouldDrawWithBlending()) {
    return DC_LAYER_FAILED_TRANSPARENT;
  }
  return DC_LAYER_SUCCESS;
}

// Any occluding quads in the quad list on top of the overlay/underlay
bool HasOccludingQuads(
    const gfx::RectF& target_quad,
    QuadList::ConstIterator quad_list_begin,
    QuadList::ConstIterator quad_list_end,
    const DCLayerOverlayProcessor::FilterOperationsMap& render_pass_filters) {
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
        float max_pixel_movement = filters->MaximumPixelMovement();
        overlap_rect =
            GetExpandedRectWithPixelMovingFilter(rpdq, max_pixel_movement);
        has_pixel_moving_filter = true;
      }
    }

    if (!has_pixel_moving_filter)
      overlap_rect = ClippedQuadRectangle(quad);

    if (quad->material == DrawQuad::Material::kSolidColor) {
      SkColor color = SolidColorDrawQuad::MaterialCast(quad)->color;
      float alpha = (SkColorGetA(color) * (1.0f / 255.0f)) * opacity;
      if (quad->ShouldDrawWithBlending() &&
          alpha < std::numeric_limits<float>::epsilon())
        continue;
    }
    if (overlap_rect.Intersects(target_quad))
      return true;
  }
  return false;
}

gfx::Rect CalculateOccludingDamageRect(
    const SharedQuadState* shared_quad_state,
    const SurfaceDamageRectList& surface_damage_rect_list,
    const gfx::Rect& quad_rect_in_root_target_space) {
  if (!shared_quad_state->overlay_damage_index.has_value())
    return quad_rect_in_root_target_space;

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
  occluding_damage_rect.Intersect(quad_rect_in_root_target_space);

  return occluding_damage_rect;
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

void RecordDCLayerResult(DCLayerResult result, QuadList::Iterator it) {
  // Skip recording unsupported quads since that'd dwarf the data we care about.
  if (result == DC_LAYER_FAILED_UNSUPPORTED_QUAD)
    return;

  switch (it->material) {
    case DrawQuad::Material::kYuvVideoContent:
      RecordVideoDCLayerResult(
          result, YUVVideoDrawQuad::MaterialCast(*it)->protected_video_type);
      break;
    case DrawQuad::Material::kTextureContent:
      UMA_HISTOGRAM_ENUMERATION("GPU.DirectComposition.DCLayerResult.Texture",
                                result);
      break;
    default:
      break;
  }
}

// This function records the damage rect rect of the current frame.
void RecordOverlayHistograms(DCLayerOverlayList* dc_layer_overlays,
                             bool has_occluding_surface_damage,
                             const gfx::Rect* damage_rect) {
  // If an underlay is found, we record the damage rect of this frame as an
  // underlay.
  bool is_overlay = true;
  for (const auto& dc_layer : *dc_layer_overlays) {
    if (dc_layer.z_order != 1) {
      is_overlay = false;
      break;
    }
  }

  OverlayProcessorInterface::RecordOverlayDamageRectHistograms(
      is_overlay, has_occluding_surface_damage, damage_rect->IsEmpty());
}
}  // namespace

DCLayerOverlay::DCLayerOverlay() = default;
DCLayerOverlay::DCLayerOverlay(const DCLayerOverlay& other) = default;
DCLayerOverlay& DCLayerOverlay::operator=(const DCLayerOverlay& other) =
    default;
DCLayerOverlay::~DCLayerOverlay() = default;

DCLayerOverlayProcessor::DCLayerOverlayProcessor(
    const DebugRendererSettings* debug_settings,
    int allowed_yuv_overlay_count,
    bool skip_initialization_for_testing)
    : has_overlay_support_(skip_initialization_for_testing),
      use_overlay_damage_list_(base::FeatureList::IsEnabled(
          features::kDirectCompositionUseOverlayDamageList)),
      allowed_yuv_overlay_count_(
          use_overlay_damage_list_ ? allowed_yuv_overlay_count : 1),
      debug_settings_(debug_settings),
      viz_task_runner_(skip_initialization_for_testing
                           ? nullptr
                           : base::ThreadTaskRunnerHandle::Get()) {
  if (!skip_initialization_for_testing) {
    UpdateHasHwOverlaySupport();
    ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
  }
}

DCLayerOverlayProcessor::~DCLayerOverlayProcessor() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
}

// Called on the Viz Compositor thread
void DCLayerOverlayProcessor::UpdateHasHwOverlaySupport() {
  DCHECK(viz_task_runner_->BelongsToCurrentThread());
  has_overlay_support_ = gl::AreOverlaysSupportedWin();
}

// Not on the Viz Compositor thread
void DCLayerOverlayProcessor::OnDisplayAdded() {
  viz_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DCLayerOverlayProcessor::UpdateHasHwOverlaySupport,
                     base::Unretained(this)));
}

// Not on the Viz Compositor thread
void DCLayerOverlayProcessor::OnDisplayRemoved() {
  viz_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DCLayerOverlayProcessor::UpdateHasHwOverlaySupport,
                     base::Unretained(this)));
}

void DCLayerOverlayProcessor::ClearOverlayState() {
  previous_frame_underlay_rect_ = gfx::Rect();
  previous_frame_overlay_rect_union_ = gfx::Rect();
  previous_frame_processed_overlay_count_ = 0;
  previous_frame_overlay_rects_.clear();
}

void DCLayerOverlayProcessor::RemoveOverlayDamageRect(
    const QuadList::Iterator& it,
    const gfx::Rect& quad_rectangle,
    const gfx::Rect& occluding_damage_rect,
    gfx::Rect* damage_rect) {
  if (use_overlay_damage_list_) {
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
  } else {
    // This is done by subtract the overlay rect fromt the root damage rect.
    damage_rect->Subtract(quad_rectangle);
    damage_rect->Union(occluding_damage_rect);
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
  if (use_overlay_damage_list_) {
    DCHECK_EQ(static_cast<size_t>(current_frame_processed_overlay_count_),
              current_frame_overlay_rects_.size());
    DCHECK_EQ(static_cast<size_t>(previous_frame_processed_overlay_count_),
              previous_frame_overlay_rects_.size());

    size_t current_frame_overlay_count = current_frame_overlay_rects_.size();
    if (current_frame_overlay_count > 0 &&
        current_frame_overlay_count == previous_frame_overlay_rects_.size() &&
        display_rect == previous_display_rect_) {
      bool same_overlays = true;
      for (size_t i = 0; i < current_frame_overlay_count; ++i) {
        if (previous_frame_overlay_rects_[i] !=
            current_frame_overlay_rects_[i]) {
          same_overlays = false;
          break;
        }
      }

      if (same_overlays) {
        // No need to add back the overlay rect union from the previous frame
        // if no changes in overlays.
        previous_frame_overlay_rect_union_ = gfx::Rect();

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
  } else {
    // If the current overlay has changed in size/position from the previous
    // frame, we have to add the overlay quads from the previous frame to the
    // damage rect for GL compositor. It's hard to optimize multiple overlays.
    // So always add the overlay rects back in this case.
    if (current_frame_processed_overlay_count_ == 1 &&
        previous_frame_processed_overlay_count_ == 1 &&
        current_frame_overlay_rect_union_ ==
            previous_frame_overlay_rect_union_) {
      previous_frame_overlay_rect_union_ = gfx::Rect();
    }
  }

  damage_rect->Union(previous_frame_overlay_rect_union_);
  damage_rect->Intersect(gfx::ToEnclosingRect(display_rect));
}

void DCLayerOverlayProcessor::InsertDebugBorderDrawQuad(
    const DCLayerOverlayList* dc_layer_overlays,
    AggregatedRenderPass* render_pass,
    const gfx::RectF& display_rect,
    gfx::Rect* damage_rect) {
  auto* shared_quad_state = render_pass->CreateAndAppendSharedQuadState();
  auto& quad_list = render_pass->quad_list;

  // Add debug borders for the root damage rect after overlay promotion.
  SkColor border_color = SK_ColorGREEN;
  auto it = quad_list.InsertBeforeAndInvalidateAllPointers<DebugBorderDrawQuad>(
      quad_list.begin(), 1u);
  auto* debug_quad = static_cast<DebugBorderDrawQuad*>(*it);

  gfx::Rect rect = *damage_rect;
  rect.Inset(kDCLayerDebugBorderInsets);
  debug_quad->SetNew(shared_quad_state, rect, rect, border_color,
                     kDCLayerDebugBorderWidth);

  // Add debug borders for overlays/underlays
  for (const auto& dc_layer : *dc_layer_overlays) {
    gfx::RectF overlay_rect(dc_layer.quad_rect);
    dc_layer.transform.TransformRect(&overlay_rect);
    if (dc_layer.is_clipped)
      overlay_rect.Intersect(gfx::RectF(dc_layer.clip_rect));

    // Overlay:red, Underlay:blue.
    SkColor border_color = dc_layer.z_order > 0 ? SK_ColorRED : SK_ColorBLUE;
    auto it =
        quad_list.InsertBeforeAndInvalidateAllPointers<DebugBorderDrawQuad>(
            quad_list.begin(), 1u);
    auto* debug_quad = static_cast<DebugBorderDrawQuad*>(*it);

    gfx::Rect rect = gfx::ToEnclosingRect(overlay_rect);
    rect.Inset(kDCLayerDebugBorderInsets);
    debug_quad->SetNew(shared_quad_state, rect, rect, border_color,
                       kDCLayerDebugBorderWidth);
  }

  // Mark the entire output as damaged because the border quads might not be
  // inside the current damage rect.  It's far simpler to mark the entire output
  // as damaged instead of accounting for individual border quads which can
  // change positions across frames.
  damage_rect->Union(gfx::ToEnclosingRect(display_rect));
}

bool DCLayerOverlayProcessor::IsPreviousFrameUnderlayRect(
    const gfx::Rect& quad_rectangle,
    size_t index) {
  if (use_overlay_damage_list_) {
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
  } else {
    return (quad_rectangle == previous_frame_underlay_rect_) && (index == 0);
  }
}

void DCLayerOverlayProcessor::Process(
    DisplayResourceProvider* resource_provider,
    const gfx::RectF& display_rect,
    const FilterOperationsMap& render_pass_filters,
    const FilterOperationsMap& render_pass_backdrop_filters,
    AggregatedRenderPassList* render_pass_list,
    gfx::Rect* damage_rect,
    SurfaceDamageRectList surface_damage_rect_list,
    DCLayerOverlayList* dc_layer_overlays) {
  gfx::Rect this_frame_underlay_rect;
  bool this_frame_has_occluding_damage_rect = false;
  processed_yuv_overlay_count_ = 0;
  surface_damage_rect_list_ = std::move(surface_damage_rect_list);

  // Output rects of child render passes that have backdrop filters in target
  // space. These rects are used to determine if the overlay rect could be read
  // by backdrop filters.
  std::vector<gfx::Rect> backdrop_filter_rects;

  auto* root_render_pass = render_pass_list->back().get();
  if (render_pass_list->back()->is_color_conversion_pass) {
    DCHECK_GT(render_pass_list->size(), 1u);
    root_render_pass = (*render_pass_list)[render_pass_list->size() - 2].get();
  }

  // Used for generating the candidate index list.
  QuadList* quad_list = &root_render_pass->quad_list;
  std::vector<size_t> candidate_index_list;
  size_t index = 0;

  // Used for looping through candidate_index_list to UpdateDCLayerOverlays()
  size_t prev_index = 0;
  auto prev_it = quad_list->begin();

  // Used for whether overlay should be skipped
  int yuv_quads_in_quad_list = 0;
  bool has_protected_video_or_texture_overlays = false;

  for (auto it = quad_list->begin(); it != quad_list->end(); ++it, ++index) {
    if (it->material == DrawQuad::Material::kAggregatedRenderPass) {
      const auto* rpdq = AggregatedRenderPassDrawQuad::MaterialCast(*it);
      auto render_pass_it =
          render_pass_backdrop_filters.find(rpdq->render_pass_id);
      if (render_pass_it != render_pass_backdrop_filters.end()) {
        backdrop_filter_rects.push_back(
            gfx::ToEnclosingRect(ClippedQuadRectangle(rpdq)));
      }
      continue;
    }

    DCLayerResult result;
    switch (it->material) {
      case DrawQuad::Material::kYuvVideoContent:
        result = ValidateYUVQuad(
            YUVVideoDrawQuad::MaterialCast(*it), backdrop_filter_rects,
            has_overlay_support_, allowed_yuv_overlay_count_,
            processed_yuv_overlay_count_, resource_provider);
        yuv_quads_in_quad_list++;
        if (result == DC_LAYER_SUCCESS)
          processed_yuv_overlay_count_++;
        break;
      case DrawQuad::Material::kTextureContent:
        result = ValidateTextureQuad(TextureDrawQuad::MaterialCast(*it),
                                     backdrop_filter_rects, resource_provider);
        break;
      default:
        result = DC_LAYER_FAILED_UNSUPPORTED_QUAD;
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

    if (candidate_index_list.size() == 0) {
      prev_index = index;
      prev_it = it;
    }

    candidate_index_list.push_back(index);
  }

  // We might not save power if there are more than one videos and only part of
  // them are promoted to overlay. Skip overlays for this frame unless there are
  // protected video or texture overlays.
  bool reject_overlays = false;
  if (yuv_quads_in_quad_list != processed_yuv_overlay_count_ &&
      !has_protected_video_or_texture_overlays) {
    reject_overlays = true;
  }

  // A YUV quad might be rejected later due to not allowed as an underlay.
  // Recount the YUV overlays when they are added to the overlay list
  // successfully.
  processed_yuv_overlay_count_ = 0;

  // Copy the overlay quad info to dc_layer_overlays and replace/delete overlay
  // quads in quad_list.
  for (auto index : candidate_index_list) {
    auto it = std::next(prev_it, index - prev_index);
    prev_it = it;
    prev_index = index;

    if (reject_overlays) {
      RecordDCLayerResult(DC_LAYER_FAILED_TOO_MANY_OVERLAYS, it);
      continue;
    }
    gfx::Rect quad_rectangle_in_target_space =
        gfx::ToEnclosingRect(ClippedQuadRectangle(*it));

    // Quad is considered an "overlay" if it has no occluders.
    bool is_overlay =
        !HasOccludingQuads(gfx::RectF(quad_rectangle_in_target_space),
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

    // Get the occluding damage rect for underlay.
    gfx::Rect occluding_damage_rect;
    if (!is_overlay) {
      occluding_damage_rect = CalculateOccludingDamageRect(
          it->shared_quad_state, surface_damage_rect_list_,
          quad_rectangle_in_target_space);

      // Used by a histogram.
      if (!occluding_damage_rect.IsEmpty())
        this_frame_has_occluding_damage_rect = true;
    }

    UpdateDCLayerOverlays(
        display_rect, root_render_pass, it, quad_rectangle_in_target_space,
        occluding_damage_rect, is_overlay, &prev_it, &prev_index,
        &this_frame_underlay_rect, damage_rect, dc_layer_overlays);
  }

  // Update previous frame state after processing root pass. If there is no
  // overlay in this frame, |previous_frame_overlay_rect_union_| will be added
  // to the damage_rect here for GL composition because the overlay image from
  // the previous frame is missing in the GL composition path. If any overlay is
  // found in this frame, the previous overlay rects would have been handled
  // above and |previous_frame_overlay_rect_union_| becomes empty.
  UpdateRootDamageRect(display_rect, damage_rect);

  damages_to_be_removed_.clear();
  previous_frame_overlay_rect_union_ = current_frame_overlay_rect_union_;
  current_frame_overlay_rect_union_ = gfx::Rect();
  previous_frame_processed_overlay_count_ =
      current_frame_processed_overlay_count_;
  current_frame_processed_overlay_count_ = 0;
  std::swap(previous_frame_overlay_rects_, current_frame_overlay_rects_);
  current_frame_overlay_rects_.clear();
  previous_display_rect_ = display_rect;
  previous_frame_underlay_rect_ = this_frame_underlay_rect;

  if (!dc_layer_overlays->empty()) {
    base::UmaHistogramExactLinear(
        "GPU.DirectComposition.DCLayer.YUVOverlayCount",
        /*sample=*/processed_yuv_overlay_count_,
        /*value_max=*/10);

    RecordOverlayHistograms(dc_layer_overlays,
                            this_frame_has_occluding_damage_rect, damage_rect);
  }

  if (debug_settings_->show_dc_layer_debug_borders) {
    InsertDebugBorderDrawQuad(dc_layer_overlays, root_render_pass, display_rect,
                              damage_rect);
  }
}

void DCLayerOverlayProcessor::UpdateDCLayerOverlays(
    const gfx::RectF& display_rect,
    AggregatedRenderPass* render_pass,
    const QuadList::Iterator& it,
    const gfx::Rect& quad_rectangle_in_target_space,
    const gfx::Rect& occluding_damage_rect,
    bool is_overlay,
    QuadList::Iterator* new_it,
    size_t* new_index,
    gfx::Rect* this_frame_underlay_rect,
    gfx::Rect* damage_rect,
    DCLayerOverlayList* dc_layer_overlays) {
  // Record the result first before ProcessForOverlay().
  RecordDCLayerResult(DC_LAYER_SUCCESS, it);

  DCLayerOverlay dc_layer;
  switch (it->material) {
    case DrawQuad::Material::kYuvVideoContent:
      FromYUVQuad(YUVVideoDrawQuad::MaterialCast(*it),
                  render_pass->transform_to_root_target, &dc_layer);
      processed_yuv_overlay_count_++;
      break;
    case DrawQuad::Material::kTextureContent:
      FromTextureQuad(TextureDrawQuad::MaterialCast(*it),
                      render_pass->transform_to_root_target, &dc_layer);
      break;
    default:
      NOTREACHED();
  }

  // Underlays are less efficient, so attempt regular overlays first. Only
  // check root render pass because we can only check for occlusion within a
  // render pass.
  if (is_overlay) {
    *new_it =
        ProcessForOverlay(display_rect, render_pass,
                          quad_rectangle_in_target_space, it, damage_rect);
    (*new_index)++;
  } else {
    ProcessForUnderlay(display_rect, render_pass,
                       quad_rectangle_in_target_space, occluding_damage_rect,
                       it, damage_rect, this_frame_underlay_rect, &dc_layer);
  }

  gfx::Rect rect_in_root = cc::MathUtil::MapEnclosingClippedRect(
      render_pass->transform_to_root_target, quad_rectangle_in_target_space);
  current_frame_overlay_rect_union_.Union(rect_in_root);
  current_frame_overlay_rects_.push_back({rect_in_root, is_overlay});

  dc_layer_overlays->push_back(dc_layer);

  // Recorded for each overlay.
  UMA_HISTOGRAM_BOOLEAN("GPU.DirectComposition.IsUnderlay", !is_overlay);

  current_frame_processed_overlay_count_++;
}

QuadList::Iterator DCLayerOverlayProcessor::ProcessForOverlay(
    const gfx::RectF& display_rect,
    AggregatedRenderPass* render_pass,
    const gfx::Rect& quad_rectangle,
    const QuadList::Iterator& it,
    gfx::Rect* damage_rect) {
  // The quad is on top, so promote it to an overlay and remove all damage
  // underneath it.
  const bool display_rect_changed = (display_rect != previous_display_rect_);
  const bool is_axis_aligned = it->shared_quad_state->quad_to_target_transform
                                   .Preserves2dAxisAlignment();
  const bool needs_blending = it->ShouldDrawWithBlending();

  if (is_axis_aligned && !display_rect_changed && !needs_blending) {
    RemoveOverlayDamageRect(it, quad_rectangle,
                            /*occluding_damage_rect=*/gfx::Rect(), damage_rect);
  }

  return render_pass->quad_list.EraseAndInvalidateAllPointers(it);
}

void DCLayerOverlayProcessor::ProcessForUnderlay(
    const gfx::RectF& display_rect,
    AggregatedRenderPass* render_pass,
    const gfx::Rect& quad_rectangle,
    const gfx::Rect& occluding_damage_rect,
    const QuadList::Iterator& it,
    gfx::Rect* damage_rect,
    gfx::Rect* this_frame_underlay_rect,
    DCLayerOverlay* dc_layer) {
  // Assign decreasing z-order so that underlays processed earlier, and hence
  // which are above the subsequent underlays, are placed above in the direct
  // composition visual tree.
  dc_layer->z_order = -1 - current_frame_processed_overlay_count_;

  const SharedQuadState* shared_quad_state = it->shared_quad_state;
  const gfx::Rect rect = it->visible_rect;
  const bool needs_blending = it->needs_blending;

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
  bool is_opaque = false;

  if (it->ShouldDrawWithBlending() &&
      shared_quad_state->blend_mode == SkBlendMode::kSrcOver) {
    SharedQuadState* new_shared_quad_state =
        render_pass->shared_quad_state_list.AllocateAndCopyFrom(
            shared_quad_state);
    new_shared_quad_state->blend_mode = SkBlendMode::kDstOut;

    auto* replacement =
        render_pass->quad_list.ReplaceExistingElement<SolidColorDrawQuad>(it);
    // Use needs_blending from original quad because blending might be because
    // of this flag or opacity.
    replacement->SetAll(new_shared_quad_state, rect, rect, needs_blending,
                        SK_ColorBLACK, true /* force_anti_aliasing_off */);
  } else {
    // When the opacity == 1.0, drawing with transparent will be done without
    // blending and will have the proper effect of completely clearing the
    // layer.
    render_pass->ReplaceExistingQuadWithOpaqueTransparentSolidColor(it);
    is_opaque = true;
  }

  bool display_rect_changed = (display_rect != previous_display_rect_);
  bool underlay_rect_unchanged = IsPreviousFrameUnderlayRect(
      quad_rectangle, current_frame_processed_overlay_count_);
  bool is_axis_aligned =
      shared_quad_state->quad_to_target_transform.Preserves2dAxisAlignment();

  if (is_axis_aligned && is_opaque && underlay_rect_unchanged &&
      !display_rect_changed) {
    // If this underlay rect is the same as for last frame, Remove its area
    // from the damage of the main surface, as the cleared area was already
    // cleared last frame.

    // If none of the quads on top give any damage, we can skip compositing
    // these quads. The output root damage rect might be empty after we remove
    // the damage from the video quad. We can save power if the root damage
    // rect is empty.
    RemoveOverlayDamageRect(it, quad_rectangle, occluding_damage_rect,
                            damage_rect);
  } else {
    // Entire replacement quad must be redrawn.
    damage_rect->Union(quad_rectangle);
    surface_damage_rect_list_.push_back(quad_rectangle);
  }

  // We only compare current frame's first underlay with the previous frame's
  // first underlay. Non-opaque regions can have different alpha from one frame
  // to another so this optimization doesn't work.
  if (current_frame_processed_overlay_count_ == 0 && is_axis_aligned &&
      is_opaque) {
    *this_frame_underlay_rect = quad_rectangle;
  }
}

}  // namespace viz
