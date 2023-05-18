// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_FACTORY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_FACTORY_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkDeferredDisplayList.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/gfx/overlay_transform.h"

namespace gfx {
class Rect;
}

namespace viz {
class AggregatedRenderPassDrawQuad;
class DisplayResourceProvider;
class SolidColorDrawQuad;
class TextureDrawQuad;
class VideoHoleDrawQuad;

// This is a factory to help with the creation of |OverlayCandidates|.  On
// construction, this factory captures the required objects to create candidates
// from a draw quad.  Common computations for all possible candidates can be
// made at construction time. This class is const after construction and not
// copy/moveable to avoid capture ownership issues.
class VIZ_SERVICE_EXPORT OverlayCandidateFactory {
 public:
  using CandidateStatus = OverlayCandidate::CandidateStatus;

  // The coordinate space of |render_pass| is the target space for candidates
  // produced by this factory.
  OverlayCandidateFactory(
      const AggregatedRenderPass* render_pass,
      DisplayResourceProvider* resource_provider,
      const SurfaceDamageRectList* surface_damage_rect_list,
      const SkM44* output_color_matrix,
      const gfx::RectF primary_rect,
      const OverlayProcessorInterface::FilterOperationsMap* render_pass_filters,
      bool is_delegated_context = false,
      bool supports_clip_rect = false,
      bool supports_arbitrary_transform = false,
      bool supports_rounded_display_masks = false);

  OverlayCandidateFactory(const OverlayCandidateFactory&) = delete;
  OverlayCandidateFactory& operator=(const OverlayCandidateFactory&) = delete;

  ~OverlayCandidateFactory();

  // Returns |kSuccess| and fills in |candidate| if |draw_quad| is of a known
  // quad type and contains an overlayable resource. |primary_rect| can be empty
  // in the case of a null primary plane. |candidate| is expected to be a
  // freshly constructed |OverlayCandidate| object.
  CandidateStatus FromDrawQuad(const DrawQuad* quad,
                               OverlayCandidate& candidate) const;

  // Returns an estimate of this |quad|'s actual visible damage area as float
  // pixels squared. This visible damage is computed by combining from input
  // |surface_damage_rect_list_| with the occluding rects in the quad_list. This
  // is an estimate since the occluded damage area is calculated on a per quad
  // basis. The |quad_list_begin| and |quad_list_end| provide the range of valid
  // occluders of this |candidate|.
  // TODO(petermcneeley): Can we replace this with |visible_rect| in |DrawQuad|?
  float EstimateVisibleDamage(const DrawQuad* quad,
                              const OverlayCandidate& candidate,
                              QuadList::ConstIterator quad_list_begin,
                              QuadList::ConstIterator quad_list_end) const;

  // Returns true if any of the quads in the list given by |quad_list_begin|
  // and |quad_list_end| have an associated filter and occlude |candidate|.
  bool IsOccludedByFilteredQuad(
      const OverlayCandidate& candidate,
      QuadList::ConstIterator quad_list_begin,
      QuadList::ConstIterator quad_list_end,
      const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
          render_pass_backdrop_filters) const;

  // Returns true if any of the quads in the list given by |quad_list_begin|
  // and |quad_list_end| occlude |candidate|.
  bool IsOccluded(const OverlayCandidate& candidate,
                  QuadList::ConstIterator quad_list_begin,
                  QuadList::ConstIterator quad_list_end) const;

  gfx::Rect GetUnassignedDamage() { return unassigned_surface_damage_; }

 private:
  CandidateStatus FromDrawQuadResource(const DrawQuad* quad,
                                       ResourceId resource_id,
                                       bool y_flipped,
                                       OverlayCandidate& candidate) const;

  CandidateStatus FromTextureQuad(const TextureDrawQuad* quad,
                                  OverlayCandidate& candidate) const;

  CandidateStatus FromTileQuad(const TileDrawQuad* quad,
                               OverlayCandidate& candidate) const;

  CandidateStatus FromAggregateQuad(const AggregatedRenderPassDrawQuad* quad,
                                    OverlayCandidate& candidate) const;

  CandidateStatus FromSolidColorQuad(const SolidColorDrawQuad* quad,
                                     OverlayCandidate& candidate) const;

  CandidateStatus FromVideoHoleQuad(const VideoHoleDrawQuad* quad,
                                    OverlayCandidate& candidate) const;

  void HandleClipAndSubsampling(OverlayCandidate& candidate) const;

  void AssignDamage(const DrawQuad* quad, OverlayCandidate& candidate) const;

  // Damage returned from this function is in target space.
  // If quad doesn't have damage from the surface damage list, this returns the
  // intersection of unassigned damage and the smallest axis-aligned rectangle
  // containing |display_rect| in target space.
  gfx::RectF GetDamageRect(const DrawQuad* quad,
                           const OverlayCandidate& candidate) const;

  gfx::RectF GetDamageEstimate(const OverlayCandidate& candidate) const;

  // Apply clipping "geometrically" by adjusting the |quad->rect| and
  // |quad->uv_rect|. May return CandidateStatus::kFailVisible if the clipping
  // to be applied is empty.
  CandidateStatus DoGeometricClipping(const DrawQuad* quad,
                                      OverlayCandidate& candidate) const;

  raw_ptr<const AggregatedRenderPass> render_pass_;
  raw_ptr<DisplayResourceProvider> resource_provider_;
  raw_ptr<const SurfaceDamageRectList> surface_damage_rect_list_;
  const gfx::RectF primary_rect_;
  raw_ptr<const OverlayProcessorInterface::FilterOperationsMap>
      render_pass_filters_;
  const bool is_delegated_context_;
  const bool supports_clip_rect_;
  const bool supports_arbitrary_transform_;
  const bool supports_rounded_display_masks_;

  // The union of all surface damages that are not specifically assigned to a
  // draw quad.
  gfx::Rect unassigned_surface_damage_;
  bool has_custom_color_matrix_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_FACTORY_H_
