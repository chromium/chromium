// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_CLIENT_CONTROLLED_SHELL_SURFACE_H_
#define COMPONENTS_EXO_CLIENT_CONTROLLED_SHELL_SURFACE_H_

#include <memory>
#include <string>

#include "ash/display/screen_orientation_controller.h"
#include "ash/wm/client_controlled_state.h"
#include "base/callback.h"
#include "base/macros.h"
#include "components/exo/client_controlled_accelerators.h"
#include "components/exo/shell_surface_base.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/compositor_lock.h"
#include "ui/display/display_observer.h"

namespace ash {
class NonClientFrameViewAsh;
class ImmersiveFullscreenController;
class RoundedCornerDecorator;
class WideFrameView;

namespace mojom {
enum class WindowPinType;
}
}  // namespace ash

namespace exo {
class Surface;
class ClientControlledAcceleratorTarget;

enum class Orientation { PORTRAIT, LANDSCAPE };
enum class ZoomChange { IN, OUT, RESET };

// This class implements a ShellSurface whose window state and bounds are
// controlled by a remote shell client rather than the window manager. The
// position specified as part of the geometry is relative to the origin of
// the screen coordinate system.
class ClientControlledShellSurface : public ShellSurfaceBase,
                                     public display::DisplayObserver,
                                     public ui::CompositorLockClient {
 public:
  ClientControlledShellSurface(Surface* surface,
                               bool can_minimize,
                               int container);
  ~ClientControlledShellSurface() override;

  using GeometryChangedCallback =
      base::RepeatingCallback<void(const gfx::Rect& geometry)>;

  void set_geometry_changed_callback(const GeometryChangedCallback& callback) {
    geometry_changed_callback_ = callback;
  }

  void set_server_reparent_window(bool reparent) {
    server_reparent_window_ = reparent;
  }

  // Set bounds in root window coordinates relative to the given display.
  void SetBounds(int64_t display_id, const gfx::Rect& bounds);

  // Called when the client was maximized.
  void SetMaximized();

  // Called when the client was minimized.
  void SetMinimized();

  // Called when the client was restored.
  void SetRestored();

  // Called when the client changed the fullscreen state.
  void SetFullscreen(bool fullscreen);

  // Called when the client was snapped to left.
  void SetSnappedToLeft();

  // Called when the client was snapped to right.
  void SetSnappedToRight();

  // Called when the client was set to PIP.
  void SetPip();

  // Set the callback to run when the surface state changed.
  using StateChangedCallback =
      base::RepeatingCallback<void(ash::WindowStateType old_state_type,
                                   ash::WindowStateType new_state_type)>;
  void set_state_changed_callback(
      const StateChangedCallback& state_changed_callback) {
    state_changed_callback_ = state_changed_callback;
  }

  // Set the callback to run when the surface bounds changed.
  using BoundsChangedCallback =
      base::RepeatingCallback<void(ash::WindowStateType current_state,
                                   ash::WindowStateType requested_state,
                                   int64_t display_id,
                                   const gfx::Rect& bounds_in_display,
                                   bool is_resize,
                                   int bounds_change)>;
  void set_bounds_changed_callback(
      const BoundsChangedCallback& bounds_changed_callback) {
    bounds_changed_callback_ = bounds_changed_callback;
  }

  bool has_bounds_changed_callback() const {
    return static_cast<bool>(bounds_changed_callback_);
  }

  // Set the callback to run when the drag operation started.
  using DragStartedCallback = base::RepeatingCallback<void(int direction)>;
  void set_drag_started_callback(const DragStartedCallback& callback) {
    drag_started_callback_ = callback;
  }

  // Set the callback to run when the drag operation finished.
  using DragFinishedCallback = base::RepeatingCallback<void(int, int, bool)>;
  void set_drag_finished_callback(const DragFinishedCallback& callback) {
    drag_finished_callback_ = callback;
  }

  // Set callback to run when user requests to change a zoom level.
  using ChangeZoomLevelCallback = base::RepeatingCallback<void(ZoomChange)>;
  void set_change_zoom_level_callback(const ChangeZoomLevelCallback& callback) {
    change_zoom_level_callback_ = callback;
  }

  // Returns true if this shell surface is currently being dragged.
  bool IsDragging();

  // Pin/unpin the surface. Pinned surface cannot be switched to
  // other windows unless its explicitly unpinned.
  void SetPinned(ash::WindowPinType type);

  // Sets the surface to be on top of all other windows.
  void SetAlwaysOnTop(bool always_on_top);

  // Sets the IME to be blocked so that all events are forwarded by Exo.
  void SetImeBlocked(bool ime_blocked);

  // Controls the visibility of the system UI when this surface is active.
  void SetSystemUiVisibility(bool autohide);

  // Set orientation for surface.
  void SetOrientation(Orientation orientation);

  // Set shadow bounds in surface coordinates. Empty bounds disable the shadow.
  void SetShadowBounds(const gfx::Rect& bounds);

  // Set the pending scale.
  void SetScale(double scale);

  // Commit the pending scale if it was changed. The scale set by SetScale() is
  // otherwise committed by OnPostWidgetCommit().
  void CommitPendingScale();

  // Set top inset for surface.
  void SetTopInset(int height);

  // Set resize outset for surface.
  void SetResizeOutset(int outset);

  // Sends the request to change the zoom level to the client.
  void ChangeZoomLevel(ZoomChange change);

  // Sends the window state change event to client.
  void OnWindowStateChangeEvent(ash::WindowStateType old_state,
                                ash::WindowStateType next_state);

  // Sends the window bounds change event to client. |display_id| specifies in
  // which display the surface should live in. |drag_bounds_change| is
  // a masked value of ash::WindowResizer::kBoundsChange_Xxx, and specifies
  // how the bounds was changed. The bounds change event may also come from a
  // snapped window state change |requested_state|.
  void OnBoundsChangeEvent(ash::WindowStateType current_state,
                           ash::WindowStateType requested_state,
                           int64_t display_id,
                           const gfx::Rect& bounds,
                           int drag_bounds_change);

  // Sends the window drag events to client.
  void OnDragStarted(int component);
  void OnDragFinished(bool cancel, const gfx::Point& location);

  // Starts the drag operation.
  void StartDrag(int component, const gfx::Point& location);

  // Set if the surface can be maximzied.
  void SetCanMaximize(bool can_maximize);

  // Update the auto hide frame state.
  void UpdateAutoHideFrame();

  // Set the frame button state. The |visible_button_mask| and
  // |enabled_button_mask| is a bit mask whose position is defined
  // in views::CaptionButtonIcon enum.
  void SetFrameButtons(uint32_t frame_visible_button_mask,
                       uint32_t frame_enabled_button_mask);

  // Set the extra title for the surface.
  void SetExtraTitle(const base::string16& extra_title);

  // Set specific orientation lock for this surface. When this surface is in
  // foreground and the display can be rotated (e.g. tablet mode), apply the
  // behavior defined by |orientation_lock|. See more details in
  // //ash/display/screen_orientation_controller.h.
  void SetOrientationLock(ash::OrientationLockType orientation_lock);

  // Overridden from SurfaceDelegate:
  bool IsInputEnabled(Surface* surface) const override;
  void OnSetFrame(SurfaceFrameType type) override;
  void OnSetFrameColors(SkColor active_color, SkColor inactive_color) override;

  // Overridden from views::WidgetDelegate:
  bool CanMaximize() const override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  void SaveWindowPlacement(const gfx::Rect& bounds,
                           ui::WindowShowState show_state) override;
  bool GetSavedWindowPlacement(const views::Widget* widget,
                               gfx::Rect* bounds,
                               ui::WindowShowState* show_state) const override;

  // Overridden from views::View:
  gfx::Size GetMaximumSize() const override;
  void OnDeviceScaleFactorChanged(float old_dsf, float new_dsf) override;

  // Overridden from aura::WindowObserver:
  void OnWindowAddedToRootWindow(aura::Window* window) override;

  // Overridden from display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // Overridden from ui::CompositorLockClient:
  void CompositorLockTimedOut() override;

  // A factory callback to create ClientControlledState::Delegate.
  using DelegateFactoryCallback = base::RepeatingCallback<
      std::unique_ptr<ash::ClientControlledState::Delegate>(void)>;

  // Set the factory callback for unit test.
  static void SetClientControlledStateDelegateFactoryForTest(
      const DelegateFactoryCallback& callback);

  ash::WideFrameView* wide_frame_for_test() { return wide_frame_.get(); }

  // Exposed for testing. Returns the effective scale as opposed to
  // |pending_scale_|.
  double scale() const { return scale_; }

 private:
  class ScopedSetBoundsLocally;
  class ScopedLockedToRoot;

  // Overridden from ShellSurfaceBase:
  void SetWidgetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetShadowBounds() const override;
  void InitializeWindowState(ash::WindowState* window_state) override;
  float GetScale() const override;
  base::Optional<gfx::Rect> GetWidgetBounds() const override;
  gfx::Point GetSurfaceOrigin() const override;
  bool OnPreWidgetCommit() override;
  void OnPostWidgetCommit() override;
  void OnSurfaceDestroying(Surface* surface) override;

  // Update frame status. This may create (or destroy) a wide frame
  // that spans the full work area width if the surface didn't cover
  // the work area.
  void UpdateFrame();

  void UpdateCaptionButtonModel();

  void UpdateBackdrop();

  void UpdateFrameWidth();

  void AttemptToStartDrag(int component, const gfx::Point& location);

  // Lock the compositor if it's not already locked, or extends the
  // lock timeout if it's already locked.
  // TODO(reveman): Remove this when using configure callbacks for orientation.
  // crbug.com/765954
  void EnsureCompositorIsLockedForOrientationChange();

  ash::WindowState* GetWindowState();
  ash::NonClientFrameViewAsh* GetFrameView();
  const ash::NonClientFrameViewAsh* GetFrameView() const;

  GeometryChangedCallback geometry_changed_callback_;

  int top_inset_height_ = 0;
  int pending_top_inset_height_ = 0;

  double scale_ = 1.0;
  double pending_scale_ = 1.0;

  uint32_t frame_visible_button_mask_ = 0;
  uint32_t frame_enabled_button_mask_ = 0;

  StateChangedCallback state_changed_callback_;
  BoundsChangedCallback bounds_changed_callback_;
  DragStartedCallback drag_started_callback_;
  DragFinishedCallback drag_finished_callback_;
  ChangeZoomLevelCallback change_zoom_level_callback_;

  // TODO(reveman): Use configure callbacks for orientation. crbug.com/765954
  Orientation pending_orientation_ = Orientation::LANDSCAPE;
  Orientation orientation_ = Orientation::LANDSCAPE;
  Orientation expected_orientation_ = Orientation::LANDSCAPE;

  ash::ClientControlledState* client_controlled_state_ = nullptr;

  ash::WindowStateType pending_window_state_ = ash::WindowStateType::kNormal;

  bool pending_always_on_top_ = false;

  ash::WindowPinType current_pin_;

  bool can_maximize_ = true;

  std::unique_ptr<ash::ImmersiveFullscreenController>
      immersive_fullscreen_controller_;

  std::unique_ptr<ash::WideFrameView> wide_frame_;

  std::unique_ptr<ash::RoundedCornerDecorator> decorator_;

  std::unique_ptr<ui::CompositorLock> orientation_compositor_lock_;

  // The orientation to be applied when widget is being created. Only set when
  // widget is not created yet orientation lock is being set.
  ash::OrientationLockType initial_orientation_lock_ =
      ash::OrientationLockType::kAny;

  bool preserve_widget_bounds_ = false;

  // Checking DragDetails is not sufficient to determine if a bounds
  // request happened during a drag move or resize. If the window resizer
  // requests a bounds update after completing the drag but before the
  // drag details are cleaned up, we want to consider that as a regular
  // bounds update, not a drag move/resize update.
  bool in_drag_ = false;

  // N uses older protocol which expects that server will reparent the window.
  // TODO(oshima): Remove this once all boards are migrated to P or above.
  bool server_reparent_window_ = false;

  bool ignore_bounds_change_request_ = false;

  // True if the window state has changed during the commit.
  bool state_changed_ = false;

  // Client controlled specific accelerator target.
  std::unique_ptr<ClientControlledAcceleratorTarget> accelerator_target_;

  DISALLOW_COPY_AND_ASSIGN(ClientControlledShellSurface);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_CLIENT_CONTROLLED_SHELL_SURFACE_H_
