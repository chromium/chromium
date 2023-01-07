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
#include "components/viz/service/surfaces/surface_saved_frame_storage.h"
#include "components/viz/service/transitions/transferable_resource_tracker.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframe_effect.h"
#include "ui/gfx/animation/keyframe/keyframe_model.h"

namespace viz {

class Surface;
struct ReturnedResource;
struct TransferableResource;

// This class is responsible for processing CompositorFrameTransitionDirectives,
// and keeping track of the animation state.
class VIZ_SERVICE_EXPORT SurfaceAnimationManager {
 public:
  using TransitionDirectiveCompleteCallback =
      base::RepeatingCallback<void(uint32_t)>;

  explicit SurfaceAnimationManager(SharedBitmapManager* shared_bitmap_manager);
  ~SurfaceAnimationManager();

  void SetDirectiveFinishedCallback(
      TransitionDirectiveCompleteCallback sequence_id_finished_callback);

  // Process any new transitions on the compositor frame metadata. Note that
  // this keeps track of the latest processed sequence id and repeated calls
  // with same sequence ids will have no effect.
  void ProcessTransitionDirectives(
      const std::vector<CompositorFrameTransitionDirective>& directives,
      Surface* active_surface);

  // Resource ref count management.
  void RefResources(const std::vector<TransferableResource>& resources);
  void UnrefResources(const std::vector<ReturnedResource>& resources);

  // Replaced SharedElementResourceIds with corresponding ResourceIds if
  // necessary.
  void ReplaceSharedElementResources(Surface* surface);

  SurfaceSavedFrameStorage* GetSurfaceSavedFrameStorageForTesting();

 private:
  friend class SurfaceAnimationManagerTest;
  class StorageWithSurface;

  // Helpers to process specific directives.
  bool ProcessSaveDirective(const CompositorFrameTransitionDirective& directive,
                            StorageWithSurface& storage);
  bool ProcessAnimateRendererDirective(
      const CompositorFrameTransitionDirective& directive,
      StorageWithSurface& storage);
  bool ProcessReleaseDirective();

  bool FilterSharedElementsWithRenderPassOrResource(
      std::vector<TransferableResource>* resource_list,
      const base::flat_map<SharedElementResourceId,
                           const CompositorRenderPass*>* element_id_to_pass,
      const DrawQuad& quad,
      CompositorRenderPass& copy_pass);

  enum class State { kIdle, kAnimatingRenderer };

  TransitionDirectiveCompleteCallback sequence_id_finished_callback_;

  uint32_t last_processed_sequence_id_ = 0;

  TransferableResourceTracker transferable_resource_tracker_;
  SurfaceSavedFrameStorage surface_saved_frame_storage_;

  absl::optional<TransferableResourceTracker::ResourceFrame> saved_textures_;

  State state_ = State::kIdle;

  base::flat_set<SharedElementResourceId> empty_resource_ids_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_TRANSITIONS_SURFACE_ANIMATION_MANAGER_H_
