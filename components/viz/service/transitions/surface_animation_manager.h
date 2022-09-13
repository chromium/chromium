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
class CompositorFrame;
struct ReturnedResource;
struct TransferableResource;

// This class is responsible for processing CompositorFrameTransitionDirectives,
// and keeping track of the animation state.
// TODO(vmpstr): This class should also be responsible for interpolating frames
// and providing the result back to the surface, but that is currently not
// implemented.
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
  // Uses `storage` for saving or retrieving animation parameters and saved
  // frames.
  // Returns true if this call caused an animation to begin. This is a signal
  // that we need to interpolate the current active frame, even if we would
  // normally not do so in the middle of the animation.
  bool ProcessTransitionDirectives(
      const std::vector<CompositorFrameTransitionDirective>& directives,
      Surface* active_surface);

  // Returns true if this manager needs to observe begin frames to advance
  // animations.
  bool NeedsBeginFrame() const;

  // Notify when a begin frame happens and a frame is advanced.
  void NotifyFrameAdvanced();

  // Interpolates from the saved frame to the current active frame on the
  // surface, storing the result back on the surface.
  void InterpolateFrame(Surface* surface);

  // Resource ref count management.
  void RefResources(const std::vector<TransferableResource>& resources);
  void UnrefResources(const std::vector<ReturnedResource>& resources);

  // Updates the current frame time, without doing anything else.
  void UpdateFrameTime(base::TimeTicks now);

  // Replaced SharedElementResourceIds with corresponding ResourceIds if
  // necessary.
  void ReplaceSharedElementResources(Surface* surface);

  SurfaceSavedFrameStorage* GetSurfaceSavedFrameStorageForTesting();

 private:
  friend class SurfaceAnimationManagerTest;
  FRIEND_TEST_ALL_PREFIXES(SurfaceAnimationManagerTest, CustomRootConfig);
  FRIEND_TEST_ALL_PREFIXES(SurfaceAnimationManagerTest, CustomSharedConfig);

  class StorageWithSurface;

  struct RenderPassDrawData {
    RenderPassDrawData();
    RenderPassDrawData(RenderPassDrawData&&);
    ~RenderPassDrawData();

    RenderPassDrawData& operator=(RenderPassDrawData&&) = default;

    std::unique_ptr<CompositorRenderPass> render_pass;
    absl::optional<CompositorRenderPassDrawQuad> draw_quad;
    float opacity = 1.f;
  };

  void CreateRootAnimationCurves(const gfx::Size& output_size);
  void CreateSharedElementCurves();

  // Helpers to process specific directives.
  bool ProcessSaveDirective(const CompositorFrameTransitionDirective& directive,
                            StorageWithSurface& storage);
  // Returns true if the animation has started.
  bool ProcessAnimateDirective(
      const CompositorFrameTransitionDirective& directive,
      StorageWithSurface& storage);
  bool ProcessAnimateRendererDirective(
      const CompositorFrameTransitionDirective& directive,
      StorageWithSurface& storage);
  bool ProcessReleaseDirective();

  // Finishes the animation and advance state to kLastFrame if it's time to do
  // so. This call is only valid if state is kAnimating.
  void FinishAnimationIfNeeded();

  // Disposes of any saved state and switches state to kIdle. This call is only
  // valid if state is kLastFrame.
  void FinalizeAndDisposeOfState();

  // A helper function to copy render passes, while interpolating shared
  // elements.
  void CopyAndInterpolateSharedElements(
      const std::vector<std::unique_ptr<CompositorRenderPass>>& source_passes,
      CompositorRenderPass* animation_pass,
      CompositorFrame* interpolated_frame);

  // Helper function to create an animation pass which interpolates needed
  // components.
  std::unique_ptr<CompositorRenderPass> CreateAnimationCompositorRenderPass(
      const gfx::Rect& output_rect) const;

  // Given a render pass, this makes a copy of it while filtering animated
  // render pass draw quads.
  static bool FilterSharedElementQuads(
      base::flat_map<CompositorRenderPassId, RenderPassDrawData>*
          shared_draw_data,
      const DrawQuad& quad,
      CompositorRenderPass& copy_pass);

  bool FilterSharedElementsWithRenderPassOrResource(
      std::vector<TransferableResource>* resource_list,
      const base::flat_map<SharedElementResourceId,
                           const CompositorRenderPass*>* element_id_to_pass,
      const DrawQuad& quad,
      CompositorRenderPass& copy_pass);

  // Tick both the root and shared animations.
  void TickAnimations(base::TimeTicks new_time);

  // Returns true if we have a running animation for root or shared elements.
  bool HasRunningAnimations() const;

  // The state machine can take the following paths :
  // 1) Viz driven animation : kIdle -> kAnimating -> kLastFrame -> kIdle
  // 2) Renderer driven animation : kIdle -> kAnimatingRenderer -> kIdle
  enum class State { kIdle, kAnimatingRenderer, kAnimating, kLastFrame };

  TransitionDirectiveCompleteCallback sequence_id_finished_callback_;

  uint32_t last_processed_sequence_id_ = 0;

  TransferableResourceTracker transferable_resource_tracker_;

  absl::optional<TransferableResourceTracker::ResourceFrame> saved_textures_;
  absl::optional<CompositorFrameTransitionDirective> save_directive_;
  absl::optional<CompositorFrameTransitionDirective> animate_directive_;

  // State represents the total state of the animation for this manager. It is
  // adjusted in step with the root animation. In other words, if the root
  // animation ends then the total animation is considered (almost) ended as
  // well. We keep track of a separate state, since we need to produce a
  // kLastFrame value after the root animation ends which is responsible for
  // produce a clean active frame without any interpolations.
  State state_ = State::kIdle;

  // This is the animation state for the root elements.
  class RootAnimationState : public gfx::FloatAnimationCurve::Target,
                             public gfx::TransformAnimationCurve::Target {
   public:
    RootAnimationState();
    RootAnimationState(RootAnimationState&&);
    ~RootAnimationState() override;

    enum TargetProperty : int {
      kSrcOpacity = 1,
      kDstOpacity,
      kSrcTransform,
      kDstTransform,
    };

    void OnFloatAnimated(const float& value,
                         int target_property_id,
                         gfx::KeyframeModel* keyframe_model) override;

    void OnTransformAnimated(const gfx::TransformOperations& operations,
                             int target_property_id,
                             gfx::KeyframeModel* keyframe_model) override;

    void Reset();

    gfx::KeyframeEffect& driver() { return driver_; }
    const gfx::KeyframeEffect& driver() const { return driver_; }

    float src_opacity() const { return src_opacity_; }
    float dst_opacity() const { return dst_opacity_; }
    const gfx::TransformOperations& src_transform() const {
      return src_transform_;
    }
    const gfx::TransformOperations& dst_transform() const {
      return dst_transform_;
    }

   private:
    gfx::KeyframeEffect driver_;
    float src_opacity_ = 1.0f;
    float dst_opacity_ = 1.0f;
    gfx::TransformOperations src_transform_;
    gfx::TransformOperations dst_transform_;
  };

  // This is the animation state for a pair of shared elements.
  class SharedAnimationState : public gfx::FloatAnimationCurve::Target,
                               public gfx::TransformAnimationCurve::Target,
                               public gfx::SizeAnimationCurve::Target {
   public:
    SharedAnimationState();
    SharedAnimationState(SharedAnimationState&&);
    ~SharedAnimationState() override;

    enum TargetProperty : int {
      // The following properties are used to blend the content of src and dest
      // textues to produce a combined image.
      kContentOpacity = 1,
      kContentSize,

      // The following properties are used when drawing the combined image to
      // the target buffer.
      kCombinedOpacity,
      kCombinedTransform,
    };

    void OnFloatAnimated(const float& value,
                         int target_property_id,
                         gfx::KeyframeModel* keyframe_model) override;

    void OnTransformAnimated(const gfx::TransformOperations& operations,
                             int target_property_id,
                             gfx::KeyframeModel* keyframe_model) override;

    void OnSizeAnimated(const gfx::SizeF& value,
                        int target_property_id,
                        gfx::KeyframeModel* keyframe_model) override;

    void Reset();

    gfx::KeyframeEffect& driver() { return driver_; }
    const gfx::KeyframeEffect& driver() const { return driver_; }

    float content_opacity() const { return content_opacity_; }
    const gfx::SizeF& content_size() const { return content_size_; }
    float combined_opacity() const { return combined_opacity_; }
    const gfx::TransformOperations& combined_transform() const {
      return combined_transform_;
    }

   private:
    gfx::KeyframeEffect driver_;
    float content_opacity_ = 1.0f;
    gfx::SizeF content_size_;
    float combined_opacity_ = 1.0f;
    gfx::TransformOperations combined_transform_;
  };

  // This is responsible for keeping track of the saved frame, accumulating copy
  // output results.
  SurfaceSavedFrameStorage surface_saved_frame_storage_;

  // This is the root animation state.
  RootAnimationState root_animation_;

  // This is a vector of animation states for each of the shared elements. Note
  // that the position in the vector matches both the position of the shared
  // texture in the saved_textures_->shared vector and the corresponding
  // position in the animate_directive->shared_render_pass_ids() vector.
  std::vector<SharedAnimationState> shared_animations_;

  base::TimeTicks latest_time_;

  base::flat_set<SharedElementResourceId> empty_resource_ids_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_TRANSITIONS_SURFACE_ANIMATION_MANAGER_H_
