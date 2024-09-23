// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZCR_REMOTE_SHELL_IMPL_H_
#define COMPONENTS_EXO_WAYLAND_ZCR_REMOTE_SHELL_IMPL_H_

#include <remote-shell-unstable-v1-server-protocol.h>
#include <remote-shell-unstable-v2-server-protocol.h>

#include "ash/wm/window_state.h"
#include "base/memory/raw_ptr.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/input_method_surface.h"
#include "components/exo/notification_surface.h"
#include "components/exo/seat.h"
#include "components/exo/seat_observer.h"
#include "components/exo/surface.h"
#include "components/exo/toast_surface.h"
#include "components/exo/wayland/wayland_display_observer.h"
#include "components/exo/wayland/zcr_remote_shell.h"
#include "components/exo/wayland/zcr_remote_shell_event_mapping.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager_observer.h"

namespace exo {
namespace wayland {

using chromeos::WindowStateType;

class WaylandRemoteOutput : public WaylandDisplayObserver {
 public:
  WaylandRemoteOutput(wl_resource* resource,
                      WaylandRemoteOutputEventMapping event_mapping,
                      WaylandDisplayHandler* display_handler);
  WaylandRemoteOutput(const WaylandRemoteOutput&) = delete;
  WaylandRemoteOutput& operator=(const WaylandRemoteOutput&) = delete;
  ~WaylandRemoteOutput() override;

  // Overridden from WaylandDisplayObserver:
  bool SendDisplayMetrics(const display::Display& display,
                          uint32_t changed_metrics) override;
  void SendActiveDisplay() override;
  void OnOutputDestroyed() override;

 private:
  const raw_ptr<wl_resource> resource_;

  bool initial_config_sent_ = false;

  WaylandRemoteOutputEventMapping const event_mapping_;

  raw_ptr<WaylandDisplayHandler> display_handler_;
};

// Implements remote shell interface and monitors workspace state needed
// for the remote shell interface.
class WaylandRemoteShell : public display::DisplayObserver,
                           public SeatObserver,
                           public display::DisplayManagerObserver {
 public:
  using OutputResourceProvider = base::RepeatingCallback<wl_resource*(int64_t)>;

  WaylandRemoteShell(Display* display,
                     wl_resource* remote_shell_resource,
                     OutputResourceProvider output_provider,
                     WaylandRemoteShellEventMapping event_mapping,
                     bool use_default_scale_cancellation_default);

  WaylandRemoteShell(const WaylandRemoteShell&) = delete;

  WaylandRemoteShell& operator=(const WaylandRemoteShell&) = delete;

  ~WaylandRemoteShell() override;

  std::unique_ptr<ClientControlledShellSurface> CreateShellSurface(
      Surface* surface,
      int container);

  std::unique_ptr<ClientControlledShellSurface::Delegate>
  CreateShellSurfaceDelegate(wl_resource* resource);
  std::unique_ptr<NotificationSurface> CreateNotificationSurface(
      Surface* surface,
      const std::string& notification_key);

  std::unique_ptr<InputMethodSurface> CreateInputMethodSurface(
      Surface* surface);

  std::unique_ptr<ToastSurface> CreateToastSurface(Surface* surface);

  void SetUseDefaultScaleCancellation(bool use_default_scale);

  void OnRemoteSurfaceDestroyed(wl_resource* resource);

  // Overridden from display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;
  void OnDisplayTabletStateChanged(display::TabletState state) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // display::DisplayManagerObserver:
  void OnWillProcessDisplayChanges() override;
  void OnDidProcessDisplayChanges(
      const DisplayConfigurationChange& configuration_change) override;

  // Overridden from SeatObserver:
  void OnSurfaceFocused(Surface* gained_focus,
                        Surface* lost_focus,
                        bool has_focused_surface) override;

  WaylandRemoteShellEventMapping const event_mapping_;

 private:
  friend class WaylandRemoteShellTest;

  void ScheduleSendDisplayMetrics(int delay_ms);

  // Returns the transform that a display's output is currently adjusted for.
  wl_output_transform DisplayTransform(display::Display::Rotation rotation);

  void SendDisplayMetrics();

  void FocusedSurfaceChanged(Surface* gained_active_surface,
                             Surface* lost_active_surface,
                             bool has_focused_client);

  void OnRemoteSurfaceBoundsChanged(wl_resource* resource,
                                    WindowStateType current_state,
                                    WindowStateType requested_state,
                                    int64_t display_id,
                                    const gfx::Rect& bounds_in_display,
                                    bool resize,
                                    int bounds_change,
                                    bool is_adjusted_bounds);

  void SendBoundsChanged(wl_resource* resource,
                         int64_t display_id,
                         const gfx::Rect& bounds_in_display,
                         uint32_t reason);

  void OnRemoteSurfaceStateChanged(wl_resource* resource,
                                   WindowStateType old_state_type,
                                   WindowStateType new_state_type);

  void OnRemoteSurfaceChangeZoomLevel(wl_resource* resource, ZoomChange change);

  void OnRemoteSurfaceGeometryChanged(wl_resource* resource,
                                      const gfx::Rect& geometry);
  struct BoundsChangeData {
    int64_t display_id;
    gfx::Rect bounds_in_display;
    uint32_t reason;
    BoundsChangeData(int64_t display_id,
                     const gfx::Rect& bounds,
                     uint32_t reason)
        : display_id(display_id), bounds_in_display(bounds), reason(reason) {}
  };

  // The exo display instance. Not owned.
  const raw_ptr<Display> display_;

  // The remote shell resource associated with observer.
  const raw_ptr<wl_resource> remote_shell_resource_;

  // Callback to get the wl_output resource for a given display_id.
  OutputResourceProvider const output_provider_;

  // When true, the compositor should use the default_device_scale_factor to
  // undo the scaling on the client buffers. When false, the compositor should
  // use the device_scale_factor for the display for this scaling cancellation.
  // in v2 this is always false.
  bool use_default_scale_cancellation_;

  bool in_display_update_ = false;
  bool needs_send_display_metrics_ = true;

  int layout_mode_ = ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_WINDOWED;

  base::flat_map<wl_resource*, BoundsChangeData> pending_bounds_changes_;

  bool last_has_focused_client_ = false;

  display::ScopedDisplayObserver display_observer_{this};

  base::ScopedObservation<display::DisplayManager,
                          display::DisplayManagerObserver>
      display_manager_observation_{this};

  const raw_ptr<Seat> seat_;

  base::WeakPtrFactory<WaylandRemoteShell> weak_ptr_factory_{this};

  friend class WaylandRemoteSurfaceDelegate;
};

class WaylandRemoteSurfaceDelegate
    : public ClientControlledShellSurface::Delegate {
 public:
  WaylandRemoteSurfaceDelegate(base::WeakPtr<WaylandRemoteShell> shell,
                               wl_resource* resource,
                               WaylandRemoteShellEventMapping event_mapping);
  ~WaylandRemoteSurfaceDelegate() override;
  WaylandRemoteSurfaceDelegate(const WaylandRemoteSurfaceDelegate&) = delete;
  WaylandRemoteSurfaceDelegate& operator=(const WaylandRemoteSurfaceDelegate&) =
      delete;

 private:
  // ClientControlledShellSurfaceDelegate:
  void OnGeometryChanged(const gfx::Rect& geometry) override;
  void OnStateChanged(chromeos::WindowStateType old_state_type,
                      chromeos::WindowStateType new_state_type) override;
  void OnBoundsChanged(chromeos::WindowStateType current_state,
                       chromeos::WindowStateType requested_state,
                       int64_t display_id,
                       const gfx::Rect& bounds_in_display,
                       bool is_resize,
                       int bounds_change,
                       bool is_adjusted_bounds) override;
  void OnDragStarted(int component) override;
  void OnDragFinished(int x, int y, bool canceled) override;
  void OnZoomLevelChanged(ZoomChange zoom_change) override;

  base::WeakPtr<WaylandRemoteShell> shell_;
  raw_ptr<wl_resource> resource_;
  WaylandRemoteShellEventMapping const event_mapping_;
};

namespace zcr_remote_shell {

double GetDefaultDeviceScaleFactor();

gfx::Rect ScaleBoundsToPixelSnappedToParent(
    const gfx::Size& parent_size_in_pixel,
    const gfx::Size& parent_size,
    float device_scale_factor,
    const gfx::Rect& child_bounds);

void remote_surface_destroy(wl_client* client, wl_resource* resource);
void remote_surface_set_app_id(wl_client* client,
                               wl_resource* resource,
                               const char* app_id);
void remote_surface_set_window_geometry(wl_client* client,
                                        wl_resource* resource,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height);
void remote_surface_set_orientation(wl_client* client,
                                    wl_resource* resource,
                                    int32_t orientation);

void remote_surface_set_title(wl_client* client,
                              wl_resource* resource,
                              const char* title);
void remote_surface_set_top_inset(wl_client* client,
                                  wl_resource* resource,
                                  int32_t height);

void remote_surface_maximize(wl_client* client, wl_resource* resource);
void remote_surface_minimize(wl_client* client, wl_resource* resource);
void remote_surface_restore(wl_client* client, wl_resource* resource);

void remote_surface_fullscreen(wl_client* client, wl_resource* resource);

void remote_surface_pin(wl_client* client,
                        wl_resource* resource,
                        int32_t trusted);

void remote_surface_unpin(wl_client* client, wl_resource* resource);
void remote_surface_set_system_modal(wl_client* client, wl_resource* resource);

void remote_surface_unset_system_modal(wl_client* client,
                                       wl_resource* resource);

void remote_surface_set_rectangular_surface_shadow(wl_client* client,
                                                   wl_resource* resource,
                                                   int32_t x,
                                                   int32_t y,
                                                   int32_t width,
                                                   int32_t height);
void remote_surface_set_systemui_visibility(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t visibility);
void remote_surface_set_always_on_top(wl_client* client, wl_resource* resource);
void remote_surface_unset_always_on_top(wl_client* client,
                                        wl_resource* resource);

void remote_surface_start_move(wl_client* client,
                               wl_resource* resource,
                               int32_t x,
                               int32_t y);
void remote_surface_set_can_maximize(wl_client* client, wl_resource* resource);
void remote_surface_unset_can_maximize(wl_client* client,
                                       wl_resource* resource);
void remote_surface_set_min_size(wl_client* client,
                                 wl_resource* resource,
                                 int32_t width,
                                 int32_t height);
void remote_surface_set_max_size(wl_client* client,
                                 wl_resource* resource,
                                 int32_t width,
                                 int32_t height);
void remote_surface_set_aspect_ratio(wl_client* client,
                                     wl_resource* resource,
                                     int32_t aspect_ratio_width,
                                     int32_t aspect_ratio_height);
void remote_surface_set_snapped_to_left(wl_client* client,
                                        wl_resource* resource);

void remote_surface_set_snapped_to_right(wl_client* client,
                                         wl_resource* resource);
void remote_surface_start_resize(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t direction,
                                 int32_t x,
                                 int32_t y);
void remote_surface_set_frame(wl_client* client,
                              wl_resource* resource,
                              uint32_t type);
void remote_surface_set_frame_buttons(wl_client* client,
                                      wl_resource* resource,
                                      uint32_t visible_button_mask,
                                      uint32_t enabled_button_mask);
void remote_surface_set_extra_title(wl_client* client,
                                    wl_resource* resource,
                                    const char* extra_title);
void remote_surface_set_orientation_lock(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t orientation_lock);
void remote_surface_pip(wl_client* client, wl_resource* resource);
void remote_surface_set_bounds(wl_client* client,
                               wl_resource* resource,
                               uint32_t display_id_hi,
                               uint32_t display_id_lo,
                               int32_t x,
                               int32_t y,
                               int32_t width,
                               int32_t height);

void remote_surface_set_accessibility_id_DEPRECATED(wl_client* client,
                                                    wl_resource* resource,
                                                    int32_t accessibility_id);
void remote_surface_set_pip_original_window(wl_client* client,
                                            wl_resource* resource);
void remote_surface_unset_pip_original_window(wl_client* client,
                                              wl_resource* resource);

void remote_surface_set_system_gesture_exclusion(wl_client* client,
                                                 wl_resource* resource,
                                                 wl_resource* region_resource);
void remote_surface_set_resize_lock(wl_client* client, wl_resource* resource);
void remote_surface_unset_resize_lock(wl_client* client, wl_resource* resource);

void remote_surface_set_bounds_in_output(wl_client* client,
                                         wl_resource* resource,
                                         wl_resource* output_resource,
                                         int32_t x,
                                         int32_t y,
                                         int32_t width,
                                         int32_t height);

void remote_surface_set_resize_lock_type(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t mode);

void remote_surface_set_scale_factor(wl_client* client,
                                     wl_resource* resource,
                                     uint mode);

void remote_surface_set_window_corner_radii(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t upper_left_radius,
                                            uint32_t upper_right_radius,
                                            uint32_t lower_right_radius,
                                            uint32_t lower_left_radius);

void remote_surface_set_shadow_corner_radii(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t upper_left_radius,
                                            uint32_t upper_right_radius,
                                            uint32_t lower_right_radius,
                                            uint32_t lower_left_radius);

void remote_surface_set_float(wl_client* client, wl_resource* resource);

void remote_surface_block_ime(wl_client* client, wl_resource* resource);

void remote_surface_unblock_ime(wl_client* client, wl_resource* resource);

void remote_surface_set_window_type(wl_client* client,
                                    wl_resource* resource,
                                    uint32_t type);

void remote_surface_set_scale(wl_client* client,
                              wl_resource* resource,
                              wl_fixed_t scale);

void remote_surface_activate(wl_client* client,
                             wl_resource* resource,
                             uint32_t serial);

void remote_surface_unfullscreen(wl_client* client, wl_resource* resource);

void remote_surface_fullscreen(wl_client* client, wl_resource* resource);

void remote_surface_ack_configure_DEPRECATED(wl_client* client,
                                             wl_resource* resource,
                                             uint32_t serial);

void remote_surface_move_DEPRECATED(wl_client* client, wl_resource* resource);

void remote_surface_resize_DEPRECATED(wl_client* client, wl_resource* resource);

void remote_surface_set_resize_outset_DEPRECATED(wl_client* client,
                                                 wl_resource* resource,
                                                 int32_t outset);

void remote_surface_set_rectangular_shadow_DEPRECATED(wl_client* client,
                                                      wl_resource* resource,
                                                      int32_t x,
                                                      int32_t y,
                                                      int32_t width,
                                                      int32_t height);

void remote_surface_set_rectangular_shadow_background_opacity_DEPRECATED(
    wl_client* client,
    wl_resource* resource,
    wl_fixed_t opacity);

////////////////////////////////////////////////////////////////////////////////
// notification_surface_interface:

void notification_surface_set_app_id(wl_client* client,
                                     wl_resource* resource,
                                     const char* app_id);

////////////////////////////////////////////////////////////////////////////////
// input_method_surface_interface:

void input_method_surface_set_bounds_in_output(wl_client* client,
                                               wl_resource* resource,
                                               wl_resource* output_resource,
                                               int32_t x,
                                               int32_t y,
                                               int32_t width,
                                               int32_t height);

void input_method_surface_set_bounds(wl_client* client,
                                     wl_resource* resource,
                                     uint32_t display_id_hi,
                                     uint32_t display_id_lo,
                                     int32_t x,
                                     int32_t y,
                                     int32_t width,
                                     int32_t height);

////////////////////////////////////////////////////////////////////////////////
// toast_surface_interface:

void toast_surface_set_bounds_in_output(wl_client* client,
                                        wl_resource* resource,
                                        wl_resource* output_resource,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height);

void toast_surface_set_position(wl_client* client,
                                wl_resource* resource,
                                uint32_t display_id_hi,
                                uint32_t display_id_lo,
                                int32_t x,
                                int32_t y);

void toast_surface_set_size(wl_client* client,
                            wl_resource* resource,
                            int32_t width,
                            int32_t height);

void toast_surface_set_bounds_in_output(wl_client* client,
                                        wl_resource* resource,
                                        wl_resource* output_resource,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height);

void toast_surface_set_scale_factor(wl_client* client,
                                    wl_resource* resource,
                                    uint scale_factor_as_uint);

////////////////////////////////////////////////////////////////////////////////
// remote_shell_interface:

void remote_shell_set_use_default_scale_cancellation(
    wl_client*,
    wl_resource* resource,
    int32_t use_default_scale_cancellation);

}  // namespace zcr_remote_shell
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_ZCR_REMOTE_SHELL_IMPL_H_
