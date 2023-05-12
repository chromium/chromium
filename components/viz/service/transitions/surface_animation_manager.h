// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_TRANSITIONS_SURFACE_ANIMATION_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_TRANSITIONS_SURFACE_ANIMATION_MANAGER_H_

#include <limits>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display/shared_bitmap_manager.h"
#include "components/viz/service/surfaces/surface_saved_frame.h"
#include "components/viz/service/transitions/transferable_resource_tracker.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframe_effect.h"
#include "ui/gfx/animation/keyframe/keyframe_model.h"

namespace viz {

class Surface;
struct ReturnedResource;
struct TransferableResource;

// This class is responsible for managing a single transition sequence. Each
// sequence has save/animate/release directives in that order. Instances of this
// class are 1:1 with this sequence.
//
// This class is owned by CompositorFrameSinkSupport but can be moved between
// CompositorFrameSinkSupports for transitions between 2 renderer CC instances.
class VIZ_SERVICE_EXPORT SurfaceAnimationManager {
 public:
  using TransitionDirectiveCompleteCallback =
      base::OnceCallback<void(const CompositorFrameTransitionDirective&)>;

  static std::unique_ptr<SurfaceAnimationManager> CreateWithSave(
      const CompositorFrameTransitionDirective& directive,
      Surface* surface,
      SharedBitmapManager* shared_bitmap_manager,
      TransitionDirectiveCompleteCallback sequence_id_finished_callback);

  ~SurfaceAnimationManager();

  void Animate();

  // Resource ref count management.
  void RefResources(const std::vector<TransferableResource>& resources);
  void UnrefResources(const std::vector<ReturnedResource>& resources);

  // Replaced ViewTransitionElementResourceIds with corresponding ResourceIds if
  // necessary.
  void ReplaceSharedElementResources(Surface* surface);

  void CompleteSaveForTesting();

 private:
  friend class SurfaceAnimationManagerTest;

  SurfaceAnimationManager(
      const CompositorFrameTransitionDirective& directive,
      Surface* surface,
      SharedBitmapManager* shared_bitmap_manager,
      TransitionDirectiveCompleteCallback sequence_id_finished_callback);

  bool ProcessSaveDirective(const CompositorFrameTransitionDirective& directive,
                            Surface* surface);

  bool FilterSharedElementsWithRenderPassOrResource(
      std::vector<TransferableResource>* resource_list,
      const base::flat_map<ViewTransitionElementResourceId,
                           CompositorRenderPass*>* element_id_to_pass,
      const DrawQuad& quad,
      CompositorRenderPass& copy_pass);

  bool animating_ = false;
  TransferableResourceTracker transferable_resource_tracker_;

  std::unique_ptr<SurfaceSavedFrame> saved_frame_;
  base::flat_set<ViewTransitionElementResourceId> empty_resource_ids_;

  absl::optional<TransferableResourceTracker::ResourceFrame> saved_textures_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_TRANSITIONS_SURFACE_ANIMATION_MANAGER_H_
