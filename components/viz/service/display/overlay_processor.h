// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/service/display/ca_layer_overlay.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/viz_service_export.h"

namespace cc {
class DisplayResourceProvider;
}

namespace viz {
class OutputSurface;

class VIZ_SERVICE_EXPORT OverlayProcessor {
 public:
  // Enum used for UMA histogram. These enum values must not be changed or
  // reused.
  enum class StrategyType {
    kUnknown = 0,
    kNoStrategyUsed = 1,
    kFullscreen = 2,
    kSingleOnTop = 3,
    kUnderlay = 4,
    kUnderlayCast = 5,
    kMaxValue = kUnderlayCast,
  };

  using FilterOperationsMap =
      base::flat_map<RenderPassId, cc::FilterOperations*>;

  class VIZ_SERVICE_EXPORT Strategy {
   public:
    virtual ~Strategy() {}
    // Returns false if the strategy cannot be made to work with the
    // current set of render passes. Returns true if the strategy was successful
    // and adds any additional passes necessary to represent overlays to
    // |render_passes|.
    virtual bool Attempt(
        const SkMatrix44& output_color_matrix,
        const FilterOperationsMap& render_pass_backdrop_filters,
        DisplayResourceProvider* resource_provider,
        RenderPass* render_pass,
        OverlayCandidateList* candidates,
        std::vector<gfx::Rect>* content_bounds) = 0;

    virtual StrategyType GetUMAEnum() const;
  };
  using StrategyList = std::vector<std::unique_ptr<Strategy>>;

  explicit OverlayProcessor(OutputSurface* surface);
  virtual ~OverlayProcessor();
  // Virtual to allow testing different strategies.
  virtual void Initialize();

  gfx::Rect GetAndResetOverlayDamage();

  // Attempt to replace quads from the specified root render pass with overlays
  // or CALayers. This must be called every frame.
  void ProcessForOverlays(
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_passes,
      const SkMatrix44& output_color_matrix,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      OverlayCandidateList* overlay_candidates,
      CALayerOverlayList* ca_layer_overlays,
      DCLayerOverlayList* dc_layer_overlays,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds);

  // Determine if we can eliminate (all remaining quads are black or
  // transparent) or crop (non-black content is a small sub-rectangle) the
  // primary framebuffer.
  static void EliminateOrCropPrimary(
      const QuadList& quad_list,
      const QuadList::Iterator& candidate_iterator,
      OverlayCandidate* primary,
      OverlayCandidateList* candidate_list);

 protected:
  StrategyList strategies_;
  OutputSurface* surface_;
  gfx::Rect overlay_damage_rect_;
  gfx::Rect previous_frame_underlay_rect_;
  bool previous_frame_underlay_was_unoccluded_ = false;

 private:
  bool ProcessForCALayers(
      DisplayResourceProvider* resource_provider,
      RenderPass* render_pass,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      OverlayCandidateList* overlay_candidates,
      CALayerOverlayList* ca_layer_overlays,
      gfx::Rect* damage_rect);
  bool ProcessForDCLayers(
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_passes,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      OverlayCandidateList* overlay_candidates,
      DCLayerOverlayList* dc_layer_overlays,
      gfx::Rect* damage_rect);
  // Update |damage_rect| by removing damage casued by |candidates|.
  void UpdateDamageRect(OverlayCandidateList* candidates,
                        const gfx::Rect& previous_frame_underlay_rect,
                        bool previous_frame_underlay_was_unoccluded,
                        gfx::Rect* damage_rect);

  DCLayerOverlayProcessor dc_processor_;

  DISALLOW_COPY_AND_ASSIGN(OverlayProcessor);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_H_
