// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zaura_shell.h"

#include <aura-shell-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <xdg-shell-server-protocol.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/window_properties.h"
#include "ash/wm/window_state.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/exo/display.h"
#include "components/exo/seat.h"
#include "components/exo/seat_observer.h"
#include "components/exo/shell_surface.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/wayland/serial_tracker.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_display_observer.h"
#include "components/exo/wayland/wl_output.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/env.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/compositor/layer.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_util.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/display/display_util.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/wayland/xdg_shell.h"
#include "components/exo/wm_helper_chromeos.h"
#include "ui/aura/client/aura_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace exo {
namespace wayland {

namespace {

constexpr int kAuraShellSeatObserverPriority = 1;
static_assert(Seat::IsValidObserverPriority(kAuraShellSeatObserverPriority),
              "kAuraShellSeatObserverPriority is not in the valid range.");

// A property key containing a boolean set to true if na aura surface object is
// associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasAuraSurfaceKey, false)

bool TransformRelativeToScreenIsAxisAligned(aura::Window* window) {
  gfx::Transform transform_relative_to_screen;
  DCHECK(window->layer()->GetTargetTransformRelativeTo(
      window->GetRootWindow()->layer(), &transform_relative_to_screen));
  transform_relative_to_screen.ConcatTransform(
      window->GetRootWindow()->layer()->GetTargetTransform());
  return transform_relative_to_screen.Preserves2dAxisAlignment();
}

// This does not handle non-axis aligned rotations, but we don't have any
// slightly (e.g. 45 degree) windows so it is okay.
gfx::Rect GetTransformedBoundsInScreen(aura::Window* window) {
  DCHECK(TransformRelativeToScreenIsAxisAligned(window));
  // This assumes that opposite points on the window bounds rectangle will
  // be mapped to opposite points on the output rectangle.
  gfx::Point a = window->bounds().origin();
  gfx::Point b = window->bounds().bottom_right();
  ::wm::ConvertPointToScreen(window->parent(), &a);
  ::wm::ConvertPointToScreen(window->parent(), &b);
  return gfx::Rect(std::min(a.x(), b.x()), std::min(a.y(), b.y()),
                   std::abs(a.x() - b.x()), std::abs(a.y() - b.y()));
}

SurfaceFrameType AuraSurfaceFrameType(uint32_t frame_type) {
  switch (frame_type) {
    case ZAURA_SURFACE_FRAME_TYPE_NONE:
      return SurfaceFrameType::NONE;
    case ZAURA_SURFACE_FRAME_TYPE_NORMAL:
      return SurfaceFrameType::NORMAL;
    case ZAURA_SURFACE_FRAME_TYPE_SHADOW:
      return SurfaceFrameType::SHADOW;
    default:
      VLOG(2) << "Unkonwn aura-shell frame type: " << frame_type;
      return SurfaceFrameType::NONE;
  }
}

zaura_surface_occlusion_state WaylandOcclusionState(
    const aura::Window::OcclusionState occlusion_state) {
  switch (occlusion_state) {
    case aura::Window::OcclusionState::UNKNOWN:
      return ZAURA_SURFACE_OCCLUSION_STATE_UNKNOWN;
    case aura::Window::OcclusionState::VISIBLE:
      return ZAURA_SURFACE_OCCLUSION_STATE_VISIBLE;
    case aura::Window::OcclusionState::OCCLUDED:
      return ZAURA_SURFACE_OCCLUSION_STATE_OCCLUDED;
    case aura::Window::OcclusionState::HIDDEN:
      return ZAURA_SURFACE_OCCLUSION_STATE_HIDDEN;
  }
  return ZAURA_SURFACE_OCCLUSION_STATE_UNKNOWN;
}

void aura_surface_set_frame(wl_client* client,
                            wl_resource* resource,
                            uint32_t type) {
  GetUserDataAs<AuraSurface>(resource)->SetFrame(AuraSurfaceFrameType(type));
}

void aura_surface_set_parent(wl_client* client,
                             wl_resource* resource,
                             wl_resource* parent_resource,
                             int32_t x,
                             int32_t y) {
  GetUserDataAs<AuraSurface>(resource)->SetParent(
      parent_resource ? GetUserDataAs<AuraSurface>(parent_resource) : nullptr,
      gfx::Point(x, y));
}

void aura_surface_set_frame_colors(wl_client* client,
                                   wl_resource* resource,
                                   uint32_t active_color,
                                   uint32_t inactive_color) {
  GetUserDataAs<AuraSurface>(resource)->SetFrameColors(active_color,
                                                       inactive_color);
}

void aura_surface_set_startup_id(wl_client* client,
                                 wl_resource* resource,
                                 const char* startup_id) {
  GetUserDataAs<AuraSurface>(resource)->SetStartupId(startup_id);
}

void aura_surface_set_application_id(wl_client* client,
                                     wl_resource* resource,
                                     const char* application_id) {
  GetUserDataAs<AuraSurface>(resource)->SetApplicationId(application_id);
}

void aura_surface_set_client_surface_id_DEPRECATED(wl_client* client,
                                                   wl_resource* resource,
                                                   int client_surface_id) {
  // DEPRECATED. Use aura_surface_set_client_surface_str_id
  std::string client_surface_str_id = base::NumberToString(client_surface_id);
  GetUserDataAs<AuraSurface>(resource)->SetClientSurfaceId(
      client_surface_str_id.c_str());
}

void aura_surface_set_occlusion_tracking(wl_client* client,
                                         wl_resource* resource) {
  GetUserDataAs<AuraSurface>(resource)->SetOcclusionTracking(true);
}

void aura_surface_unset_occlusion_tracking(wl_client* client,
                                           wl_resource* resource) {
  GetUserDataAs<AuraSurface>(resource)->SetOcclusionTracking(false);
}

void aura_surface_activate(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraSurface>(resource)->Activate();
}

void aura_surface_draw_attention(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraSurface>(resource)->DrawAttention();
}

void aura_surface_set_fullscreen_mode(wl_client* client,
                                      wl_resource* resource,
                                      uint32_t mode) {
  GetUserDataAs<AuraSurface>(resource)->SetFullscreenMode(mode);
}

void aura_surface_set_client_surface_str_id(wl_client* client,
                                            wl_resource* resource,
                                            const char* client_surface_id) {
  GetUserDataAs<AuraSurface>(resource)->SetClientSurfaceId(client_surface_id);
}

void aura_surface_set_server_start_resize(wl_client* client,
                                          wl_resource* resource) {
  GetUserDataAs<AuraSurface>(resource)->SetServerStartResize();
}

void aura_surface_intent_to_snap(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t snap_direction) {
  GetUserDataAs<AuraSurface>(resource)->IntentToSnap(snap_direction);
}

void aura_surface_set_snap_left(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraSurface>(resource)->SetSnapPrimary();
}

void aura_surface_set_snap_right(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraSurface>(resource)->SetSnapSecondary();
}

void aura_surface_unset_snap(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraSurface>(resource)->UnsetSnap();
}

void aura_surface_set_window_session_id(wl_client* client,
                                        wl_resource* resource,
                                        int32_t id) {
  GetUserDataAs<AuraSurface>(resource)->SetWindowSessionId(id);
}

void aura_surface_set_can_go_back(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraSurface>(resource)->SetCanGoBack();
}

void aura_surface_unset_can_go_back(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraSurface>(resource)->UnsetCanGoBack();
}

void aura_surface_set_pip(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraSurface>(resource)->SetPip();
}

void aura_surface_unset_pip(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraSurface>(resource)->UnsetPip();
}

void aura_surface_set_aspect_ratio(wl_client* client,
                                   wl_resource* resource,
                                   int32_t width,
                                   int32_t height) {
  GetUserDataAs<AuraSurface>(resource)->SetAspectRatio(
      gfx::SizeF(width, height));
}

void aura_surface_move_to_desk(wl_client* client,
                               wl_resource* resource,
                               int index) {
  GetUserDataAs<AuraSurface>(resource)->MoveToDesk(index);
}

void aura_surface_set_initial_workspace(wl_client* client,
                                        wl_resource* resource,
                                        const char* initial_workspace) {
  GetUserDataAs<AuraSurface>(resource)->SetInitialWorkspace(initial_workspace);
}

void aura_surface_set_pin(wl_client* client,
                          wl_resource* resource,
                          int32_t trusted) {
  GetUserDataAs<AuraSurface>(resource)->Pin(trusted);
}

void aura_surface_unset_pin(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraSurface>(resource)->Unpin();
}

const struct zaura_surface_interface aura_surface_implementation = {
    aura_surface_set_frame,
    aura_surface_set_parent,
    aura_surface_set_frame_colors,
    aura_surface_set_startup_id,
    aura_surface_set_application_id,
    aura_surface_set_client_surface_id_DEPRECATED,
    aura_surface_set_occlusion_tracking,
    aura_surface_unset_occlusion_tracking,
    aura_surface_activate,
    aura_surface_draw_attention,
    aura_surface_set_fullscreen_mode,
    aura_surface_set_client_surface_str_id,
    aura_surface_set_server_start_resize,
    aura_surface_intent_to_snap,
    aura_surface_set_snap_left,
    aura_surface_set_snap_right,
    aura_surface_unset_snap,
    aura_surface_set_window_session_id,
    aura_surface_set_can_go_back,
    aura_surface_unset_can_go_back,
    aura_surface_set_pip,
    aura_surface_unset_pip,
    aura_surface_set_aspect_ratio,
    aura_surface_move_to_desk,
    aura_surface_set_initial_workspace,
    aura_surface_set_pin,
    aura_surface_unset_pin,
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// aura_surface_interface:

AuraSurface::AuraSurface(Surface* surface, wl_resource* resource)
    : surface_(surface), resource_(resource) {
  surface_->AddSurfaceObserver(this);
  surface_->SetProperty(kSurfaceHasAuraSurfaceKey, true);
  WMHelper::GetInstance()->AddActivationObserver(this);
}

AuraSurface::~AuraSurface() {
  WMHelper::GetInstance()->RemoveActivationObserver(this);
  if (surface_) {
    surface_->RemoveSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasAuraSurfaceKey, false);
  }
}

void AuraSurface::SetFrame(SurfaceFrameType type) {
  if (surface_)
    surface_->SetFrame(type);
}

void AuraSurface::SetServerStartResize() {
  if (surface_)
    surface_->SetServerStartResize();
}

void AuraSurface::SetFrameColors(SkColor active_frame_color,
                                 SkColor inactive_frame_color) {
  if (surface_)
    surface_->SetFrameColors(active_frame_color, inactive_frame_color);
}

void AuraSurface::SetParent(AuraSurface* parent, const gfx::Point& position) {
  if (surface_)
    surface_->SetParent(parent ? parent->surface_ : nullptr, position);
}

void AuraSurface::SetStartupId(const char* startup_id) {
  if (surface_)
    surface_->SetStartupId(startup_id);
}

void AuraSurface::SetApplicationId(const char* application_id) {
  if (surface_)
    surface_->SetApplicationId(application_id);
}

void AuraSurface::SetClientSurfaceId(const char* client_surface_id) {
  if (surface_)
    surface_->SetClientSurfaceId(client_surface_id);
}

void AuraSurface::SetOcclusionTracking(bool tracking) {
  if (surface_)
    surface_->SetOcclusionTracking(tracking);
}

void AuraSurface::Activate() {
  if (surface_)
    surface_->RequestActivation();
}

void AuraSurface::DrawAttention() {
  if (!surface_)
    return;
  // TODO(hollingum): implement me.
  LOG(WARNING) << "Surface requested attention, but that is not implemented";
}

void AuraSurface::SetFullscreenMode(uint32_t mode) {
  if (!surface_)
    return;

  switch (mode) {
    case ZAURA_SURFACE_FULLSCREEN_MODE_PLAIN:
      surface_->SetUseImmersiveForFullscreen(false);
      break;
    case ZAURA_SURFACE_FULLSCREEN_MODE_IMMERSIVE:
      surface_->SetUseImmersiveForFullscreen(true);
      break;
    default:
      VLOG(2) << "aura_surface_set_fullscreen_mode(): unknown fullscreen_mode: "
              << mode;
      break;
  }
}

void AuraSurface::IntentToSnap(uint32_t snap_direction) {
  switch (snap_direction) {
    case ZAURA_SURFACE_SNAP_DIRECTION_NONE:
      surface_->HideSnapPreview();
      break;
    case ZAURA_SURFACE_SNAP_DIRECTION_LEFT:
      surface_->ShowSnapPreviewToPrimary();
      break;
    case ZAURA_SURFACE_SNAP_DIRECTION_RIGHT:
      surface_->ShowSnapPreviewToSecondary();
      break;
  }
}

void AuraSurface::SetSnapPrimary() {
  surface_->SetSnappedToPrimary();
}

void AuraSurface::SetSnapSecondary() {
  surface_->SetSnappedToSecondary();
}

void AuraSurface::UnsetSnap() {
  surface_->UnsetSnap();
}

void AuraSurface::SetWindowSessionId(int32_t window_session_id) {
  surface_->SetWindowSessionId(window_session_id);
}

void AuraSurface::SetCanGoBack() {
  surface_->SetCanGoBack();
}

void AuraSurface::UnsetCanGoBack() {
  surface_->UnsetCanGoBack();
}

void AuraSurface::SetPip() {
  surface_->SetPip();
}

void AuraSurface::UnsetPip() {
  surface_->UnsetPip();
}

void AuraSurface::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  surface_->SetAspectRatio(aspect_ratio);
}

// Overridden from SurfaceObserver:
void AuraSurface::OnSurfaceDestroying(Surface* surface) {
  surface->RemoveSurfaceObserver(this);
  surface_ = nullptr;
}

void AuraSurface::OnWindowOcclusionChanged(Surface* surface) {
  if (!surface_ || !surface_->IsTrackingOcclusion())
    return;
  auto* window = surface_->window();
  ComputeAndSendOcclusion(window->GetOcclusionState(),
                          window->occluded_region_in_root());
}

void AuraSurface::OnFrameLockingChanged(Surface* surface, bool lock) {
  if (wl_resource_get_version(resource_) <
      ZAURA_SURFACE_LOCK_FRAME_NORMAL_SINCE_VERSION)
    return;
  if (lock)
    zaura_surface_send_lock_frame_normal(resource_);
  else
    zaura_surface_send_unlock_frame_normal(resource_);
}

void AuraSurface::OnWindowActivating(ActivationReason reason,
                                     aura::Window* gaining_active,
                                     aura::Window* losing_active) {
  if (!surface_ || !losing_active)
    return;

  auto* window = surface_->window();
  // Check if this surface is a child of a window that is losing focus.
  auto* widget = views::Widget::GetTopLevelWidgetForNativeView(window);
  if (!widget || losing_active != widget->GetNativeWindow() ||
      !surface_->IsTrackingOcclusion())
    return;

  // Result may be changed by animated windows, so compute it explicitly.
  // We need to send occlusion updates before activation changes because
  // we can only trigger onUserLeaveHint (which triggers Android PIP) upon
  // losing activation. Windows that have animations applied to them are
  // normally ignored by the occlusion tracker, but in this case we want
  // to send the occlusion state after animations finish before activation
  // changes. This lets us support showing a new window triggering PIP,
  // which normally would not work due to the window show animation delaying
  // any occlusion update.
  // This happens before any window stacking changes occur, which means that
  // calling the occlusion tracker here for activation changes which change
  // the window stacking order may not produce correct results. But,
  // showing a new window will have it stacked on top already, so this will not
  // be a problem.
  // TODO(edcourtney): Currently, this does not work for activating via the
  //   overview, because starting the overview activates some overview specific
  //   window. To support overview, we would need to have it keep the original
  //   window activated and also do this inside OnWindowStackingChanged.
  //   See crbug.com/948492.
  auto* occlusion_tracker =
      aura::Env::GetInstance()->GetWindowOcclusionTracker();
  if (occlusion_tracker->HasIgnoredAnimatingWindows()) {
    const auto& occlusion_data =
        occlusion_tracker->ComputeTargetOcclusionForWindow(window);
    ComputeAndSendOcclusion(occlusion_data.occlusion_state,
                            occlusion_data.occluded_region);
  }
}

void AuraSurface::SendOcclusionFraction(float occlusion_fraction) {
  if (wl_resource_get_version(resource_) < 8)
    return;
  // TODO(edcourtney): For now, we are treating every occlusion change as
  // from a user action.
  zaura_surface_send_occlusion_changed(
      resource_, wl_fixed_from_double(occlusion_fraction),
      ZAURA_SURFACE_OCCLUSION_CHANGE_REASON_USER_ACTION);
  wl_client_flush(wl_resource_get_client(resource_));
}

void AuraSurface::SendOcclusionState(
    const aura::Window::OcclusionState occlusion_state) {
  if (wl_resource_get_version(resource_) < 21)
    return;
  zaura_surface_send_occlusion_state_changed(
      resource_, WaylandOcclusionState(occlusion_state));
  wl_client_flush(wl_resource_get_client(resource_));
}

void AuraSurface::ComputeAndSendOcclusion(
    const aura::Window::OcclusionState occlusion_state,
    const SkRegion& occluded_region) {
  SendOcclusionState(occlusion_state);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Should re-write in locked case - we don't want to trigger PIP upon
  // locking the screen.
  if (ash::Shell::Get()->session_controller()->IsScreenLocked()) {
    SendOcclusionFraction(0.0f);
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Send the occlusion fraction.
  auto* window = surface_->window();
  float fraction_occluded = 0.0f;
  switch (occlusion_state) {
    case aura::Window::OcclusionState::VISIBLE: {
      const gfx::Rect display_bounds_in_screen =
          display::Screen::GetScreen()
              ->GetDisplayNearestWindow(window)
              .bounds();
      const gfx::Rect bounds_in_screen = GetTransformedBoundsInScreen(window);
      const int tracked_area =
          bounds_in_screen.width() * bounds_in_screen.height();
      SkRegion tracked_and_occluded_region = occluded_region;
      tracked_and_occluded_region.op(gfx::RectToSkIRect(bounds_in_screen),
                                     SkRegion::Op::kIntersect_Op);

      // Clip the area outside of the display.
      gfx::Rect area_inside_display = bounds_in_screen;
      area_inside_display.Intersect(display_bounds_in_screen);
      int occluded_area = tracked_area - area_inside_display.width() *
                                             area_inside_display.height();

      for (SkRegion::Iterator i(tracked_and_occluded_region); !i.done();
           i.next()) {
        occluded_area += i.rect().width() * i.rect().height();
      }
      if (tracked_area) {
        fraction_occluded = static_cast<float>(occluded_area) /
                            static_cast<float>(tracked_area);
      }
      break;
    }
    case aura::Window::OcclusionState::OCCLUDED:
    case aura::Window::OcclusionState::HIDDEN:
      // Consider the OCCLUDED and HIDDEN cases as 100% occlusion.
      fraction_occluded = 1.0f;
      break;
    case aura::Window::OcclusionState::UNKNOWN:
      return;  // Window is not tracked.
  }
  SendOcclusionFraction(fraction_occluded);
}

void AuraSurface::OnDeskChanged(Surface* surface, int state) {
  if (wl_resource_get_version(resource_) <
      ZAURA_SURFACE_DESK_CHANGED_SINCE_VERSION) {
    return;
  }

  zaura_surface_send_desk_changed(resource_, state);
}

void AuraSurface::ThrottleFrameRate(bool on) {
  if (wl_resource_get_version(resource_) <
      ZAURA_SURFACE_START_THROTTLE_SINCE_VERSION) {
    return;
  }
  if (on)
    zaura_surface_send_start_throttle(resource_);
  else
    zaura_surface_send_end_throttle(resource_);
  wl_client_flush(wl_resource_get_client(resource_));
}

void AuraSurface::MoveToDesk(int desk_index) {
  constexpr int kToggleVisibleOnAllWorkspacesValue = -1;
  if (desk_index == kToggleVisibleOnAllWorkspacesValue) {
    surface_->SetVisibleOnAllWorkspaces();
  } else {
    surface_->MoveToDesk(desk_index);
  }
}

void AuraSurface::SetInitialWorkspace(const char* initial_workspace) {
  surface_->SetInitialWorkspace(initial_workspace);
}

void AuraSurface::Pin(bool trusted) {
  surface_->Pin(trusted);
}

void AuraSurface::Unpin() {
  surface_->Unpin();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

chromeos::OrientationType OrientationLock(uint32_t orientation_lock) {
  switch (orientation_lock) {
    case ZAURA_TOPLEVEL_ORIENTATION_LOCK_NONE:
      return chromeos::OrientationType::kAny;
    case ZAURA_TOPLEVEL_ORIENTATION_LOCK_CURRENT:
      return chromeos::OrientationType::kCurrent;
    case ZAURA_TOPLEVEL_ORIENTATION_LOCK_PORTRAIT:
      return chromeos::OrientationType::kPortrait;
    case ZAURA_TOPLEVEL_ORIENTATION_LOCK_LANDSCAPE:
      return chromeos::OrientationType::kLandscape;
    case ZAURA_TOPLEVEL_ORIENTATION_LOCK_PORTRAIT_PRIMARY:
      return chromeos::OrientationType::kPortraitPrimary;
    case ZAURA_TOPLEVEL_ORIENTATION_LOCK_LANDSCAPE_PRIMARY:
      return chromeos::OrientationType::kLandscapePrimary;
    case ZAURA_TOPLEVEL_ORIENTATION_LOCK_PORTRAIT_SECONDARY:
      return chromeos::OrientationType::kPortraitSecondary;
    case ZAURA_TOPLEVEL_ORIENTATION_LOCK_LANDSCAPE_SECONDARY:
      return chromeos::OrientationType::kLandscapeSecondary;
  }
  VLOG(2) << "Unexpected value of orientation_lock: " << orientation_lock;
  return chromeos::OrientationType::kAny;
}

using AuraSurfaceConfigureCallback =
    base::RepeatingCallback<void(const gfx::Rect& bounds,
                                 chromeos::WindowStateType state_type,
                                 bool resizing,
                                 bool activated)>;

uint32_t HandleAuraSurfaceConfigureCallback(
    wl_resource* resource,
    SerialTracker* serial_tracker,
    const AuraSurfaceConfigureCallback& callback,
    const gfx::Rect& bounds,
    chromeos::WindowStateType state_type,
    bool resizing,
    bool activated,
    const gfx::Vector2d& origin_offset) {
  uint32_t serial =
      serial_tracker->GetNextSerial(SerialTracker::EventType::OTHER_EVENT);
  callback.Run(bounds, state_type, resizing, activated);
  xdg_surface_send_configure(resource, serial);
  wl_client_flush(wl_resource_get_client(resource));
  return serial;
}

AuraToplevel::AuraToplevel(ShellSurface* shell_surface,
                           SerialTracker* const serial_tracker,
                           wl_resource* xdg_toplevel_resource,
                           wl_resource* aura_toplevel_resource)
    : shell_surface_(shell_surface),
      serial_tracker_(serial_tracker),
      xdg_toplevel_resource_(xdg_toplevel_resource),
      aura_toplevel_resource_(aura_toplevel_resource) {
  DCHECK(shell_surface);
}

AuraToplevel::~AuraToplevel() = default;

void AuraToplevel::SetOrientationLock(uint32_t lock_type) {
  shell_surface_->SetOrientationLock(OrientationLock(lock_type));
}

void AuraToplevel::SetClientSubmitsSurfacesInPixelCoordinates(bool enable) {
  shell_surface_->set_client_submits_surfaces_in_pixel_coordinates(enable);
}

void AuraToplevel::SetWindowBounds(int32_t x,
                                   int32_t y,
                                   int32_t width,
                                   int32_t height) {
  if (!shell_surface_->IsDragged())
    shell_surface_->SetWindowBounds(gfx::Rect(x, y, width, height));
}

void AuraToplevel::OnOriginChange(const gfx::Point& origin) {
  zaura_toplevel_send_origin_change(aura_toplevel_resource_, origin.x(),
                                    origin.y());
  wl_client_flush(wl_resource_get_client(aura_toplevel_resource_));
}

void AuraToplevel::SetClientUsesScreenCoordinates() {
  supports_window_bounds_ = true;
  shell_surface_->set_client_supports_window_bounds(true);
  shell_surface_->set_configure_callback(
      base::BindRepeating(&HandleAuraSurfaceConfigureCallback,
                          xdg_toplevel_resource_, serial_tracker_,
                          base::BindRepeating(&AuraToplevel::OnConfigure,
                                              weak_ptr_factory_.GetWeakPtr())));
  shell_surface_->set_origin_change_callback(base::BindRepeating(
      &AuraToplevel::OnOriginChange, weak_ptr_factory_.GetWeakPtr()));
}

void AddState(wl_array* states, xdg_toplevel_state state) {
  xdg_toplevel_state* value = static_cast<xdg_toplevel_state*>(
      wl_array_add(states, sizeof(xdg_toplevel_state)));
  DCHECK(value);
  *value = state;
}

void AuraToplevel::OnConfigure(const gfx::Rect& bounds,
                               chromeos::WindowStateType state_type,
                               bool resizing,
                               bool activated) {
  wl_array states;
  wl_array_init(&states);
  if (state_type == chromeos::WindowStateType::kMaximized)
    AddState(&states, XDG_TOPLEVEL_STATE_MAXIMIZED);
  // TODO(crbug/1250129): Pinned states need to be handled properly.
  // TODO(crbug/1250129): Support snapped state.
  if (IsFullscreenOrPinnedWindowStateType(state_type)) {
    AddState(&states, XDG_TOPLEVEL_STATE_FULLSCREEN);
  }
  if (resizing)
    AddState(&states, XDG_TOPLEVEL_STATE_RESIZING);
  if (activated)
    AddState(&states, XDG_TOPLEVEL_STATE_ACTIVATED);

  zaura_toplevel_send_configure(aura_toplevel_resource_, bounds.x(), bounds.y(),
                                bounds.width(), bounds.height(), &states);
  wl_array_release(&states);
}

AuraPopup::AuraPopup(ShellSurfaceBase* shell_surface)
    : shell_surface_(shell_surface) {
  DCHECK(shell_surface);
}

AuraPopup::~AuraPopup() = default;

void AuraPopup::SetClientSubmitsSurfacesInPixelCoordinates(bool enable) {
  shell_surface_->set_client_submits_surfaces_in_pixel_coordinates(enable);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

////////////////////////////////////////////////////////////////////////////////
// aura_output_interface:

class AuraOutput : public WaylandDisplayObserver {
 public:
  explicit AuraOutput(wl_resource* resource) : resource_(resource) {}

  AuraOutput(const AuraOutput&) = delete;
  AuraOutput& operator=(const AuraOutput&) = delete;

  // Overridden from WaylandDisplayObserver:
  bool SendDisplayMetrics(const display::Display& display,
                          uint32_t changed_metrics) override {
    if (!(changed_metrics &
          (display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
           display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
           display::DisplayObserver::DISPLAY_METRIC_ROTATION))) {
      return false;
    }

    const WMHelper* wm_helper = WMHelper::GetInstance();
    const display::ManagedDisplayInfo& display_info =
        wm_helper->GetDisplayInfo(display.id());

    if (wl_resource_get_version(resource_) >=
        ZAURA_OUTPUT_SCALE_SINCE_VERSION) {
      display::ManagedDisplayMode active_mode;
      bool rv =
          wm_helper->GetActiveModeForDisplayId(display.id(), &active_mode);
      DCHECK(rv);
      const int32_t current_output_scale =
          std::round(display_info.zoom_factor() * 1000.f);
      std::vector<float> zoom_factors =
          display::GetDisplayZoomFactors(active_mode);

      // Ensure that the current zoom factor is a part of the list.
      auto it = std::find_if(
          zoom_factors.begin(), zoom_factors.end(),
          [&display_info](float zoom_factor) -> bool {
            return std::abs(display_info.zoom_factor() - zoom_factor) <=
                   std::numeric_limits<float>::epsilon();
          });
      if (it == zoom_factors.end())
        zoom_factors.push_back(display_info.zoom_factor());

      for (float zoom_factor : zoom_factors) {
        int32_t output_scale = std::round(zoom_factor * 1000.f);
        uint32_t flags = 0;
        if (output_scale == 1000)
          flags |= ZAURA_OUTPUT_SCALE_PROPERTY_PREFERRED;
        if (current_output_scale == output_scale)
          flags |= ZAURA_OUTPUT_SCALE_PROPERTY_CURRENT;

        // TODO(malaykeshav): This can be removed in the future when client
        // has been updated.
        if (wl_resource_get_version(resource_) < 6)
          output_scale = std::round(1000.f / zoom_factor);

        zaura_output_send_scale(resource_, flags, output_scale);
      }
    }

    if (wl_resource_get_version(resource_) >=
        ZAURA_OUTPUT_CONNECTION_SINCE_VERSION) {
      zaura_output_send_connection(resource_,
                                   display.IsInternal()
                                       ? ZAURA_OUTPUT_CONNECTION_TYPE_INTERNAL
                                       : ZAURA_OUTPUT_CONNECTION_TYPE_UNKNOWN);
    }

    if (wl_resource_get_version(resource_) >=
        ZAURA_OUTPUT_DEVICE_SCALE_FACTOR_SINCE_VERSION) {
      zaura_output_send_device_scale_factor(
          resource_, display_info.device_scale_factor() * 1000);
    }

    return true;
  }

 private:
  wl_resource* const resource_;
};

////////////////////////////////////////////////////////////////////////////////
// aura_shell_interface:

#if BUILDFLAG(IS_CHROMEOS_ASH)

// IDs of bugs that have been fixed in the exo implementation. These are
// propagated to clients on aura_shell bind and can be used to gate client
// logic on the presence of certain fixes.
const uint32_t kFixedBugIds[] = {
    1151508,  // Do not remove, used for sanity checks by
              // |wayland_simple_client|
};

// Implements aura shell interface and monitors workspace state needed
// for the aura shell interface.
class WaylandAuraShell : public ash::DesksController::Observer,
                         public ash::TabletModeObserver,
                         public SeatObserver {
 public:
  WaylandAuraShell(wl_resource* aura_shell_resource, Display* display)
      : aura_shell_resource_(aura_shell_resource), seat_(display->seat()) {
    WMHelperChromeOS* helper = WMHelperChromeOS::GetInstance();
    helper->AddTabletModeObserver(this);
    ash::DesksController::Get()->AddObserver(this);
    if (wl_resource_get_version(aura_shell_resource_) >=
        ZAURA_SHELL_LAYOUT_MODE_SINCE_VERSION) {
      auto layout_mode = helper->InTabletMode()
                             ? ZAURA_SHELL_LAYOUT_MODE_TABLET
                             : ZAURA_SHELL_LAYOUT_MODE_WINDOWED;
      zaura_shell_send_layout_mode(aura_shell_resource_, layout_mode);
    }
    if (wl_resource_get_version(aura_shell_resource_) >=
        ZAURA_SHELL_BUG_FIX_SINCE_VERSION) {
      for (uint32_t bug_id : kFixedBugIds) {
        zaura_shell_send_bug_fix(aura_shell_resource_, bug_id);
      }
    }
    display->seat()->AddObserver(this, kAuraShellSeatObserverPriority);

    OnDesksChanged();
    OnDeskActivationChanged();
  }
  WaylandAuraShell(const WaylandAuraShell&) = delete;
  WaylandAuraShell& operator=(const WaylandAuraShell&) = delete;
  ~WaylandAuraShell() override {
    WMHelperChromeOS* helper = WMHelperChromeOS::GetInstance();
    helper->RemoveTabletModeObserver(this);
    ash::DesksController::Get()->RemoveObserver(this);
    if (seat_)
      seat_->RemoveObserver(this);
  }

  // Overridden from ash::TabletModeObserver:
  void OnTabletModeStarted() override {
    if (wl_resource_get_version(aura_shell_resource_) >=
        ZAURA_SHELL_LAYOUT_MODE_SINCE_VERSION)
      zaura_shell_send_layout_mode(aura_shell_resource_,
                                   ZAURA_SHELL_LAYOUT_MODE_TABLET);
  }
  void OnTabletModeEnding() override {
    if (wl_resource_get_version(aura_shell_resource_) >=
        ZAURA_SHELL_LAYOUT_MODE_SINCE_VERSION)
      zaura_shell_send_layout_mode(aura_shell_resource_,
                                   ZAURA_SHELL_LAYOUT_MODE_WINDOWED);
  }
  void OnTabletModeEnded() override {}

  // Overridden from SeatObserver:
  void OnSurfaceFocused(Surface* gained_focus,
                        Surface* lost_focus,
                        bool has_focused_surface) override {
    FocusedSurfaceChanged(gained_focus, lost_focus, has_focused_surface);
  }

  // ash::DesksController::Observer:
  void OnDeskAdded(const ash::Desk* desk) override { OnDesksChanged(); }
  void OnDeskRemoved(const ash::Desk* desk) override { OnDesksChanged(); }
  void OnDeskReordered(int old_index, int new_index) override {
    OnDesksChanged();
  }
  void OnDeskActivationChanged(const ash::Desk* activated,
                               const ash::Desk* deactivated) override {
    OnDeskActivationChanged();
  }
  void OnDeskSwitchAnimationLaunching() override {}
  void OnDeskSwitchAnimationFinished() override {}
  void OnDeskNameChanged(const ash::Desk* desk,
                         const std::u16string& new_name) override {
    OnDesksChanged();
  }

 private:
  void OnDesksChanged() {
    if (wl_resource_get_version(aura_shell_resource_) <
        ZAURA_SHELL_DESKS_CHANGED_SINCE_VERSION) {
      return;
    }

    wl_array desk_names;
    wl_array_init(&desk_names);

    for (const auto& desk : ash::DesksController::Get()->desks()) {
      std::string name = base::UTF16ToUTF8(desk->name());
      char* desk_name =
          static_cast<char*>(wl_array_add(&desk_names, name.size() + 1));
      strcpy(desk_name, name.c_str());
    }

    zaura_shell_send_desks_changed(aura_shell_resource_, &desk_names);
    wl_array_release(&desk_names);
  }

  void OnDeskActivationChanged() {
    if (wl_resource_get_version(aura_shell_resource_) <
        ZAURA_SHELL_DESK_ACTIVATION_CHANGED_SINCE_VERSION) {
      return;
    }

    zaura_shell_send_desk_activation_changed(
        aura_shell_resource_,
        ash::DesksController::Get()->GetActiveDeskIndex());
  }

  void FocusedSurfaceChanged(Surface* gained_active_surface,
                             Surface* lost_active_surface,
                             bool has_focused_client) {
    if (wl_resource_get_version(aura_shell_resource_) <
        ZAURA_SHELL_ACTIVATED_SINCE_VERSION)
      return;
    if (gained_active_surface == lost_active_surface &&
        last_has_focused_client_ == has_focused_client)
      return;
    last_has_focused_client_ = has_focused_client;

    wl_resource* gained_active_surface_resource =
        gained_active_surface ? GetSurfaceResource(gained_active_surface)
                              : nullptr;
    wl_resource* lost_active_surface_resource =
        lost_active_surface ? GetSurfaceResource(lost_active_surface) : nullptr;

    wl_client* client = wl_resource_get_client(aura_shell_resource_);

    // If surface that gained active is not owned by the aura shell then
    // set to null.
    if (gained_active_surface_resource &&
        wl_resource_get_client(gained_active_surface_resource) != client) {
      gained_active_surface_resource = nullptr;
    }

    // If surface that lost active is not owned by the aura shell then set
    // to null.
    if (lost_active_surface_resource &&
        wl_resource_get_client(lost_active_surface_resource) != client) {
      lost_active_surface_resource = nullptr;
    }

    zaura_shell_send_activated(aura_shell_resource_,
                               gained_active_surface_resource,
                               lost_active_surface_resource);

    wl_client_flush(client);
  }

  // The aura shell resource associated with observer.
  wl_resource* const aura_shell_resource_;
  Seat* const seat_;

  bool last_has_focused_client_ = false;

  base::WeakPtrFactory<WaylandAuraShell> weak_ptr_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
// aura_toplevel_interface:

void aura_toplevel_set_orientation_lock(wl_client* client,
                                        wl_resource* resource,
                                        uint32_t orientation_lock) {
  GetUserDataAs<AuraToplevel>(resource)->SetOrientationLock(orientation_lock);
}

void aura_toplevel_set_client_supports_window_bounds(wl_client* client,
                                                     wl_resource* resource) {
  GetUserDataAs<AuraToplevel>(resource)->SetClientUsesScreenCoordinates();
}

void aura_toplevel_surface_submission_in_pixel_coordinates(
    wl_client* client,
    wl_resource* resource) {
  GetUserDataAs<AuraToplevel>(resource)
      ->SetClientSubmitsSurfacesInPixelCoordinates(true);
}

void aura_toplevel_set_window_bounds(wl_client* client,
                                     wl_resource* resource,
                                     int32_t x,
                                     int32_t y,
                                     int32_t width,
                                     int32_t height,
                                     wl_resource* output) {
  // TODO(crbug.com/1261321): Use output hint.
  GetUserDataAs<AuraToplevel>(resource)->SetWindowBounds(x, y, width, height);
}

const struct zaura_toplevel_interface aura_toplevel_implementation = {
    aura_toplevel_set_orientation_lock,
    aura_toplevel_surface_submission_in_pixel_coordinates,
    aura_toplevel_set_client_supports_window_bounds,
    aura_toplevel_set_window_bounds,
};

void aura_popup_surface_submission_in_pixel_coordinates(wl_client* client,
                                                        wl_resource* resource) {
  GetUserDataAs<AuraPopup>(resource)
      ->SetClientSubmitsSurfacesInPixelCoordinates(true);
}

const struct zaura_popup_interface aura_popup_implementation = {
    aura_popup_surface_submission_in_pixel_coordinates,
};

void aura_shell_get_aura_toplevel(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  wl_resource* xdg_toplevel_resource) {
  ShellSurfaceData shell_surface_data =
      GetShellSurfaceFromToplevelResource(xdg_toplevel_resource);
  wl_resource* aura_toplevel_resource = wl_resource_create(
      client, &zaura_toplevel_interface, wl_resource_get_version(resource), id);

  SetImplementation(
      aura_toplevel_resource, &aura_toplevel_implementation,
      std::make_unique<AuraToplevel>(
          shell_surface_data.shell_surface, shell_surface_data.serial_tracker,
          shell_surface_data.surface_resource, aura_toplevel_resource));
}

void aura_shell_get_aura_popup(wl_client* client,
                               wl_resource* resource,
                               uint32_t id,
                               wl_resource* surface_resource) {
  wl_resource* aura_popup_resource = wl_resource_create(
      client, &zaura_popup_interface, wl_resource_get_version(resource), id);

  ShellSurfaceBase* shell_surface =
      GetShellSurfaceFromPopupResource(surface_resource);

  SetImplementation(aura_popup_resource, &aura_popup_implementation,
                    std::make_unique<AuraPopup>(shell_surface));
}

#else
void aura_shell_get_aura_toplevel(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  wl_resource* surface_resource) {
  NOTREACHED();
}

void aura_shell_get_aura_popup(wl_client* client,
                               wl_resource* resource,
                               uint32_t id,
                               wl_resource* surface_resource) {
  NOTREACHED();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH))

void aura_shell_get_aura_surface(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t id,
                                 wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasAuraSurfaceKey)) {
    wl_resource_post_error(
        resource, ZAURA_SHELL_ERROR_AURA_SURFACE_EXISTS,
        "an aura surface object for that surface already exists");
    return;
  }

  wl_resource* aura_surface_resource = wl_resource_create(
      client, &zaura_surface_interface, wl_resource_get_version(resource), id);

  SetImplementation(
      aura_surface_resource, &aura_surface_implementation,
      std::make_unique<AuraSurface>(surface, aura_surface_resource));
}

void aura_shell_get_aura_output(wl_client* client,
                                wl_resource* resource,
                                uint32_t id,
                                wl_resource* output_resource) {
  WaylandDisplayHandler* display_handler =
      GetUserDataAs<WaylandDisplayHandler>(output_resource);

  wl_resource* aura_output_resource = wl_resource_create(
      client, &zaura_output_interface, wl_resource_get_version(resource), id);

  auto aura_output = std::make_unique<AuraOutput>(aura_output_resource);
  display_handler->AddObserver(aura_output.get());

  SetImplementation(aura_output_resource, nullptr, std::move(aura_output));
}

void aura_shell_surface_submission_in_pixel_coordinates(wl_client* client,
                                                        wl_resource* resource) {
  LOG(WARNING) << "Deprecated. The server doesn't support this request.";
}

const struct zaura_shell_interface aura_shell_implementation = {
    aura_shell_get_aura_surface,
    aura_shell_get_aura_output,
    aura_shell_surface_submission_in_pixel_coordinates,
    aura_shell_get_aura_toplevel,
    aura_shell_get_aura_popup,
};
}  // namespace

void bind_aura_shell(wl_client* client,
                     void* data,
                     uint32_t version,
                     uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zaura_shell_interface,
                         std::min(version, kZAuraShellVersion), id);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  Display* display = static_cast<Display*>(data);
  SetImplementation(resource, &aura_shell_implementation,
                    std::make_unique<WaylandAuraShell>(resource, display));
#else
  wl_resource_set_implementation(resource, &aura_shell_implementation, nullptr,
                                 nullptr);
#endif
}

}  // namespace wayland
}  // namespace exo
