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
#include "components/exo/surface.h"
#include "components/exo/surface_delegate.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "ui/display/display_observer.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace viz {
class ContextProvider;
}

namespace exo {
class LayerTreeFrameSinkHolder;

// This class provides functionality for hosting a surface tree. The surface
// tree is hosted in the |host_window_|.
class SurfaceTreeHost : public SurfaceDelegate,
                        public display::DisplayObserver,
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

  const gfx::Point& root_surface_origin() const { return root_surface_origin_; }

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
  void SetFloat() override {}
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override {}
  void MoveToDesk(int desk_index) override {}
  void SetVisibleOnAllWorkspaces() override {}
  void SetInitialWorkspace(const char* initial_workspace) override {}
  void Pin(bool trusted) override {}
  void Unpin() override {}
  void SetSystemModal(bool system_modal) override {}
  void SetTopInset(int height) override {}
  SecurityDelegate* GetSecurityDelegate() override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // viz::ContextLostObserver:
  void OnContextLost() override;

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

  // Updates the host window's size to cover surfaces that must be visible and
  // not clipped.
  // It also updates root surface origin accordingly to the origin.
  void UpdateHostWindowSizeAndRootSurfaceOrigin();

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

  // If local_surface_id is newer than |host_window_|'s ui layer, push the
  // current local_surface_id to |host_window_| and its ui layer to produce a
  // different SurfaceDrawQuad.
  void MaybeActivateSurface();

  // Returns the primary SurfaceId.
  viz::SurfaceId GetSurfaceId() const;

 private:
  void InitHostWindow(const std::string& window_name);

  viz::CompositorFrame PrepareToSubmitCompositorFrame();

  // The local_surface_id that the |frame_sink_| is submitting with, it should
  // never be older than the local_surface_id of |host_window_|'s layer.
  const viz::LocalSurfaceId& GetCurrentLocalSurfaceId() const;

  void HandleContextLost();

  void CleanUpCallbacks();

  float CalculateScaleFactor(const absl::optional<float>& scale_factor) const;

  std::unique_ptr<LayerTreeFrameSinkHolder> CreateLayerTreeFrameSinkHolder();

  // The FrameSinkId associated with this.
  viz::FrameSinkId frame_sink_id_;
  std::unique_ptr<viz::ChildLocalSurfaceIdAllocator>
      child_local_surface_id_allocator_;

  raw_ptr<Surface, ExperimentalAsh> root_surface_ = nullptr;

  // Position of root surface relative to topmost, leftmost sub-surface. The
  // host window should be translated by the negation of this vector.
  gfx::Point root_surface_origin_;

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
  absl::optional<float> pending_scale_factor_;

  // This is the client-set scale factor that is being used for the current
  // buffer.
  absl::optional<float> scale_factor_;

  viz::FrameTokenGenerator next_token_;

  scoped_refptr<viz::ContextProvider> context_provider_;

  display::ScopedDisplayObserver display_observer_{this};

  int64_t display_id_ = display::kInvalidDisplayId;

  bool client_submits_surfaces_in_pixel_coordinates_ = false;

  raw_ptr<SecurityDelegate, ExperimentalAsh> security_delegate_ = nullptr;

  std::set<gpu::SyncToken> prev_frame_verified_tokens_;

  bool bounds_is_dirty_ = true;

  base::WeakPtrFactory<SurfaceTreeHost> weak_ptr_factory_{this};
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SURFACE_TREE_HOST_H_
