// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SHELL_SURFACE_H_
#define COMPONENTS_EXO_SHELL_SURFACE_H_

#include <optional>

#include "ash/focus_cycler.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_state_observer.h"
#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_observer.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"

namespace wm {
class ScopedAnimationDisabler;
}  // namespace wm

namespace ui {
class CompositorLock;
class Layer;
}  // namespace ui

namespace exo {
class Surface;

// This class implements toplevel surface for which position and state are
// managed by the shell.
class ShellSurface : public ShellSurfaceBase, public ash::WindowStateObserver {
 public:
  // The |origin| is the initial position in screen coordinates. The position
  // specified as part of the geometry is relative to the shell surface.
  ShellSurface(Surface* surface,
               const gfx::Point& origin,
               bool can_minimize,
               int container);
  explicit ShellSurface(Surface* surface);

  ShellSurface(const ShellSurface&) = delete;
  ShellSurface& operator=(const ShellSurface&) = delete;

  ~ShellSurface() override;

  // Set the callback to run when the client is asked to configure the surface.
  // The size is a hint, in the sense that the client is free to ignore it if
  // it doesn't resize, pick a smaller size (to satisfy aspect ratio or resize
  // in steps of NxM pixels).
  using ConfigureCallback = base::RepeatingCallback<uint32_t(
      const gfx::Rect& bounds,
      chromeos::WindowStateType state_type,
      bool resizing,
      bool activated,
      const gfx::Vector2d& origin_offset,
      float raster_scale,
      aura::Window::OcclusionState occlusion_state,
      std::optional<chromeos::WindowStateType> restore_state_type)>;
  using OriginChangeCallback =
      base::RepeatingCallback<void(const gfx::Point& origin)>;
  using RotateFocusCallback =
      base::RepeatingCallback<uint32_t(ash::FocusCycler::Direction direction,
                                       bool restart)>;
  using OverviewChangeCallback =
      base::RepeatingCallback<void(bool in_overview)>;

  void set_configure_callback(const ConfigureCallback& configure_callback) {
    configure_callback_ = configure_callback;
  }

  void set_origin_change_callback(
      const OriginChangeCallback& origin_change_callback) {
    origin_change_callback_ = origin_change_callback;
  }

  void set_rotate_focus_callback(const RotateFocusCallback callback) {
    rotate_focus_callback_ = callback;
  }

  void set_overview_change_callback(const OverviewChangeCallback callback) {
    overview_change_callback_ = callback;
  }

  // When the client is asked to configure the surface, it should acknowledge
  // the configure request sometime before the commit. |serial| is the serial
  // from the configure callback.
  void AcknowledgeConfigure(uint32_t serial);

  // Set the "parent" of this surface. This window should be stacked above a
  // parent.
  void SetParent(ShellSurface* parent);

  bool CanMaximize() const override;

  // Maximizes the shell surface.
  void Maximize();

  // Minimize the shell surface.
  void Minimize();

  // Restore the shell surface.
  void Restore();

  // Set fullscreen state for shell surface. When `fullscreen` is true,
  // `display_id` indicates the id of the display where the surface should be
  // shown on, otherwise it gets ignored. When `display::kInvalidDisplayId` is
  // specified the current display will be used.
  void SetFullscreen(bool fullscreen, int64_t display_id);

  // Make the shell surface popup type.
  void SetPopup();

  // Invokes when the surface has reached the end of its own focus rotation.
  // This signals ash to to continue its own focus rotation.
  void AckRotateFocus(uint32_t serial, bool handled);

  // Set event grab on the surface.
  void Grab();

  // Start an interactive resize of surface. |component| is one of the windows
  // HT constants (see ui/base/hit_test.h) and describes in what direction the
  // surface should be resized.
  bool StartResize(int component);

  // Start an interactive move of surface.
  bool StartMove();

  // Sends a wayland request to the surface to rotate focus within itself. If
  // the client was able to rotate, it will return a "handled" response,
  // otherwise it will respond with a "not handled" response.
  // If the client does not support the wayland event, the base class'
  // impl is invoked. In practice, this means that the surface will be focused,
  // but it will not rotate focus within its panes.
  bool RotatePaneFocusFromView(views::View* focused_view,
                               bool forward,
                               bool enable_wrapping) override;

  // Return the initial show state for this surface.
  ui::mojom::WindowShowState initial_show_state() {
    return initial_show_state_;
  }

  void AddObserver(ShellSurfaceObserver* observer);
  void RemoveObserver(ShellSurfaceObserver* observer);

  void MaybeSetCompositorLockForNextConfigure(int milliseconds);

  // Overridden from SurfaceDelegate:
  void OnSetFrame(SurfaceFrameType type) override;
  void OnSetParent(Surface* parent, const gfx::Point& position) override;

  // Overridden from SurfaceTreeHost:
  void MaybeActivateSurface() override;
  ui::Layer* GetCommitTargetLayer() override;
  const ui::Layer* GetCommitTargetLayer() const override;

  // Overridden from ShellSurfaceBase:
  void InitializeWindowState(ash::WindowState* window_state) override;
  std::optional<gfx::Rect> GetWidgetBounds() const override;
  gfx::Point GetSurfaceOrigin() const override;
  void SetUseImmersiveForFullscreen(bool value) override;
  void OnDidProcessDisplayChanges(
      const DisplayConfigurationChange& configuration_change) override;

  // Overridden from aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old_value) override;

  // Overridden from ash::WindowStateObserver:
  void OnPreWindowStateTypeChange(ash::WindowState* window_state,
                                  chromeos::WindowStateType old_type) override;
  void OnPostWindowStateTypeChange(ash::WindowState* window_state,
                                   chromeos::WindowStateType old_type) override;

  // Overridden from wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // Overridden from ShellSurfaceBase:
  void OnSurfaceCommit() override;
  gfx::Rect ComputeAdjustedBounds(const gfx::Rect& bounds) const override;
  void SetWidgetBounds(const gfx::Rect& bounds,
                       bool adjusted_by_server) override;
  bool OnPreWidgetCommit() override;
  void ShowWidget(bool activate) override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  void SetRootSurface(Surface* root_surface) override;

  // Overridden from ui::LayerOwner::Observer:
  void OnLayerRecreated(ui::Layer* old_layer) override;

  void EndDrag();

  int resize_component_for_test() const { return resize_component_; }

 private:
  struct Config;

  // Helper class used to coalesce a number of changes into one "configure"
  // callback. Callbacks are suppressed while an instance of this class is
  // instantiated and instead called when the instance is destroyed.
  // If |force_configure_| is true ShellSurface::Configure() will be called
  // even if no changes to shell surface took place during the lifetime of the
  // ScopedConfigure instance.
  class ScopedConfigure {
   public:
    ScopedConfigure(ShellSurface* shell_surface, bool force_configure);

    ScopedConfigure(const ScopedConfigure&) = delete;
    ScopedConfigure& operator=(const ScopedConfigure&) = delete;

    ~ScopedConfigure();

    void set_needs_configure() { needs_configure_ = true; }

   private:
    const raw_ptr<ShellSurface> shell_surface_;
    const bool force_configure_;
    bool needs_configure_ = false;
  };

  class OcclusionObserver : public aura::WindowObserver {
   public:
    explicit OcclusionObserver(ShellSurface* shell_surface,
                               aura::Window* window);
    ~OcclusionObserver() override;

    aura::Window::OcclusionState state() const { return state_; }

    aura::Window::OcclusionState GetInitialStateForConfigure(
        chromeos::WindowStateType state_type);

    void MaybeConfigure(aura::Window* window);

    // aura::WindowObserver:
    void OnWindowDestroying(aura::Window* window) override;
    void OnWindowOcclusionChanged(aura::Window* window) override;

   private:
    // Keeps track of what the current state should be. During initialization,
    // we want to defer sending occlusion messages until everything is ready,
    // so this may be different to the current occlusion state.
    aura::Window::OcclusionState state_;
    const raw_ptr<ShellSurface> shell_surface_;
    base::ScopedObservation<aura::Window, aura::WindowObserver>
        window_observation_{this};
  };

  // Set the parent window of this surface.
  void SetParentWindow(aura::Window* parent);

  // Sets up a transient window manager for this window if it can (i.e. if the
  // surface has a widget with a parent).
  void MaybeMakeTransient();

  // Asks the client to configure its surface. Optionally, the user can override
  // the behaviour to check for window dragging by setting ends_drag to true.
  void Configure(bool ends_drag = false);

  bool GetCanResizeFromSizeConstraints() const override;

  bool AttemptToStartDrag(int component);

  // Utility methods to resolve the initial bounds for the first commit.
  gfx::Rect GetInitialBoundsForState(
      const chromeos::WindowStateType state) const;
  display::Display GetDisplayForInitialBounds() const;

  void UpdateLayerSurfaceRange(ui::Layer* layer,
                               const viz::LocalSurfaceId& current_lsi);

  // Called when the widget window's position in screen coordinates may have
  // changed.
  // TODO(tluk): Screen position changes should be merged into Configure().
  void OnWidgetScreenPositionChanged();

  std::unique_ptr<wm::ScopedAnimationDisabler> animations_disabler_;
  std::optional<OcclusionObserver> occlusion_observer_;

  // Temporarily stores the `host_window()`'s layer when it's recreated for
  // animation. Client-side commits may be directed towards the `old_layer_`
  // instead of `host_window()->layer()` due to the asynchronous config/ack
  // flow.
  base::WeakPtr<ui::Layer> old_layer_;

  std::unique_ptr<ui::CompositorLock> configure_compositor_lock_;

  ConfigureCallback configure_callback_;
  OriginChangeCallback origin_change_callback_;
  RotateFocusCallback rotate_focus_callback_;
  OverviewChangeCallback overview_change_callback_;

  raw_ptr<ScopedConfigure> scoped_configure_ = nullptr;
  base::circular_deque<std::unique_ptr<Config>> pending_configs_;
  // Stores the config which is acked but not yet committed. This will keep the
  // compositor locked until reset after Commit() is called.
  std::unique_ptr<Config> config_waiting_for_commit_;

  // Window resizing is an asynchronous operation. See
  // https://crbug.com/1336706#c22 for a more detailed explanation.
  // |origin_offset_| is typically (0,0). During an asynchronous resizing
  // |origin_offset_| is set to a non-zero value such that it appears as though
  // the ExoShellSurfaceHost has not moved even though ExoShellSurface has
  // already been moved and resized to the new position.
  gfx::Vector2d origin_offset_;
  gfx::Vector2d pending_origin_offset_;
  gfx::Vector2d pending_origin_offset_accumulator_;
  gfx::Rect old_screen_bounds_for_pending_move_;

  int resize_component_ = HTCAPTION;  // HT constant (see ui/base/hit_test.h)
  int pending_resize_component_ = HTCAPTION;
  // TODO(oshima): Use WindowStateType instead.
  ui::mojom::WindowShowState initial_show_state_ =
      ui::mojom::WindowShowState::kDefault;
  bool notify_bounds_changes_ = true;
  bool window_state_is_changing_ = false;
  float pending_raster_scale_ = 1.0;

  struct InflightFocusRotateRequest {
    uint32_t serial;
    ash::FocusCycler::Direction direction;
  };
  std::queue<InflightFocusRotateRequest> rotate_focus_inflight_requests_;

  base::ObserverList<ShellSurfaceObserver> observers_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SHELL_SURFACE_H_
