// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_remote_shell_impl.h"

#include "ash/public/cpp/arc_resize_lock_type.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/wm/window_resizer.h"
#include "base/bit_cast.h"
#include "base/command_line.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "components/exo/display.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wm_helper.h"
#include "ui/display/screen.h"
#include "ui/views/window/caption_button_types.h"
#include "ui/wm/core/window_animations.h"

namespace exo {
namespace wayland {

// Ensure that V1 and V2 constants remain identical.
static_assert(
    static_cast<int>(ZCR_REMOTE_SURFACE_V1_SYSTEMUI_VISIBILITY_STATE_VISIBLE) ==
        static_cast<int>(
            ZCR_REMOTE_SURFACE_V2_SYSTEMUI_VISIBILITY_STATE_VISIBLE),
    "ZCR_REMOTE_SURFACE_V1_SYSTEMUI_VISIBILITY_STATE_VISIBLE and "
    "ZCR_REMOTE_SURFACE_V2_SYSTEMUI_VISIBILITY_STATE_VISIBLE should be equal");
static_assert(
    static_cast<int>(
        ZCR_REMOTE_SURFACE_V1_SYSTEMUI_VISIBILITY_STATE_AUTOHIDE_NON_STICKY) ==
        static_cast<int>(
            ZCR_REMOTE_SURFACE_V2_SYSTEMUI_VISIBILITY_STATE_AUTOHIDE_NON_STICKY),
    "ZCR_REMOTE_SURFACE_V1_SYSTEMUI_VISIBILITY_STATE_AUTOHIDE_NON_STICKY and "
    "ZCR_REMOTE_SURFACE_V2_SYSTEMUI_VISIBILITY_STATE_AUTOHIDE_NON_STICKY "
    "should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_OUTPUT_V1_SYSTEMUI_BEHAVIOR_VISIBLE) ==
        static_cast<int>(ZCR_REMOTE_OUTPUT_V2_SYSTEMUI_BEHAVIOR_VISIBLE),
    "ZCR_REMOTE_OUTPUT_V1_SYSTEMUI_BEHAVIOR_VISIBLE and "
    "ZCR_REMOTE_OUTPUT_V2_SYSTEMUI_BEHAVIOR_VISIBLE should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_OUTPUT_V1_SYSTEMUI_BEHAVIOR_HIDDEN) ==
        static_cast<int>(ZCR_REMOTE_OUTPUT_V2_SYSTEMUI_BEHAVIOR_HIDDEN),
    "ZCR_REMOTE_OUTPUT_V1_SYSTEMUI_BEHAVIOR_HIDDEN and "
    "ZCR_REMOTE_OUTPUT_V2_SYSTEMUI_BEHAVIOR_HIDDEN should be equal");
static_assert(static_cast<int>(ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_NONE) ==
                  static_cast<int>(ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_NONE),
              "ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_NONE and "
              "ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_NONE should be equal");
static_assert(static_cast<int>(ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOP) ==
                  static_cast<int>(ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_TOP),
              "ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOP and "
              "ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_TOP should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPRIGHT) ==
        static_cast<int>(ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_TOPRIGHT),
    "ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPRIGHT and "
    "ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_TOPRIGHT should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_RIGHT) ==
        static_cast<int>(ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_RIGHT),
    "ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_RIGHT and "
    "ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_RIGHT should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMRIGHT) ==
        static_cast<int>(ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_BOTTOMRIGHT),
    "ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMRIGHT and "
    "ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_BOTTOMRIGHT should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOM) ==
        static_cast<int>(ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_BOTTOM),
    "ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOM and "
    "ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_BOTTOM should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMLEFT) ==
        static_cast<int>(ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_BOTTOMLEFT),
    "ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMLEFT and "
    "ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_BOTTOMLEFT should be equal");
static_assert(static_cast<int>(ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_LEFT) ==
                  static_cast<int>(ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_LEFT),
              "ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_LEFT and "
              "ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_LEFT should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPLEFT) ==
        static_cast<int>(ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_TOPLEFT),
    "ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPLEFT and "
    "ZCR_REMOTE_SURFACE_V2_RESIZE_DIRECTION_TOPLEFT should be equal");
static_assert(static_cast<int>(ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_TABLET) ==
                  static_cast<int>(ZCR_REMOTE_SHELL_V2_LAYOUT_MODE_TABLET),
              "ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_TABLET and "
              "ZCR_REMOTE_SHELL_V2_LAYOUT_MODE_TABLET should be equal");
static_assert(static_cast<int>(ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_WINDOWED) ==
                  static_cast<int>(ZCR_REMOTE_SHELL_V2_LAYOUT_MODE_WINDOWED),
              "ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_WINDOWED and "
              "ZCR_REMOTE_SHELL_V2_LAYOUT_MODE_WINDOWED should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_SHELL_V1_DESKTOP_FOCUS_STATE_CLIENT_FOCUSED) ==
        static_cast<int>(
            ZCR_REMOTE_SHELL_V2_DESKTOP_FOCUS_STATE_CLIENT_FOCUSED),
    "ZCR_REMOTE_SHELL_V1_DESKTOP_FOCUS_STATE_CLIENT_FOCUSED and "
    "ZCR_REMOTE_SHELL_V2_DESKTOP_FOCUS_STATE_CLIENT_FOCUSED should be equal");
static_assert(
    static_cast<int>(
        ZCR_REMOTE_SHELL_V1_DESKTOP_FOCUS_STATE_OTHER_CLIENT_FOCUSED) ==
        static_cast<int>(
            ZCR_REMOTE_SHELL_V2_DESKTOP_FOCUS_STATE_OTHER_CLIENT_FOCUSED),
    "ZCR_REMOTE_SHELL_V1_DESKTOP_FOCUS_STATE_OTHER_CLIENT_FOCUSED and "
    "ZCR_REMOTE_SHELL_V2_DESKTOP_FOCUS_STATE_OTHER_CLIENT_FOCUSED should be "
    "equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_SHELL_V1_DESKTOP_FOCUS_STATE_NO_FOCUS) ==
        static_cast<int>(ZCR_REMOTE_SHELL_V2_DESKTOP_FOCUS_STATE_NO_FOCUS),
    "ZCR_REMOTE_SHELL_V1_DESKTOP_FOCUS_STATE_NO_FOCUS and "
    "ZCR_REMOTE_SHELL_V2_DESKTOP_FOCUS_STATE_NO_FOCUS should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_RESIZE) ==
        static_cast<int>(ZCR_REMOTE_SURFACE_V2_BOUNDS_CHANGE_REASON_RESIZE),
    "ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_RESIZE and "
    "ZCR_REMOTE_SURFACE_V2_BOUNDS_CHANGE_REASON_RESIZE should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_MOVE) ==
        static_cast<int>(ZCR_REMOTE_SURFACE_V2_BOUNDS_CHANGE_REASON_MOVE),
    "ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_MOVE and "
    "ZCR_REMOTE_SURFACE_V2_BOUNDS_CHANGE_REASON_MOVE should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_PIP) ==
        static_cast<int>(ZCR_REMOTE_SURFACE_V2_BOUNDS_CHANGE_REASON_PIP),
    "ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_PIP and "
    "ZCR_REMOTE_SURFACE_V2_BOUNDS_CHANGE_REASON_PIP should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_DRAG_RESIZE) ==
        static_cast<int>(
            ZCR_REMOTE_SURFACE_V2_BOUNDS_CHANGE_REASON_DRAG_RESIZE),
    "ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_DRAG_RESIZE and "
    "ZCR_REMOTE_SURFACE_V2_BOUNDS_CHANGE_REASON_DRAG_RESIZE should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_DRAG_MOVE) ==
        static_cast<int>(ZCR_REMOTE_SURFACE_V2_BOUNDS_CHANGE_REASON_DRAG_MOVE),
    "ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_DRAG_MOVE and "
    "ZCR_REMOTE_SURFACE_V2_BOUNDS_CHANGE_REASON_DRAG_MOVE should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_SNAP_TO_LEFT) ==
        static_cast<int>(
            ZCR_REMOTE_SURFACE_V2_BOUNDS_CHANGE_REASON_SNAP_TO_LEFT),
    "ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_SNAP_TO_LEFT and "
    "ZCR_REMOTE_SURFACE_V2_BOUNDS_CHANGE_REASON_SNAP_TO_LEFT should be equal");
static_assert(
    static_cast<int>(
        ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_SNAP_TO_RIGHT) ==
        static_cast<int>(
            ZCR_REMOTE_SURFACE_V2_BOUNDS_CHANGE_REASON_SNAP_TO_RIGHT),
    "ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_SNAP_TO_RIGHT and "
    "ZCR_REMOTE_SURFACE_V2_BOUNDS_CHANGE_REASON_SNAP_TO_RIGHT should be equal");
static_assert(static_cast<int>(ZCR_REMOTE_SHELL_V1_STATE_TYPE_NORMAL) ==
                  static_cast<int>(ZCR_REMOTE_SURFACE_V2_STATE_TYPE_NORMAL),
              "ZCR_REMOTE_SHELL_V1_STATE_TYPE_NORMAL and "
              "ZCR_REMOTE_SURFACE_V2_STATE_TYPE_NORMAL should be equal");
static_assert(static_cast<int>(ZCR_REMOTE_SHELL_V1_STATE_TYPE_MINIMIZED) ==
                  static_cast<int>(ZCR_REMOTE_SURFACE_V2_STATE_TYPE_MINIMIZED),
              "ZCR_REMOTE_SHELL_V1_STATE_TYPE_MINIMIZED and "
              "ZCR_REMOTE_SURFACE_V2_STATE_TYPE_MINIMIZED should be equal");
static_assert(static_cast<int>(ZCR_REMOTE_SHELL_V1_STATE_TYPE_MAXIMIZED) ==
                  static_cast<int>(ZCR_REMOTE_SURFACE_V2_STATE_TYPE_MAXIMIZED),
              "ZCR_REMOTE_SHELL_V1_STATE_TYPE_MAXIMIZED and "
              "ZCR_REMOTE_SURFACE_V2_STATE_TYPE_MAXIMIZED should be equal");
static_assert(static_cast<int>(ZCR_REMOTE_SHELL_V1_STATE_TYPE_FULLSCREEN) ==
                  static_cast<int>(ZCR_REMOTE_SURFACE_V2_STATE_TYPE_FULLSCREEN),
              "ZCR_REMOTE_SHELL_V1_STATE_TYPE_FULLSCREEN and "
              "ZCR_REMOTE_SURFACE_V2_STATE_TYPE_FULLSCREEN should be equal");
static_assert(static_cast<int>(ZCR_REMOTE_SHELL_V1_STATE_TYPE_PINNED) ==
                  static_cast<int>(ZCR_REMOTE_SURFACE_V2_STATE_TYPE_PINNED),
              "ZCR_REMOTE_SHELL_V1_STATE_TYPE_PINNED and "
              "ZCR_REMOTE_SURFACE_V2_STATE_TYPE_PINNED should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_SHELL_V1_STATE_TYPE_TRUSTED_PINNED) ==
        static_cast<int>(ZCR_REMOTE_SURFACE_V2_STATE_TYPE_TRUSTED_PINNED),
    "ZCR_REMOTE_SHELL_V1_STATE_TYPE_TRUSTED_PINNED and "
    "ZCR_REMOTE_SURFACE_V2_STATE_TYPE_TRUSTED_PINNED should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_SHELL_V1_STATE_TYPE_LEFT_SNAPPED) ==
        static_cast<int>(ZCR_REMOTE_SURFACE_V2_STATE_TYPE_LEFT_SNAPPED),
    "ZCR_REMOTE_SHELL_V1_STATE_TYPE_LEFT_SNAPPED and "
    "ZCR_REMOTE_SURFACE_V2_STATE_TYPE_LEFT_SNAPPED should be equal");
static_assert(
    static_cast<int>(ZCR_REMOTE_SHELL_V1_STATE_TYPE_RIGHT_SNAPPED) ==
        static_cast<int>(ZCR_REMOTE_SURFACE_V2_STATE_TYPE_RIGHT_SNAPPED),
    "ZCR_REMOTE_SHELL_V1_STATE_TYPE_RIGHT_SNAPPED and "
    "ZCR_REMOTE_SURFACE_V2_STATE_TYPE_RIGHT_SNAPPED should be equal");
static_assert(static_cast<int>(ZCR_REMOTE_SHELL_V1_STATE_TYPE_PIP) ==
                  static_cast<int>(ZCR_REMOTE_SURFACE_V2_STATE_TYPE_PIP),
              "ZCR_REMOTE_SHELL_V1_STATE_TYPE_PIP and "
              "ZCR_REMOTE_SURFACE_V2_STATE_TYPE_PIP should be equal");
static_assert(static_cast<int>(ZCR_REMOTE_SURFACE_V1_ZOOM_CHANGE_IN) ==
                  static_cast<int>(ZCR_REMOTE_SURFACE_V2_ZOOM_CHANGE_IN),
              "ZCR_REMOTE_SURFACE_V1_ZOOM_CHANGE_IN and "
              "ZCR_REMOTE_SURFACE_V2_ZOOM_CHANGE_IN should be equal");
static_assert(static_cast<int>(ZCR_REMOTE_SURFACE_V1_ZOOM_CHANGE_OUT) ==
                  static_cast<int>(ZCR_REMOTE_SURFACE_V2_ZOOM_CHANGE_OUT),
              "ZCR_REMOTE_SURFACE_V1_ZOOM_CHANGE_OUT and "
              "ZCR_REMOTE_SURFACE_V2_ZOOM_CHANGE_OUT should be equal");
static_assert(static_cast<int>(ZCR_REMOTE_SURFACE_V1_ZOOM_CHANGE_RESET) ==
                  static_cast<int>(ZCR_REMOTE_SURFACE_V2_ZOOM_CHANGE_RESET),
              "ZCR_REMOTE_SURFACE_V1_ZOOM_CHANGE_RESET and "
              "ZCR_REMOTE_SURFACE_V2_ZOOM_CHANGE_RESET should be equal");

using chromeos::WindowStateType;

// We don't send configure immediately after tablet mode switch
// because layout can change due to orientation lock state or accelerometer.
constexpr int kConfigureDelayAfterLayoutSwitchMs = 300;

constexpr int kRemoteShellSeatObserverPriority = 0;
static_assert(Seat::IsValidObserverPriority(kRemoteShellSeatObserverPriority),
              "kRemoteShellSeatObserverPriority is not in the valid range.");

// Convert to 8.24 fixed format.
int32_t To8_24Fixed(double value) {
  constexpr int kDecimalBits = 24;
  return static_cast<int32_t>(value * (1 << kDecimalBits));
}

ash::ShelfLayoutManager* GetShelfLayoutManagerForDisplay(
    const display::Display& display) {
  auto* root = ash::Shell::GetRootWindowForDisplayId(display.id());
  return ash::Shelf::ForWindow(root)->shelf_layout_manager();
}

int SystemUiVisibility(const display::Display& display) {
  auto* shelf_layout_manager = GetShelfLayoutManagerForDisplay(display);
  switch (shelf_layout_manager->visibility_state()) {
    case ash::SHELF_VISIBLE:
      return ZCR_REMOTE_SURFACE_V1_SYSTEMUI_VISIBILITY_STATE_VISIBLE;
    case ash::SHELF_AUTO_HIDE:
    case ash::SHELF_HIDDEN:
      return ZCR_REMOTE_SURFACE_V1_SYSTEMUI_VISIBILITY_STATE_AUTOHIDE_NON_STICKY;
  }
  NOTREACHED_IN_MIGRATION() << "Got unexpected shelf visibility state "
                            << shelf_layout_manager->visibility_state();
  return 0;
}

int SystemUiBehavior(const display::Display& display) {
  auto* shelf_layout_manager = GetShelfLayoutManagerForDisplay(display);
  switch (shelf_layout_manager->auto_hide_behavior()) {
    case ash::ShelfAutoHideBehavior::kNever:
      return ZCR_REMOTE_OUTPUT_V1_SYSTEMUI_BEHAVIOR_VISIBLE;
    case ash::ShelfAutoHideBehavior::kAlways:
    case ash::ShelfAutoHideBehavior::kAlwaysHidden:
      return ZCR_REMOTE_OUTPUT_V1_SYSTEMUI_BEHAVIOR_HIDDEN;
  }
  NOTREACHED_IN_MIGRATION() << "Got unexpected shelf visibility behavior.";
  return 0;
}

uint32_t ResizeDirection(int component) {
  switch (component) {
    case HTCAPTION:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_NONE;
    case HTTOP:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOP;
    case HTTOPRIGHT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPRIGHT;
    case HTRIGHT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_RIGHT;
    case HTBOTTOMRIGHT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMRIGHT;
    case HTBOTTOM:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOM;
    case HTBOTTOMLEFT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMLEFT;
    case HTLEFT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_LEFT;
    case HTTOPLEFT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPLEFT;
    default:
      LOG(ERROR) << "Unknown component:" << component;
      break;
  }
  DUMP_WILL_BE_NOTREACHED();
  return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_NONE;
}

// This is a workaround for b/148977363
void MaybeApplyCTSHack(int layout_mode,
                       const gfx::Size& size_in_pixel,
                       gfx::Insets* insets_in_client_pixel,
                       gfx::Insets* stable_insets_in_client_pixel) {
  constexpr int kBadBottomInsets = 90;
  if (layout_mode == ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_TABLET &&
      size_in_pixel.width() == 3000 && size_in_pixel.height() == 2000 &&
      stable_insets_in_client_pixel->bottom() == kBadBottomInsets) {
    stable_insets_in_client_pixel->set_bottom(kBadBottomInsets + 1);
    if (insets_in_client_pixel->bottom() == kBadBottomInsets)
      insets_in_client_pixel->set_bottom(kBadBottomInsets + 1);
  }
}

bool WaylandRemoteOutput::SendDisplayMetrics(const display::Display& display,
                                             uint32_t changed_metrics) {
  if (wl_resource_get_version(resource_) <
      event_mapping_.stable_insets_since_version) {
    return false;
  }

  if (initial_config_sent_ &&
      !(changed_metrics & display::DisplayObserver::DISPLAY_METRIC_WORK_AREA)) {
    return false;
  }

  if (!initial_config_sent_) {
    initial_config_sent_ = true;

    uint32_t display_id_hi = static_cast<uint32_t>(display.id() >> 32);
    uint32_t display_id_lo = static_cast<uint32_t>(display.id());
    if (event_mapping_.send_display_id)
      event_mapping_.send_display_id(resource_, display_id_hi, display_id_lo);

    constexpr int64_t DISPLAY_ID_PORT_MASK = 0xff;
    uint32_t port = static_cast<uint32_t>(display.id() & DISPLAY_ID_PORT_MASK);
    if (event_mapping_.send_port)
      event_mapping_.send_port(resource_, port);

    wl_array data;
    wl_array_init(&data);

    const auto& bytes =
        WMHelper::GetInstance()->GetDisplayIdentificationData(display.id());
    for (uint8_t byte : bytes) {
      uint8_t* ptr =
          static_cast<uint8_t*>(wl_array_add(&data, sizeof(uint8_t)));
      DCHECK(ptr);
      *ptr = byte;
    }
    event_mapping_.send_identification_data(resource_, &data);
    wl_array_release(&data);
  }

  float device_scale_factor = display.device_scale_factor();
  gfx::Size size_in_pixel = display.GetSizeInPixel();

  gfx::Insets insets_in_pixel = GetWorkAreaInsetsInPixel(
      display, device_scale_factor, size_in_pixel, display.work_area());
  event_mapping_.send_insets(resource_, insets_in_pixel.left(),
                             insets_in_pixel.top(), insets_in_pixel.right(),
                             insets_in_pixel.bottom());

  gfx::Insets stable_insets_in_pixel = GetWorkAreaInsetsInPixel(
      display, device_scale_factor, size_in_pixel, GetStableWorkArea(display));
  event_mapping_.send_stable_insets(
      resource_, stable_insets_in_pixel.left(), stable_insets_in_pixel.top(),
      stable_insets_in_pixel.right(), stable_insets_in_pixel.bottom());

  // Currently no client uses zcr_remote_output_v1 systemui_visibility.
  // Only systemui_behavior is sent here.
  if (wl_resource_get_version(resource_) >=
      event_mapping_.system_ui_behavior_since_version) {
    int systemui_behavior = SystemUiBehavior(display);
    event_mapping_.send_systemui_behavior(resource_, systemui_behavior);
  }

  return true;
}

void WaylandRemoteOutput::SendActiveDisplay() {}

void WaylandRemoteOutput::OnOutputDestroyed() {
  display_handler_->RemoveObserver(this);
  display_handler_ = nullptr;
}

WaylandRemoteSurfaceDelegate::WaylandRemoteSurfaceDelegate(
    base::WeakPtr<WaylandRemoteShell> shell,
    wl_resource* resource,
    WaylandRemoteShellEventMapping event_mapping)
    : shell_(std::move(shell)),
      resource_(resource),
      event_mapping_(event_mapping) {}

WaylandRemoteSurfaceDelegate::~WaylandRemoteSurfaceDelegate() {
  if (shell_)
    shell_->OnRemoteSurfaceDestroyed(resource_);
}

// ClientControlledShellSurfaceDelegate:
void WaylandRemoteSurfaceDelegate::OnGeometryChanged(
    const gfx::Rect& geometry) {
  if (shell_)
    shell_->OnRemoteSurfaceGeometryChanged(resource_, geometry);
}
void WaylandRemoteSurfaceDelegate::OnStateChanged(
    chromeos::WindowStateType old_state_type,
    chromeos::WindowStateType new_state_type) {
  shell_->OnRemoteSurfaceStateChanged(resource_, old_state_type,
                                      new_state_type);
}
void WaylandRemoteSurfaceDelegate::OnBoundsChanged(
    chromeos::WindowStateType current_state,
    chromeos::WindowStateType requested_state,
    int64_t display_id,
    const gfx::Rect& bounds_in_display,
    bool is_resize,
    int bounds_change,
    bool is_adjusted_bounds) {
  if (shell_) {
    shell_->OnRemoteSurfaceBoundsChanged(
        resource_, current_state, requested_state, display_id,
        bounds_in_display, is_resize, bounds_change, is_adjusted_bounds);
  }
}
void WaylandRemoteSurfaceDelegate::OnDragStarted(int component) {
  event_mapping_.send_drag_started(resource_, ResizeDirection(component));
  wl_client_flush(wl_resource_get_client(resource_));
}
void WaylandRemoteSurfaceDelegate::OnDragFinished(int x, int y, bool canceled) {
  event_mapping_.send_drag_finished(resource_, x, y, canceled ? 1 : 0);
  wl_client_flush(wl_resource_get_client(resource_));
}
void WaylandRemoteSurfaceDelegate::OnZoomLevelChanged(ZoomChange zoom_change) {
  if (wl_resource_get_version(resource_) >=
          event_mapping_.change_zoom_level_since_version &&
      shell_) {
    shell_->OnRemoteSurfaceChangeZoomLevel(resource_, zoom_change);
  }
}

WaylandRemoteOutput::WaylandRemoteOutput(
    wl_resource* resource,
    WaylandRemoteOutputEventMapping event_mapping,
    WaylandDisplayHandler* display_handler)
    : resource_(resource),
      event_mapping_(event_mapping),
      display_handler_(display_handler) {
  display_handler_->AddObserver(this);
}

WaylandRemoteOutput::~WaylandRemoteOutput() {
  if (display_handler_)
    display_handler_->RemoveObserver(this);
}

using OutputResourceProvider = base::RepeatingCallback<wl_resource*(int64_t)>;

WaylandRemoteShell::WaylandRemoteShell(
    Display* display,
    wl_resource* remote_shell_resource,
    OutputResourceProvider output_provider,
    WaylandRemoteShellEventMapping event_mapping,
    bool use_default_scale_cancellation_default)
    : event_mapping_(event_mapping),
      display_(display),
      remote_shell_resource_(remote_shell_resource),
      output_provider_(output_provider),
      use_default_scale_cancellation_(use_default_scale_cancellation_default),
      seat_(display->seat()) {
  WMHelper* helper = WMHelper::GetInstance();
  helper->AddFrameThrottlingObserver();
  helper->SetDefaultScaleCancellation(use_default_scale_cancellation_);

  layout_mode_ = display::Screen::GetScreen()->InTabletMode()
                     ? ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_TABLET
                     : ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_WINDOWED;

  if (wl_resource_get_version(remote_shell_resource_) >=
      event_mapping_.layout_mode_since_version) {
    event_mapping_.send_layout_mode(remote_shell_resource_, layout_mode_);
  }

  if (wl_resource_get_version(remote_shell_resource_) >=
      event_mapping_.default_device_scale_factor_since_version) {
    double scale_factor = GetDefaultDeviceScaleFactor();
    int32_t fixed_scale = To8_24Fixed(scale_factor);
    event_mapping_.send_default_device_scale_factor(remote_shell_resource_,
                                                    fixed_scale);
  }

  display_manager_observation_.Observe(ash::Shell::Get()->display_manager());

  SendDisplayMetrics();

  // The activation event has been moved to aura_shell, but the
  // desktop_focus_state event is still in remote_shell, which needs to be
  // called before the activation event.
  display->seat()->AddObserver(this, kRemoteShellSeatObserverPriority);
}

WaylandRemoteShell::~WaylandRemoteShell() {
  WMHelper* helper = WMHelper::GetInstance();
  helper->RemoveFrameThrottlingObserver();
  if (seat_)
    seat_->RemoveObserver(this);
}

std::unique_ptr<ClientControlledShellSurface>
WaylandRemoteShell::CreateShellSurface(Surface* surface, int container) {
  return display_->CreateOrGetClientControlledShellSurface(
      surface, container, use_default_scale_cancellation_,
      /*supports_floated_state=*/event_mapping_.has_bounds_change_reason_float);
}

std::unique_ptr<ClientControlledShellSurface::Delegate>
WaylandRemoteShell::CreateShellSurfaceDelegate(wl_resource* resource) {
  return std::make_unique<WaylandRemoteSurfaceDelegate>(
      weak_ptr_factory_.GetWeakPtr(), resource, event_mapping_);
}

std::unique_ptr<NotificationSurface>
WaylandRemoteShell::CreateNotificationSurface(
    Surface* surface,
    const std::string& notification_key) {
  return display_->CreateNotificationSurface(surface, notification_key);
}

std::unique_ptr<InputMethodSurface>
WaylandRemoteShell::CreateInputMethodSurface(Surface* surface) {
  return display_->CreateInputMethodSurface(surface,
                                            use_default_scale_cancellation_);
}

std::unique_ptr<ToastSurface> WaylandRemoteShell::CreateToastSurface(
    Surface* surface) {
  return display_->CreateToastSurface(surface, use_default_scale_cancellation_);
}

void WaylandRemoteShell::SetUseDefaultScaleCancellation(
    bool use_default_scale) {
  use_default_scale_cancellation_ = use_default_scale;
  WMHelper::GetInstance()->SetDefaultScaleCancellation(use_default_scale);
}

void WaylandRemoteShell::OnRemoteSurfaceDestroyed(wl_resource* resource) {
  // Sometimes resource might be destroyed after bounds change is scheduled to
  // |pending_bounds_change_| but before that bounds change is emitted. Erase
  // it from |pending_bounds_changes_| to prevent crashes. See also
  // https://crbug.com/1163271.
  pending_bounds_changes_.erase(resource);
}

void WaylandRemoteShell::OnDisplayAdded(const display::Display& new_display) {
  ScheduleSendDisplayMetrics(0);
}

void WaylandRemoteShell::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  ScheduleSendDisplayMetrics(0);
}

void WaylandRemoteShell::OnDisplayTabletStateChanged(
    display::TabletState state) {
  if (wl_resource_get_version(remote_shell_resource_) >=
      event_mapping_.layout_mode_since_version) {
    if (state == display::TabletState::kInTabletMode) {
      layout_mode_ = ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_TABLET;
    } else if (state == display::TabletState::kExitingTabletMode) {
      layout_mode_ = ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_WINDOWED;
    }
    event_mapping_.send_layout_mode(remote_shell_resource_, layout_mode_);
  }

  if (display::IsTabletStateChanging(state)) {
    ScheduleSendDisplayMetrics(kConfigureDelayAfterLayoutSwitchMs);
  }
}

void WaylandRemoteShell::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  // No need to update when a primary display has changed without bounds
  // change. See WaylandDisplayObserver::OnDisplayMetricsChanged
  // for more details.
  if (changed_metrics &
      (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
       DISPLAY_METRIC_ROTATION | DISPLAY_METRIC_WORK_AREA)) {
    ScheduleSendDisplayMetrics(0);
  }
}

void WaylandRemoteShell::OnWillProcessDisplayChanges() {
  in_display_update_ = true;
}

void WaylandRemoteShell::OnDidProcessDisplayChanges(
    const DisplayConfigurationChange& configuration_change) {
  in_display_update_ = false;
}

void WaylandRemoteShell::ScheduleSendDisplayMetrics(int delay_ms) {
  needs_send_display_metrics_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WaylandRemoteShell::SendDisplayMetrics,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(delay_ms));
}

// Returns the transform that a display's output is currently adjusted for.
wl_output_transform WaylandRemoteShell::DisplayTransform(
    display::Display::Rotation rotation) {
  switch (rotation) {
    case display::Display::ROTATE_0:
      return WL_OUTPUT_TRANSFORM_NORMAL;
    case display::Display::ROTATE_90:
      return WL_OUTPUT_TRANSFORM_90;
    case display::Display::ROTATE_180:
      return WL_OUTPUT_TRANSFORM_180;
    case display::Display::ROTATE_270:
      return WL_OUTPUT_TRANSFORM_270;
  }
  NOTREACHED_IN_MIGRATION();
  return WL_OUTPUT_TRANSFORM_NORMAL;
}

void WaylandRemoteShell::SendDisplayMetrics() {
  if (!needs_send_display_metrics_)
    return;
  needs_send_display_metrics_ = false;

  const display::Screen* screen = display::Screen::GetScreen();
  double default_dsf = GetDefaultDeviceScaleFactor();

  for (const auto& display : screen->GetAllDisplays()) {
    double device_scale_factor = display.device_scale_factor();

    uint32_t display_id_hi = static_cast<uint32_t>(display.id() >> 32);
    uint32_t display_id_lo = static_cast<uint32_t>(display.id());
    gfx::Size size_in_pixel = display.GetSizeInPixel();

    wl_array data;
    wl_array_init(&data);

    const auto& bytes =
        WMHelper::GetInstance()->GetDisplayIdentificationData(display.id());
    for (uint8_t byte : bytes) {
      uint8_t* ptr =
          static_cast<uint8_t*>(wl_array_add(&data, sizeof(uint8_t)));
      DCHECK(ptr);
      *ptr = byte;
    }

    if (wl_resource_get_version(remote_shell_resource_) >=
        event_mapping_.send_workspace_info_since_version) {
      // Apply the scale factor used on the remote shell client (ARC).
      const gfx::Rect& bounds = display.bounds();

      // Note: The origin is used just to identify the workspace on the client
      // side, and does not account the actual pixel size of other workspace
      // on the client side.
      int x_px = base::ClampRound(bounds.x() * default_dsf);
      int y_px = base::ClampRound(bounds.y() * default_dsf);

      float server_to_client_pixel_scale = default_dsf / device_scale_factor;

      gfx::Size size_in_client_pixel =
          gfx::ScaleToRoundedSize(size_in_pixel, server_to_client_pixel_scale);

      gfx::Insets insets_in_client_pixel = GetWorkAreaInsetsInPixel(
          display, default_dsf, size_in_client_pixel, display.work_area());

      gfx::Insets stable_insets_in_client_pixel =
          GetWorkAreaInsetsInPixel(display, default_dsf, size_in_client_pixel,
                                   GetStableWorkArea(display));

      // TODO(b/148977363): Fix the issue and remove the hack.
      MaybeApplyCTSHack(layout_mode_, size_in_pixel, &insets_in_client_pixel,
                        &stable_insets_in_client_pixel);

      int systemui_visibility = SystemUiVisibility(display);
      if (event_mapping_.send_workspace_info)
        event_mapping_.send_workspace_info(
            remote_shell_resource_, display_id_hi, display_id_lo, x_px, y_px,
            size_in_client_pixel.width(), size_in_client_pixel.height(),
            insets_in_client_pixel.left(), insets_in_client_pixel.top(),
            insets_in_client_pixel.right(), insets_in_client_pixel.bottom(),
            stable_insets_in_client_pixel.left(),
            stable_insets_in_client_pixel.top(),
            stable_insets_in_client_pixel.right(),
            stable_insets_in_client_pixel.bottom(), systemui_visibility,
            DisplayTransform(display.rotation()), display.IsInternal(), &data);
    } else {
      NOTREACHED_IN_MIGRATION()
          << "The remote shell resource version being used ("
          << wl_resource_get_version(remote_shell_resource_)
          << ") is not supported.";
    }

    wl_array_release(&data);
  }
  if (event_mapping_.send_configure)
    event_mapping_.send_configure(remote_shell_resource_, layout_mode_);

  base::flat_set<wl_client*> clients;
  clients.insert(wl_resource_get_client(remote_shell_resource_));

  for (const auto& bounds_change : pending_bounds_changes_) {
    SendBoundsChanged(bounds_change.first, bounds_change.second.display_id,
                      bounds_change.second.bounds_in_display,
                      bounds_change.second.reason);
    clients.insert(wl_resource_get_client(bounds_change.first));
  }
  pending_bounds_changes_.clear();

  for (auto* client : clients)
    wl_client_flush(client);
}

void WaylandRemoteShell::OnSurfaceFocused(Surface* gained_focus,
                                          Surface* lost_focus,
                                          bool has_focused_client) {
  FocusedSurfaceChanged(gained_focus, lost_focus, has_focused_client);
}

void WaylandRemoteShell::FocusedSurfaceChanged(Surface* gained_active_surface,
                                               Surface* lost_active_surface,
                                               bool has_focused_client) {
  if (gained_active_surface == lost_active_surface &&
      last_has_focused_client_ == has_focused_client) {
    return;
  }
  last_has_focused_client_ = has_focused_client;

  wl_resource* gained_active_surface_resource =
      gained_active_surface ? GetSurfaceResource(gained_active_surface)
                            : nullptr;
  wl_resource* lost_active_surface_resource =
      lost_active_surface ? GetSurfaceResource(lost_active_surface) : nullptr;

  wl_client* client = wl_resource_get_client(remote_shell_resource_);

  // If surface that gained active is not owned by remote shell client then
  // set it to null.
  if (gained_active_surface_resource &&
      wl_resource_get_client(gained_active_surface_resource) != client) {
    gained_active_surface_resource = nullptr;
  }

  // If surface that lost active is not owned by remote shell client then
  // set it to null.
  if (lost_active_surface_resource &&
      wl_resource_get_client(lost_active_surface_resource) != client) {
    lost_active_surface_resource = nullptr;
  }

  if (wl_resource_get_version(remote_shell_resource_) >=
      event_mapping_.desktop_focus_state_changed_since_version) {
    uint32_t focus_state;
    if (gained_active_surface_resource) {
      focus_state = ZCR_REMOTE_SHELL_V1_DESKTOP_FOCUS_STATE_CLIENT_FOCUSED;
    } else if (has_focused_client) {
      focus_state =
          ZCR_REMOTE_SHELL_V1_DESKTOP_FOCUS_STATE_OTHER_CLIENT_FOCUSED;
    } else {
      focus_state = ZCR_REMOTE_SHELL_V1_DESKTOP_FOCUS_STATE_NO_FOCUS;
    }
    event_mapping_.send_desktop_focus_state_changed(remote_shell_resource_,
                                                    focus_state);
  }

  if (event_mapping_.send_activated) {
    event_mapping_.send_activated(remote_shell_resource_,
                                  gained_active_surface_resource,
                                  lost_active_surface_resource);
  }

  wl_client_flush(client);
}

void WaylandRemoteShell::OnRemoteSurfaceBoundsChanged(
    wl_resource* resource,
    WindowStateType current_state,
    WindowStateType requested_state,
    int64_t display_id,
    const gfx::Rect& bounds_in_display,
    bool resize,
    int bounds_change,
    bool is_adjusted_bounds) {
  uint32_t reason =
      ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_RESIZE;
  if (!resize)
    reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_MOVE;
  if (current_state == WindowStateType::kPip)
    reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_PIP;
  if (bounds_change & ash::WindowResizer::kBoundsChange_Resizes) {
    reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_DRAG_RESIZE;
  } else if (bounds_change & ash::WindowResizer::kBoundsChange_Repositions) {
    reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_DRAG_MOVE;
  }
  // Override the reason only if the window enters snapped or floated mode. If
  // the window resizes by dragging in snapped or floated mode, we need to keep
  // the original reason.
  if (requested_state != current_state) {
    if (requested_state == WindowStateType::kPrimarySnapped) {
      reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_SNAP_TO_LEFT;
    } else if (requested_state == WindowStateType::kSecondarySnapped) {
      reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_SNAP_TO_RIGHT;
    } else if (requested_state == WindowStateType::kFloated &&
               event_mapping_.has_bounds_change_reason_float) {
      reason = ZCR_REMOTE_SURFACE_V2_BOUNDS_CHANGE_REASON_FLOAT;
    }
  }

  if (in_display_update_ || needs_send_display_metrics_) {
    if (is_adjusted_bounds && pending_bounds_changes_.count(resource) > 0) {
      // If there is any ash-requested bounds for the resource, do not overwrite
      // it with the adjusted bounds which is based on the bounds before the
      // display update, which is to be obsolete soon.
      return;
    }
    // We store only the latest bounds for each |resource|.
    pending_bounds_changes_.insert_or_assign(
        std::move(resource),
        BoundsChangeData(display_id, bounds_in_display, reason));
    return;
  }
  SendBoundsChanged(resource, display_id, bounds_in_display, reason);

  wl_client_flush(wl_resource_get_client(resource));
}

void WaylandRemoteShell::SendBoundsChanged(
    wl_resource* resource,
    int64_t display_id,
    const gfx::Rect& bounds_in_display,
    uint32_t reason) {
  if (event_mapping_.send_bounds_changed)
    event_mapping_.send_bounds_changed(
        resource, static_cast<uint32_t>(display_id >> 32),
        static_cast<uint32_t>(display_id), bounds_in_display.x(),
        bounds_in_display.y(), bounds_in_display.width(),
        bounds_in_display.height(), reason);

  if (wl_resource_get_version(resource) >=
      event_mapping_.bounds_changed_in_output_since_version) {
    wl_resource* output = output_provider_.Run(display_id);
    if (output == nullptr) {
      LOG(WARNING) << "Failed to get wayland_output resource for display_id: "
                   << display_id;
      return;
    }
    event_mapping_.send_bounds_changed_in_output(
        resource, output, bounds_in_display.x(), bounds_in_display.y(),
        bounds_in_display.width(), bounds_in_display.height(), reason);
  }
}

void WaylandRemoteShell::OnRemoteSurfaceStateChanged(
    wl_resource* resource,
    WindowStateType old_state_type,
    WindowStateType new_state_type) {
  DCHECK_NE(old_state_type, new_state_type);
  LOG_IF(ERROR, pending_bounds_changes_.count(resource) > 0)
      << "Sending window state while there is a pending bounds change. This "
         "should not happen.";

  uint32_t state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_NORMAL;
  switch (new_state_type) {
    case WindowStateType::kMinimized:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_MINIMIZED;
      break;
    case WindowStateType::kMaximized:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_MAXIMIZED;
      break;
    case WindowStateType::kFullscreen:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_FULLSCREEN;
      break;
    case WindowStateType::kPinned:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_PINNED;
      break;
    case WindowStateType::kTrustedPinned:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_TRUSTED_PINNED;
      break;
    case WindowStateType::kPrimarySnapped:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_LEFT_SNAPPED;
      break;
    case WindowStateType::kSecondarySnapped:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_RIGHT_SNAPPED;
      break;
    case WindowStateType::kPip:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_PIP;
      break;
    default:
      break;
  }

  event_mapping_.send_state_type_changed(resource, state_type);
  wl_client_flush(wl_resource_get_client(resource));
}

void WaylandRemoteShell::OnRemoteSurfaceChangeZoomLevel(wl_resource* resource,
                                                        ZoomChange change) {
  int32_t value = 0;
  switch (change) {
    case ZoomChange::IN:
      value = ZCR_REMOTE_SURFACE_V1_ZOOM_CHANGE_IN;
      break;
    case ZoomChange::OUT:
      value = ZCR_REMOTE_SURFACE_V1_ZOOM_CHANGE_OUT;
      break;
    case ZoomChange::RESET:
      value = ZCR_REMOTE_SURFACE_V1_ZOOM_CHANGE_RESET;
      break;
  }
  event_mapping_.send_change_zoom_level(resource, value);
  wl_client_flush(wl_resource_get_client(resource));
}

void WaylandRemoteShell::OnRemoteSurfaceGeometryChanged(
    wl_resource* resource,
    const gfx::Rect& geometry) {
  LOG_IF(ERROR, pending_bounds_changes_.count(resource) > 0)
      << "Sending the new window geometry while there is a pending bounds "
         "change. This should not happen.";
  event_mapping_.send_window_geometry_changed(resource, geometry.x(),
                                              geometry.y(), geometry.width(),
                                              geometry.height());
  wl_client_flush(wl_resource_get_client(resource));
}

namespace switches {

// This flag can be used to emulate device scale factor for remote shell.
constexpr char kForceRemoteShellScale[] = "force-remote-shell-scale";

}  // namespace switches

using chromeos::WindowStateType;

namespace zcr_remote_shell {

// Returns the scale factor to be used by remote shell clients.
double GetDefaultDeviceScaleFactor() {
  // A flag used by VM to emulate a device scale for a particular board.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceRemoteShellScale)) {
    std::string value =
        command_line->GetSwitchValueASCII(switches::kForceRemoteShellScale);
    double scale = 1.0;
    if (base::StringToDouble(value, &scale))
      return std::max(1.0, scale);
  }
  return ::exo::GetDefaultDeviceScaleFactor();
}

// Scale the |child_bounds| in such a way that if it should fill the
// |parent_size|'s width/height, it returns the |parent_size_in_pixel|'s
// width/height.
gfx::Rect ScaleBoundsToPixelSnappedToParent(
    const gfx::Size& parent_size_in_pixel,
    const gfx::Size& parent_size,
    float device_scale_factor,
    const gfx::Rect& child_bounds) {
  int right = child_bounds.right();
  int bottom = child_bounds.bottom();

  int new_x = base::ClampRound(child_bounds.x() * device_scale_factor);
  int new_y = base::ClampRound(child_bounds.y() * device_scale_factor);

  int new_right = right == parent_size.width()
                      ? parent_size_in_pixel.width()
                      : base::ClampRound(right * device_scale_factor);

  int new_bottom = bottom == parent_size.height()
                       ? parent_size_in_pixel.height()
                       : base::ClampRound(bottom * device_scale_factor);
  return gfx::Rect(new_x, new_y, new_right - new_x, new_bottom - new_y);
}

void ScaleSkRegion(const SkRegion& src, float scale, SkRegion* dst) {
  SkRegion::Iterator iter(src);
  for (; !iter.done(); iter.next()) {
    SkIRect r;
    r.fLeft = base::ClampFloor(iter.rect().fLeft * scale);
    r.fTop = base::ClampFloor(iter.rect().fTop * scale);
    r.fRight = base::ClampCeil(iter.rect().fRight * scale);
    r.fBottom = base::ClampCeil(iter.rect().fBottom * scale);
    dst->op(r, SkRegion::kUnion_Op);
  }
}

int Component(uint32_t direction) {
  switch (direction) {
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_NONE:
      return HTNOWHERE;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOP:
      return HTTOP;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPRIGHT:
      return HTTOPRIGHT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_RIGHT:
      return HTRIGHT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMRIGHT:
      return HTBOTTOMRIGHT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOM:
      return HTBOTTOM;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMLEFT:
      return HTBOTTOMLEFT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_LEFT:
      return HTLEFT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPLEFT:
      return HTTOPLEFT;
    default:
      VLOG(2) << "Unknown direction:" << direction;
      break;
  }
  return HTNOWHERE;
}

uint32_t CaptionButtonMask(uint32_t mask) {
  uint32_t caption_button_icon_mask = 0;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_BACK)
    caption_button_icon_mask |= 1 << views::CAPTION_BUTTON_ICON_BACK;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_MENU)
    caption_button_icon_mask |= 1 << views::CAPTION_BUTTON_ICON_MENU;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_MINIMIZE)
    caption_button_icon_mask |= 1 << views::CAPTION_BUTTON_ICON_MINIMIZE;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_MAXIMIZE_RESTORE)
    caption_button_icon_mask |= 1
                                << views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_CLOSE)
    caption_button_icon_mask |= 1 << views::CAPTION_BUTTON_ICON_CLOSE;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_ZOOM)
    caption_button_icon_mask |= 1 << views::CAPTION_BUTTON_ICON_ZOOM;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_CENTER)
    caption_button_icon_mask |= 1 << views::CAPTION_BUTTON_ICON_CENTER;
  if (mask & ZCR_REMOTE_SURFACE_V2_FRAME_BUTTON_TYPE_FLOAT) {
    caption_button_icon_mask |= 1 << views::CAPTION_BUTTON_ICON_FLOAT;
  }
  return caption_button_icon_mask;
}

////////////////////////////////////////////////////////////////////////////////
// remote_surface_interface:

SurfaceFrameType RemoteShellSurfaceFrameType(uint32_t frame_type) {
  switch (frame_type) {
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_NONE:
      return SurfaceFrameType::NONE;
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_NORMAL:
      return SurfaceFrameType::NORMAL;
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_SHADOW:
      return SurfaceFrameType::SHADOW;
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_AUTOHIDE:
      return SurfaceFrameType::AUTOHIDE;
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_OVERLAY:
      return SurfaceFrameType::OVERLAY;
    case ZCR_REMOTE_SURFACE_V2_FRAME_TYPE_OVERLAP:
      return SurfaceFrameType::OVERLAP;
    default:
      VLOG(2) << "Unknown remote-shell frame type: " << frame_type;
      return SurfaceFrameType::NONE;
  }
}

void remote_surface_set_app_id(wl_client* client,
                               wl_resource* resource,
                               const char* app_id) {
  GetUserDataAs<ShellSurfaceBase>(resource)->SetApplicationId(app_id);
}

void remote_surface_set_window_geometry(wl_client* client,
                                        wl_resource* resource,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height) {
  // DEPRECATED - Use set_bounds to send bounds info with a display_id.
  GetUserDataAs<ShellSurfaceBase>(resource)->SetGeometry(
      gfx::Rect(x, y, width, height));
}

void remote_surface_set_orientation(wl_client* client,
                                    wl_resource* resource,
                                    int32_t orientation) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetOrientation(
      orientation == ZCR_REMOTE_SURFACE_V1_ORIENTATION_PORTRAIT
          ? Orientation::PORTRAIT
          : Orientation::LANDSCAPE);
}

void remote_surface_set_scale(wl_client* client,
                              wl_resource* resource,
                              wl_fixed_t scale) {
  // DEPRECATED (b/141715728) - The server updates the client's scale.
  NOTREACHED_IN_MIGRATION();
}

void remote_surface_set_rectangular_shadow_DEPRECATED(wl_client* client,
                                                      wl_resource* resource,
                                                      int32_t x,
                                                      int32_t y,
                                                      int32_t width,
                                                      int32_t height) {
  NOTREACHED_IN_MIGRATION();
}

void remote_surface_set_rectangular_shadow_background_opacity_DEPRECATED(
    wl_client* client,
    wl_resource* resource,
    wl_fixed_t opacity) {
  NOTREACHED_IN_MIGRATION();
}

void remote_surface_set_title(wl_client* client,
                              wl_resource* resource,
                              const char* title) {
  GetUserDataAs<ShellSurfaceBase>(resource)->SetTitle(
      std::u16string(base::UTF8ToUTF16(title)));
}

void remote_surface_set_top_inset(wl_client* client,
                                  wl_resource* resource,
                                  int32_t height) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetTopInset(height);
}

void remote_surface_activate(wl_client* client,
                             wl_resource* resource,
                             uint32_t serial) {
  ShellSurfaceBase* shell_surface = GetUserDataAs<ShellSurfaceBase>(resource);
  shell_surface->Activate();
}

void remote_surface_maximize(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetMaximized();
}

void remote_surface_minimize(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetMinimized();
}

void remote_surface_restore(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetRestored();
}

void remote_surface_fullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetFullscreen(
      true, display::kInvalidDisplayId);
}

void remote_surface_unfullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetFullscreen(
      false, display::kInvalidDisplayId);
}

void remote_surface_pin(wl_client* client,
                        wl_resource* resource,
                        int32_t trusted) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetPinned(
      trusted ? chromeos::WindowPinType::kTrustedPinned
              : chromeos::WindowPinType::kPinned);
}

void remote_surface_unpin(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetPinned(
      chromeos::WindowPinType::kNone);
}

void remote_surface_set_system_modal(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSystemModal(true);
}

void remote_surface_unset_system_modal(wl_client* client,
                                       wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSystemModal(false);
}

void remote_surface_set_rectangular_surface_shadow(wl_client* client,
                                                   wl_resource* resource,
                                                   int32_t x,
                                                   int32_t y,
                                                   int32_t width,
                                                   int32_t height) {
  // Shadow Bounds are set in pixels, and should not be scaled.
  ClientControlledShellSurface* shell_surface =
      GetUserDataAs<ClientControlledShellSurface>(resource);
  shell_surface->SetShadowBounds(gfx::Rect(x, y, width, height));
}

void remote_surface_set_systemui_visibility(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t visibility) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSystemUiVisibility(
      visibility != ZCR_REMOTE_SURFACE_V1_SYSTEMUI_VISIBILITY_STATE_VISIBLE);
}

void remote_surface_set_always_on_top(wl_client* client,
                                      wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetAlwaysOnTop(true);
}

void remote_surface_unset_always_on_top(wl_client* client,
                                        wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetAlwaysOnTop(false);
}

void remote_surface_ack_configure_DEPRECATED(wl_client* client,
                                             wl_resource* resource,
                                             uint32_t serial) {
  NOTREACHED_IN_MIGRATION();
}

void remote_surface_move_DEPRECATED(wl_client* client, wl_resource* resource) {
  NOTREACHED_IN_MIGRATION();
}

void remote_surface_set_window_type(wl_client* client,
                                    wl_resource* resource,
                                    uint32_t type) {
  auto* widget = GetUserDataAs<ShellSurfaceBase>(resource)->GetWidget();
  if (!widget)
    return;

  switch (type) {
    case ZCR_REMOTE_SURFACE_V1_WINDOW_TYPE_NORMAL:
      widget->GetNativeWindow()->SetProperty(ash::kHideInOverviewKey, false);
      break;
    case ZCR_REMOTE_SURFACE_V1_WINDOW_TYPE_SYSTEM_UI:
      // TODO(takise): Consider removing this as this window type was added for
      // the old assistant and is not longer used.
      widget->GetNativeWindow()->SetProperty(ash::kHideInOverviewKey, true);
      wm::SetWindowVisibilityAnimationType(
          widget->GetNativeWindow(), wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
      break;
    case ZCR_REMOTE_SURFACE_V1_WINDOW_TYPE_HIDDEN_IN_OVERVIEW:
      widget->GetNativeWindow()->SetProperty(ash::kHideInOverviewKey, true);
      break;
  }
}

void remote_surface_resize_DEPRECATED(wl_client* client,
                                      wl_resource* resource) {
  // DEPRECATED
  NOTREACHED_IN_MIGRATION();
}

void remote_surface_set_resize_outset_DEPRECATED(wl_client* client,
                                                 wl_resource* resource,
                                                 int32_t outset) {
  // DEPRECATED
  NOTREACHED_IN_MIGRATION();
}

void remote_surface_start_move(wl_client* client,
                               wl_resource* resource,
                               int32_t x,
                               int32_t y) {
  ClientControlledShellSurface* shell_surface =
      GetUserDataAs<ClientControlledShellSurface>(resource);
  float scale = shell_surface->GetClientToDpScale();
  gfx::PointF p(x, y);
  shell_surface->StartDrag(HTCAPTION, gfx::ScalePoint(p, scale));
}

void remote_surface_set_can_maximize(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetCanMaximize(true);
}

void remote_surface_unset_can_maximize(wl_client* client,
                                       wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetCanMaximize(false);
}

void remote_surface_set_min_size(wl_client* client,
                                 wl_resource* resource,
                                 int32_t width,
                                 int32_t height) {
  ClientControlledShellSurface* shell_surface =
      GetUserDataAs<ClientControlledShellSurface>(resource);
  float scale = shell_surface->GetClientToDpPendingScale();
  gfx::Size s(width, height);
  shell_surface->SetMinimumSize(gfx::ScaleToRoundedSize(s, scale));
}

void remote_surface_set_max_size(wl_client* client,
                                 wl_resource* resource,
                                 int32_t width,
                                 int32_t height) {
  ClientControlledShellSurface* shell_surface =
      GetUserDataAs<ClientControlledShellSurface>(resource);
  float scale = shell_surface->GetClientToDpPendingScale();
  gfx::Size s(width, height);
  shell_surface->SetMaximumSize(gfx::ScaleToRoundedSize(s, scale));
}

void remote_surface_set_aspect_ratio(wl_client* client,
                                     wl_resource* resource,
                                     int32_t aspect_ratio_width,
                                     int32_t aspect_ratio_height) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetAspectRatio(
      gfx::SizeF(aspect_ratio_width, aspect_ratio_height));
}

void remote_surface_set_snapped_to_left(wl_client* client,
                                        wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSnapPrimary(
      chromeos::kDefaultSnapRatio);
}

void remote_surface_set_snapped_to_right(wl_client* client,
                                         wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSnapSecondary(
      chromeos::kDefaultSnapRatio);
}

void remote_surface_start_resize(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t direction,
                                 int32_t x,
                                 int32_t y) {
  ClientControlledShellSurface* shell_surface =
      GetUserDataAs<ClientControlledShellSurface>(resource);
  float scale = shell_surface->GetClientToDpScale();
  gfx::PointF p(x, y);
  shell_surface->StartDrag(Component(direction), gfx::ScalePoint(p, scale));
}

void remote_surface_set_frame(wl_client* client,
                              wl_resource* resource,
                              uint32_t type) {
  ClientControlledShellSurface* shell_surface =
      GetUserDataAs<ClientControlledShellSurface>(resource);
  shell_surface->root_surface()->SetFrame(RemoteShellSurfaceFrameType(type));
}

void remote_surface_set_frame_buttons(wl_client* client,
                                      wl_resource* resource,
                                      uint32_t visible_button_mask,
                                      uint32_t enabled_button_mask) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetFrameButtons(
      CaptionButtonMask(visible_button_mask),
      CaptionButtonMask(enabled_button_mask));
}

void remote_surface_set_extra_title(wl_client* client,
                                    wl_resource* resource,
                                    const char* extra_title) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetExtraTitle(
      std::u16string(base::UTF8ToUTF16(extra_title)));
}

chromeos::OrientationType OrientationLock(uint32_t orientation_lock) {
  switch (orientation_lock) {
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_NONE:
      return chromeos::OrientationType::kAny;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_CURRENT:
      return chromeos::OrientationType::kCurrent;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_PORTRAIT:
      return chromeos::OrientationType::kPortrait;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_LANDSCAPE:
      return chromeos::OrientationType::kLandscape;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_PORTRAIT_PRIMARY:
      return chromeos::OrientationType::kPortraitPrimary;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_PORTRAIT_SECONDARY:
      return chromeos::OrientationType::kPortraitSecondary;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_LANDSCAPE_PRIMARY:
      return chromeos::OrientationType::kLandscapePrimary;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_LANDSCAPE_SECONDARY:
      return chromeos::OrientationType::kLandscapeSecondary;
  }
  VLOG(2) << "Unexpected value of orientation_lock: " << orientation_lock;
  return chromeos::OrientationType::kAny;
}

void remote_surface_set_orientation_lock(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t orientation_lock) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetOrientationLock(
      OrientationLock(orientation_lock));
}

void remote_surface_pip(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetPip();
}

void remote_surface_set_bounds(wl_client* client,
                               wl_resource* resource,
                               uint32_t display_id_hi,
                               uint32_t display_id_lo,
                               int32_t x,
                               int32_t y,
                               int32_t width,
                               int32_t height) {
  // Bounds are set in pixels, and should not be scaled.
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetBounds(
      static_cast<int64_t>(display_id_hi) << 32 | display_id_lo,
      gfx::Rect(x, y, width, height));
}

void remote_surface_block_ime(wl_client* client, wl_resource* resource) {
  NOTIMPLEMENTED();
}

void remote_surface_unblock_ime(wl_client* client, wl_resource* resource) {
  NOTIMPLEMENTED();
}

void remote_surface_set_accessibility_id_DEPRECATED(wl_client* client,
                                                    wl_resource* resource,
                                                    int32_t accessibility_id) {
  NOTREACHED_IN_MIGRATION();
}

void remote_surface_set_pip_original_window(wl_client* client,
                                            wl_resource* resource) {
  auto* widget = GetUserDataAs<ShellSurfaceBase>(resource)->GetWidget();
  if (!widget) {
    LOG(ERROR) << "no widget found for setting pip original window";
    return;
  }

  widget->GetNativeWindow()->SetProperty(ash::kPipOriginalWindowKey, true);
}

void remote_surface_unset_pip_original_window(wl_client* client,
                                              wl_resource* resource) {
  auto* widget = GetUserDataAs<ShellSurfaceBase>(resource)->GetWidget();
  if (!widget) {
    LOG(ERROR) << "no widget found for unsetting pip original window";
    return;
  }

  widget->GetNativeWindow()->SetProperty(ash::kPipOriginalWindowKey, false);
}

void remote_surface_set_system_gesture_exclusion(wl_client* client,
                                                 wl_resource* resource,
                                                 wl_resource* region_resource) {
  auto* shell_surface = GetUserDataAs<ClientControlledShellSurface>(resource);
  auto* widget = shell_surface->GetWidget();
  if (!widget) {
    LOG(ERROR) << "no widget found for setting system gesture exclusion";
    return;
  }

  if (region_resource) {
    SkRegion* dst = new SkRegion;
    ScaleSkRegion(*GetUserDataAs<SkRegion>(region_resource),
                  shell_surface->GetClientToDpScale(), dst);
    widget->GetNativeWindow()->SetProperty(ash::kSystemGestureExclusionKey,
                                           dst);
  } else {
    widget->GetNativeWindow()->ClearProperty(ash::kSystemGestureExclusionKey);
  }
}

void remote_surface_set_resize_lock(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetResizeLockType(
      ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE);
}

void remote_surface_unset_resize_lock(wl_client* client,
                                      wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetResizeLockType(
      ash::ArcResizeLockType::NONE);
}

void remote_surface_set_bounds_in_output(wl_client* client,
                                         wl_resource* resource,
                                         wl_resource* output_resource,
                                         int32_t x,
                                         int32_t y,
                                         int32_t width,
                                         int32_t height) {
  WaylandDisplayHandler* display_handler =
      GetUserDataAs<WaylandDisplayHandler>(output_resource);
  // Bounds are set in pixels, and should not be scaled.
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetBounds(
      display_handler->id(), gfx::Rect(x, y, width, height));
}

void remote_surface_set_resize_lock_type(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t type) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetResizeLockType(
      static_cast<ash::ArcResizeLockType>(type));
}

void remote_surface_set_float(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetFloatToLocation(
      chromeos::FloatStartLocation::kBottomRight);
}

void remote_surface_set_scale_factor(wl_client* client,
                                     wl_resource* resource,
                                     uint scale_factor_as_uint) {
  static_assert(sizeof(uint32_t) == sizeof(float),
                "Sizes much match for reinterpret cast to be meaningful");
  float scale_factor = base::bit_cast<float>(scale_factor_as_uint);
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetScaleFactor(
      scale_factor);
}

void remote_surface_set_window_corner_radii(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t upper_left_radius,
                                            uint32_t upper_right_radius,
                                            uint32_t lower_right_radius,
                                            uint32_t lower_left_radius) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetWindowCornersRadii(
      gfx::RoundedCornersF(upper_left_radius, upper_right_radius,
                           lower_right_radius, lower_left_radius));
}

void remote_surface_set_shadow_corner_radii(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t upper_left_radius,
                                            uint32_t upper_right_radius,
                                            uint32_t lower_right_radius,
                                            uint32_t lower_left_radius) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetShadowCornersRadii(
      gfx::RoundedCornersF(upper_left_radius, upper_right_radius,
                           lower_right_radius, lower_left_radius));
}


////////////////////////////////////////////////////////////////////////////////
// notification_surface_interface:

void notification_surface_set_app_id(wl_client* client,
                                     wl_resource* resource,
                                     const char* app_id) {
  GetUserDataAs<NotificationSurface>(resource)->SetApplicationId(app_id);
}

////////////////////////////////////////////////////////////////////////////////
// input_method_surface_interface:

void input_method_surface_set_bounds(wl_client* client,
                                     wl_resource* resource,
                                     uint32_t display_id_hi,
                                     uint32_t display_id_lo,
                                     int32_t x,
                                     int32_t y,
                                     int32_t width,
                                     int32_t height) {
  GetUserDataAs<InputMethodSurface>(resource)->SetBounds(
      static_cast<int64_t>(display_id_hi) << 32 | display_id_lo,
      gfx::Rect(x, y, width, height));
}

void input_method_surface_set_bounds_in_output(wl_client* client,
                                               wl_resource* resource,
                                               wl_resource* output_resource,
                                               int32_t x,
                                               int32_t y,
                                               int32_t width,
                                               int32_t height) {
  WaylandDisplayHandler* display_handler =
      GetUserDataAs<WaylandDisplayHandler>(output_resource);
  GetUserDataAs<InputMethodSurface>(resource)->SetBounds(
      display_handler->id(), gfx::Rect(x, y, width, height));
}

////////////////////////////////////////////////////////////////////////////////
// toast_surface_interface:

void toast_surface_set_position(wl_client* client,
                                wl_resource* resource,
                                uint32_t display_id_hi,
                                uint32_t display_id_lo,
                                int32_t x,
                                int32_t y) {
  const int64_t display_id =
      static_cast<int64_t>(display_id_hi) << 32 | display_id_lo;
  GetUserDataAs<ToastSurface>(resource)->SetBoundsOrigin(display_id,
                                                         gfx::Point(x, y));
}

void toast_surface_set_size(wl_client* client,
                            wl_resource* resource,
                            int32_t width,
                            int32_t height) {
  GetUserDataAs<ToastSurface>(resource)->SetBoundsSize(
      gfx::Size(width, height));
}

void toast_surface_set_bounds_in_output(wl_client* client,
                                        wl_resource* resource,
                                        wl_resource* output_resource,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height) {
  WaylandDisplayHandler* display_handler =
      GetUserDataAs<WaylandDisplayHandler>(output_resource);
  GetUserDataAs<ToastSurface>(resource)->SetBounds(
      display_handler->id(), gfx::Rect(x, y, width, height));
}

void toast_surface_set_scale_factor(wl_client* client,
                                    wl_resource* resource,
                                    uint scale_factor_as_uint) {
  static_assert(sizeof(uint32_t) == sizeof(float),
                "Sizes must match for reinterpret cast to be meaningful");
  float scale_factor = base::bit_cast<float>(scale_factor_as_uint);
  GetUserDataAs<ToastSurface>(resource)->SetScaleFactor(scale_factor);
}

////////////////////////////////////////////////////////////////////////////////
// remote_shell_interface:

void remote_shell_set_use_default_scale_cancellation(
    wl_client*,
    wl_resource* resource,
    int32_t use_default_scale_cancellation) {
  auto* shell = GetUserDataAs<WaylandRemoteShell>(resource);
  if (wl_resource_get_version(resource) <
      shell->event_mapping_.set_use_default_scale_cancellation_since_version) {
    return;
  }
  shell->SetUseDefaultScaleCancellation(use_default_scale_cancellation != 0);
}

}  // namespace zcr_remote_shell

}  // namespace wayland
}  // namespace exo
