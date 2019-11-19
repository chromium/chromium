// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SHELL_SURFACE_H_
#define COMPONENTS_EXO_SHELL_SURFACE_H_

#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_state_observer.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "components/exo/shell_surface_base.h"
#include "ui/base/ui_base_types.h"

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
               bool activatable,
               bool can_minimize,
               int container);
  explicit ShellSurface(Surface* surface);
  ~ShellSurface() override;

  // Set the callback to run when the client is asked to configure the surface.
  // The size is a hint, in the sense that the client is free to ignore it if
  // it doesn't resize, pick a smaller size (to satisfy aspect ratio or resize
  // in steps of NxM pixels).
  using ConfigureCallback =
      base::RepeatingCallback<uint32_t(const gfx::Size& size,
                                       ash::WindowStateType state_type,
                                       bool resizing,
                                       bool activated,
                                       const gfx::Vector2d& origin_offset)>;
  void set_configure_callback(const ConfigureCallback& configure_callback) {
    configure_callback_ = configure_callback;
  }

  // When the client is asked to configure the surface, it should acknowledge
  // the configure request sometime before the commit. |serial| is the serial
  // from the configure callback.
  void AcknowledgeConfigure(uint32_t serial);

  // Set the "parent" of this surface. This window should be stacked above a
  // parent.
  void SetParent(ShellSurface* parent);

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

  // Before widget initialization, this method will be called. Depending on the
  // implementation, it may return true to force the surface to launch in a
  // maximized state.
  virtual bool ShouldAutoMaximize();

  // Return the initial show state for this surface.
  ui::WindowShowState initial_show_state() { return initial_show_state_; }

  // Overridden from SurfaceDelegate:
  void OnSetParent(Surface* parent, const gfx::Point& position) override;

  // Overridden from ShellSurfaceBase:
  void InitializeWindowState(ash::WindowState* window_state) override;
  base::Optional<gfx::Rect> GetWidgetBounds() const override;
  gfx::Point GetSurfaceOrigin() const override;

  // Overridden from aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // Overridden from ash::WindowStateObserver:
  void OnPreWindowStateTypeChange(ash::WindowState* window_state,
                                  ash::WindowStateType old_type) override;
  void OnPostWindowStateTypeChange(ash::WindowState* window_state,
                                   ash::WindowStateType old_type) override;

  // Overridden from wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // Overridden from ShellSurfaceBase:
  void SetWidgetBounds(const gfx::Rect& bounds) override;
  bool OnPreWidgetCommit() override;
  void OnPostWidgetCommit() override;

 private:
  class ScopedAnimationsDisabled;
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
    ~ScopedConfigure();

    void set_needs_configure() { needs_configure_ = true; }

   private:
    ShellSurface* const shell_surface_;
    const bool force_configure_;
    bool needs_configure_ = false;

    DISALLOW_COPY_AND_ASSIGN(ScopedConfigure);
  };

  // Set the parent window of this surface.
  void SetParentWindow(aura::Window* parent);

  // Sets up a transient window manager for this window if it can (i.e. if the
  // surface has a widget with a parent).
  void MaybeMakeTransient();

  // Asks the client to configure its surface. Optionally, the user can override
  // the behaviour to check for window dragging by setting ends_drag to true.
  void Configure(bool ends_drag = false);

  void AttemptToStartDrag(int component);

  void EndDrag();

  std::unique_ptr<ScopedAnimationsDisabled> scoped_animations_disabled_;

  std::unique_ptr<ui::CompositorLock> configure_compositor_lock_;
  ConfigureCallback configure_callback_;
  ScopedConfigure* scoped_configure_ = nullptr;
  base::circular_deque<std::unique_ptr<Config>> pending_configs_;

  gfx::Vector2d origin_offset_;
  gfx::Vector2d pending_origin_offset_;
  gfx::Vector2d pending_origin_offset_accumulator_;
  int resize_component_ = HTCAPTION;  // HT constant (see ui/base/hit_test.h)
  int pending_resize_component_ = HTCAPTION;
  ui::WindowShowState initial_show_state_ = ui::SHOW_STATE_DEFAULT;
  bool ignore_window_bounds_changes_ = false;

  DISALLOW_COPY_AND_ASSIGN(ShellSurface);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SHELL_SURFACE_H_
