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
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display/aggregated_frame.h"
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
class StreamVideoDrawQuad;
class TextureDrawQuad;
class VideoHoleDrawQuad;

class VIZ_SERVICE_EXPORT OverlayCandidate {
 public:
  // When adding or changing these return status' check how the callee uses
  // these failure codes. Currently, these feed into the logging via the enum
  // |OverlayProcessorDelegated::DelegationStatus|.
  enum class CandidateStatus {
    kSuccess,
    kFailNotOverlay,
    kFailNotAxisAligned,
    kFailColorMatrix,
    kFailOpacity,
    kFailBlending,
    kFailQuadNotSupported,
    kFailVisible,
    kFailBufferFormat,
    kFailNearFilter,
    kFailPriority,
  };
  using TrackingId = uint32_t;
  static constexpr TrackingId kDefaultTrackingId{0};

  // Returns true if |quad| will not block quads underneath from becoming
  // an overlay.
  static bool IsInvisibleQuad(const DrawQuad* quad);

  // Returns true if any of the quads in the list given by |quad_list_begin|
  // and |quad_list_end| are visible and on top of |candidate|.
  static bool IsOccluded(const OverlayCandidate& candidate,
                         QuadList::ConstIterator quad_list_begin,
                         QuadList::ConstIterator quad_list_end);

  // Modifies the |candidate|'s |display_rect| to be clipped within |clip_rect|.
  // This function will also update the |uv_rect| based on what clipping was
  // applied to |display_rect|.
  static void ApplyClip(OverlayCandidate& candidate,
                        const gfx::RectF& clip_rect);

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
  // Optional HDR Metadata for the buffer.
  absl::optional<gfx::HDRMetadata> hdr_metadata;
  // Size of the resource, in pixels.
  gfx::Size resource_size_in_pixels;
  // Rect on the display to position the overlay to. Implementer must convert
  // to integer coordinates if setting |overlay_handled| to true.
  gfx::RectF display_rect;
  // Crop within the buffer to be placed inside |display_rect|.
  gfx::RectF uv_rect = gfx::RectF(0.f, 0.f, 1.f, 1.f);
  // Clip rect in the target content space after composition, or empty if the
  // quad is not clipped.
  absl::optional<gfx::Rect> clip_rect;
  // If the quad doesn't require blending.
  bool is_opaque = false;
  // If the quad has a mask filter.
  bool has_mask_filter = false;
  // Texture resource to present in an overlay.
  ResourceId resource_id = kInvalidResourceId;
  // Mailbox from resource_id. It is used by SkiaRenderer.
  gpu::Mailbox mailbox;

#if BUILDFLAG(IS_ANDROID)
  // For candidates from StreamVideoDrawQuads, this records whether the quad is
  // marked as being backed by a SurfaceTexture or not.  If so, it's not really
  // promotable to an overlay.
  bool is_backed_by_surface_texture = false;
  // Crop within the buffer to be placed inside |display_rect| before
  // |clip_rect| was applied. Valid only for surface control.
  gfx::RectF unclipped_uv_rect = gfx::RectF(0.f, 0.f, 1.f, 1.f);
  // |display_rect| before |clip_rect| was applied. Valid only for surface
  // control.
  gfx::RectF unclipped_display_rect = gfx::RectF(0.f, 0.f, 1.f, 1.f);
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
  float damage_area_estimate = 0.f;

  // Damage in buffer space (extents bound by |resource_size_in_pixels|).
  gfx::RectF damage_rect;

  static constexpr uint32_t kInvalidDamageIndex = UINT_MAX;
  // Damage index for |SurfaceDamageRectList|.
  uint32_t overlay_damage_index = kInvalidDamageIndex;

  // Is true if an HW overlay is required for the quad content.
  bool requires_overlay = false;

  // Represents either a background of this overlay candidate or a color of a
  // solid color quad, which can be checked via the |is_solid_color|.
  absl::optional<SkColor> color;

  // Helps to identify whether this is a solid color quad or not.
  bool is_solid_color = false;

  // If |rpdq| is present, then the renderer must draw the filter effects and
  // copy the result into the buffer backing of a render pass.
  const AggregatedRenderPassDrawQuad* rpdq = nullptr;
  // The DDL for generating render pass overlay buffer with SkiaRenderer. This
  // is the recorded output of rendering the |rpdq|.
  sk_sp<SkDeferredDisplayList> ddl;

  // The bounds in pixels of the rendered |rpdq|.
  // TODO(petermcneeley) : Refactor the usage of this member to be compatible
  // with |uv_rect| member in this class.
  gfx::RectF bounds_rect;

  // Quad |shared_quad_state| opacity is ubiquitous for quad types
  // AggregateRenderPassDrawQuad, TileDrawQuad, SolidColorDrawQuad. A delegate
  // context must support non opaque opacity for these types.
  float opacity = 1.0f;

  // Hints for overlay prioritization when delegated composition is used.
  gfx::OverlayPriorityHint priority_hint = gfx::OverlayPriorityHint::kNone;

  // Specifies the rounded corners of overlay candidate.
  gfx::RRectF rounded_corners;

  // A (ideally) unique key used to temporally identify a specific overlay
  // candidate. This key can have collisions more that would be expected by the
  // birthday paradox of 32 bits. If two or more candidates come from the same
  // surface and have the same |DrawQuad::rect| they will have the same
  // |tracking_id|.
  TrackingId tracking_id = kDefaultTrackingId;
};

using OverlayCandidateList = std::vector<OverlayCandidate>;

// This is a factory to help with the creation of |OverlayCandidates|.  On
// construction, this factory captures the required objects to create candidates
// from a draw quad.  Common computations for all possible candidates can be
// made at construction time. This class is const after construction and not
// copy/moveable to avoid capture ownership issues.
class VIZ_SERVICE_EXPORT OverlayCandidateFactory {
 public:
  using CandidateStatus = OverlayCandidate::CandidateStatus;

  OverlayCandidateFactory(const AggregatedRenderPass* render_pass,
                          DisplayResourceProvider* resource_provider,
                          const SurfaceDamageRectList* surface_damage_rect_list,
                          const SkM44* output_color_matrix,
                          const gfx::RectF primary_rect,
                          bool is_delegated_context = false);

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

  CandidateStatus FromStreamVideoQuad(const StreamVideoDrawQuad* quad,
                                      OverlayCandidate& candidate) const;

  CandidateStatus FromVideoHoleQuad(const VideoHoleDrawQuad* quad,
                                    OverlayCandidate& candidate) const;

  void HandleClipAndSubsampling(OverlayCandidate& candidate) const;

  void AssignDamage(const DrawQuad* quad, OverlayCandidate& candidate) const;

  // Damage returned from this function is in target content space.
  gfx::RectF GetDamageRect(const DrawQuad* quad,
                           const OverlayCandidate& candidate) const;

  const AggregatedRenderPass* render_pass_;
  DisplayResourceProvider* resource_provider_;
  const SurfaceDamageRectList* surface_damage_rect_list_;
  const SkM44* output_color_matrix_;
  const gfx::RectF primary_rect_;
  bool is_delegated_context_;

  // The union of all surface damages that are not specifically assigned to a
  // draw quad.
  gfx::Rect unassigned_surface_damage_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_H_
