// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SHELL_SURFACE_BASE_H_
#define COMPONENTS_EXO_SHELL_SURFACE_BASE_H_

#include <stdint.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "ash/display/window_tree_host_manager.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "components/exo/surface_observer.h"
#include "components/exo/surface_tree_host.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/aura/client/capture_client_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/hit_test_utils.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/tooltip_observer.h"

namespace ash {
class WindowState;
}  // namespace ash

namespace views {
class ClientView;
}  // namespace views

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace exo {
class Surface;

// This class provides functions for treating a surfaces like toplevel,
// fullscreen or popup widgets, move, resize or maximize them, associate
// metadata like title and class, etc.
class ShellSurfaceBase : public SurfaceTreeHost,
                         public SurfaceObserver,
                         public aura::WindowObserver,
                         public aura::client::CaptureClientObserver,
                         public views::WidgetDelegate,
                         public views::WidgetObserver,
                         public views::View,
                         public wm::ActivationChangeObserver,
                         public wm::TooltipObserver {
 public:
  // The |origin| is the initial position in screen coordinates. The position
  // specified as part of the geometry is relative to the shell surface.
  ShellSurfaceBase(Surface* surface,
                   const gfx::Point& origin,
                   bool can_minimize,
                   int container);

  ShellSurfaceBase(const ShellSurfaceBase&) = delete;
  ShellSurfaceBase& operator=(const ShellSurfaceBase&) = delete;

  ~ShellSurfaceBase() override;

  // Set the callback to run when the user wants the shell surface to be closed.
  // The receiver can chose to not close the window on this signal.
  void set_close_callback(const base::RepeatingClosure& close_callback) {
    close_callback_ = close_callback;
  }

  // Set the callback to run when the user has requested to close the surface.
  // This runs before the normal |close_callback_| and should not be used to
  // actually close the surface.
  void set_pre_close_callback(const base::RepeatingClosure& close_callback) {
    pre_close_callback_ = close_callback;
  }

  // Set the callback to run when the surface is destroyed.
  void set_surface_destroyed_callback(
      base::OnceClosure surface_destroyed_callback) {
    surface_destroyed_callback_ = std::move(surface_destroyed_callback);
  }

  // Whether the connected client supports setting window bounds and is
  // expecting to receive window origin in configure updates.
  bool client_supports_window_bounds() const {
    return client_supports_window_bounds_;
  }

  void set_client_supports_window_bounds(bool enable) {
    client_supports_window_bounds_ = enable;
  }

  // Activates the shell surface. Brings it to the foreground.
  void Activate();
  void RequestActivation();

  // Deactivates the shell surface. Makes it not the foreground.
  void Deactivate();
  void RequestDeactivation();

  // Set title for the surface.
  void SetTitle(const std::u16string& title);

  // Set icon for the surface.
  void SetIcon(const gfx::ImageSkia& icon);

  // Set the application ID for the surface.
  void SetApplicationId(const char* application_id);

  // Set the startup ID for the surface.
  void SetStartupId(const char* startup_id);

  // Set the child ax tree ID for the surface.
  void SetChildAxTreeId(ui::AXTreeID child_ax_tree_id);

  // Set geometry for surface. The geometry represents the "visible bounds"
  // for the surface from the user's perspective.
  void SetGeometry(const gfx::Rect& geometry);

  // If set, geometry is in display rather than window or screen coordinates.
  void SetDisplay(int64_t display_id);

  // Set origin in screen coordinate space.
  void SetOrigin(const gfx::Point& origin);

  // Set activatable state for surface.
  void SetActivatable(bool activatable);

  // Set container for surface.
  void SetContainer(int container);

  // Set the maximum size for the surface. If the size smaller than the minimum
  // size is given, it's ignored. However, the given maximum size is restored
  // once the minimum size is set to that smaller than the maximum size.
  void SetMaximumSize(const gfx::Size& size);

  // Set the miniumum size for the surface.
  void SetMinimumSize(const gfx::Size& size);

  // Set the flag if the surface can maximize or not.
  void SetCanMinimize(bool can_minimize);

  // Set whether the window is persistable.  This should be called before the
  // widget is created.
  void SetPersistable(bool persistable);

  // Sets the window corner radii.
  void SetWindowCornersRadii(const gfx::RoundedCornersF& radii);

  // Sets the shadow corner radii.
  void SetShadowCornersRadii(const gfx::RoundedCornersF& radii);

  // Set normal shadow bounds, |shadow_bounds_|, to |bounds| to be used and
  // applied via `UpdateShadow()`. Set and update resize shadow bounds with
  // |widget_|'s origin and |bounds| via `UpdateResizeShadowBoundsOfWindow()`.
  void SetBoundsForShadows(const std::optional<gfx::Rect>& bounds);

  // Make the shell surface menu type.
  void SetMenu();

  // Prevents shell surface from being moved.
  void DisableMovement();

  // Update the resizability for the surface.
  virtual void UpdateResizability();

  // Rebind a surface as the root surface of the shell surface.
  void RebindRootSurface(Surface* root_surface,
                         bool can_minimize,
                         int container);

  // Set the window bounds. The bounds specify 'visible bounds' of the
  // shell surface.
  void SetWindowBounds(const gfx::Rect& bounds_in_screen);

  // Set `restore_session_id_` and `restore_window_id_` to be the browser
  // session id and restore id, respectively.
  void SetRestoreInfo(int32_t restore_id, int32_t restore_window_id);

  // Set `restore_window_id_source` to be the app id for Restore to fetch window
  // id for.
  void SetRestoreInfoWithWindowIdSource(
      int32_t restore_id,
      const std::string& restore_window_id_source);

  // Unfloats the shell surface.
  void UnsetFloat();

  // Returns a trace value representing the state of the surface.
  std::unique_ptr<base::trace_event::TracedValue> AsTracedValue() const;

  // An overlay creation parameters. The view is owned by the
  // overlay.
  struct OverlayParams {
    explicit OverlayParams(std::unique_ptr<views::View> overlay);
    ~OverlayParams();

    bool translucent = false;
    bool overlaps_frame = true;
    std::optional<bool> can_resize;
    // TODO(oshima): It's unlikely for overlay not to request focus.
    // Remove this.
    bool focusable = true;
    std::optional<gfx::RoundedCornersF> corners_radii;
    std::unique_ptr<views::View> contents_view;
  };

  // Add a new overlay. Currently only one overlay is supported.
  // It is caller's responsibility to make sure there is no overlay
  // before calling this.
  void AddOverlay(OverlayParams&& params);

  // Remove the current overlay. This is no-op if there is no overlay.
  void RemoveOverlay();

  bool HasOverlay() const { return !!overlay_widget_; }

  // Set specific orientation lock for this surface. When this surface is in
  // foreground and the display can be rotated (e.g. tablet mode), apply the
  // behavior defined by |orientation_lock|. See more details in
  // //ash/display/screen_orientation_controller.h.
  void SetOrientationLock(chromeos::OrientationType orientation_lock);

  // Sets the z order for the window. If the window's widget has not yet been
  // initialized, it saves `z_order` for when it is initialized.
  void SetZOrder(ui::ZOrderLevel z_order);

  // Sets the shape of the toplevel window, applied on commit. If shape is null
  // this will unset the window shape.
  void SetShape(std::optional<cc::Region> shape);

  // SurfaceDelegate:
  void OnSurfaceCommit() override;
  bool IsInputEnabled(Surface* surface) const override;
  void OnSetFrame(SurfaceFrameType type) override;
  void OnSetFrameColors(SkColor active_color, SkColor inactive_color) override;
  void OnSetStartupId(const char* startup_id) override;
  void OnSetApplicationId(const char* application_id) override;
  void SetUseImmersiveForFullscreen(bool value) override;
  void ShowSnapPreviewToPrimary() override;
  void ShowSnapPreviewToSecondary() override;
  void HideSnapPreview() override;
  void SetSnapPrimary(float snap_ratio) override;
  void SetSnapSecondary(float snap_ratio) override;
  void UnsetSnap() override;
  void OnActivationRequested() override;
  void OnSetServerStartResize() override;
  void SetCanGoBack() override;
  void UnsetCanGoBack() override;
  void SetPip() override;
  void UnsetPip() override;
  void SetFloatToLocation(
      chromeos::FloatStartLocation float_start_location) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void MoveToDesk(int desk_index) override;
  void SetVisibleOnAllWorkspaces() override;
  void SetInitialWorkspace(const char* initial_workspace) override;
  void Pin(bool trusted) override;
  void Unpin() override;
  void SetSystemModal(bool system_modal) override;
  void SetTopInset(int height) override;

  // SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;
  void OnFrameLockingChanged(Surface*, bool) override {}
  void OnDeskChanged(Surface*, int) override {}
  void OnTooltipShown(Surface* surface,
                      const std::u16string& text,
                      const gfx::Rect& bounds) override {}
  void OnTooltipHidden(Surface* surface) override {}

  // CaptureClientObserver:
  void OnCaptureChanged(aura::Window* lost_capture,
                        aura::Window* gained_capture) override;

  // views::WidgetDelegate:
  bool OnCloseRequested(views::Widget::ClosedReason close_reason) override;
  void WindowClosing() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  views::View* GetContentsView() override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  bool ShouldSaveWindowPlacement() const override;
  bool WidgetHasHitTestMask() const override;
  void GetWidgetHitTestMask(SkPath* mask) const override;
  views::ClientView* CreateClientView(views::Widget* widget) override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  // This returns the surface's min/max size. If you want to know the
  // widget/window's min/mx size, you must use
  // ShellSurfaceBase::GetWidget()->GetXxxSize.
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  views::FocusTraversable* GetFocusTraversable() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old_value) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // wm::TooltipObserver:
  void OnTooltipShown(aura::Window* target,
                      const std::u16string& text,
                      const gfx::Rect& bounds) override;
  void OnTooltipHidden(aura::Window* target) override;

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // SurfaceTreeHost:
  void SetRootSurface(Surface* root_surface) override;
  float GetPendingScaleFactor() const override;

  bool frame_enabled() const {
    return frame_type_ != SurfaceFrameType::NONE &&
           frame_type_ != SurfaceFrameType::SHADOW;
  }

  bool frame_overlapped() const {
    return frame_type_ == SurfaceFrameType::OVERLAP;
  }

  Surface* surface_for_testing() { return root_surface(); }
  bool get_shadow_bounds_changed_for_testing() {
    return shadow_bounds_changed_;
  }

  bool server_side_resize() const { return server_side_resize_; }

  const views::Widget* overlay_widget_for_testing() const {
    return overlay_widget_.get();
  }

  // Returns true if surface is currently being dragged.
  bool IsDragged() const;

  void set_in_extended_drag(bool in_extended_drag) {
    in_extended_drag_ = in_extended_drag;
  }

  const std::optional<cc::Region>& shape_dp() const { return shape_dp_; }

  // Window corners radii in dps.
  const std::optional<gfx::RoundedCornersF>& window_corners_radii() const {
    return window_corners_radii_dp_;
  }

  const std::optional<gfx::RoundedCornersF>& shadow_corner_radii() const {
    return shadow_corners_radii_dp_;
  }

  // Returns true if the shell surface has completed its initial configure
  // and commit sequence (i.e. is ready and initialized).
  bool IsReady() const;

 protected:
  bool has_frame_colors() const { return has_frame_colors_; }

  // Creates the |widget_| for |surface_|. |show_state| is the initial state
  // of the widget (e.g. maximized).
  void CreateShellSurfaceWidget(ui::mojom::WindowShowState show_state);

  // Returns true if the window is the ShellSurface's widget's window.
  bool IsShellSurfaceWindow(const aura::Window* window) const;

  // Lets subclasses modify Widget parameters immediately before widget
  // creation.
  virtual void OverrideInitParams(views::Widget::InitParams* params) {}

  // Returns true if surface is currently being resized.
  bool IsResizing() const;

  // Updates the bounds of widget to match the current surface bounds.
  void UpdateWidgetBounds();

  // Returns a bounds that WindowManager might have applied the constraints to.
  virtual gfx::Rect ComputeAdjustedBounds(const gfx::Rect& bounds) const;

  // Called by UpdateWidgetBounds to set widget bounds. If the
  // `adjusted_by_server` is true, the bounds requested by a client is updated
  // to satisfy the constraints.
  virtual void SetWidgetBounds(const gfx::Rect& bounds,
                               bool adjusted_by_server) = 0;

  // Updates the bounds of host window to match the current widget bounds.
  void UpdateHostWindowOrigin();

  // Creates, deletes and update the shadow bounds based on
  // |shadow_bounds_|.
  void UpdateShadow();

  virtual void UpdateFrameType();

  void UpdateWindowRoundedCorners();

  // Applies |system_modal_| to |widget_|.
  void UpdateSystemModal();

  // Applies `shape_rects_dp_` to the host window's layer.
  void UpdateShape();

  // Returns the "visible bounds" for the surface from the user's perspective.
  // TODO(b/299688152): Make this non virtual back once ARC is updated not to
  // use geometry to specify the window bounds.
  virtual gfx::Rect GetVisibleBounds() const;

  // Returns the bounds of the client area.
  gfx::Rect GetClientViewBounds() const;

  // Computes the widget bounds using visible bounds.
  gfx::Rect GetWidgetBoundsFromVisibleBounds() const;

  // In the local coordinate system of the window.
  virtual gfx::Rect GetShadowBounds() const;

  // Start the event capture on this surface.
  void StartCapture();

  const gfx::Rect& geometry() const { return geometry_; }
  aura::Window* parent() const { return parent_; }

  // Install custom window targeter. Used to restore window targeter.
  void InstallCustomWindowTargeter();

  // Creates a NonClientFrameView for shell surface.
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameViewInternal(
      views::Widget* widget);

  virtual void OnPostWidgetCommit();
  virtual void ShowWidget(bool active);

  void SetParentInternal(aura::Window* window);
  void SetContainerInternal(int container);

  // Converts min/max sizes to resizeability. This needs to be overridden as
  // different clients have different default min/max values.
  virtual bool GetCanResizeFromSizeConstraints() const = 0;

  // Returns true if this surface will exit fullscreen from a restore or
  // maximize request. Currently only true for Lacros.
  bool ShouldExitFullscreenFromRestoreOrMaximized();

  static bool IsPopupWithGrab(aura::Window* window);

  raw_ptr<views::Widget> widget_ = nullptr;
  bool movement_disabled_ = false;
  // This value is in the screen coordinates.
  gfx::Point origin_;

  // Container Window Id (see ash/public/cpp/shell_window_ids.h)
  int container_;
  gfx::Rect geometry_;
  gfx::Rect pending_geometry_;
  std::optional<gfx::Rect> initial_bounds_;
  std::optional<cc::Region> shape_dp_;
  std::optional<cc::Region> pending_shape_dp_;

  // Radii of window corners in dps. Currently only specified by clients that do
  // server-side rounded windows.
  std::optional<gfx::RoundedCornersF> window_corners_radii_dp_;
  std::optional<gfx::RoundedCornersF> pending_window_corners_radii_dp_;

  // Radii of shadow corners in dps.
  std::optional<gfx::RoundedCornersF> shadow_corners_radii_dp_;
  std::optional<gfx::RoundedCornersF> pending_shadow_corners_radii_dp_;

  int64_t display_id_ = display::kInvalidDisplayId;
  int64_t pending_display_id_ = display::kInvalidDisplayId;
  std::optional<gfx::Rect> shadow_bounds_;
  bool shadow_bounds_changed_ = false;
  SurfaceFrameType frame_type_ = SurfaceFrameType::NONE;
  bool is_popup_ = false;
  bool is_menu_ = false;
  bool has_grab_ = false;
  bool server_side_resize_ = false;
  bool needs_layout_on_show_ = false;
  bool client_supports_window_bounds_ = false;

  // The requested size constraint for this window.
  // Actual return value of GetMaximumSize() may be modified to make sure it's
  // equal to or greater than the minimum size constraint.
  gfx::Size requested_minimum_size_;
  gfx::Size requested_maximum_size_;

  // Effective and pending top inset (header) heights, that are reserved or
  // occupied by the top window frame.
  int top_inset_height_ = 0;
  int pending_top_inset_height_ = 0;

  // The orientation to be applied when widget is being created. Only set when
  // widget is not created yet orientation lock is being set. This is currently
  // only used by ClientControlledShellSurface.
  chromeos::OrientationType initial_orientation_lock_ =
      chromeos::OrientationType::kAny;

 private:
  FRIEND_TEST_ALL_PREFIXES(ShellSurfaceTest,
                           HostWindowBoundsUpdatedAfterCommitWidget);
  FRIEND_TEST_ALL_PREFIXES(ShellSurfaceTest,
                           HostWindowBoundsUpdatedWithNegativeCoordinate);
  FRIEND_TEST_ALL_PREFIXES(ShellSurfaceTest,
                           HostWindowIncludesAllSubSurfacesWithScaleFactor);
  FRIEND_TEST_ALL_PREFIXES(ShellSurfaceTest,
                           ShadowBoundsWithNegativeCoordinate);
  FRIEND_TEST_ALL_PREFIXES(ShellSurfaceTest, ShadowBoundsWithScaleFactor);
  FRIEND_TEST_ALL_PREFIXES(ShellSurfaceTest,
                           LocalSurfaceIdUpdatedOnHostWindowOriginChanged);
  FRIEND_TEST_ALL_PREFIXES(
      ShellSurfaceTest,
      LocalSurfaceIdUpdatedOnHostWindowOriginChangedWithScaleFactor);
  FRIEND_TEST_ALL_PREFIXES(ShellSurfaceTest, SubpixelPositionOffset);

  // Updates the shadow's rounded corner associated with the `widget_`.
  void UpdateShadowRoundedCorners();

  // Called on widget creation to initialize its window state.
  // TODO(reveman): Remove virtual functions below to avoid FBC problem.
  virtual void InitializeWindowState(ash::WindowState* window_state) = 0;

  // Returns the scale of the surface tree relative to the shell surface.
  virtual float GetScale() const;

  // Return the bounds of the widget/origin of surface taking visible
  // bounds and current resize direction into account.
  virtual std::optional<gfx::Rect> GetWidgetBounds() const = 0;
  virtual gfx::Point GetSurfaceOrigin() const = 0;

  // Commit is deferred if this returns false.
  virtual bool OnPreWidgetCommit() = 0;

  void CommitWidget();

  bool IsFrameDecorationSupported(SurfaceFrameType frame_type);

  void UpdatePinned();

  void UpdateTopInset();

  // Returns the resizability of the window. Useful to get the resizability
  // without actually updating it.
  bool CalculateCanResize() const;

  raw_ptr<aura::Window> parent_ = nullptr;
  bool activatable_ = true;
  bool can_minimize_ = true;
  bool has_frame_colors_ = false;
  SkColor active_frame_color_ = SK_ColorBLACK;
  SkColor inactive_frame_color_ = SK_ColorBLACK;
  bool pending_show_widget_ = false;
  std::optional<std::string> application_id_;
  std::optional<std::string> startup_id_;
  bool immersive_implied_by_fullscreen_ = true;
  base::RepeatingClosure close_callback_;
  base::RepeatingClosure pre_close_callback_;
  base::OnceClosure surface_destroyed_callback_;
  bool system_modal_ = false;
  bool non_system_modal_window_was_active_ = false;
  gfx::Size pending_minimum_size_;
  gfx::Size pending_maximum_size_;
  gfx::SizeF pending_aspect_ratio_;
  bool pending_pip_ = false;
  bool in_extended_drag_ = false;
  std::optional<std::string> initial_workspace_;
  std::optional<ui::ZOrderLevel> initial_z_order_;

  // Restore members. These pass window restore related ids from exo clients,
  // e.g. Lacros, so that the window can be created with the correct restore
  // info looked up using the ids.
  std::optional<int32_t> restore_session_id_;
  std::optional<int32_t> restore_window_id_;
  std::optional<std::string> restore_window_id_source_;

  // Member determines if the owning process is persistable.
  bool persistable_ = true;

  // Overlay members.
  std::unique_ptr<views::Widget> overlay_widget_;
  bool skip_ime_processing_ = false;
  bool overlay_overlaps_frame_ = true;
  std::optional<bool> overlay_can_resize_;

  // We independently store whether a widget should be activated on creation.
  // The source of truth is on widget, but there are two problems:
  //   (1) The widget has no activation state before it has shown.
  //   (2) In the wayland protocol, asynchronous buffer-commit causes a surface
  //   to be shown. Because this is asynchronous, it's possible for a surface to
  //   be deactivated before shown.
  bool initially_activated_ = true;

  // Pin members.
  chromeos::WindowPinType current_pinned_state_ =
      chromeos::WindowPinType::kNone;
  chromeos::WindowPinType pending_pinned_state_ =
      chromeos::WindowPinType::kNone;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SHELL_SURFACE_BASE_H_
