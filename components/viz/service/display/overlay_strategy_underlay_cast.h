// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_UNDERLAY_CAST_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_UNDERLAY_CAST_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "build/chromecast_buildflags.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/overlay_transform.h"

#if BUILDFLAG(IS_CHROMECAST)
#include "chromecast/media/service/mojom/video_geometry_setter.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#endif

namespace viz {
// Similar to underlay strategy plus Cast-specific handling of content bounds.
class VIZ_SERVICE_EXPORT OverlayStrategyUnderlayCast
    : public OverlayStrategyUnderlay {
 public:
  explicit OverlayStrategyUnderlayCast(
      OverlayProcessorUsingStrategy* capability_checker);
  ~OverlayStrategyUnderlayCast() override;

  bool Attempt(const SkMatrix44& output_color_matrix,
               const OverlayProcessorInterface::FilterOperationsMap&
                   render_pass_backdrop_filters,
               DisplayResourceProvider* resource_provider,
               AggregatedRenderPassList* render_pass,
               SurfaceDamageRectList* surface_damage_rect_list,
               const PrimaryPlane* primary_plane,
               OverlayCandidateList* candidate_list,
               std::vector<gfx::Rect>* content_bounds) override;

  void ProposePrioritized(const SkMatrix44& output_color_matrix,
                          const OverlayProcessorInterface::FilterOperationsMap&
                              render_pass_backdrop_filters,
                          DisplayResourceProvider* resource_provider,
                          AggregatedRenderPassList* render_pass_list,
                          SurfaceDamageRectList* surface_damage_rect_list,
                          const PrimaryPlane* primary_plane,
                          OverlayProposedCandidateList* candidates,
                          std::vector<gfx::Rect>* content_bounds) override;

  bool AttemptPrioritized(
      const SkMatrix44& output_color_matrix,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_pass_list,
      SurfaceDamageRectList* surface_damage_rect_list,
      const PrimaryPlane* primary_plane,
      OverlayCandidateList* candidates,
      std::vector<gfx::Rect>* content_bounds,
      OverlayProposedCandidate* proposed_candidate) override;

  // Callback that's made whenever an overlay quad is processed in the
  // compositor. Used to allow hardware video plane to be positioned to match
  // compositor hole.
  using OverlayCompositedCallback =
      base::RepeatingCallback<void(const gfx::RectF&, gfx::OverlayTransform)>;
  static void SetOverlayCompositedCallback(const OverlayCompositedCallback& cb);

#if BUILDFLAG(IS_CHROMECAST)
  // In Chromecast build, OverlayStrategyUnderlayCast needs a valid mojo
  // interface to VideoGeometrySetter Service (shared by all instances of
  // OverlaystrategyUnderlayCast). This must be called before compositor starts.
  // Ideally, it can be called after compositor thread is created.
  // Must be called on compositor thread.
  static void ConnectVideoGeometrySetter(
      mojo::PendingRemote<chromecast::media::mojom::VideoGeometrySetter>
          video_geometry_setter);
#endif

  OverlayStrategy GetUMAEnum() const override;

 private:
  // Keep track if an overlay is being used on the previous frame.
  bool is_using_overlay_ = false;

  DISALLOW_COPY_AND_ASSIGN(OverlayStrategyUnderlayCast);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_UNDERLAY_CAST_H_
