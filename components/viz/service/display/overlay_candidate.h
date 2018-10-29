// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_H_

#include <map>
#include <vector>

#include "base/containers/flat_map.h"
#include "build/build_config.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/viz_service_export.h"
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
class TileDrawQuad;

class VIZ_SERVICE_EXPORT OverlayCandidate {
 public:
  // Returns true and fills in |candidate| if |draw_quad| is of a known quad
  // type and contains an overlayable resource.
  static bool FromDrawQuad(DisplayResourceProvider* resource_provider,
                           const SkMatrix44& output_color_matrix,
                           const DrawQuad* quad,
                           OverlayCandidate* candidate);
  // Returns true if |quad| will not block quads underneath from becoming
  // an overlay.
  static bool IsInvisibleQuad(const DrawQuad* quad);

  // Returns true if any of the quads in the list given by |quad_list_begin|
  // and |quad_list_end| are visible and on top of |candidate|.
  static bool IsOccluded(const OverlayCandidate& candidate,
                         QuadList::ConstIterator quad_list_begin,
                         QuadList::ConstIterator quad_list_end);

  // Returns true if any of the quads in the list given by |quad_list_begin|
  // and |quad_list_end| have a filter associated and occlude |candidate|.
  static bool IsOccludedByFilteredQuad(
      const OverlayCandidate& candidate,
      QuadList::ConstIterator quad_list_begin,
      QuadList::ConstIterator quad_list_end,
      const base::flat_map<RenderPassId, cc::FilterOperations*>&
          render_pass_backdrop_filters);

  OverlayCandidate();
  OverlayCandidate(const OverlayCandidate& other);
  ~OverlayCandidate();

  // Transformation to apply to layer during composition.
  gfx::OverlayTransform transform;
  // Format of the buffer to scanout.
  gfx::BufferFormat format;
  // Size of the resource, in pixels.
  gfx::Size resource_size_in_pixels;
  // Rect on the display to position the overlay to. Implementer must convert
  // to integer coordinates if setting |overlay_handled| to true.
  gfx::RectF display_rect;
  // Crop within the buffer to be placed inside |display_rect|.
  gfx::RectF uv_rect;
  // Clip rect in the target content space after composition.
  gfx::Rect clip_rect;
  // If the quad is clipped after composition.
  bool is_clipped;
  // If the quad doesn't require blending.
  bool is_opaque;
  // True if the texture for this overlay should be the same one used by the
  // output surface's main overlay.
  bool use_output_surface_for_resource;
  // Texture resource to present in an overlay.
  unsigned resource_id;

#if defined(OS_ANDROID)
  // For candidates from StreamVideoDrawQuads, this records whether the quad is
  // marked as being backed by a SurfaceTexture or not.  If so, it's not really
  // promotable to an overlay.
  bool is_backed_by_surface_texture;

  // Filled in by the OverlayCandidateValidator to indicate whether this is a
  // promotable candidate or not.
  bool is_promotable_hint;
#endif

  // Stacking order of the overlay plane relative to the main surface,
  // which is 0. Signed to allow for "underlays".
  int plane_z_order;
  // True if the overlay does not have any visible quads on top of it. Set by
  // the strategy so the OverlayProcessor can consider subtracting damage caused
  // by underlay quads.
  bool is_unoccluded;

  // To be modified by the implementer if this candidate can go into
  // an overlay.
  bool overlay_handled;

  // Gpu fence to wait for before overlay is ready for display.
  unsigned gpu_fence_id;

 private:
  static bool FromDrawQuadResource(DisplayResourceProvider* resource_provider,
                                   const DrawQuad* quad,
                                   ResourceId resource_id,
                                   bool y_flipped,
                                   OverlayCandidate* candidate);
  static bool FromTextureQuad(DisplayResourceProvider* resource_provider,
                              const TextureDrawQuad* quad,
                              OverlayCandidate* candidate);
  static bool FromTileQuad(DisplayResourceProvider* resource_provider,
                           const TileDrawQuad* quad,
                           OverlayCandidate* candidate);
  static bool FromStreamVideoQuad(DisplayResourceProvider* resource_provider,
                                  const StreamVideoDrawQuad* quad,
                                  OverlayCandidate* candidate);
};

class VIZ_SERVICE_EXPORT OverlayCandidateList
    : public std::vector<OverlayCandidate> {
 public:
  OverlayCandidateList();
  OverlayCandidateList(const OverlayCandidateList&);
  OverlayCandidateList(OverlayCandidateList&&);
  ~OverlayCandidateList();

  OverlayCandidateList& operator=(const OverlayCandidateList&);
  OverlayCandidateList& operator=(OverlayCandidateList&&);

  // [id] == candidate's |display_rect| for all promotable resources.
  using PromotionHintInfoMap = std::map<ResourceId, gfx::RectF>;

  // For android, this provides a set of resources that could be promoted to
  // overlay, if one backs them with a SurfaceView.
  PromotionHintInfoMap promotion_hint_info_map_;

  // Helper to insert |candidate| into |promotion_hint_info_|.
  void AddPromotionHint(const OverlayCandidate& candidate);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_H_
