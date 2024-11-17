// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_candidate_factory.h"

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/viz_utils.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/overlay_transform_utils.h"
#include "ui/gfx/video_types.h"

namespace viz {

namespace {

const SharedImageFormat kOverlayFormats[] = {
    SinglePlaneFormat::kRGBX_8888, SinglePlaneFormat::kRGBA_8888,
    SinglePlaneFormat::kBGRX_8888, SinglePlaneFormat::kBGRA_8888,
    SinglePlaneFormat::kBGR_565,   MultiPlaneFormat::kNV12,
    MultiPlaneFormat::kP010};

enum Axis { NONE, AXIS_POS_X, AXIS_NEG_X, AXIS_POS_Y, AXIS_NEG_Y };

Axis VectorToAxis(const gfx::Vector3dF& vec) {
  if (!cc::MathUtil::IsWithinEpsilon(vec.z(), 0.f))
    return NONE;
  const bool x_zero = cc::MathUtil::IsWithinEpsilon(vec.x(), 0.f);
  const bool y_zero = cc::MathUtil::IsWithinEpsilon(vec.y(), 0.f);
  if (x_zero && !y_zero)
    return (vec.y() > 0.f) ? AXIS_POS_Y : AXIS_NEG_Y;
  else if (y_zero && !x_zero)
    return (vec.x() > 0.f) ? AXIS_POS_X : AXIS_NEG_X;
  else
    return NONE;
}

gfx::OverlayTransform GetOverlayTransform(const gfx::Transform& quad_transform,
                                          bool y_flipped,
                                          bool supports_flip_rotate_transform) {
  if (!quad_transform.Preserves2dAxisAlignment()) {
    return gfx::OVERLAY_TRANSFORM_INVALID;
  }

  gfx::Vector3dF x_axis = cc::MathUtil::GetXAxis(quad_transform);
  gfx::Vector3dF y_axis = cc::MathUtil::GetYAxis(quad_transform);
  if (y_flipped) {
    y_axis.Scale(-1.f);
  }

  Axis x_to = VectorToAxis(x_axis);
  Axis y_to = VectorToAxis(y_axis);

  if (x_to == AXIS_POS_X && y_to == AXIS_POS_Y) {
    return gfx::OVERLAY_TRANSFORM_NONE;
  } else if (x_to == AXIS_NEG_X && y_to == AXIS_POS_Y) {
    return gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL;
  } else if (x_to == AXIS_POS_X && y_to == AXIS_NEG_Y) {
    return gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL;
  } else if (x_to == AXIS_NEG_Y && y_to == AXIS_POS_X) {
    return gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270;
  } else if (x_to == AXIS_NEG_X && y_to == AXIS_NEG_Y) {
    return gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180;
  } else if (x_to == AXIS_POS_Y && y_to == AXIS_NEG_X) {
    return gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90;
  } else if (supports_flip_rotate_transform) {
    if (x_to == AXIS_POS_Y && y_to == AXIS_POS_X) {
      return gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_90;
    } else if (x_to == AXIS_NEG_Y && y_to == AXIS_NEG_X) {
      return gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_270;
    }
    return gfx::OVERLAY_TRANSFORM_INVALID;
  } else {
    return gfx::OVERLAY_TRANSFORM_INVALID;
  }
}

constexpr double kEpsilon = 0.0001;

// Determine why the transformation isn't axis aligned. A transform with z
// components or perspective would require a full 4x4 matrix to delegate, a
// transform with a shear component would require a 2x2 matrix to delegate, and
// a 2d rotation transform could be delegated with an angle.
// This is only useful for delegated compositing.
OverlayCandidate::CandidateStatus GetReasonForTransformNotAxisAligned(
    const gfx::Transform& transform) {
  if (transform.HasPerspective() || !transform.IsFlat())
    return OverlayCandidate::CandidateStatus::kFailNotAxisAligned3dTransform;

  // The transform has a shear component if the x and y sub-vectors are not
  // perpendicular (have a non-zero dot product).
  gfx::Vector2dF x_part(transform.rc(0, 0), transform.rc(1, 0));
  gfx::Vector2dF y_part(transform.rc(0, 1), transform.rc(1, 1));
  // Normalize to avoid numerical issues.
  x_part.InvScale(x_part.Length());
  y_part.InvScale(y_part.Length());
  if (std::abs(gfx::DotProduct(x_part, y_part)) > kEpsilon)
    return OverlayCandidate::CandidateStatus::kFailNotAxisAligned2dShear;

  return OverlayCandidate::CandidateStatus::kFailNotAxisAligned2dRotation;
}

// Returns true if the overlay candidate bounds rect overlap with at least one
// of the rounded corners bounding rects.
bool ShouldApplyRoundedCorner(OverlayCandidate& candidate,
                              const DrawQuad* quad) {
  const gfx::RectF target_rect =
      OverlayCandidate::DisplayRectInTargetSpace(candidate);
  return QuadRoundedCornersBoundsIntersects(quad, target_rect);
}

bool RequiresBlendingForReasonOtherThanRoundedCorners(const DrawQuad* quad) {
  return quad->ShouldDrawWithBlendingForReasonOtherThanMaskFilter() ||
         quad->shared_quad_state->mask_filter_info.HasGradientMask();
}

}  // namespace

OverlayCandidateFactory::OverlayContext::OverlayContext() = default;
OverlayCandidateFactory::OverlayContext::OverlayContext(const OverlayContext&) =
    default;

OverlayCandidate::CandidateStatus OverlayCandidateFactory::FromDrawQuad(
    const DrawQuad* quad,
    OverlayCandidate& candidate) const {
  // It is currently not possible to set a color conversion matrix on an HW
  // overlay plane.
  // TODO(https://crbug.com/792757): Remove this check once the bug is resolved.
  if (has_custom_color_matrix_) {
    return CandidateStatus::kFailColorMatrix;
  }

  const SharedQuadState* sqs = quad->shared_quad_state;

  // We don't support an opacity value different than one for an overlay plane.
  // Render pass quads should have their |sqs| opacity integrated directly into
  // their final output buffers.
  if (!context_.is_delegated_context &&
      !cc::MathUtil::IsWithinEpsilon(sqs->opacity, 1.0f)) {
    return CandidateStatus::kFailOpacity;
  }
  candidate.opacity = sqs->opacity;

  // We support only kSrc (no blending) and kSrcOver (blending with premul).
  if (!(sqs->blend_mode == SkBlendMode::kSrc ||
        sqs->blend_mode == SkBlendMode::kSrcOver)) {
    return CandidateStatus::kFailBlending;
  }

  if (sqs->mask_filter_info.HasGradientMask()) {
    return CandidateStatus::kFailMaskFilterNotSupported;
  }

  candidate.requires_overlay = OverlayCandidate::RequiresOverlay(quad);
  candidate.overlay_damage_index =
      sqs->overlay_damage_index.value_or(OverlayCandidate::kInvalidDamageIndex);

  if (sqs->layer_id != 0) {
    static_assert(
        std::is_same<decltype(SharedQuadState::layer_id), uint32_t>::value);
    static_assert(std::is_same<decltype(SharedQuadState::layer_namespace_id),
                               uint32_t>::value);
    candidate.aggregated_layer_id =
        static_cast<uint64_t>(sqs->layer_id) |
        (static_cast<uint64_t>(sqs->layer_namespace_id) << 32);
  }

  auto status = CandidateStatus::kFailQuadNotSupported;
  switch (quad->material) {
    case DrawQuad::Material::kTextureContent:
      status = FromTextureQuad(TextureDrawQuad::MaterialCast(quad), candidate);
      break;
    case DrawQuad::Material::kVideoHole:
      status =
          FromVideoHoleQuad(VideoHoleDrawQuad::MaterialCast(quad), candidate);
      break;
    case DrawQuad::Material::kSolidColor:
      if (context_.is_delegated_context) {
        status = FromSolidColorQuad(SolidColorDrawQuad::MaterialCast(quad),
                                    candidate);
      }
      break;
    case DrawQuad::Material::kAggregatedRenderPass:
      if (context_.is_delegated_context) {
        status = FromAggregateQuad(
            AggregatedRenderPassDrawQuad::MaterialCast(quad), candidate);
      }
      break;
    case DrawQuad::Material::kTiledContent:
      if (context_.is_delegated_context) {
        status = FromTileQuad(TileDrawQuad::MaterialCast(quad), candidate);
      }
      break;
    default:
      break;
  }

  return status;
}

OverlayCandidateFactory::OverlayCandidateFactory(
    const AggregatedRenderPass* render_pass,
    const DisplayResourceProvider* resource_provider,
    const SurfaceDamageRectList* surface_damage_rect_list,
    const SkM44* output_color_matrix,
    const gfx::RectF primary_rect,
    const OverlayProcessorInterface::FilterOperationsMap* render_pass_filters,
    const OverlayContext& context)
    : render_pass_(render_pass),
      resource_provider_(resource_provider),
      surface_damage_rect_list_(surface_damage_rect_list),
      primary_rect_(primary_rect),
      render_pass_filters_(render_pass_filters),
      context_(context) {
  DCHECK(context_.supports_clip_rect || !context_.supports_arbitrary_transform);
  DCHECK(!context_.disable_wire_size_optimization ||
         context_.supports_arbitrary_transform);

  has_custom_color_matrix_ = *output_color_matrix != SkM44();

  // TODO(crbug.com/40224514): Replace this set with a simple ordered linear
  // search when this bug is resolved.
  base::flat_set<size_t> indices_with_quad_damage;
  for (auto* sqs : render_pass_->shared_quad_state_list) {
    // If a |sqs| has a damage index it will only be associated with a single
    // draw quad.
    if (sqs->overlay_damage_index.has_value()) {
      indices_with_quad_damage.insert(sqs->overlay_damage_index.value());
    }
  }

  for (size_t i = 0; i < (*surface_damage_rect_list_).size(); i++) {
    // Add this damage only if it does not correspond to a specific quad.
    // Ideally any damage that we might want to separate out (think overlays)
    // will not end up in this |unassigned_surface_damage_| rect.
    if (!indices_with_quad_damage.contains(i)) {
      unassigned_surface_damage_.Union((*surface_damage_rect_list_)[i]);
    }
  }
}

OverlayCandidateFactory::~OverlayCandidateFactory() = default;

float OverlayCandidateFactory::EstimateVisibleDamage(
    const DrawQuad* quad,
    const OverlayCandidate& candidate,
    QuadList::ConstIterator quad_list_begin,
    QuadList::ConstIterator quad_list_end) const {
  gfx::Rect quad_damage = gfx::ToEnclosingRect(GetDamageEstimate(candidate));
  float occluded_damage_estimate_total = 0.f;
  for (auto overlap_iter = quad_list_begin; overlap_iter != quad_list_end;
       ++overlap_iter) {
    gfx::Rect overlap_rect = gfx::ToEnclosingRect(cc::MathUtil::MapClippedRect(
        overlap_iter->shared_quad_state->quad_to_target_transform,
        gfx::RectF(overlap_iter->rect)));

    // Opaque quad that (partially) occludes this candidate.
    if (!OverlayCandidate::IsInvisibleQuad(*overlap_iter) &&
        !overlap_iter->ShouldDrawWithBlending()) {
      overlap_rect.Intersect(quad_damage);
      occluded_damage_estimate_total += overlap_rect.size().GetArea();
    }
  }
  // In the case of overlapping UI the |occluded_damage_estimate_total| may
  // exceed the |quad|'s damage rect that is in consideration. This is the
  // reason why this computation is an estimate and why we have the max clamping
  // below.
  return std::max(
      0.f, quad_damage.size().GetArea() - occluded_damage_estimate_total);
}

bool OverlayCandidateFactory::IsOccludedByFilteredQuad(
    const OverlayCandidate& candidate,
    QuadList::ConstIterator quad_list_begin,
    QuadList::ConstIterator quad_list_end,
    const base::flat_map<AggregatedRenderPassId,
                         raw_ptr<cc::FilterOperations, CtnExperimental>>&
        render_pass_backdrop_filters) const {
  gfx::RectF target_rect =
      OverlayCandidate::DisplayRectInTargetSpace(candidate);
  for (auto overlap_iter = quad_list_begin; overlap_iter != quad_list_end;
       ++overlap_iter) {
    if (auto* render_pass_draw_quad =
            overlap_iter->DynamicCast<AggregatedRenderPassDrawQuad>()) {
      gfx::RectF overlap_rect = cc::MathUtil::MapClippedRect(
          overlap_iter->shared_quad_state->quad_to_target_transform,
          gfx::RectF(overlap_iter->rect));

      if (target_rect.Intersects(overlap_rect) &&
          render_pass_backdrop_filters.count(
              render_pass_draw_quad->render_pass_id)) {
        return true;
      }
    }
  }
  return false;
}

bool OverlayCandidateFactory::IsOccluded(
    const OverlayCandidate& candidate,
    QuadList::ConstIterator quad_list_begin,
    QuadList::ConstIterator quad_list_end) const {
  // The rects are rounded as they're snapped by the compositor to pixel unless
  // it is AA'ed, in which case, it won't be overlaid.
  gfx::Rect target_rect =
      gfx::ToRoundedRect(OverlayCandidate::DisplayRectInTargetSpace(candidate));

  // Check that no visible quad overlaps the candidate.
  for (auto overlap_iter = quad_list_begin; overlap_iter != quad_list_end;
       ++overlap_iter) {
    gfx::Rect overlap_rect = gfx::ToRoundedRect(cc::MathUtil::MapClippedRect(
        overlap_iter->shared_quad_state->quad_to_target_transform,
        gfx::RectF(overlap_iter->rect)));

    if (!OverlayCandidate::IsInvisibleQuad(*overlap_iter) &&
        target_rect.Intersects(overlap_rect)) {
      return true;
    }
  }
  return false;
}

OverlayCandidate::CandidateStatus OverlayCandidateFactory::FromDrawQuadResource(
    const DrawQuad* quad,
    ResourceId resource_id,
    bool y_flipped,
    OverlayCandidate& candidate) const {
  if (resource_id != kInvalidResourceId &&
      !resource_provider_->IsOverlayCandidate(resource_id)) {
    return CandidateStatus::kFailNotOverlay;
  }

  if (quad->visible_rect.IsEmpty())
    return CandidateStatus::kFailVisible;

  if (resource_id != kInvalidResourceId) {
    candidate.format = resource_provider_->GetSharedImageFormat(resource_id);
    candidate.color_space = resource_provider_->GetColorSpace(resource_id);
    candidate.needs_detiling =
        resource_provider_->GetNeedsDetiling(resource_id);
    candidate.hdr_metadata = resource_provider_->GetHDRMetadata(resource_id);

    if (!context_.is_delegated_context &&
        !base::Contains(kOverlayFormats, candidate.format)) {
      return CandidateStatus::kFailBufferFormat;
    }
  }

  SetDisplayRect(*quad, candidate);

  const SharedQuadState* sqs = quad->shared_quad_state;

  if (auto status =
          ApplyTransform(sqs->quad_to_target_transform, y_flipped, candidate);
      status != CandidateStatus::kSuccess) {
    return status;
  }

  candidate.is_opaque =
      !quad->ShouldDrawWithBlendingForReasonOtherThanMaskFilter();

  if (resource_id != kInvalidResourceId) {
    candidate.resource_size_in_pixels =
        resource_provider_->GetResourceBackedSize(resource_id);
  }

  AssignDamage(quad, candidate);
  candidate.resource_id = resource_id;

  if (resource_id != kInvalidResourceId) {
    candidate.mailbox = resource_provider_->GetMailbox(resource_id);

    if (!context_.is_delegated_context) {
      struct TrackingIdData {
        gfx::Rect rect;
        FrameSinkId frame_sink_id;
      };

      TrackingIdData track_data{
          quad->rect,
          resource_provider_->GetSurfaceId(resource_id).frame_sink_id()};
      // Assert that there is no padding - otherwise the bytes-based hash below
      // may differ for otherwise equal objects.
      static_assert(sizeof(track_data) ==
                    sizeof(decltype(track_data.rect)) +
                        sizeof(decltype(track_data.frame_sink_id)));
      // Intentionally throwing away the high bits (assuming that hash entropy
      // is uniformly spread across all the bits).
      size_t original_hash =
          base::FastHash(base::byte_span_from_ref(track_data));
      uint32_t narrow_hash = static_cast<uint32_t>(original_hash);
      candidate.tracking_id = narrow_hash;
    }
  }

  candidate.clip_rect = sqs->clip_rect;
  if (context_.is_delegated_context) {
    const bool quad_within_window =
        primary_rect_.Contains(candidate.display_rect);
    const bool transform_supports_clipping =
        context_.supports_arbitrary_transform ||
        absl::holds_alternative<gfx::OverlayTransform>(candidate.transform);
    // Out of window clipping is enabled on Lacros only when it is supported.
    // TODO(crbug.com/40246811): Remove the condition on `quad_within_window`
    // when M117 becomes widely supported.
    bool can_delegate_clipping =
        context_.supports_clip_rect &&
        (quad_within_window || context_.supports_out_of_window_clip_rect) &&
        transform_supports_clipping;

    bool is_rpdq = !!quad->DynamicCast<AggregatedRenderPassDrawQuad>();
    if (is_rpdq) {
      can_delegate_clipping &= context_.transform_and_clip_rpdq;
    }

    if (can_delegate_clipping) {
      // If we know the clip_rect won't intersect the display_rect at all, we
      // can skip it. We must account for any transform to the display_rect.
      if (candidate.clip_rect.has_value() &&
          !OverlayCandidate::DisplayRectInTargetSpace(candidate).Intersects(
              gfx::RectF(*candidate.clip_rect))) {
        return CandidateStatus::kFailVisible;
      }
    } else {
      // Clipping is applied after transforms, so we can't delegate transforms
      // if we can't delegate clipping.
      if (absl::holds_alternative<gfx::Transform>(candidate.transform)) {
        return CandidateStatus::kFailHasTransformButCantClip;
      }

      // Apply clipping to the |display_rect| and |uv_rect| directly.
      auto status = DoGeometricClipping(quad, candidate);
      if (status != CandidateStatus::kSuccess) {
        return status;
      }
    }
  }

  candidate.has_mask_filter = !sqs->mask_filter_info.IsEmpty();

  // Conditionally set the rounded corners once the candidate's |display_rect|
  // is known.
  if (context_.disable_wire_size_optimization ||
      ShouldApplyRoundedCorner(candidate, quad)) {
    if (!context_.supports_mask_filter) {
      return CandidateStatus::kFailMaskFilterNotSupported;
    }
    candidate.rounded_corners = sqs->mask_filter_info.rounded_corner_bounds();
  }

  return CandidateStatus::kSuccess;
}

void OverlayCandidateFactory::SetDisplayRect(
    const DrawQuad& quad,
    OverlayCandidate& candidate) const {
  if (context_.is_delegated_context && quad.visible_rect != quad.rect) {
    candidate.display_rect = gfx::RectF(quad.visible_rect);
    // Update uv_rect to account for the content clipping.
    candidate.uv_rect = cc::MathUtil::ScaleRectProportional(
        candidate.uv_rect, gfx::RectF(quad.rect),
        gfx::RectF(quad.visible_rect));
  } else {
    candidate.display_rect = gfx::RectF(quad.rect);
  }

  if (context_.is_delegated_context) {
    // Expand display_rect if quad is a render pass with a filter that expands
    // its bounds.
    if (auto* rpdq = quad.DynamicCast<AggregatedRenderPassDrawQuad>()) {
      auto filter_it = render_pass_filters_->find(rpdq->render_pass_id);
      if (filter_it != render_pass_filters_->end()) {
        candidate.display_rect = gfx::RectF(
            filter_it->second->ExpandRectForPixelMovement(quad.visible_rect));
        // uv_rect will be updated in SkiaRenderer because the buffer size will
        // be rounded up some.
      }
    }
  }
}

OverlayCandidate::CandidateStatus OverlayCandidateFactory::DoGeometricClipping(
    const DrawQuad* quad,
    OverlayCandidate& candidate) const {
  gfx::RectF clip_to_apply = candidate.display_rect;

  if (candidate.clip_rect.has_value()) {
    clip_to_apply.Intersect(gfx::RectF(*candidate.clip_rect));
  }

  // TODO(crbug.com/40216317) : Tile quads can overlay other quads
  // and the window by one pixel. Exo does not yet clip these quads so we
  // need to clip here with the |primary_rect|.
  clip_to_apply.Intersect(primary_rect_);

  if (clip_to_apply.IsEmpty()) {
    return CandidateStatus::kFailVisible;
  }

  OverlayCandidate::ApplyClip(candidate, clip_to_apply);
  candidate.clip_rect = std::nullopt;

  return CandidateStatus::kSuccess;
}

OverlayCandidate::CandidateStatus OverlayCandidateFactory::ApplyTransform(
    const gfx::Transform& quad_to_target_transform,
    const bool y_flipped,
    OverlayCandidate& candidate) const {
  // Try to bake |quad_to_target_transform| into |display_rect| to avoid sending
  // a full |gfx::Transform|.
  if (!context_.disable_wire_size_optimization) {
    gfx::OverlayTransform overlay_transform =
        GetOverlayTransform(quad_to_target_transform, y_flipped,
                            context_.supports_flip_rotate_transform);
    if (overlay_transform != gfx::OVERLAY_TRANSFORM_INVALID) {
      candidate.transform = overlay_transform;
      candidate.display_rect =
          quad_to_target_transform.MapRect(candidate.display_rect);
      return OverlayCandidate::CandidateStatus::kSuccess;
    }
  }

  // Otherwise, try to set an arbitrary transform, if possible.
  if (context_.supports_arbitrary_transform &&
      (!quad_to_target_transform.HasPerspective() ||
       quad_to_target_transform.Preserves2dAffine())) {
    gfx::Transform transform = quad_to_target_transform;
    if (y_flipped) {
      transform.PreConcat(gfx::OverlayTransformToTransform(
          gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL, candidate.display_rect.size()));
    }
    candidate.transform = transform;
    return OverlayCandidate::CandidateStatus::kSuccess;
  }

  return context_.is_delegated_context
             ? GetReasonForTransformNotAxisAligned(quad_to_target_transform)
             : CandidateStatus::kFailNotAxisAligned;
}

OverlayCandidate::CandidateStatus OverlayCandidateFactory::FromAggregateQuad(
    const AggregatedRenderPassDrawQuad* quad,
    OverlayCandidate& candidate) const {
  auto rtn = FromDrawQuadResource(quad, kInvalidResourceId, false, candidate);
  if (rtn == CandidateStatus::kSuccess) {
    candidate.rpdq = quad;
  }
  return rtn;
}

OverlayCandidate::CandidateStatus OverlayCandidateFactory::FromSolidColorQuad(
    const SolidColorDrawQuad* quad,
    OverlayCandidate& candidate) const {
  auto rtn = FromDrawQuadResource(quad, kInvalidResourceId, false, candidate);

  if (rtn == CandidateStatus::kSuccess) {
    candidate.color = quad->color;
    // Mark this candidate a solid color as the |color| member can be either a
    // background of the overlay or a color of the solid color quad.
    candidate.is_solid_color = true;
  }
  return rtn;
}

// For VideoHoleDrawQuad, only calculate geometry information and put it in the
// |candidate|.
OverlayCandidate::CandidateStatus OverlayCandidateFactory::FromVideoHoleQuad(
    const VideoHoleDrawQuad* quad,
    OverlayCandidate& candidate) const {
  candidate.display_rect = gfx::RectF(quad->rect);
  const SharedQuadState* sqs = quad->shared_quad_state;
  if (context_.supports_arbitrary_transform) {
    candidate.transform = sqs->quad_to_target_transform;
  } else {
    gfx::OverlayTransform overlay_transform =
        GetOverlayTransform(sqs->quad_to_target_transform, false,
                            context_.supports_flip_rotate_transform);
    if (overlay_transform == gfx::OVERLAY_TRANSFORM_INVALID)
      return CandidateStatus::kFailNotAxisAligned;
    candidate.transform = overlay_transform;
    candidate.display_rect =
        sqs->quad_to_target_transform.MapRect(candidate.display_rect);
  }
  candidate.is_opaque =
      !quad->ShouldDrawWithBlendingForReasonOtherThanMaskFilter();

  AssignDamage(quad, candidate);

  // Intentionally throwing away the high bits (assuming that hash entropy is
  // uniformly spread across all the bits).
  size_t original_hash = base::FastHash(quad->overlay_plane_id.AsBytes());
  uint32_t narrow_hash = static_cast<uint32_t>(original_hash);
  candidate.tracking_id = narrow_hash;

  return CandidateStatus::kSuccess;
}

OverlayCandidate::CandidateStatus OverlayCandidateFactory::FromTileQuad(
    const TileDrawQuad* quad,
    OverlayCandidate& candidate) const {
  if (quad->nearest_neighbor)
    return CandidateStatus::kFailNearFilter;

  candidate.resource_size_in_pixels =
      resource_provider_->GetResourceBackedSize(quad->resource_id());
  candidate.uv_rect = gfx::ScaleRect(
      quad->tex_coord_rect, 1.f / candidate.resource_size_in_pixels.width(),
      1.f / candidate.resource_size_in_pixels.height());

  auto rtn = FromDrawQuadResource(quad, quad->resource_id(), false, candidate);
  return rtn;
}

OverlayCandidate::CandidateStatus OverlayCandidateFactory::FromTextureQuad(
    const TextureDrawQuad* quad,
    OverlayCandidate& candidate) const {
  if (!context_.is_delegated_context &&
      quad->overlay_priority_hint == OverlayPriority::kLow) {
    // For current implementation low priority means this does not promote to
    // overlay.
    return CandidateStatus::kFailPriority;
  }

  if (!quad->rounded_display_masks_info.IsEmpty() &&
      !context_.supports_rounded_display_masks) {
    DCHECK(!context_.is_delegated_context);
    return CandidateStatus::kFailRoundedDisplayMasksNotSupported;
  }

  if (quad->nearest_neighbor)
    return CandidateStatus::kFailNearFilter;

  if (context_.is_delegated_context) {
    // Always convey |background_color| even when transparent. This allows for
    // the wayland server to make blending optimizations even when the quad is
    // considered opaque. Specifically Exo will try to ensure the opaqueness of
    // alpha formats by adding a black background which can cause difficulty in
    // overlay promotion (see the code in the lines below).
    candidate.color = quad->background_color;
  } else if (quad->background_color != SkColors::kTransparent &&
             (quad->background_color != SkColors::kBlack ||
              RequiresBlendingForReasonOtherThanRoundedCorners(quad))) {
    // The condition above is very specific to the implementation of DRM/KMS
    // scanout. An opaque plane with buffer that has buffer element component
    // alpha will default black for the blend. Basically we can simulate a black
    // background using the default color when blending an opaque overlay. This
    // trick, of course, only works for black.
    return CandidateStatus::kFailBlending;
  }

  candidate.uv_rect = BoundingRect(quad->uv_top_left, quad->uv_bottom_right);

  auto rtn = FromDrawQuadResource(quad, quad->resource_id(), quad->y_flipped,
                                  candidate);
  if (rtn == CandidateStatus::kSuccess) {
    // Only handle clip rect for required overlays
    if (!context_.is_delegated_context && candidate.requires_overlay) {
      HandleClipAndSubsampling(candidate);
    }

    // Texture quads for UI elements like scroll bars have empty
    // |size_in_pixels| as 'set_resource_size_in_pixels' is not called as these
    // quads are not intended to become overlays.
    if (!quad->resource_size_in_pixels().IsEmpty()) {
      if (candidate.requires_overlay) {
        candidate.priority_hint = gfx::OverlayPriorityHint::kHardwareProtection;
      } else if (quad->is_video_frame) {
        candidate.priority_hint = gfx::OverlayPriorityHint::kVideo;
      } else {
        candidate.priority_hint = gfx::OverlayPriorityHint::kRegular;
      }
    }

#if BUILDFLAG(IS_ANDROID)
    candidate.is_video_in_surface_view =
        quad->is_stream_video &&
        !resource_provider_->IsBackedBySurfaceTexture(quad->resource_id());
    if (quad->is_stream_video) {
      // StreamVideoDrawQuad used to set the resource_size_in_pixels directly
      // from the quad rather than from the resource.
      candidate.resource_size_in_pixels = quad->resource_size_in_pixels();
    }
#endif

    candidate.has_rounded_display_masks =
        !quad->rounded_display_masks_info.IsEmpty();
  }
  return rtn;
}

void OverlayCandidateFactory::HandleClipAndSubsampling(
    OverlayCandidate& candidate) const {
  // The purpose of this is to enable overlays that are required (i.e. protected
  // content) to be able to be shown in all cases. This will allow them to pass
  // the clipping check and also the 2x alignment requirement for subsampling in
  // the Intel DRM driver. This should not be used in cases where the surface
  // will not always be promoted to an overlay as it will lead to shifting of
  // the content when it switches between composition and overlay.
  if (!candidate.clip_rect)
    return;

  // Make sure it's in a format we can deal with, we only support YUV and P010.
  if (candidate.format != MultiPlaneFormat::kNV12 &&
      candidate.format != MultiPlaneFormat::kP010) {
    return;
  }
  // Clip the clip rect to the primary plane. An overlay will only be shown on
  // a single display, so we want to perform our calculations within the bounds
  // of that display.
  if (!primary_rect_.IsEmpty())
    candidate.clip_rect->Intersect(gfx::ToNearestRect(primary_rect_));

  // Baking |clip_rect| into the |uv_rect| and |display_rect| doesn't make sense
  // when there is an arbitrary transform between the two because the transform
  // may not preserve axis alignment.
  DCHECK(absl::holds_alternative<gfx::OverlayTransform>(candidate.transform));

  // Candidates that need detiling have a UV rect that indicates the
  // relationship between the visible rect and the backing buffer dimensions
  // (coded size). This rect is calculated assuming no rotation, so we need to
  // rotate it before applying our own clipping.
  if (candidate.needs_detiling &&
      absl::holds_alternative<gfx::OverlayTransform>(candidate.transform)) {
    switch (absl::get<gfx::OverlayTransform>(candidate.transform)) {
      case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90:
        candidate.uv_rect =
            gfx::RectF(1.0f - candidate.uv_rect.height(), candidate.uv_rect.x(),
                       candidate.uv_rect.height(), candidate.uv_rect.width());
        break;
      case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180:
        candidate.uv_rect = gfx::RectF(
            1.0f - candidate.uv_rect.width(), 1.0f - candidate.uv_rect.height(),
            candidate.uv_rect.width(), candidate.uv_rect.height());
        break;
      case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270:
        candidate.uv_rect =
            gfx::RectF(candidate.uv_rect.y(), 1.0f - candidate.uv_rect.width(),
                       candidate.uv_rect.height(), candidate.uv_rect.width());
        break;
      default:
        break;
    }
  }

  // Calculate |uv_rect| of |clip_rect| in |display_rect|
  // TODO(rivr): Handle candidates with an overlay transform applied.
  gfx::RectF uv_rect = cc::MathUtil::ScaleRectProportional(
      candidate.uv_rect, candidate.display_rect,
      gfx::RectF(*candidate.clip_rect));

  // In case that |uv_rect| of candidate is not (0, 0, 1, 1)
  candidate.uv_rect.Intersect(uv_rect);

  // Update |display_rect| to avoid unexpected scaling and the candidate should
  // not be regarded as clippped after this.
  candidate.display_rect.Intersect(gfx::RectF(*candidate.clip_rect));
  candidate.clip_rect.reset();
  gfx::Rect rounded_display_rect = gfx::ToRoundedRect(candidate.display_rect);
  candidate.display_rect.SetRect(
      rounded_display_rect.x(), rounded_display_rect.y(),
      rounded_display_rect.width(), rounded_display_rect.height());

  // Now correct |uv_rect| if required so that the source rect aligns on a pixel
  // boundary that is a multiple of the chroma subsampling.

  // Get the rect for the source coordinates.
  gfx::RectF src_rect = gfx::ScaleRect(
      candidate.uv_rect, candidate.resource_size_in_pixels.width(),
      candidate.resource_size_in_pixels.height());
  // Make it an integral multiple of the subsampling factor.
  auto subsample_round = [](float val) {
    constexpr int kSubsamplingFactor = 2;
    return (std::lround(val) / kSubsamplingFactor) * kSubsamplingFactor;
  };

  src_rect.set_x(subsample_round(src_rect.x()));
  src_rect.set_y(subsample_round(src_rect.y()));
  src_rect.set_width(subsample_round(src_rect.width()));
  src_rect.set_height(subsample_round(src_rect.height()));
  // Scale it back into UV space and set it in the candidate.
  candidate.uv_rect =
      gfx::ScaleRect(src_rect, 1.0f / candidate.resource_size_in_pixels.width(),
                     1.0f / candidate.resource_size_in_pixels.height());
}

void OverlayCandidateFactory::AssignDamage(const DrawQuad* quad,
                                           OverlayCandidate& candidate) const {
  candidate.damage_rect = GetDamageRect(quad);
  // For underlays the function 'EstimateVisibleDamage()' is called to update
  // |damage_area_estimate| to more accurately reflect the actual visible
  // damage.
  if (!context_.is_delegated_context) {
    candidate.damage_area_estimate =
        GetDamageEstimate(candidate).size().GetArea();
  }
}

gfx::RectF OverlayCandidateFactory::GetDamageEstimate(
    const OverlayCandidate& candidate) const {
  // If we have assigned damage we can trust that.
  if (!candidate.damage_rect.IsEmpty()) {
    return candidate.damage_rect;
  }

  // Otherwise we will see how much unassigned damage covers the display_rect.
  return gfx::IntersectRects(
      OverlayCandidate::DisplayRectInTargetSpace(candidate),
      gfx::RectF(unassigned_surface_damage_));
}

gfx::RectF OverlayCandidateFactory::GetDamageRect(const DrawQuad* quad) const {
  const SharedQuadState* sqs = quad->shared_quad_state;
  if (!sqs->overlay_damage_index.has_value()) {
    return gfx::RectF();
  }

  size_t overlay_damage_index = sqs->overlay_damage_index.value();
  // Invalid index.
  if (overlay_damage_index >= surface_damage_rect_list_->size()) {
    DCHECK(false);
    return gfx::RectF();
  }

  auto damage = gfx::RectF((*surface_damage_rect_list_)[overlay_damage_index]);
  DBG_DRAW_RECT("damage_assigned", damage);
  return damage;
}

}  // namespace viz
