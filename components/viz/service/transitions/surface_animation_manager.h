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
#include "components/viz/service/frame_sinks/surface_resource_holder.h"
#include "components/viz/service/surfaces/surface_saved_frame.h"
#include "components/viz/service/transitions/transferable_resource_tracker.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframe_effect.h"
#include "ui/gfx/animation/keyframe/keyframe_model.h"

namespace gpu {
class SharedImageInterface;
}

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
class VIZ_SERVICE_EXPORT SurfaceAnimationManager
    : public ReservedResourceDelegate {
 public:
  using SaveDirectiveCompleteCallback =
      base::OnceCallback<void(const CompositorFrameTransitionDirective&)>;

  static std::unique_ptr<SurfaceAnimationManager> CreateWithSave(
      const CompositorFrameTransitionDirective& directive,
      Surface* surface,
      SharedBitmapManager* shared_bitmap_manager,
      gpu::SharedImageInterface* shared_image_interface,
      ReservedResourceIdTracker* id_tracker,
      SaveDirectiveCompleteCallback sequence_id_finished_callback);

  // Replaces ViewTransitionElementResourceIds with corresponding ResourceIds if
  // necessary.
  static void ReplaceSharedElementResources(
      Surface* surface,
      const base::flat_map<blink::ViewTransitionToken,
                           std::unique_ptr<SurfaceAnimationManager>>&
          token_to_animation_manager);

  ~SurfaceAnimationManager() override;

  // Returns false if it is invalid to start the animation phase.
  bool Animate();

  // ReservedResourceDelegate:
  void ReceiveFromChild(
      const std::vector<TransferableResource>& resources) override;
  void RefResources(
      const std::vector<TransferableResource>& resources) override;
  void UnrefResources(const std::vector<ReturnedResource>& resources) override;

 private:
  friend class SurfaceAnimationManagerTest;

  static bool FilterSharedElementsWithRenderPassOrResource(
      std::vector<TransferableResource>* resource_list,
      const base::flat_map<ViewTransitionElementResourceId,
                           CompositorRenderPass*>* element_id_to_pass,
      const base::flat_map<blink::ViewTransitionToken,
                           std::unique_ptr<SurfaceAnimationManager>>*
          token_to_animation_manager,
      const DrawQuad& quad,
      CompositorRenderPass& copy_pass);

  SurfaceAnimationManager(
      const CompositorFrameTransitionDirective& directive,
      Surface* surface,
      SharedBitmapManager* shared_bitmap_manager,
      gpu::SharedImageInterface* shared_image_interface,
      ReservedResourceIdTracker* id_tracker,
      SaveDirectiveCompleteCallback sequence_id_finished_callback);

  void OnSaveDirectiveProcessed(
      SaveDirectiveCompleteCallback callback,
      const CompositorFrameTransitionDirective& directive);

  // Maps the textures cached by the save directive to transferable resources.
  // Shared element resource IDs can only be replaced with cached textures after
  // this step.
  void ImportTextures();

  enum class Stage { kPendingCopy, kWaitingForAnimate, kAnimating };
  Stage stage_ = Stage::kPendingCopy;

  TransferableResourceTracker transferable_resource_tracker_;

  SurfaceSavedFrame saved_frame_;
  base::flat_set<ViewTransitionElementResourceId> empty_resource_ids_;

  std::optional<TransferableResourceTracker::ResourceFrame> saved_textures_;

  base::WeakPtrFactory<SurfaceAnimationManager> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_TRANSITIONS_SURFACE_ANIMATION_MANAGER_H_
