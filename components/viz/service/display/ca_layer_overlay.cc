// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/ca_layer_overlay.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "gpu/GLES2/gl2extchromium.h"

namespace viz {

namespace {

// If there are too many RenderPassDrawQuads, we shouldn't use Core
// Animation to present them as individual layers, since that potentially
// doubles the amount of work needed to present them. cc has to blit them into
// an IOSurface, and then Core Animation has to blit them to the final surface.
// https://crbug.com/636884.
const int kTooManyRenderPassDrawQuads = 30;

// This enum is used for histogram states and should only have new values added
// to the end before COUNT.
enum CALayerResult {
  CA_LAYER_SUCCESS = 0,
  CA_LAYER_FAILED_UNKNOWN = 1,
  // CA_LAYER_FAILED_IO_SURFACE_NOT_CANDIDATE = 2,
  CA_LAYER_FAILED_STREAM_VIDEO_NOT_CANDIDATE = 3,
  // CA_LAYER_FAILED_STREAM_VIDEO_TRANSFORM = 4,
  CA_LAYER_FAILED_TEXTURE_NOT_CANDIDATE = 5,
  // CA_LAYER_FAILED_TEXTURE_Y_FLIPPED = 6,
  CA_LAYER_FAILED_TILE_NOT_CANDIDATE = 7,
  CA_LAYER_FAILED_QUAD_BLEND_MODE = 8,
  // CA_LAYER_FAILED_QUAD_TRANSFORM = 9,
  // CA_LAYER_FAILED_QUAD_CLIPPING = 10,
  CA_LAYER_FAILED_DEBUG_BORDER = 11,
  CA_LAYER_FAILED_PICTURE_CONTENT = 12,
  // CA_LAYER_FAILED_RENDER_PASS = 13,
  CA_LAYER_FAILED_SURFACE_CONTENT = 14,
  CA_LAYER_FAILED_YUV_VIDEO_CONTENT = 15,
  CA_LAYER_FAILED_DIFFERENT_CLIP_SETTINGS = 16,
  CA_LAYER_FAILED_DIFFERENT_VERTEX_OPACITIES = 17,
  // CA_LAYER_FAILED_RENDER_PASS_FILTER_SCALE = 18,
  CA_LAYER_FAILED_RENDER_PASS_BACKDROP_FILTERS = 19,
  // CA_LAYER_FAILED_RENDER_PASS_MASK = 20,
  CA_LAYER_FAILED_RENDER_PASS_FILTER_OPERATION = 21,
  CA_LAYER_FAILED_RENDER_PASS_SORTING_CONTEXT_ID = 22,
  CA_LAYER_FAILED_TOO_MANY_RENDER_PASS_DRAW_QUADS = 23,
  // CA_LAYER_FAILED_QUAD_ROUNDED_CORNER = 24,
  CA_LAYER_FAILED_QUAD_ROUNDED_CORNER_CLIP_MISMATCH = 25,
  CA_LAYER_FAILED_QUAD_ROUNDED_CORNER_NOT_UNIFORM = 26,
  CA_LAYER_FAILED_COUNT,
};

bool FilterOperationSupported(const cc::FilterOperation& operation) {
  switch (operation.type()) {
    case cc::FilterOperation::GRAYSCALE:
    case cc::FilterOperation::SEPIA:
    case cc::FilterOperation::SATURATE:
    case cc::FilterOperation::HUE_ROTATE:
    case cc::FilterOperation::INVERT:
    case cc::FilterOperation::BRIGHTNESS:
    case cc::FilterOperation::CONTRAST:
    case cc::FilterOperation::OPACITY:
    case cc::FilterOperation::BLUR:
    case cc::FilterOperation::DROP_SHADOW:
      return true;
    default:
      return false;
  }
}

CALayerResult FromRenderPassQuad(
    DisplayResourceProvider* resource_provider,
    const RenderPassDrawQuad* quad,
    const base::flat_map<RenderPassId, cc::FilterOperations*>&
        render_pass_filters,
    const base::flat_map<RenderPassId, cc::FilterOperations*>&
        render_pass_backdrop_filters,
    CALayerOverlay* ca_layer_overlay) {
  if (render_pass_backdrop_filters.count(quad->render_pass_id)) {
    return CA_LAYER_FAILED_RENDER_PASS_BACKDROP_FILTERS;
  }

  if (quad->shared_quad_state->sorting_context_id != 0)
    return CA_LAYER_FAILED_RENDER_PASS_SORTING_CONTEXT_ID;

  auto it = render_pass_filters.find(quad->render_pass_id);
  if (it != render_pass_filters.end()) {
    for (const auto& operation : it->second->operations()) {
      bool success = FilterOperationSupported(operation);
      if (!success)
        return CA_LAYER_FAILED_RENDER_PASS_FILTER_OPERATION;
    }
  }

  ca_layer_overlay->rpdq = quad;
  ca_layer_overlay->contents_rect = gfx::RectF(0, 0, 1, 1);

  return CA_LAYER_SUCCESS;
}

CALayerResult FromStreamVideoQuad(DisplayResourceProvider* resource_provider,
                                  const StreamVideoDrawQuad* quad,
                                  CALayerOverlay* ca_layer_overlay) {
  unsigned resource_id = quad->resource_id();
  if (!resource_provider->IsOverlayCandidate(resource_id))
    return CA_LAYER_FAILED_STREAM_VIDEO_NOT_CANDIDATE;
  ca_layer_overlay->contents_resource_id = resource_id;
  ca_layer_overlay->contents_rect =
      BoundingRect(quad->uv_top_left, quad->uv_bottom_right);
  return CA_LAYER_SUCCESS;
}

CALayerResult FromSolidColorDrawQuad(const SolidColorDrawQuad* quad,
                                     CALayerOverlay* ca_layer_overlay,
                                     bool* skip) {
  // Do not generate quads that are completely transparent.
  if (SkColorGetA(quad->color) == 0) {
    *skip = true;
    return CA_LAYER_SUCCESS;
  }
  ca_layer_overlay->background_color = quad->color;
  return CA_LAYER_SUCCESS;
}

CALayerResult FromTextureQuad(DisplayResourceProvider* resource_provider,
                              const TextureDrawQuad* quad,
                              CALayerOverlay* ca_layer_overlay) {
  unsigned resource_id = quad->resource_id();
  if (!resource_provider->IsOverlayCandidate(resource_id))
    return CA_LAYER_FAILED_TEXTURE_NOT_CANDIDATE;
  if (quad->y_flipped) {
    // The anchor point is at the bottom-left corner of the CALayer. The
    // transformation that flips the contents of the layer without changing its
    // frame is the composition of a vertical flip about the anchor point, and a
    // translation by the height of the layer.
    ca_layer_overlay->shared_state->transform.preTranslate(
        0, ca_layer_overlay->bounds_rect.height(), 0);
    ca_layer_overlay->shared_state->transform.preScale(1, -1, 1);
  }
  ca_layer_overlay->contents_resource_id = resource_id;
  ca_layer_overlay->contents_rect =
      BoundingRect(quad->uv_top_left, quad->uv_bottom_right);
  ca_layer_overlay->background_color = quad->background_color;
  for (int i = 1; i < 4; ++i) {
    if (quad->vertex_opacity[i] != quad->vertex_opacity[0])
      return CA_LAYER_FAILED_DIFFERENT_VERTEX_OPACITIES;
  }
  ca_layer_overlay->shared_state->opacity *= quad->vertex_opacity[0];
  ca_layer_overlay->filter = quad->nearest_neighbor ? GL_NEAREST : GL_LINEAR;
  return CA_LAYER_SUCCESS;
}

CALayerResult FromTileQuad(DisplayResourceProvider* resource_provider,
                           const TileDrawQuad* quad,
                           CALayerOverlay* ca_layer_overlay) {
  unsigned resource_id = quad->resource_id();
  if (!resource_provider->IsOverlayCandidate(resource_id))
    return CA_LAYER_FAILED_TILE_NOT_CANDIDATE;
  ca_layer_overlay->contents_resource_id = resource_id;
  ca_layer_overlay->contents_rect = quad->tex_coord_rect;
  ca_layer_overlay->contents_rect.Scale(1.f / quad->texture_size.width(),
                                        1.f / quad->texture_size.height());
  ca_layer_overlay->filter = quad->nearest_neighbor ? GL_NEAREST : GL_LINEAR;
  return CA_LAYER_SUCCESS;
}

class CALayerOverlayProcessor {
 public:
  CALayerResult FromDrawQuad(
      DisplayResourceProvider* resource_provider,
      const gfx::RectF& display_rect,
      const DrawQuad* quad,
      const base::flat_map<RenderPassId, cc::FilterOperations*>&
          render_pass_filters,
      const base::flat_map<RenderPassId, cc::FilterOperations*>&
          render_pass_backdrop_filters,
      CALayerOverlay* ca_layer_overlay,
      bool* skip,
      bool* render_pass_draw_quad) {
    if (quad->shared_quad_state->blend_mode != SkBlendMode::kSrcOver)
      return CA_LAYER_FAILED_QUAD_BLEND_MODE;

    // Early-out for invisible quads.
    if (quad->shared_quad_state->opacity == 0.f) {
      *skip = true;
      return CA_LAYER_SUCCESS;
    }

    // Support rounded corner bounds when they have the same rect as the clip
    // rect, and all corners have the same radius. Note that it is entirely
    // possible to make rounded corner rects independent of clip rect (by adding
    // another CALayer to the tree). Handling non-single border radii is also,
    // but requires APIs not supported on all macOS versions.
    if (!quad->shared_quad_state->rounded_corner_bounds.IsEmpty()) {
      DCHECK(quad->shared_quad_state->is_clipped);
      if (quad->shared_quad_state->rounded_corner_bounds.GetType() >
          gfx::RRectF::Type::kSingle) {
        return CA_LAYER_FAILED_QUAD_ROUNDED_CORNER_NOT_UNIFORM;
      }
    }

    // Enable edge anti-aliasing only on layer boundaries.
    ca_layer_overlay->edge_aa_mask = 0;
    if (quad->IsLeftEdge())
      ca_layer_overlay->edge_aa_mask |= GL_CA_LAYER_EDGE_LEFT_CHROMIUM;
    if (quad->IsRightEdge())
      ca_layer_overlay->edge_aa_mask |= GL_CA_LAYER_EDGE_RIGHT_CHROMIUM;
    if (quad->IsBottomEdge())
      ca_layer_overlay->edge_aa_mask |= GL_CA_LAYER_EDGE_BOTTOM_CHROMIUM;
    if (quad->IsTopEdge())
      ca_layer_overlay->edge_aa_mask |= GL_CA_LAYER_EDGE_TOP_CHROMIUM;

    if (most_recent_shared_quad_state_ != quad->shared_quad_state) {
      most_recent_shared_quad_state_ = quad->shared_quad_state;
      most_recent_overlay_shared_state_ = new CALayerOverlaySharedState;
      // Set rect clipping and sorting context ID.
      most_recent_overlay_shared_state_->sorting_context_id =
          quad->shared_quad_state->sorting_context_id;
      most_recent_overlay_shared_state_->is_clipped =
          quad->shared_quad_state->is_clipped;
      most_recent_overlay_shared_state_->clip_rect =
          gfx::RectF(quad->shared_quad_state->clip_rect);
      most_recent_overlay_shared_state_->rounded_corner_bounds =
          quad->shared_quad_state->rounded_corner_bounds;

      most_recent_overlay_shared_state_->opacity =
          quad->shared_quad_state->opacity;
      most_recent_overlay_shared_state_->transform =
          quad->shared_quad_state->quad_to_target_transform.matrix();
    }
    ca_layer_overlay->shared_state = most_recent_overlay_shared_state_;

    ca_layer_overlay->bounds_rect = gfx::RectF(quad->rect);

    *render_pass_draw_quad = quad->material == DrawQuad::Material::kRenderPass;
    switch (quad->material) {
      case DrawQuad::Material::kTextureContent:
        return FromTextureQuad(resource_provider,
                               TextureDrawQuad::MaterialCast(quad),
                               ca_layer_overlay);
      case DrawQuad::Material::kTiledContent:
        return FromTileQuad(resource_provider, TileDrawQuad::MaterialCast(quad),
                            ca_layer_overlay);
      case DrawQuad::Material::kSolidColor:
        return FromSolidColorDrawQuad(SolidColorDrawQuad::MaterialCast(quad),
                                      ca_layer_overlay, skip);
      case DrawQuad::Material::kStreamVideoContent:
        return FromStreamVideoQuad(resource_provider,
                                   StreamVideoDrawQuad::MaterialCast(quad),
                                   ca_layer_overlay);
      case DrawQuad::Material::kDebugBorder:
        return CA_LAYER_FAILED_DEBUG_BORDER;
      case DrawQuad::Material::kPictureContent:
        return CA_LAYER_FAILED_PICTURE_CONTENT;
      case DrawQuad::Material::kRenderPass:
        return FromRenderPassQuad(
            resource_provider, RenderPassDrawQuad::MaterialCast(quad),
            render_pass_filters, render_pass_backdrop_filters,
            ca_layer_overlay);
      case DrawQuad::Material::kSurfaceContent:
        return CA_LAYER_FAILED_SURFACE_CONTENT;
      case DrawQuad::Material::kYuvVideoContent:
        return CA_LAYER_FAILED_YUV_VIDEO_CONTENT;
      default:
        break;
    }

    return CA_LAYER_FAILED_UNKNOWN;
  }

 private:
  const SharedQuadState* most_recent_shared_quad_state_ = nullptr;
  scoped_refptr<CALayerOverlaySharedState> most_recent_overlay_shared_state_;
};

}  // namespace

CALayerOverlay::CALayerOverlay() : filter(GL_LINEAR) {}

CALayerOverlay::CALayerOverlay(const CALayerOverlay& other) = default;

CALayerOverlay::~CALayerOverlay() {}

bool ProcessForCALayerOverlays(
    DisplayResourceProvider* resource_provider,
    const gfx::RectF& display_rect,
    const QuadList& quad_list,
    const base::flat_map<RenderPassId, cc::FilterOperations*>&
        render_pass_filters,
    const base::flat_map<RenderPassId, cc::FilterOperations*>&
        render_pass_backdrop_filters,
    CALayerOverlayList* ca_layer_overlays) {
  CALayerResult result = CA_LAYER_SUCCESS;
  ca_layer_overlays->reserve(quad_list.size());

  int render_pass_draw_quad_count = 0;
  CALayerOverlayProcessor processor;
  for (auto it = quad_list.BackToFrontBegin(); it != quad_list.BackToFrontEnd();
       ++it) {
    const DrawQuad* quad = *it;
    CALayerOverlay ca_layer;
    bool skip = false;
    bool render_pass_draw_quad = false;
    result = processor.FromDrawQuad(
        resource_provider, display_rect, quad, render_pass_filters,
        render_pass_backdrop_filters, &ca_layer, &skip, &render_pass_draw_quad);
    if (result != CA_LAYER_SUCCESS)
      break;

    if (render_pass_draw_quad) {
      ++render_pass_draw_quad_count;
      if (render_pass_draw_quad_count > kTooManyRenderPassDrawQuads) {
        result = CA_LAYER_FAILED_TOO_MANY_RENDER_PASS_DRAW_QUADS;
        break;
      }
    }

    if (skip)
      continue;

    // It is not possible to correctly represent two different clipping settings
    // within one sorting context.
    if (!ca_layer_overlays->empty()) {
      const CALayerOverlay& previous_ca_layer = ca_layer_overlays->back();
      if (ca_layer.shared_state->sorting_context_id &&
          previous_ca_layer.shared_state->sorting_context_id ==
              ca_layer.shared_state->sorting_context_id) {
        if (previous_ca_layer.shared_state->is_clipped !=
                ca_layer.shared_state->is_clipped ||
            previous_ca_layer.shared_state->clip_rect !=
                ca_layer.shared_state->clip_rect) {
          result = CA_LAYER_FAILED_DIFFERENT_CLIP_SETTINGS;
          break;
        }
      }
    }

    ca_layer_overlays->push_back(ca_layer);
  }

  UMA_HISTOGRAM_ENUMERATION("Compositing.Renderer.CALayerResult", result,
                            CA_LAYER_FAILED_COUNT);

  if (result != CA_LAYER_SUCCESS) {
    ca_layer_overlays->clear();
    return false;
  }
  return true;
}

}  // namespace viz
