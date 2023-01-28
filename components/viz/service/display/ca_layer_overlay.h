// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_CA_LAYER_OVERLAY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_CA_LAYER_OVERLAY_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/ca_layer_result.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/ca_renderer_layer_params.h"

namespace viz {
class AggregatedRenderPassDrawQuad;
class DisplayResourceProvider;
class DrawQuad;

// Holds information that is frequently shared between consecutive
// CALayerOverlays.
class VIZ_SERVICE_EXPORT CALayerOverlaySharedState
    : public base::RefCountedThreadSafe<CALayerOverlaySharedState> {
 public:
  CALayerOverlaySharedState() {}
  // Layers in a non-zero sorting context exist in the same 3D space and should
  // intersect.
  unsigned sorting_context_id = 0;
  // If |is_clipped| is true, then clip to |clip_rect| in the target space, and
  // |clip_rect_corner_radius| represents the corner radius of the clip rect.
  bool is_clipped = false;
  gfx::RectF clip_rect;
  gfx::RRectF rounded_corner_bounds;
  // The transform to apply to the CALayer.
  gfx::Transform transform;

 private:
  friend class base::RefCountedThreadSafe<CALayerOverlaySharedState>;
  ~CALayerOverlaySharedState() {}
};

// Holds all information necessary to construct a CALayer from a DrawQuad.
class VIZ_SERVICE_EXPORT CALayerOverlay {
 public:
  CALayerOverlay();
  CALayerOverlay(const CALayerOverlay& other);
  ~CALayerOverlay();
  CALayerOverlay& operator=(const CALayerOverlay& other);

  // State that is frequently shared between consecutive CALayerOverlays.
  scoped_refptr<CALayerOverlaySharedState> shared_state;

  // Texture that corresponds to an IOSurface to set as the content of the
  // CALayer. If this is 0 then the CALayer is a solid color.
  ResourceId contents_resource_id = kInvalidResourceId;
  // Mailbox from contents_resource_id. It is used by SkiaRenderer.
  gpu::Mailbox mailbox;
  // The contents rect property for the CALayer.
  gfx::RectF contents_rect;
  // The bounds for the CALayer in pixels.
  gfx::RectF bounds_rect;
  // The opacity property for the CAayer.
  float opacity = 1;
  // The background color property for the CALayer.
  SkColor4f background_color = SkColors::kTransparent;
  // The edge anti-aliasing mask property for the CALayer.
  unsigned edge_aa_mask = 0;
  // The minification and magnification filters for the CALayer.
  unsigned filter = 0;
  // The HDR mode for this quad.
  gfx::HDRMode hdr_mode = gfx::HDRMode::kDefault;
  // The HDR metadata for this quad.
  absl::optional<gfx::HDRMetadata> hdr_metadata;
  // The protected video status of the AVSampleBufferDisplayLayer.
  gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kClear;
  // If |rpdq| is present, then the renderer must draw the filter effects and
  // copy the result into an IOSurface.
  const AggregatedRenderPassDrawQuad* rpdq = nullptr;
};

typedef std::vector<CALayerOverlay> CALayerOverlayList;

// TODO(weiliangc): Eventually fold this class into OverlayProcessorMac and fold
// CALayerOverlay into OverlayCandidate.
class VIZ_SERVICE_EXPORT CALayerOverlayProcessor {
 public:
  CALayerOverlayProcessor();
  CALayerOverlayProcessor(const CALayerOverlayProcessor&) = delete;
  CALayerOverlayProcessor& operator=(const CALayerOverlayProcessor&) = delete;

  virtual ~CALayerOverlayProcessor() = default;

  void SetIsVideoCaptureEnabled(bool enabled) {
    video_capture_enabled_ = enabled;
  }
  bool AreClipSettingsValid(const CALayerOverlay& ca_layer_overlay,
                            CALayerOverlayList* ca_layer_overlay_list) const;
  void PutForcedOverlayContentIntoUnderlays(
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPass* render_pass,
      const gfx::RectF& display_rect,
      QuadList* quad_list,
      const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
          render_pass_filters,
      const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
          render_pass_backdrop_filters,
      CALayerOverlayList* ca_layer_overlays) const;

  // Returns true if all quads in the root render pass have been replaced by
  // CALayerOverlays. Virtual for testing.
  virtual bool ProcessForCALayerOverlays(
      AggregatedRenderPass* render_passes,
      DisplayResourceProvider* resource_provider,
      const gfx::RectF& display_rect,
      const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
          render_pass_filters,
      const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
          render_pass_backdrop_filters,
      CALayerOverlayList* ca_layer_overlays);

  gfx::CALayerResult ca_layer_result() { return ca_layer_result_; }

 private:
  // Returns whether future candidate quads should be considered
  bool PutQuadInSeparateOverlay(
      QuadList::Iterator at,
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPass* render_pass,
      const gfx::RectF& display_rect,
      const DrawQuad* quad,
      const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
          render_pass_filters,
      const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
          render_pass_backdrop_filters,
      gfx::ProtectedVideoType protected_video_type,
      CALayerOverlayList* ca_layer_overlays) const;

  void SaveCALayerResult(gfx::CALayerResult result);

  // Set to false if the APIs required for overlays are not present, or the
  // feature has been disabled.
  const bool overlays_allowed_;

  // Controls the feature of replacying all quads with overlays is enabled.
  const bool enable_ca_renderer_;

  // Controls the feature of putting HDR videos into underlays if the
  // CARenderer fails (so that we can use the tone mapping provided by macOS).
  const bool enable_hdr_underlays_;

  // The CARenderer is disabled when video capture is enabled.
  // https://crbug.com/836351, https://crbug.com/1290384
  bool video_capture_enabled_ = false;

  size_t max_quad_list_size_for_videos_ = 0;

  // The error code in ProcessForCALayerOverlays()
  gfx::CALayerResult ca_layer_result_ = gfx::kCALayerSuccess;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_CA_LAYER_OVERLAY_H_
