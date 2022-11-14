// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SHELL_SURFACE_H_
#define COMPONENTS_EXO_SHELL_SURFACE_H_

#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_state_observer.h"
#include "base/containers/circular_deque.h"
#include "base/observer_list.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_observer.h"
#include "ui/base/ui_base_types.h"

namespace ash {
class ScopedAnimationDisabler;
}  // namespace ash

namespace ui {
class CompositorLock;
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
  using ConfigureCallback =
      base::RepeatingCallback<uint32_t(const gfx::Rect& bounds,
                                       chromeos::WindowStateType state_type,
                                       bool resizing,
                                       bool activated,
                                       const gfx::Vector2d& origin_offset)>;
  using OriginChangeCallback =
      base::RepeatingCallback<void(const gfx::Point& origin)>;

  void set_configure_callback(const ConfigureCallback& configure_callback) {
    configure_callback_ = configure_callback;
  }

  void set_origin_change_callback(
      const OriginChangeCallback& origin_change_callback) {
    origin_change_callback_ = origin_change_callback;
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

  // Set fullscreen state for shell surface.
  void SetFullscreen(bool fullscreen);

  // Make the shell surface popup type.
  void SetPopup();

  // Set event grab on the surface.
  void Grab();

  // Start an interactive resize of surface. |component| is one of the windows
  // HT constants (see ui/base/hit_test.h) and describes in what direction the
  // surface should be resized.
  void StartResize(int component);

  // Start an interactive move of surface.
  void StartMove();

  // Return the initial show state for this surface.
  ui::WindowShowState initial_show_state() { return initial_show_state_; }

  void AddObserver(ShellSurfaceObserver* observer);
  void RemoveObserver(ShellSurfaceObserver* observer);

  // Overridden from SurfaceDelegate:
  void OnSetFrame(SurfaceFrameType type) override;
  void OnSetParent(Surface* parent, const gfx::Point& position) override;

  // Overridden from ShellSurfaceBase:
  void InitializeWindowState(ash::WindowState* window_state) override;
  absl::optional<gfx::Rect> GetWidgetBounds() const override;
  gfx::Point GetSurfaceOrigin() const override;
  void SetUseImmersiveForFullscreen(bool value) override;

  // Overridden from aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;

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
  gfx::Rect ComputeAdjustedBounds(const gfx::Rect& bounds) const override;
  void SetWidgetBounds(const gfx::Rect& bounds,
                       bool adjusted_by_server) override;
  bool OnPreWidgetCommit() override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;

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
    ShellSurface* const shell_surface_;
    const bool force_configure_;
    bool needs_configure_ = false;
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

  void AttemptToStartDrag(int component);

  std::unique_ptr<ash::ScopedAnimationDisabler> animations_disabler_;

  std::unique_ptr<ui::CompositorLock> configure_compositor_lock_;
  ConfigureCallback configure_callback_;
  OriginChangeCallback origin_change_callback_;
  ScopedConfigure* scoped_configure_ = nullptr;
  base::circular_deque<std::unique_ptr<Config>> pending_configs_;

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
  ui::WindowShowState initial_show_state_ = ui::SHOW_STATE_DEFAULT;
  bool notify_bounds_changes_ = true;
  bool window_state_is_changing_ = false;

  base::ObserverList<ShellSurfaceObserver> observers_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SHELL_SURFACE_H_
