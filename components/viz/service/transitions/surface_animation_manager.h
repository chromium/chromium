// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_TRANSITIONS_SURFACE_ANIMATION_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_TRANSITIONS_SURFACE_ANIMATION_MANAGER_H_

#include <limits>
#include <memory>
#include <vector>

#include "base/time/time.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/surfaces/surface_saved_frame.h"
#include "components/viz/service/transitions/transferable_resource_tracker.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframe_effect.h"

namespace viz {

class Surface;
class SurfaceSavedFrameStorage;
struct ReturnedResource;
struct TransferableResource;

// This class is responsible for processing CompositorFrameTransitionDirectives,
// and keeping track of the animation state.
// TODO(vmpstr): This class should also be responsible for interpolating frames
// and providing the result back to the surface, but that is currently not
// implemented.
class VIZ_SERVICE_EXPORT SurfaceAnimationManager
    : public gfx::FloatAnimationCurve::Target,
      public gfx::TransformAnimationCurve::Target {
 public:
  using TransitionDirectiveCompleteCallback =
      base::RepeatingCallback<void(uint32_t)>;

  SurfaceAnimationManager();
  ~SurfaceAnimationManager() override;

  void SetDirectiveFinishedCallback(
      TransitionDirectiveCompleteCallback sequence_id_finished_callback);

  // Process any new transitions on the compositor frame metadata. Note that
  // this keeps track of the latest processed sequence id and repeated calls
  // with same sequence ids will have no effect.
  // Uses `storage` for saving or retrieving animation parameters and saved
  // frames.
  // Returns true if this call caused an animation to begin. This is a signal
  // that we need to interpolate the current active frame, even if we would
  // normally not do so in the middle of the animation.
  bool ProcessTransitionDirectives(
      const std::vector<CompositorFrameTransitionDirective>& directives,
      SurfaceSavedFrameStorage* storage);

  // Returns true if this manager needs to observe begin frames to advance
  // animations.
  bool NeedsBeginFrame() const;

  // Notify when a begin frame happens and a frame is advanced.
  void NotifyFrameAdvanced(base::TimeTicks new_time);

  // Interpolates from the saved frame to the current active frame on the
  // surface, storing the result back on the surface.
  void InterpolateFrame(Surface* surface);

  // Resource ref count management.
  void RefResources(const std::vector<TransferableResource>& resources);
  void UnrefResources(const std::vector<ReturnedResource>& resources);

  void OnFloatAnimated(const float& value,
                       int target_property_id,
                       gfx::KeyframeModel* keyframe_model) override;

  void OnTransformAnimated(const gfx::TransformOperations& operations,
                           int target_property_id,
                           gfx::KeyframeModel* keyframe_model) override;

 protected:
  float src_opacity() const { return src_opacity_; }
  float dst_opacity() const { return dst_opacity_; }
  gfx::TransformOperations src_transform() const { return src_transform_; }
  gfx::TransformOperations dst_transform() const { return dst_transform_; }

 private:
  enum TargetProperty : int {
    kSrcOpacity = 1,
    kDstOpacity,
    kSrcTransform,
    kDstTransform,
  };

  void UpdateAnimationCurves(const gfx::Size& output_size);

  // Helpers to process specific directives.
  bool ProcessSaveDirective(const CompositorFrameTransitionDirective& directive,
                            SurfaceSavedFrameStorage* storage);
  // Returns true if the animation has started.
  bool ProcessAnimateDirective(
      const CompositorFrameTransitionDirective& directive,
      SurfaceSavedFrameStorage* storage);

  // Finishes the animation and advance state to kLastFrame if it's time to do
  // so. This call is only valid if state is kAnimating.
  void FinishAnimationIfNeeded();

  // Disposes of any saved state and switches state to kIdle. This call is only
  // valid if state is kLastFrame.
  void FinalizeAndDisposeOfState();

  enum class State { kIdle, kAnimating, kLastFrame };

  TransitionDirectiveCompleteCallback sequence_id_finished_callback_;

  uint32_t last_processed_sequence_id_ = 0;

  TransferableResourceTracker transferable_resource_tracker_;

  base::Optional<TransferableResourceTracker::ResourceFrame> saved_textures_;
  base::Optional<CompositorFrameTransitionDirective> save_directive_;
  base::Optional<CompositorFrameTransitionDirective> animate_directive_;

  // TODO(vmpstr): if SurfaceAnimationManager ultimately manages multiple
  // animations, then the following should be encapsulated in a per-animation
  // class.
  State state_ = State::kIdle;
  gfx::KeyframeEffect animator_;
  float src_opacity_ = 1.0f;
  float dst_opacity_ = 1.0f;
  gfx::TransformOperations src_transform_;
  gfx::TransformOperations dst_transform_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_TRANSITIONS_SURFACE_ANIMATION_MANAGER_H_
