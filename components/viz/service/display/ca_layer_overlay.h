// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_CA_LAYER_OVERLAY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_CA_LAYER_OVERLAY_H_

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/ca_layer_result.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/ca_renderer_layer_params.h"

namespace viz {
class DisplayResourceProvider;
class DrawQuad;

// TODO(weiliangc): Eventually fold this class into OverlayProcessorMac.
class VIZ_SERVICE_EXPORT CALayerOverlayProcessor {
 public:
  CALayerOverlayProcessor();
  CALayerOverlayProcessor(const CALayerOverlayProcessor&) = delete;
  CALayerOverlayProcessor& operator=(const CALayerOverlayProcessor&) = delete;

  virtual ~CALayerOverlayProcessor() = default;

  bool AreClipSettingsValid(const OverlayCandidate& ca_layer_overlay,
                            OverlayCandidateList* ca_layer_overlay_list) const;
  void PutForcedOverlayContentIntoUnderlays(
      const DisplayResourceProvider* resource_provider,
      AggregatedRenderPass* render_pass,
      const gfx::RectF& display_rect,
      QuadList* quad_list,
      const base::flat_map<AggregatedRenderPassId,
                           raw_ptr<cc::FilterOperations, CtnExperimental>>&
          render_pass_filters,
      const base::flat_map<AggregatedRenderPassId,
                           raw_ptr<cc::FilterOperations, CtnExperimental>>&
          render_pass_backdrop_filters,
      OverlayCandidateList* ca_layer_overlays) const;

  // Returns true if all quads in the root render pass have been replaced by
  // CALayerOverlays. Virtual for testing.
  virtual bool ProcessForCALayerOverlays(
      AggregatedRenderPass* render_passes,
      const DisplayResourceProvider* resource_provider,
      const gfx::RectF& display_rect,
      const base::flat_map<AggregatedRenderPassId,
                           raw_ptr<cc::FilterOperations, CtnExperimental>>&
          render_pass_filters,
      const base::flat_map<AggregatedRenderPassId,
                           raw_ptr<cc::FilterOperations, CtnExperimental>>&
          render_pass_backdrop_filters,
      OverlayCandidateList* ca_layer_overlays);

  gfx::CALayerResult ca_layer_result() { return ca_layer_result_; }

 private:
  // Returns whether future candidate quads should be considered
  bool PutQuadInSeparateOverlay(
      QuadList::Iterator at,
      const DisplayResourceProvider* resource_provider,
      AggregatedRenderPass* render_pass,
      const gfx::RectF& display_rect,
      const DrawQuad* quad,
      const base::flat_map<AggregatedRenderPassId,
                           raw_ptr<cc::FilterOperations, CtnExperimental>>&
          render_pass_filters,
      const base::flat_map<AggregatedRenderPassId,
                           raw_ptr<cc::FilterOperations, CtnExperimental>>&
          render_pass_backdrop_filters,
      gfx::ProtectedVideoType protected_video_type,
      OverlayCandidateList* ca_layer_overlays) const;

  void SaveCALayerResult(gfx::CALayerResult result);

  // Set to false if the APIs required for overlays are not present, or the
  // feature has been disabled.
  const bool overlays_allowed_;

  // Controls the feature of replacying all quads with overlays is enabled.
  const bool enable_ca_renderer_;

  // Controls the feature of putting HDR videos into underlays if the
  // CARenderer fails (so that we can use the tone mapping provided by macOS).
  const bool enable_hdr_underlays_;

  size_t layer_limit_with_many_videos_ = 0;
  size_t layer_limit_default_ = 0;

  // The error code in ProcessForCALayerOverlays()
  gfx::CALayerResult ca_layer_result_ = gfx::kCALayerSuccess;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_CA_LAYER_OVERLAY_H_
