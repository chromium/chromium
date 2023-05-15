// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "build/build_config.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkDeferredDisplayList.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/video_types.h"

namespace gfx {
class Rect;
}

namespace viz {
class AggregatedRenderPassDrawQuad;
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
    kFailNotSharedImage,
    kFailRoundedDisplayMasksNotSupported,
  };
  using TrackingId = uint32_t;
  static constexpr TrackingId kDefaultTrackingId{0};

  // Returns true if |quad| will not block quads underneath from becoming
  // an overlay.
  static bool IsInvisibleQuad(const DrawQuad* quad);

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

  // Transformation to apply to layer during composition.
  // Note: A |gfx::OverlayTransform| transforms the buffer within its bounds and
  // does not affect |display_rect|.
  absl::variant<gfx::OverlayTransform, gfx::Transform> transform =
      gfx::OVERLAY_TRANSFORM_NONE;
  // Format of the buffer to scanout.
  gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  // ColorSpace of the buffer for scanout.
  gfx::ColorSpace color_space;
  // HDR mode for the buffer.
  gfx::HDRMode hdr_mode = gfx::HDRMode::kDefault;
  // Optional HDR Metadata for the buffer.
  absl::optional<gfx::HDRMetadata> hdr_metadata;
  // Size of the resource, in pixels.
  gfx::Size resource_size_in_pixels;
  // Rect in content space that, when combined with |transform|, is the bounds
  // to position the overlay to. When |transform| is a |gx::OverlayTransform|,
  // this is the bounds of the quad rect with its transform applied, so that
  // content and target space for this overlay are the same.
  //
  // Implementer must convert to integer coordinates if setting
  // |overlay_handled| to true.
  gfx::RectF display_rect;
  // Crop within the buffer to be placed inside |display_rect|.
  gfx::RectF uv_rect = gfx::RectF(0.f, 0.f, 1.f, 1.f);
  // Clip rect in the target space after composition, or nullopt if the quad is
  // not clipped.
  absl::optional<gfx::Rect> clip_rect;
  // If the quad doesn't require blending.
  bool is_opaque = false;
  // If the quad has a mask filter.
  bool has_mask_filter = false;
  // Texture resource to present in an overlay.
  ResourceId resource_id = kInvalidResourceId;
  // Mailbox from resource_id. It is used by SkiaRenderer.
  gpu::Mailbox mailbox;

  gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kClear;

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
  // For candidates from TextureDrawQuads with is_stream_video set to true, this
  // records whether the quad is marked as being backed by a SurfaceTexture or
  // not.  If so, it's not really promotable to an overlay.
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

  // The total area in square pixels of damage for this candidate's quad. This
  // is an estimate when 'EstimateOccludedDamage' function is used.
  float damage_area_estimate = 0.f;

  // Damage in viz Display space, the same space as |display_rect|;
  gfx::RectF damage_rect;

  static constexpr uint32_t kInvalidDamageIndex = UINT_MAX;
  // Damage index for |SurfaceDamageRectList|.
  uint32_t overlay_damage_index = kInvalidDamageIndex;

  // Is true if an HW overlay is required for the quad content.
  bool requires_overlay = false;

  // Represents either a background of this overlay candidate or a color of a
  // solid color quad, which can be checked via the |is_solid_color|.
  absl::optional<SkColor4f> color;

  // Helps to identify whether this is a solid color quad or not.
  bool is_solid_color = false;

  // Helps to identify whether this candidate has rounded-display masks or not.
  bool has_rounded_display_masks = false;

  // If |rpdq| is present, then the renderer must draw the filter effects and
  // copy the result into the buffer backing of a render pass.
  // This field is not a raw_ptr<> because of missing |.get()| in not-rewritten
  // platform specific code.
  RAW_PTR_EXCLUSION const AggregatedRenderPassDrawQuad* rpdq = nullptr;
  // The DDL for generating render pass overlay buffer with SkiaRenderer. This
  // is the recorded output of rendering the |rpdq|.
  sk_sp<SkDeferredDisplayList> ddl;

  // Quad |shared_quad_state| opacity is ubiquitous for quad types
  // AggregateRenderPassDrawQuad, TileDrawQuad, SolidColorDrawQuad. A delegate
  // context must support non opaque opacity for these types.
  float opacity = 1.0f;

  // Hints for overlay prioritization when delegated composition is used.
  gfx::OverlayPriorityHint priority_hint = gfx::OverlayPriorityHint::kNone;

  // Specifies the rounded corners of overlay candidate, in target space.
  gfx::RRectF rounded_corners;

  // Layers in a non-zero sorting context exist in the same 3D space and should
  // intersect.
  unsigned sorting_context_id = 0;

  // The edge anti-aliasing mask property for the CALayer.
  unsigned edge_aa_mask = 0;

  // If we need nearest neighbor filter for displaying this overlay.
  bool nearest_neighbor_filter = false;

  // A (ideally) unique key used to temporally identify a specific overlay
  // candidate. This key can have collisions more that would be expected by the
  // birthday paradox of 32 bits. If two or more candidates come from the same
  // surface and have the same |DrawQuad::rect| they will have the same
  // |tracking_id|.
  TrackingId tracking_id = kDefaultTrackingId;

  // Whether this overlay candidate represents the root render pass.
  bool is_root_render_pass = false;
};

using OverlayCandidateList = std::vector<OverlayCandidate>;

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_H_
