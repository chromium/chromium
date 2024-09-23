// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_H_

#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/private/chromium/GrDeferredDisplayList.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/overlay_type.h"
#include "ui/gfx/video_types.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace viz {
class AggregatedRenderPassDrawQuad;
class DrawQuad;
class DisplayResourceProvider;

class VIZ_SERVICE_EXPORT OverlayCandidate {
 public:
  // When adding or changing these return status' check how the callee uses
  // these failure codes. Currently, these feed into the logging via the enum
  // |OverlayProcessorDelegated::DelegationStatus|.
  enum class CandidateStatus {
    kSuccess,
    kFailNotOverlay,
    kFailNotAxisAligned,
    kFailNotAxisAligned3dTransform,
    kFailNotAxisAligned2dShear,
    kFailNotAxisAligned2dRotation,
    kFailColorMatrix,
    kFailOpacity,
    kFailBlending,
    kFailQuadNotSupported,
    kFailVisible,
    kFailBufferFormat,
    kFailNearFilter,
    kFailPriority,
    kFailRoundedDisplayMasksNotSupported,
    kFailMaskFilterNotSupported,
    kFailHasTransformButCantClip,
    kFailRpdqWithTransform,
  };
  using TrackingId = uint32_t;
  static constexpr TrackingId kDefaultTrackingId{0};

  // Returns true if |quad| will not block quads underneath from becoming
  // an overlay.
  static bool IsInvisibleQuad(const DrawQuad* quad);

  // Returns true if `quad` contains rounded display masks textures.
  static bool QuadHasRoundedDisplayMasks(const DrawQuad* quad);

  // Modifies the |candidate|'s |display_rect| to be clipped within |clip_rect|.
  // This function will also update the |uv_rect| based on what clipping was
  // applied to |display_rect|.
  // |clip_rect| should be in the same space as |candidate|'s |display_rect|,
  // and |candidate| should not have an arbitrary transform.
  static void ApplyClip(OverlayCandidate& candidate,
                        const gfx::RectF& clip_rect);

  // Returns true if the |quad| cannot be displayed on the main plane. This is
  // used in conjuction with protected content that can't be GPU composited and
  // will be shown via an overlay.
  static bool RequiresOverlay(const DrawQuad* quad);

  // Returns |candidate|'s |display_rect| transformed to its target space.
  // If |candidate| holds an arbitrary transform, this will be the smallest axis
  // aligned bounding rect containing |transform| applied to |display_rect|.
  // If |candidate| holds an overlay transform, this will just be
  // |display_rect|, which is already in its target space.
  static gfx::RectF DisplayRectInTargetSpace(const OverlayCandidate& candidate);

  OverlayCandidate();
  OverlayCandidate(const OverlayCandidate& other);
  ~OverlayCandidate();

  // If the quad doesn't require blending.
  bool is_opaque : 1 = false;
  // If the quad has a mask filter.
  bool has_mask_filter : 1 = false;
  // To be modified by the implementer if this candidate can go into
  // an overlay.
  bool overlay_handled : 1 = false;

  // Is true if an HW overlay is required for the quad content.
  bool requires_overlay = false;

  // Helps to identify whether this is a solid color quad or not.
  bool is_solid_color : 1 = false;

  // Helps to identify whether this candidate has rounded-display masks or not.
  bool has_rounded_display_masks : 1 = false;

  // Whether this overlay candidate represents the root render pass.
  bool is_root_render_pass : 1 = false;

  // Whether this overlay candidate is a render pass draw quad.
  bool is_render_pass_draw_quad : 1 = false;

  // If we need nearest neighbor filter for displaying this overlay.
  bool nearest_neighbor_filter : 1 = false;

  // If true, we need to run a detiling image processor on the quad before we
  // can scan it out.
  bool needs_detiling : 1 = false;

  // Rect in content space that, when combined with |transform|, is the bounds
  // to position the overlay to. When |transform| is a |gx::OverlayTransform|,
  // this is the bounds of the quad rect with its transform applied, so that
  // content and target space for this overlay are the same.
  //
  // Implementer must convert to integer coordinates if setting
  // |overlay_handled| to true.
  gfx::RectF display_rect;

  // Format of the buffer to scanout.
  SharedImageFormat format = SinglePlaneFormat::kRGBA_8888;

  gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kClear;

  // Hints for overlay prioritization when delegated composition is used.
  gfx::OverlayPriorityHint priority_hint = gfx::OverlayPriorityHint::kNone;

  // ColorSpace of the buffer for scanout.
  gfx::ColorSpace color_space;
  // Optional HDR Metadata for the buffer.
  gfx::HDRMetadata hdr_metadata;
  // Size of the resource, in pixels.
  gfx::Size resource_size_in_pixels;

  // Crop within the buffer to be placed inside |display_rect|.
  gfx::RectF uv_rect = gfx::RectF(0.f, 0.f, 1.f, 1.f);
  // Clip rect in the target space after composition, or nullopt if the quad is
  // not clipped.
  std::optional<gfx::Rect> clip_rect;

  // Texture resource to present in an overlay.
  ResourceId resource_id = kInvalidResourceId;
  // Mailbox from resource_id. It is used by SkiaRenderer.
  gpu::Mailbox mailbox;

#if BUILDFLAG(IS_WIN)
  // Indication of the overlay to be detected as possible full screen
  // letterboxing.
  // During video display, sometimes the video image does not have the same
  // shape or Picture Aspect Ratio as the display area. Letterboxing is the
  // process of scaling a widescreen image to fit a specific display, like 4:3.
  // The reverse case, scaling a 4:3 image to fit a widescreen display, is
  // sometimes called pillarboxing. However here letterboxing is also used in a
  // general sense, to mean scaling a video image to fit any given display area.
  // Check out more information from
  // https://learn.microsoft.com/en-us/windows/win32/medfound/picture-aspect-ratio#letterboxing.
  // Two conditions to make possible_video_fullscreen_letterboxing be true:
  // 1. Current page is in full screen mode which is decided by
  // AggregatedFrame::page_fullscreen_mode.
  // 2. IsPossibleFullScreenLetterboxing helper from
  // DCLayerOverlayProcessor returns true, which basically means the draw
  // quad beneath the overlay quad touches two sides of the screen while
  // starting at display origin (0, 0). Then before swap chain presentation and
  // with possible_video_fullscreen_letterboxing be true, some necessary
  // adjustment is done in order to make the video be equidistant from the sides
  // off the screen. That is, it needs to be CENTERED for the sides that are not
  // touching the screen. At this point, Desktop Window Manager(DWM) considers
  // the video as full screen letterboxing.
  bool possible_video_fullscreen_letterboxing = false;
#endif

#if BUILDFLAG(IS_ANDROID)
  // Is video using SurfaceView-like architecture. It's currently actually uses
  // `DialogOverlay` in browser instead of actual SurfaceView. But "SurfaceView"
  // is used throughout the code so is used here as well for consistency.
  bool is_video_in_surface_view = false;
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

  // The total area in square pixels of damage for this candidate's quad. This
  // is an estimate when 'EstimateOccludedDamage' function is used.
  float damage_area_estimate = 0.f;

  // Damage in viz Display space, the same space as |display_rect|;
  gfx::RectF damage_rect;

  static constexpr uint32_t kInvalidDamageIndex = UINT_MAX;
  // Damage index for |SurfaceDamageRectList|.
  uint32_t overlay_damage_index = kInvalidDamageIndex;

  // Represents either a background of this overlay candidate or a color of a
  // solid color quad, which can be checked via the |is_solid_color|.
  std::optional<SkColor4f> color;

  // If |rpdq| is present, then the renderer must draw the filter effects and
  // copy the result into the buffer backing of a render pass.
  raw_ptr<const AggregatedRenderPassDrawQuad, DanglingUntriaged> rpdq = nullptr;

  // Quad |shared_quad_state| opacity is ubiquitous for quad types
  // AggregateRenderPassDrawQuad, TileDrawQuad, SolidColorDrawQuad. A delegate
  // context must support non opaque opacity for these types.
  float opacity = 1.0f;

  // Specifies the rounded corners of overlay candidate, in target space.
  gfx::RRectF rounded_corners;

#if BUILDFLAG(IS_APPLE)
  // Layers in a non-zero sorting context exist in the same 3D space and should
  // intersect.
  unsigned sorting_context_id = 0;

  // The edge anti-aliasing mask property for the CALayer.
  unsigned edge_aa_mask = 0;
#endif  // BUILDFLAG(IS_APPLE)

  // A (ideally) unique key used to temporally identify a specific overlay
  // candidate. This key can have collisions more that would be expected by the
  // birthday paradox of 32 bits. If two or more candidates come from the same
  // surface and have the same |DrawQuad::rect| they will have the same
  // |tracking_id|.
  TrackingId tracking_id = kDefaultTrackingId;

  // A layer ID packing that includes the namespace.
  // See |SharedQuadState::layer_id| and |SharedQuadState::layer_namespace_id|.
  uint64_t aggregated_layer_id = 0;

  // Transformation to apply to layer during composition.
  // Note: A |gfx::OverlayTransform| transforms the buffer within its bounds and
  // does not affect |display_rect|.
  absl::variant<gfx::OverlayTransform, gfx::Transform> transform =
      gfx::OVERLAY_TRANSFORM_NONE;

  // Default overlay type.
  gfx::OverlayType overlay_type = gfx::OverlayType::kSimple;
};

using OverlayCandidateList = std::vector<OverlayCandidate>;

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_H_
