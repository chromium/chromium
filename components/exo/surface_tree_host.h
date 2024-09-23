// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SURFACE_TREE_HOST_H_
#define COMPONENTS_EXO_SURFACE_TREE_HOST_H_

#include <memory>
#include <set>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/exo/surface.h"
#include "components/exo/surface_delegate.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace aura {
class Window;
}  // namespace aura

namespace viz {
class RasterContextProvider;
}

namespace exo {
class LayerTreeFrameSinkHolder;

// Notifies viz that exo doesn't want to throttle sending
// `DidReceiveCompositorFrameAck` and `ReclaimResources`. We don't want exo to
// merge those into OnBeginFrame as that makes clients of exo to throttle as
// well given frame callbacks as well as buffers are sent/released in a late
// manner.
BASE_DECLARE_FEATURE(kExoDisableBeginFrameAcks);

// This class provides functionality for hosting a surface tree. The surface
// tree is hosted in the |host_window_|.
class SurfaceTreeHost : public SurfaceDelegate,
                        public display::DisplayManagerObserver,
                        public ui::LayerOwner::Observer,
                        public viz::ContextLostObserver {
 public:
  explicit SurfaceTreeHost(const std::string& window_name);
  SurfaceTreeHost(const std::string& window_name,
                  std::unique_ptr<aura::Window> host_window);

  SurfaceTreeHost(const SurfaceTreeHost&) = delete;
  SurfaceTreeHost& operator=(const SurfaceTreeHost&) = delete;

  ~SurfaceTreeHost() override;

  // Sets a root surface of a surface tree. This surface tree will be hosted in
  // the |host_window_|.
  virtual void SetRootSurface(Surface* root_surface);

  // Returns false if the hit test region is empty.
  bool HasHitTestRegion() const;

  // Sets |mask| to the path that delineates the hit test region of the hosted
  // surface tree.
  void GetHitTestMask(SkPath* mask) const;

  // Call this to indicate that the previous CompositorFrame is processed and
  // the surface is being scheduled for a draw.
  virtual void DidReceiveCompositorFrameAck();

  // Call this to indicate that the CompositorFrame with given
  // |presentation_token| has been first time presented to user.
  void DidPresentCompositorFrame(uint32_t presentation_token,
                                 const gfx::PresentationFeedback& feedback);

  // Sets the scale factor for all buffers associated with this surface. This
  // affects all future commits.
  void SetScaleFactor(float scale_factor);

  aura::Window* host_window() { return host_window_.get(); }
  const aura::Window* host_window() const { return host_window_.get(); }

  Surface* root_surface() { return root_surface_; }
  const Surface* root_surface() const { return root_surface_; }

  const gfx::Point& root_surface_origin_pixel() const {
    return root_surface_origin_pixel_;
  }

  LayerTreeFrameSinkHolder* layer_tree_frame_sink_holder() {
    return layer_tree_frame_sink_holder_.get();
  }

  using PresentationCallbacks = std::list<Surface::PresentationCallback>;

  base::queue<std::list<Surface::FrameCallback>>&
  GetFrameCallbacksForTesting() {
    return frame_callbacks_;
  }

  base::flat_map<uint32_t, PresentationCallbacks>&
  GetActivePresentationCallbacksForTesting() {
    return active_presentation_callbacks_;
  }

  uint32_t GenerateNextFrameToken() { return ++next_token_; }

  // Returns the primary SurfaceId.
  viz::SurfaceId GetSurfaceId() const;

  // SurfaceDelegate:
  void OnSurfaceCommit() override;
  bool IsSurfaceSynchronized() const override;
  bool IsInputEnabled(Surface* surface) const override;
  void OnSetFrame(SurfaceFrameType type) override {}
  void OnSetFrameColors(SkColor active_color, SkColor inactive_color) override {
  }
  void OnSetParent(Surface* parent, const gfx::Point& position) override {}
  void OnSetStartupId(const char* startup_id) override {}
  void OnSetApplicationId(const char* application_id) override {}
  void SetUseImmersiveForFullscreen(bool value) override {}
  void OnActivationRequested() override {}
  void OnNewOutputAdded() override;
  void OnSetServerStartResize() override {}
  void ShowSnapPreviewToPrimary() override {}
  void ShowSnapPreviewToSecondary() override {}
  void HideSnapPreview() override {}
  void SetSnapPrimary(float snap_ratio) override {}
  void SetSnapSecondary(float snap_ratio) override {}
  void UnsetSnap() override {}
  void SetCanGoBack() override {}
  void UnsetCanGoBack() override {}
  void SetPip() override {}
  void UnsetPip() override {}
  void SetFloatToLocation(
      chromeos::FloatStartLocation float_start_location) override {}
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override {}
  void MoveToDesk(int desk_index) override {}
  void SetVisibleOnAllWorkspaces() override {}
  void SetInitialWorkspace(const char* initial_workspace) override {}
  void Pin(bool trusted) override {}
  void Unpin() override {}
  void SetSystemModal(bool system_modal) override {}
  void SetTopInset(int height) override {}
  SecurityDelegate* GetSecurityDelegate() override;

  // display::DisplayManagerObserver:
  void OnDidProcessDisplayChanges(
      const DisplayConfigurationChange& configuration_change) override;

  // viz::ContextLostObserver:
  void OnContextLost() override;

  void OnFrameSinkLost();

  void set_client_submits_surfaces_in_pixel_coordinates(bool enabled) {
    client_submits_surfaces_in_pixel_coordinates_ = enabled;
  }

  void SetSecurityDelegate(SecurityDelegate* security_delegate);

  void SubmitCompositorFrameForTesting(viz::CompositorFrame frame);

  using LayerTreeFrameSinkHolderFactory =
      base::RepeatingCallback<std::unique_ptr<LayerTreeFrameSinkHolder>()>;

  // It should only be used at initialization time before any frames are
  // submitted.
  void SetLayerTreeFrameSinkHolderFactoryForTesting(
      LayerTreeFrameSinkHolderFactory frame_sink_holder_factory);

  // Creates a LayerTreeFrameSink for the |host_window_|.
  std::unique_ptr<cc::mojo_embedder::AsyncLayerTreeFrameSink>
  CreateLayerTreeFrameSink();

  // Overridden from ui::LayerOwner::Observer
  void OnLayerRecreated(ui::Layer* old_layer) override;

  // Applies rounded_corner_bounds (bounds + radii_in_dps) to the surface tree.
  // `rounded_corner_bounds` should be in the coordinate space of the
  // `root_surface`.
  void ApplyRoundedCornersToSurfaceTree(
      const gfx::RectF& bounds,
      const gfx::RoundedCornersF& radii_in_dps);

  scoped_refptr<viz::RasterContextProvider> SetRasterContextProviderForTesting(
      scoped_refptr<viz::RasterContextProvider> context_provider_test);

 protected:
  void UpdateDisplayOnTree();

  // Call this after a buffer has been committed but before a compositor frame
  // has been submitted.
  void WillCommit();

  // Call this to submit a compositor frame.
  void SubmitCompositorFrame();

  // Call this to submit an empty compositor frame. This may be useful if
  // the surface tree is becoming invisible but the resources (e.g. buffers)
  // need to be released back to the client.
  void SubmitEmptyCompositorFrame();

  // Updates the host window's (or the closest representative surface layer's)
  // size to cover exo surfaces that must be visible and not clipped.
  // It also updates `root_surface_origin_` accordingly to the origin.
  void UpdateSurfaceLayerSizeAndRootSurfaceOrigin();

  // Updates the host layer's opacity. This has to be called after root
  // surface's resource is updated.
  void UpdateHostLayerOpacity();

  void UpdateHostWindowOpaqueRegion();

  bool client_submits_surfaces_in_pixel_coordinates() const {
    return client_submits_surfaces_in_pixel_coordinates_;
  }

  bool bounds_is_dirty() const { return bounds_is_dirty_; }

  void set_bounds_is_dirty(bool bounds_is_dirty) {
    bounds_is_dirty_ = bounds_is_dirty;
  }

  // If the client has submitted a scale factor, we use that. Otherwise we use
  // the host window's layer's scale factor.
  virtual float GetScaleFactor() const;
  virtual float GetPendingScaleFactor() const;

  bool HasDoubleBufferedPendingScaleFactor() const;

  // Sets the appropriate transform for the given scale factor.
  // NOTE: This should only be done if the client submits in pixel coordinates.
  void SetScaleFactorTransform(float scale_factor);

  // Once a configure is acknowledged, accept the parent portion of the
  // local_surface_id from the |host_window_|.
  void UpdateLocalSurfaceIdFromParent(
      const viz::LocalSurfaceId& parent_local_surface_id);

  // Changes the local_surface_id as the viz::Surface property will change.
  void AllocateLocalSurfaceId();

  // If local_surface_id is newer than `GetCommitTargetLayer()`, update the
  // surface ranges to produce different SurfaceDrawQuads.
  virtual void MaybeActivateSurface();

  // The local_surface_id that the `layer_tree_frame_sink_holder_` is submitting
  // with.
  const viz::LocalSurfaceId& GetCurrentLocalSurfaceId() const;

  // Returns the ui::Layer that hosts client's surface commits, i.e.
  // commit_target_layer. Its property can be controlled by the client.
  // When an animation causes the host_window->layer to be cloned, before the
  // client acks the config event for that request, the old_layer prior to the
  // cloning should be the commit_target_layer.
  //
  // On SurfaceTreeHost implementations that don't have async config/ack flow,
  // this returns host_window()->layer().
  //
  // Note: due to animation cancelling, the commit_target_layer can be
  // destroyed with the cancelled animation, so this method may return nullptr.
  virtual ui::Layer* GetCommitTargetLayer();
  virtual const ui::Layer* GetCommitTargetLayer() const;

  int64_t output_display_id() const { return output_display_id_; }

  // The FrameSinkId associated with this.
  viz::FrameSinkId frame_sink_id_;

 private:
  // Returns true if contents of `host_window_` fills the bounds opaquely.
  bool ContentsFillsHostWindowOpaquely() const;

  void InitHostWindow(const std::string& window_name);

  viz::CompositorFrame PrepareToSubmitCompositorFrame();

  void HandleContextLost();
  void HandleFrameSinkLost();

  void CleanUpCallbacks();

  float CalculateScaleFactor(const std::optional<float>& scale_factor) const;

  // Applies `rounded_corner_bounds` to the `surface` and propagates the bounds
  // to its subsurfaces. `rounded_corner_bounds` should be in the local
  // coordinates of the `surface`.
  void ApplyAndPropagateRoundedCornersToSurfaceTree(
      Surface* surface,
      const gfx::RRectF& rounded_corners_bounds);

  std::unique_ptr<LayerTreeFrameSinkHolder> CreateLayerTreeFrameSinkHolder();

  std::unique_ptr<viz::ChildLocalSurfaceIdAllocator>
      child_local_surface_id_allocator_;

  raw_ptr<Surface> root_surface_ = nullptr;

  // Position of root surface relative to topmost, leftmost sub-surface. The
  // host window should be translated by the negation of this vector.
  // The coordinates is Pixel.
  gfx::Point root_surface_origin_pixel_;

  // The coordinates is DP.
  std::unique_ptr<aura::Window> host_window_;

  std::unique_ptr<LayerTreeFrameSinkHolder> layer_tree_frame_sink_holder_;
  LayerTreeFrameSinkHolderFactory frame_sink_holder_factory_;

  // This queue contains lists the callbacks to notify the client when it is a
  // good time to start producing a new frame. Each list corresponds to a
  // compositor frame, in the order of submission to
  // `layer_tree_frame_sink_holder_`.
  //
  // These callbacks move to |frame_callbacks_| when Commit() is called. They
  // fire when the effect of the Commit() is scheduled to be drawn.
  base::queue<std::list<Surface::FrameCallback>> frame_callbacks_;

  // These lists contain the callbacks to notify the client when surface
  // contents have been presented.
  base::flat_map<uint32_t, PresentationCallbacks>
      active_presentation_callbacks_;

  // When a client calls set_scale_factor they're actually setting the scale
  // factor for all future commits.
  std::optional<float> pending_scale_factor_;

  // This is the client-set scale factor that is being used for the current
  // buffer.
  std::optional<float> scale_factor_;

  viz::FrameTokenGenerator next_token_;

  scoped_refptr<viz::RasterContextProvider> context_provider_;

  base::ScopedObservation<display::DisplayManager,
                          display::DisplayManagerObserver>
      display_manager_observation_{this};

  // The display id for the output the surface is entered onto.
  int64_t output_display_id_ = display::kInvalidDisplayId;

  bool client_submits_surfaces_in_pixel_coordinates_ = false;

  raw_ptr<SecurityDelegate> security_delegate_ = nullptr;

  std::set<gpu::SyncToken> prev_frame_verified_tokens_;

  bool bounds_is_dirty_ = true;

  base::WeakPtrFactory<SurfaceTreeHost> weak_ptr_factory_{this};
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SURFACE_TREE_HOST_H_
