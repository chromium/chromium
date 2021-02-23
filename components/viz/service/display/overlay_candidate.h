// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_H_

#include <map>
#include <vector>

#include "base/containers/flat_map.h"
#include "build/build_config.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/transform.h"

namespace gfx {
class Rect;
}

namespace viz {
class DisplayResourceProvider;
class StreamVideoDrawQuad;
class TextureDrawQuad;
class VideoHoleDrawQuad;

class VIZ_SERVICE_EXPORT OverlayCandidate {
 public:
  // Returns true and fills in |candidate| if |draw_quad| is of a known quad
  // type and contains an overlayable resource. |primary_rect| can be empty in
  // the case of a null primary plane.
  static bool FromDrawQuad(DisplayResourceProvider* resource_provider,
                           SurfaceDamageRectList* surface_damage_rect_list,
                           const SkMatrix44& output_color_matrix,
                           const DrawQuad* quad,
                           const gfx::RectF& primary_rect,
                           OverlayCandidate* candidate);
  // Returns true if |quad| will not block quads underneath from becoming
  // an overlay.
  static bool IsInvisibleQuad(const DrawQuad* quad);

  // Returns true if any of the quads in the list given by |quad_list_begin|
  // and |quad_list_end| are visible and on top of |candidate|.
  static bool IsOccluded(const OverlayCandidate& candidate,
                         QuadList::ConstIterator quad_list_begin,
                         QuadList::ConstIterator quad_list_end);

  // Returns an estimate of this |quad|'s actual visible damage area. This
  // visible damage is computed by combining from input
  // |surface_damage_rect_list| with the occluding rects in the quad_list.
  // This is an estimate since the occluded damage area is calculated on a per
  // quad basis.
  static int EstimateVisibleDamage(
      const DrawQuad* quad,
      SurfaceDamageRectList* surface_damage_rect_list,
      QuadList::ConstIterator quad_list_begin,
      QuadList::ConstIterator quad_list_end);

  // Returns true if any of the quads in the list given by |quad_list_begin|
  // and |quad_list_end| have a filter associated and occlude |candidate|.
  static bool IsOccludedByFilteredQuad(
      const OverlayCandidate& candidate,
      QuadList::ConstIterator quad_list_begin,
      QuadList::ConstIterator quad_list_end,
      const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
          render_pass_backdrop_filters);

  // Returns true if the |quad| cannot be displayed on the main plane. This is
  // used in conjuction with protected content that can't be GPU composited and
  // will be shown via an overlay.
  static bool RequiresOverlay(const DrawQuad* quad);

  OverlayCandidate();
  OverlayCandidate(const OverlayCandidate& other);
  ~OverlayCandidate();

  // Transformation to apply to layer during composition.
  gfx::OverlayTransform transform = gfx::OVERLAY_TRANSFORM_NONE;
  // Format of the buffer to scanout.
  gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  // ColorSpace of the buffer for scanout.
  gfx::ColorSpace color_space;
  // Size of the resource, in pixels.
  gfx::Size resource_size_in_pixels;
  // Rect on the display to position the overlay to. Implementer must convert
  // to integer coordinates if setting |overlay_handled| to true.
  gfx::RectF display_rect;
  // Crop within the buffer to be placed inside |display_rect|.
  gfx::RectF uv_rect = gfx::RectF(0.f, 0.f, 1.f, 1.f);
  // Clip rect in the target content space after composition.
  gfx::Rect clip_rect;
  // If the quad is clipped after composition.
  bool is_clipped = false;
  // If the quad doesn't require blending.
  bool is_opaque = false;
  // Texture resource to present in an overlay.
  ResourceId resource_id = kInvalidResourceId;
  // Mailbox from resource_id. It is used by SkiaRenderer.
  gpu::Mailbox mailbox;

#if defined(OS_ANDROID)
  // For candidates from StreamVideoDrawQuads, this records whether the quad is
  // marked as being backed by a SurfaceTexture or not.  If so, it's not really
  // promotable to an overlay.
  bool is_backed_by_surface_texture = false;
#endif

  // Stacking order of the overlay plane relative to the main surface,
  // which is 0. Signed to allow for "underlays".
  int plane_z_order = 0;

  // To be modified by the implementer if this candidate can go into
  // an overlay.
  bool overlay_handled = false;

  // Gpu fence to wait for before overlay is ready for display.
  unsigned gpu_fence_id = 0;

  // The total area in square pixels of damage for this candidate's quad. This
  // is an estimate when 'EstimateOccludedDamage' function is used.
  int damage_area_estimate = 0;

  static constexpr uint32_t kInvalidDamageIndex = UINT_MAX;
  // Damage index for |SurfaceDamageRectList|.
  uint32_t overlay_damage_index = kInvalidDamageIndex;

  // Is true if an HW overlay is required for the quad content.
  bool requires_overlay = false;

  // Is true when quad is part of a |shared_quad_state| that has damage.
  // This is a fallback case for when |overlay_damage_index| is unavailable and
  // will be absent from the |SurfaceDamageRectList|.
  bool assume_damaged = false;

  // Identifier passed through by the video decoder that allows us to validate
  // if a protected surface can still be displayed. Non-zero when valid.
  uint32_t hw_protected_validation_id = 0;

 private:
  static bool FromDrawQuadResource(
      DisplayResourceProvider* resource_provider,
      SurfaceDamageRectList* surface_damage_rect_list,
      const DrawQuad* quad,
      ResourceId resource_id,
      bool y_flipped,
      OverlayCandidate* candidate);
  static bool FromTextureQuad(DisplayResourceProvider* resource_provider,
                              SurfaceDamageRectList* surface_damage_rect_list,
                              const TextureDrawQuad* quad,
                              const gfx::RectF& primary_rect,
                              OverlayCandidate* candidate);
  static bool FromStreamVideoQuad(
      DisplayResourceProvider* resource_provider,
      SurfaceDamageRectList* surface_damage_rect_list,
      const StreamVideoDrawQuad* quad,
      OverlayCandidate* candidate);
  static bool FromVideoHoleQuad(DisplayResourceProvider* resource_provider,
                                SurfaceDamageRectList* surface_damage_rect_list,
                                const VideoHoleDrawQuad* quad,
                                OverlayCandidate* candidate);
  static void HandleClipAndSubsampling(OverlayCandidate* candidate,
                                       const gfx::RectF& primary_rect);
};

using OverlayCandidateList = std::vector<OverlayCandidate>;

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_H_
