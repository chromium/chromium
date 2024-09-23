// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_CLIENT_CONTROLLED_SHELL_SURFACE_H_
#define COMPONENTS_EXO_CLIENT_CONTROLLED_SHELL_SURFACE_H_

#include <memory>
#include <string>

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/arc_resize_lock_type.h"
#include "ash/wm/client_controlled_state.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/exo/client_controlled_accelerators.h"
#include "components/exo/shell_surface_base.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/compositor/compositor_lock.h"

namespace ash {
class NonClientFrameViewAsh;
class WideFrameView;
}  // namespace ash

namespace chromeos {
class ImmersiveFullscreenController;
}  // namespace chromeos

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
                                     public ui::CompositorLockClient {
 public:
  // TODO(mukai): integrate this with ShellSurfaceBase's callback.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnGeometryChanged(const gfx::Rect& geometry) = 0;
    virtual void OnStateChanged(chromeos::WindowStateType old_state_type,
                                chromeos::WindowStateType new_state_type) = 0;
    virtual void OnBoundsChanged(chromeos::WindowStateType current_state,
                                 chromeos::WindowStateType requested_state,
                                 int64_t display_id,
                                 const gfx::Rect& bounds_in_display,
                                 bool is_resize,
                                 int bounds_change,
                                 bool is_adjusted_bounds) = 0;
    virtual void OnDragStarted(int component) = 0;
    virtual void OnDragFinished(int x, int y, bool canceled) = 0;
    virtual void OnZoomLevelChanged(ZoomChange zoom_change) = 0;
  };

  ClientControlledShellSurface(Surface* surface,
                               bool can_minimize,
                               int container,
                               bool default_scale_cancellation,
                               bool supports_floated_state);

  ClientControlledShellSurface(const ClientControlledShellSurface&) = delete;
  ClientControlledShellSurface& operator=(const ClientControlledShellSurface&) =
      delete;

  ~ClientControlledShellSurface() override;

  Delegate* set_delegate(std::unique_ptr<Delegate> delegate) {
    delegate_ = std::move(delegate);
    return delegate_.get();
  }

  void set_server_reparent_window(bool reparent) {
    server_reparent_window_ = reparent;
  }

  // Set bounds in root window coordinates relative to the given display.
  void SetBounds(int64_t display_id, const gfx::Rect& bounds);

  // Set origin of bounds for surface while preserving the size.
  void SetBoundsOrigin(int64_t display_id, const gfx::Point& origin);

  // Set size of bounds for surface while preserving the origin.
  void SetBoundsSize(const gfx::Size& size);

  // Called when the client was maximized.
  void SetMaximized();

  // Called when the client was minimized.
  void SetMinimized();

  // Called when the client was restored.
  void SetRestored();

  // Called when the client changed the fullscreen state. When `fullscreen` is
  // true, `display_id` indicates the id of the display where the surface should
  // be shown, otherwise it is ignored. When `display::kInvalidDisplayId` is
  // specified, the current display may be used.
  void SetFullscreen(bool fullscreen, int64_t display_id);

  // Returns true if this shell surface is currently being dragged.
  bool IsDragging();

  // Pin/unpin the surface. Pinned surface cannot be switched to
  // other windows unless its explicitly unpinned.
  void SetPinned(chromeos::WindowPinType type);

  // Sets the surface to be on top of all other windows.
  void SetAlwaysOnTop(bool always_on_top);

  // Controls the visibility of the system UI when this surface is active.
  void SetSystemUiVisibility(bool autohide);

  // Set orientation for surface.
  void SetOrientation(Orientation orientation);

  // Set shadow bounds in surface coordinates. Empty bounds disable the shadow.
  void SetShadowBounds(const gfx::Rect& bounds);

  // Set the pending scale.
  void SetScale(double scale);

  // Sends the request to change the zoom level to the client.
  void ChangeZoomLevel(ZoomChange change);

  // Sends the window state change event to client.
  void OnWindowStateChangeEvent(chromeos::WindowStateType old_state,
                                chromeos::WindowStateType next_state);

  // Sends the window bounds change event to client. |display_id| specifies in
  // which display the surface should live in. |drag_bounds_change| is
  // a masked value of ash::WindowResizer::kBoundsChange_Xxx, and specifies
  // how the bounds was changed. The bounds change event may also come from a
  // snapped window state change |requested_state|.
  void OnBoundsChangeEvent(chromeos::WindowStateType current_state,
                           chromeos::WindowStateType requested_state,
                           int64_t display_id,
                           const gfx::Rect& bounds,
                           int drag_bounds_change,
                           bool is_adjusted_bounds);

  // Sends the window drag events to client.
  void OnDragStarted(int component);
  void OnDragFinished(bool cancel, const gfx::PointF& location);

  // Starts the drag operation.
  void StartDrag(int component, const gfx::PointF& location);

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
  void SetExtraTitle(const std::u16string& extra_title);

  // Rebind a surface as the root surface of the shell surface.
  void RebindRootSurface(Surface* root_surface,
                         bool can_minimize,
                         int container,
                         bool default_scale_cancellation,
                         bool supports_floated_state);

  // SurfaceTreeHost:
  void DidReceiveCompositorFrameAck() override;

  // ShellSurfaceBase:
  bool IsInputEnabled(Surface* surface) const override;
  void OnSetFrame(SurfaceFrameType type) override;
  void OnSetFrameColors(SkColor active_color, SkColor inactive_color) override;
  void SetSnapPrimary(float snap_ratio) override;
  void SetSnapSecondary(float snap_ratio) override;
  void SetPip() override;
  void UnsetPip() override;
  void SetFloatToLocation(
      chromeos::FloatStartLocation float_start_location) override;
  void OnDidProcessDisplayChanges(
      const DisplayConfigurationChange& configuration_change) override;

  // views::WidgetDelegate:
  void WindowClosing() override;
  bool CanMaximize() const override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  bool ShouldSaveWindowPlacement() const override;
  void SaveWindowPlacement(const gfx::Rect& bounds,
                           ui::mojom::WindowShowState show_state) override;
  bool GetSavedWindowPlacement(
      const views::Widget* widget,
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) const override;

  // views::View:
  gfx::Size GetMaximumSize() const override;
  void OnDeviceScaleFactorChanged(float old_dsf, float new_dsf) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;

  // ui::CompositorLockClient:
  void CompositorLockTimedOut() override;

  // A factory callback to create ClientControlledState::Delegate.
  using DelegateFactoryCallback = base::RepeatingCallback<
      std::unique_ptr<ash::ClientControlledState::Delegate>(void)>;

  // Set the factory callback for unit test.
  static void SetClientControlledStateDelegateFactoryForTest(
      const DelegateFactoryCallback& callback);

  ash::WideFrameView* wide_frame_for_test() { return wide_frame_.get(); }

  // Used to scale incoming coordinates from the client to DP.
  float GetClientToDpScale() const;

  // Used to scale incoming coordinates from the client to DP before the pending
  // scale is committed.
  float GetClientToDpPendingScale() const;

  // Sets the resize lock type to the surface.
  void SetResizeLockType(ash::ArcResizeLockType resize_lock_type);

  // Update the resizability based on the resize lock type.
  void UpdateResizability() override;

  // exo::ShellSurfaceBase
  void SetSystemModal(bool system_modal) override;

 protected:
  // ShellSurfaceBase:
  float GetScale() const override;

  // SurfaceTreeHost:
  float GetScaleFactor() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ClientControlledShellSurfaceTest,
                           OverlayShadowBounds);
  class ScopedSetBoundsLocally;
  class ScopedLockedToRoot;
  class ScopedDeferWindowStateUpdate;

  // ShellSurfaceBase:
  void SetWidgetBounds(const gfx::Rect& bounds,
                       bool adjusted_by_server) override;
  gfx::Rect GetVisibleBounds() const override;
  gfx::Rect GetShadowBounds() const override;
  void InitializeWindowState(ash::WindowState* window_state) override;
  std::optional<gfx::Rect> GetWidgetBounds() const override;
  gfx::Point GetSurfaceOrigin() const override;
  bool OnPreWidgetCommit() override;
  void ShowWidget(bool activate) override;
  void OnPostWidgetCommit() override;
  void OnSurfaceDestroying(Surface* surface) override;

  // Update frame status. This may create (or destroy) a wide frame
  // that spans the full work area width if the surface didn't cover
  // the work area.
  void UpdateFrame();

  void UpdateCaptionButtonModel();

  void UpdateBackdrop();

  void UpdateFrameWidth();

  void UpdateFrameType() override;

  bool GetCanResizeFromSizeConstraints() const override;

  void AttemptToStartDrag(int component, const gfx::PointF& location);

  // Lock the compositor if it's not already locked, or extends the
  // lock timeout if it's already locked.
  // TODO(reveman): Remove this when using configure callbacks for orientation.
  // crbug.com/765954
  void EnsureCompositorIsLockedForOrientationChange();

  ash::WindowState* GetWindowState();
  ash::NonClientFrameViewAsh* GetFrameView();
  const ash::NonClientFrameViewAsh* GetFrameView() const;

  void EnsurePendingScale(bool commit_immediately);

  gfx::Rect GetClientBoundsForWindowBoundsAndWindowState(
      const gfx::Rect& window_bounds,
      chromeos::WindowStateType window_state) const;

  uint32_t frame_visible_button_mask_ = 0;
  uint32_t frame_enabled_button_mask_ = 0;

  std::unique_ptr<Delegate> delegate_;

  // TODO(reveman): Use configure callbacks for orientation. crbug.com/765954
  Orientation pending_orientation_ = Orientation::LANDSCAPE;
  Orientation orientation_ = Orientation::LANDSCAPE;
  Orientation expected_orientation_ = Orientation::LANDSCAPE;

  raw_ptr<ash::ClientControlledState> client_controlled_state_ = nullptr;

  chromeos::WindowStateType pending_window_state_ =
      chromeos::WindowStateType::kNormal;

  bool pending_always_on_top_ = false;

  SurfaceFrameType pending_frame_type_ = SurfaceFrameType::NONE;

  bool can_maximize_ = true;

  std::unique_ptr<chromeos::ImmersiveFullscreenController>
      immersive_fullscreen_controller_;

  std::unique_ptr<ash::WideFrameView> wide_frame_;

  std::unique_ptr<ui::CompositorLock> orientation_compositor_lock_;

  // The extra title to be applied when widget is being created.
  std::u16string initial_extra_title_ = std::u16string();

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

  bool display_rotating_with_pip_ = false;

  // True if the window state has changed during the commit.
  bool state_changed_ = false;

  // When false, the client handles all display scale changes, so the
  // buffer should be re-scaled to undo any scaling added by exo so that the
  // 1:1 correspondence between the pixels is maintained.
  bool use_default_scale_cancellation_ = false;

  // Client controlled specific accelerator target.
  std::unique_ptr<ClientControlledAcceleratorTarget> accelerator_target_;

  ash::ArcResizeLockType pending_resize_lock_type_ =
      ash::ArcResizeLockType::NONE;

  std::unique_ptr<ScopedDeferWindowStateUpdate>
      scoped_defer_window_state_update_;

  // True if the window supports the floated state.
  bool supports_floated_state_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_CLIENT_CONTROLLED_SHELL_SURFACE_H_
