// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zaura_shell.h"

#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <xdg-shell-server-protocol.h>

#include <limits>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/display/display_util.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/focus_cycler.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/window_state.h"
#include "base/bit_cast.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "components/exo/display.h"
#include "components/exo/seat.h"
#include "components/exo/seat_observer.h"
#include "components/exo/shell_surface.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/wayland/output_metrics.h"
#include "components/exo/wayland/serial_tracker.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_display_observer.h"
#include "components/exo/wayland/wl_output.h"
#include "components/exo/wayland/xdg_shell.h"
#include "components/exo/wayland/zaura_output_manager.h"
#include "components/version_info/version_info.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/base/wayland/wayland_display_util.h"
#include "ui/compositor/layer.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/tooltip_client.h"

namespace exo::wayland {

namespace {

constexpr int kAuraShellSeatObserverPriority = 1;
static_assert(Seat::IsValidObserverPriority(kAuraShellSeatObserverPriority),
              "kAuraShellSeatObserverPriority is not in the valid range.");
static_assert(sizeof(uint32_t) == sizeof(float),
              "Sizes much match for reinterpret cast to be meaningful");

// A property key containing a boolean set to true if na aura surface object is
// associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasAuraSurfaceKey, false)

bool TransformRelativeToScreenIsAxisAligned(aura::Window* window) {
  gfx::Transform transform_relative_to_screen;
  DCHECK(window->layer()->GetTargetTransformRelativeTo(
      window->GetRootWindow()->layer(), &transform_relative_to_screen));
  transform_relative_to_screen.PostConcat(
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

void aura_surface_set_fullscreen_mode_deprecated(wl_client* client,
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

void aura_surface_intent_to_snap_deprecated(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t snap_direction) {
  GetUserDataAs<AuraSurface>(resource)->IntentToSnap(snap_direction);
}

void aura_surface_set_snap_left_deprecated(wl_client* client,
                                           wl_resource* resource) {
  GetUserDataAs<AuraSurface>(resource)->SetSnapPrimary();
}

void aura_surface_set_snap_right_deprecated(wl_client* client,
                                            wl_resource* resource) {
  GetUserDataAs<AuraSurface>(resource)->SetSnapSecondary();
}

void aura_surface_unset_snap_deprecated(wl_client* client,
                                        wl_resource* resource) {
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

void aura_surface_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void aura_surface_show_tooltip(wl_client* client,
                               wl_resource* resource,
                               const char* text,
                               int32_t x,
                               int32_t y,
                               uint32_t trigger,
                               uint32_t show_delay,
                               uint32_t hide_delay) {
  GetUserDataAs<AuraSurface>(resource)->ShowTooltip(
      text, gfx::Point(x, y), trigger, base::Milliseconds((uint64_t)show_delay),
      base::Milliseconds((uint64_t)hide_delay));
}

void aura_surface_hide_tooltip(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraSurface>(resource)->HideTooltip();
}

void aura_surface_set_accessibility_id(wl_client* client,
                                       wl_resource* resource,
                                       int32_t id) {
  GetUserDataAs<AuraSurface>(resource)->SetAccessibilityId(id);
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
    aura_surface_set_fullscreen_mode_deprecated,
    aura_surface_set_client_surface_str_id,
    aura_surface_set_server_start_resize,
    aura_surface_intent_to_snap_deprecated,
    aura_surface_set_snap_left_deprecated,
    aura_surface_set_snap_right_deprecated,
    aura_surface_unset_snap_deprecated,
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
    aura_surface_release,
    aura_surface_show_tooltip,
    aura_surface_hide_tooltip,
    aura_surface_set_accessibility_id,
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
    wm::SetTooltipText(surface_->window(), nullptr);
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
    surface_->SetParent(parent ? parent->surface_.get() : nullptr, position);
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
  surface_->SetSnapPrimary(chromeos::kDefaultSnapRatio);
}

void AuraSurface::SetSnapSecondary() {
  surface_->SetSnapSecondary(chromeos::kDefaultSnapRatio);
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

  // Should re-write in locked case - we don't want to trigger PIP upon
  // locking the screen.
  if (ash::Shell::Get()->session_controller()->IsScreenLocked()) {
    SendOcclusionFraction(0.0f);
    return;
  }

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

      // Clip the area outside of the display.
      gfx::Rect area_inside_display = bounds_in_screen;
      area_inside_display.Intersect(display_bounds_in_screen);
      int occluded_area = tracked_area - area_inside_display.width() *
                                             area_inside_display.height();

      // We already marked the area outside the display as occluded, so
      // intersect the occluded region with the region of the window which is
      // inside the display.
      SkRegion tracked_and_occluded_region = occluded_region;
      tracked_and_occluded_region.op(gfx::RectToSkIRect(area_inside_display),
                                     SkRegion::Op::kIntersect_Op);
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

void AuraSurface::OnTooltipShown(Surface* surface,
                                 const std::u16string& text,
                                 const gfx::Rect& bounds) {
  if (wl_resource_get_version(resource_) <
      ZAURA_SURFACE_TOOLTIP_SHOWN_SINCE_VERSION) {
    return;
  }
  zaura_surface_send_tooltip_shown(resource_, base::UTF16ToUTF8(text).c_str(),
                                   bounds.x(), bounds.y(), bounds.width(),
                                   bounds.height());
}

void AuraSurface::OnTooltipHidden(Surface* surface) {
  if (wl_resource_get_version(resource_) <
      ZAURA_SURFACE_TOOLTIP_HIDDEN_SINCE_VERSION) {
    return;
  }
  zaura_surface_send_tooltip_hidden(resource_);
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

void AuraSurface::ShowTooltip(const char* text,
                              const gfx::Point& position,
                              uint32_t trigger,
                              const base::TimeDelta& show_delay,
                              const base::TimeDelta& hide_delay) {
  tooltip_text_ = base::UTF8ToUTF16(text);
  auto* window = surface_->window();
  wm::SetTooltipText(window, &tooltip_text_);
  wm::SetTooltipId(window, surface_);

  auto* tooltip_controller = ash::Shell::Get()->tooltip_controller();
  tooltip_controller->SetShowTooltipDelay(window, show_delay);
  tooltip_controller->SetHideTooltipTimeout(window, hide_delay);

  switch (trigger) {
    case ZAURA_SURFACE_TOOLTIP_TRIGGER_CURSOR:
      tooltip_controller->UpdateTooltip(window);
      break;
    case ZAURA_SURFACE_TOOLTIP_TRIGGER_KEYBOARD:
      tooltip_controller->UpdateTooltipFromKeyboardWithAnchorPoint(position,
                                                                   window);
      break;
    default:
      VLOG(2) << "Unknown aura-shell tooltip trigger type: " << trigger;
      tooltip_controller->UpdateTooltip(window);
  }
}

void AuraSurface::HideTooltip() {
  tooltip_text_ = std::u16string();
  auto* window = surface_->window();
  ash::Shell::Get()->tooltip_controller()->UpdateTooltip(window);
}

void AuraSurface::SetAccessibilityId(int id) {
  surface_->SetClientAccessibilityId(id);
}

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

using AuraSurfaceConfigureCallback = base::RepeatingCallback<void(
    const gfx::Rect& bounds,
    chromeos::WindowStateType state_type,
    bool resizing,
    bool activated,
    float raster_scale,
    aura::Window::OcclusionState occlusion_state,
    std::optional<chromeos::WindowStateType> restore_state_type)>;

uint32_t HandleAuraSurfaceConfigureCallback(
    wl_resource* resource,
    SerialTracker* serial_tracker,
    const AuraSurfaceConfigureCallback& callback,
    const gfx::Rect& bounds,
    chromeos::WindowStateType state_type,
    bool resizing,
    bool activated,
    const gfx::Vector2d& origin_offset,
    float raster_scale,
    aura::Window::OcclusionState occlusion_state,
    std::optional<chromeos::WindowStateType> restore_state_type) {
  uint32_t serial =
      serial_tracker->GetNextSerial(SerialTracker::EventType::OTHER_EVENT);
  callback.Run(bounds, state_type, resizing, activated, raster_scale,
               occlusion_state, restore_state_type);
  xdg_surface_send_configure(resource, serial);
  wl_client_flush(wl_resource_get_client(resource));
  return serial;
}

using AuraSurfaceRotateFocusCallback = base::RepeatingCallback<
    void(uint32_t serial, ash::FocusCycler::Direction direction, bool restart)>;

uint32_t HandleAuraSurfaceRotateFocusCallback(
    SerialTracker* serial_tracker,
    AuraSurfaceRotateFocusCallback callback,
    ash::FocusCycler::Direction direction,
    bool restart) {
  auto serial =
      serial_tracker->GetNextSerial(SerialTracker::EventType::OTHER_EVENT);
  callback.Run(serial, direction, restart);
  return serial;
}

AuraToplevel::AuraToplevel(ShellSurface* shell_surface,
                           SerialTracker* const serial_tracker,
                           SerialTracker* const rotation_serial_tracker,
                           wl_resource* xdg_toplevel_resource,
                           wl_resource* aura_toplevel_resource)
    : shell_surface_(shell_surface),
      serial_tracker_(serial_tracker),
      rotation_serial_tracker_(rotation_serial_tracker),
      xdg_toplevel_resource_(xdg_toplevel_resource),
      aura_toplevel_resource_(aura_toplevel_resource) {
  DCHECK(shell_surface);
}

AuraToplevel::~AuraToplevel() = default;

void AuraToplevel::OnRotatePaneFocus(uint32_t serial,
                                     ash::FocusCycler::Direction direction,
                                     bool restart) {
  auto zaura_direction = direction == ash::FocusCycler::Direction::FORWARD
                             ? ZAURA_TOPLEVEL_ROTATE_DIRECTION_FORWARD
                             : ZAURA_TOPLEVEL_ROTATE_DIRECTION_BACKWARD;
  zaura_toplevel_send_rotate_focus(
      aura_toplevel_resource_, serial, zaura_direction,
      restart ? ZAURA_TOPLEVEL_ROTATE_RESTART_STATE_RESTART
              : ZAURA_TOPLEVEL_ROTATE_RESTART_STATE_NO_RESTART);
  wl_client_flush(wl_resource_get_client(aura_toplevel_resource_));
}

void AuraToplevel::SetOrientationLock(uint32_t lock_type) {
  shell_surface_->SetOrientationLock(OrientationLock(lock_type));
}

void AuraToplevel::SetWindowCornersRadii(const gfx::RoundedCornersF& radii) {
  shell_surface_->SetWindowCornersRadii(radii);
}

void AuraToplevel::SetClientSubmitsSurfacesInPixelCoordinates(bool enable) {
  shell_surface_->set_client_submits_surfaces_in_pixel_coordinates(enable);
}

void AuraToplevel::SetWindowBounds(int32_t x,
                                   int32_t y,
                                   int32_t width,
                                   int32_t height,
                                   int64_t display_id) {
  if (!shell_surface_->IsDragged()) {
    if (display_id != display::kInvalidDisplayId) {
      shell_surface_->SetDisplay(display_id);
    }
    shell_surface_->SetWindowBounds(gfx::Rect(x, y, width, height));
  }
}

void AuraToplevel::SetRestoreInfo(int32_t restore_session_id,
                                  int32_t restore_window_id) {
  shell_surface_->SetRestoreInfo(restore_session_id, restore_window_id);
}

void AuraToplevel::SetRestoreInfoWithWindowIdSource(
    int32_t restore_session_id,
    const std::string& restore_window_id_source) {
  shell_surface_->SetRestoreInfoWithWindowIdSource(restore_session_id,
                                                   restore_window_id_source);
}

void AuraToplevel::OnOriginChange(const gfx::Point& origin) {
  zaura_toplevel_send_origin_change(aura_toplevel_resource_, origin.x(),
                                    origin.y());
  wl_client_flush(wl_resource_get_client(aura_toplevel_resource_));
}

void AuraToplevel::OnOverviewChange(bool in_overview) {
  zaura_toplevel_send_overview_change(
      aura_toplevel_resource_,
      in_overview ? ZAURA_TOPLEVEL_IN_OVERVIEW_IN_OVERVIEW
                  : ZAURA_TOPLEVEL_IN_OVERVIEW_NOT_IN_OVERVIEW);
  wl_client_flush(wl_resource_get_client(aura_toplevel_resource_));
}

void AuraToplevel::SetDecoration(SurfaceFrameType type) {
  shell_surface_->OnSetFrame(type);
}

void AuraToplevel::SetZOrder(ui::ZOrderLevel z_order) {
  shell_surface_->SetZOrder(z_order);
}

void AuraToplevel::Activate() {
  shell_surface_->RequestActivation();
}

void AuraToplevel::Deactivate() {
  shell_surface_->RequestDeactivation();
}

bool IsImmersive(uint32_t mode) {
  switch (mode) {
    case ZAURA_TOPLEVEL_FULLSCREEN_MODE_PLAIN:
      return false;
    case ZAURA_TOPLEVEL_FULLSCREEN_MODE_IMMERSIVE:
      return true;
    default:
      VLOG(2) << "Unknown immersive mode: " << mode;
      return false;
  }
}

void AuraToplevel::SetFullscreenMode(uint32_t mode) {
  shell_surface_->SetUseImmersiveForFullscreen(IsImmersive(mode));
}

void AuraToplevel::SetScaleFactor(float scale_factor) {
  shell_surface_->SetScaleFactor(scale_factor);
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
  if (wl_resource_get_version(aura_toplevel_resource_) >=
      ZAURA_TOPLEVEL_ROTATE_FOCUS_SINCE_VERSION) {
    shell_surface_->set_rotate_focus_callback(base::BindRepeating(
        HandleAuraSurfaceRotateFocusCallback, rotation_serial_tracker_,
        base::BindRepeating(&AuraToplevel::OnRotatePaneFocus,
                            weak_ptr_factory_.GetWeakPtr())));
  }
  if (wl_resource_get_version(aura_toplevel_resource_) >=
      ZAURA_TOPLEVEL_OVERVIEW_CHANGE_SINCE_VERSION) {
    shell_surface_->set_overview_change_callback(
        base::BindRepeating(base::BindRepeating(
            &AuraToplevel::OnOverviewChange, weak_ptr_factory_.GetWeakPtr())));
  }
}

void AuraToplevel::SetSystemModal(bool modal) {
  shell_surface_->SetSystemModal(modal);
}

void AuraToplevel::SetFloatToLocation(uint32_t location) {
  switch (location) {
    case ZAURA_TOPLEVEL_FLOAT_START_LOCATION_BOTTOM_RIGHT:
      shell_surface_->SetFloatToLocation(
          chromeos::FloatStartLocation::kBottomRight);
      break;
    case ZAURA_TOPLEVEL_FLOAT_START_LOCATION_BOTTOM_LEFT:
      shell_surface_->SetFloatToLocation(
          chromeos::FloatStartLocation::kBottomLeft);
      break;
    default:
      VLOG(2) << "aura_toplevel_set_float_to_location(): unknown "
                 "float_start_location: "
              << location;
      break;
  }
}

void AuraToplevel::UnsetFloat() {
  shell_surface_->UnsetFloat();
}

void AuraToplevel::SetSnapPrimary(float snap_ratio) {
  shell_surface_->SetSnapPrimary(snap_ratio);
}

void AuraToplevel::SetSnapSecondary(float snap_ratio) {
  shell_surface_->SetSnapSecondary(snap_ratio);
}

void AuraToplevel::SetPersistable(bool persistable) {
  shell_surface_->SetPersistable(persistable);
}

void AuraToplevel::SetShape(std::optional<cc::Region> shape) {
  shell_surface_->SetShape(std::move(shape));
}

void AuraToplevel::AckRotateFocus(uint32_t serial, uint32_t h) {
  auto handled = h == ZAURA_TOPLEVEL_ROTATE_HANDLED_STATE_HANDLED;
  shell_surface_->AckRotateFocus(serial, handled);
}

void AuraToplevel::SetCanMaximize(bool can_maximize) {
  shell_surface_->SetCanMaximize(can_maximize);
}

void AuraToplevel::SetCanFullscreen(bool can_fullscreen) {
  shell_surface_->SetCanFullscreen(can_fullscreen);
}

void AuraToplevel::SetShadowCornersRadii(const gfx::RoundedCornersF& radii) {
  shell_surface_->SetShadowCornersRadii(radii);
}

void AuraToplevel::IntentToSnap(uint32_t snap_direction) {
  switch (snap_direction) {
    case ZAURA_SURFACE_SNAP_DIRECTION_NONE:
      shell_surface_->HideSnapPreview();
      break;
    case ZAURA_SURFACE_SNAP_DIRECTION_LEFT:
      shell_surface_->ShowSnapPreviewToPrimary();
      break;
    case ZAURA_SURFACE_SNAP_DIRECTION_RIGHT:
      shell_surface_->ShowSnapPreviewToSecondary();
      break;
  }
}

void AuraToplevel::UnsetSnap() {
  shell_surface_->UnsetSnap();
}

void AuraToplevel::SetTopInset(int top_inset) {
  shell_surface_->SetTopInset(top_inset);
}

template <class T>
void AddState(wl_array* states, T state) {
  T* value = static_cast<T*>(wl_array_add(states, sizeof(T)));
  DCHECK(value);
  *value = state;
}

void AuraToplevel::OnConfigure(
    const gfx::Rect& bounds,
    chromeos::WindowStateType state_type,
    bool resizing,
    bool activated,
    float raster_scale,
    aura::Window::OcclusionState occlusion_state,
    std::optional<chromeos::WindowStateType> restore_state_type) {
  wl_array states;
  wl_array_init(&states);
  if (state_type == chromeos::WindowStateType::kMaximized)
    AddState(&states, XDG_TOPLEVEL_STATE_MAXIMIZED);
  // TODO(crbug.com/40197882): Support snapped state.
  if (IsFullscreenOrPinnedWindowStateType(state_type)) {
    // If pinned state is not yet supported, always set fullscreen.
    if (wl_resource_get_version(aura_toplevel_resource_) <
        ZAURA_TOPLEVEL_STATE_TRUSTED_PINNED_SINCE_VERSION) {
      AddState(&states, XDG_TOPLEVEL_STATE_FULLSCREEN);
    } else if (state_type == chromeos::WindowStateType::kFullscreen) {
      AddState(&states, XDG_TOPLEVEL_STATE_FULLSCREEN);
    } else if (state_type == chromeos::WindowStateType::kPinned) {
      AddState(&states, ZAURA_TOPLEVEL_STATE_PINNED);
    } else if (state_type == chromeos::WindowStateType::kTrustedPinned) {
      AddState(&states, ZAURA_TOPLEVEL_STATE_TRUSTED_PINNED);
    }

    if (shell_surface_->GetWidget() &&
        shell_surface_->GetWidget()->GetNativeWindow()->GetProperty(
            chromeos::kImmersiveImpliedByFullscreen)) {
      // Imemrsive state should NOT be set for pinned state.
      // TODO(crbug.com/41483774): Lacros randomly enters/exits immersive state
      // when transitioning to pinned/unpinned state. Add CHECK to guarantee
      // `state_type` is as same as chrome::WindowStateType::kFullscreen here
      // after resolving this bug.

      // TODO(oshima): Immersive should probably be default.
      // Investigate and fix.
      AddState(&states, ZAURA_TOPLEVEL_STATE_IMMERSIVE);
    }
    // If the window was maxmized before it is fullscreened, we should
    // keep this state while it is fullscreened. This is what X11 apps, and
    // thus standard wayland apps expect, and they may rely on this behavior
    // even though this is not explicitly specified in the protocol spec.
    if (restore_state_type.has_value() &&
        restore_state_type.value() == chromeos::WindowStateType::kMaximized) {
      AddState(&states, XDG_TOPLEVEL_STATE_MAXIMIZED);
    }
  }
  if (resizing)
    AddState(&states, XDG_TOPLEVEL_STATE_RESIZING);
  if (activated)
    AddState(&states, XDG_TOPLEVEL_STATE_ACTIVATED);

  if (state_type == chromeos::WindowStateType::kPrimarySnapped)
    AddState(&states, ZAURA_TOPLEVEL_STATE_SNAPPED_PRIMARY);
  if (state_type == chromeos::WindowStateType::kSecondarySnapped)
    AddState(&states, ZAURA_TOPLEVEL_STATE_SNAPPED_SECONDARY);

  if (state_type == chromeos::WindowStateType::kFloated)
    AddState(&states, ZAURA_TOPLEVEL_STATE_FLOATED);

  if (state_type == chromeos::WindowStateType::kMinimized)
    AddState(&states, ZAURA_TOPLEVEL_STATE_MINIMIZED);

  if (state_type == chromeos::WindowStateType::kPip) {
    AddState(&states, ZAURA_TOPLEVEL_STATE_PIP);
  }

  zaura_toplevel_send_configure(aura_toplevel_resource_, bounds.x(), bounds.y(),
                                bounds.width(), bounds.height(), &states);
  wl_array_release(&states);

  if (wl_resource_get_version(aura_toplevel_resource_) >=
      ZAURA_TOPLEVEL_CONFIGURE_RASTER_SCALE_SINCE_VERSION) {
    uint32_t value = base::bit_cast<uint32_t>(raster_scale);
    zaura_toplevel_send_configure_raster_scale(aura_toplevel_resource_, value);
  }

  if (wl_resource_get_version(aura_toplevel_resource_) >=
      ZAURA_TOPLEVEL_CONFIGURE_OCCLUSION_STATE_SINCE_VERSION) {
    zaura_toplevel_send_configure_occlusion_state(
        aura_toplevel_resource_, WaylandOcclusionState(occlusion_state));
  }
}

AuraPopup::AuraPopup(ShellSurfaceBase* shell_surface)
    : shell_surface_(shell_surface) {
  DCHECK(shell_surface);
}

AuraPopup::~AuraPopup() = default;

void AuraPopup::SetClientSubmitsSurfacesInPixelCoordinates(bool enable) {
  shell_surface_->set_client_submits_surfaces_in_pixel_coordinates(enable);
}

void AuraPopup::SetDecoration(SurfaceFrameType type) {
  shell_surface_->OnSetFrame(type);
}

void AuraPopup::SetMenu() {
  shell_surface_->SetMenu();
}

void AuraPopup::SetScaleFactor(float scale_factor) {
  shell_surface_->SetScaleFactor(scale_factor);
}

namespace {

void aura_output_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zaura_output_interface aura_output_implementation = {
    aura_output_release,
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// aura_output_interface:

AuraOutput::AuraOutput(wl_resource* resource,
                       WaylandDisplayHandler* display_handler)
    : resource_(resource), display_handler_(display_handler) {
  display_handler_->AddObserver(this);
}

AuraOutput::~AuraOutput() {
  if (display_handler_)
    display_handler_->RemoveObserver(this);
}

bool AuraOutput::SendDisplayMetrics(const display::Display& display,
                                    uint32_t changed_metrics) {
  if (!(changed_metrics &
        (display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
         display::DisplayObserver::DISPLAY_METRIC_WORK_AREA |
         display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
         display::DisplayObserver::DISPLAY_METRIC_ROTATION))) {
    return false;
  }

  const OutputMetrics output_metrics(display);

  if (wl_resource_get_version(resource_) >=
      ZAURA_OUTPUT_DISPLAY_ID_SINCE_VERSION) {
    zaura_output_send_display_id(resource_, output_metrics.display_id.high,
                                 output_metrics.display_id.low);
  }

  if (wl_resource_get_version(resource_) >= ZAURA_OUTPUT_SCALE_SINCE_VERSION) {
    for (const auto& output_scale : output_metrics.output_scales) {
      zaura_output_send_scale(resource_, output_scale.scale_property,
                              output_scale.scale_factor);
    }
  }

  if (wl_resource_get_version(resource_) >=
      ZAURA_OUTPUT_CONNECTION_SINCE_VERSION) {
    zaura_output_send_connection(resource_, output_metrics.connection_type);
  }

  if (wl_resource_get_version(resource_) >=
      ZAURA_OUTPUT_DEVICE_SCALE_FACTOR_SINCE_VERSION) {
    zaura_output_send_device_scale_factor(
        resource_, output_metrics.device_scale_factor_deprecated);
  }

  if (wl_resource_get_version(resource_) >= ZAURA_OUTPUT_INSETS_SINCE_VERSION) {
    SendInsets(output_metrics.logical_insets);
  }

  if (wl_resource_get_version(resource_) >=
      ZAURA_OUTPUT_LOGICAL_TRANSFORM_SINCE_VERSION) {
    SendLogicalTransform(output_metrics.logical_transform);
  }

  return true;
}

void AuraOutput::OnOutputDestroyed() {
  display_handler_->RemoveObserver(this);
  display_handler_ = nullptr;
}

bool AuraOutput::HasDisplayHandlerForTesting() const {
  return !!display_handler_;
}

void AuraOutput::SendActiveDisplay() {
  if (wl_resource_get_version(resource_) >=
      ZAURA_OUTPUT_ACTIVATED_SINCE_VERSION) {
    zaura_output_send_activated(resource_);
  }
}

void AuraOutput::SendInsets(const gfx::Insets& insets) {
  zaura_output_send_insets(resource_, insets.top(), insets.left(),
                           insets.bottom(), insets.right());
}

void AuraOutput::SendLogicalTransform(int32_t transform) {
  zaura_output_send_logical_transform(resource_, transform);
}

namespace {

////////////////////////////////////////////////////////////////////////////////
// aura_shell_interface:

// IDs of bugs that have been fixed in the exo implementation. These are
// propagated to clients on aura_shell bind and can be used to gate client
// logic on the presence of certain fixes.
const uint32_t kFixedBugIds[] = {
    1151508,  // Do not remove, used for sanity checks by
              // |wayland_simple_client|
    1352584,
    1358908,
    1400226,
    1405471,
};

// Implements aura shell interface and monitors workspace state needed
// for the aura shell interface.
class WaylandAuraShell : public ash::DesksController::Observer,
                         public ash::OverviewObserver,
                         public display::DisplayObserver,
                         public SeatObserver {
 public:
  WaylandAuraShell(wl_resource* aura_shell_resource, Display* display)
      : aura_shell_resource_(aura_shell_resource), seat_(display->seat()) {
    ash::DesksController::Get()->AddObserver(this);
    ash::Shell::Get()->overview_controller()->AddObserver(this);
    display::Screen::GetScreen()->AddObserver(this);
    if (wl_resource_get_version(aura_shell_resource_) >=
        ZAURA_SHELL_LAYOUT_MODE_SINCE_VERSION) {
      auto layout_mode = display::Screen::GetScreen()->InTabletMode()
                             ? ZAURA_SHELL_LAYOUT_MODE_TABLET
                             : ZAURA_SHELL_LAYOUT_MODE_WINDOWED;
      zaura_shell_send_layout_mode(aura_shell_resource_, layout_mode);
    }
    if (wl_resource_get_version(aura_shell_resource_) >=
        ZAURA_SHELL_COMPOSITOR_VERSION_SINCE_VERSION) {
      const std::string_view ash_version = version_info::GetVersionNumber();
      zaura_shell_send_compositor_version(aura_shell_resource_,
                                          ash_version.data());
    }
    if (wl_resource_get_version(aura_shell_resource_) >=
        ZAURA_SHELL_BUG_FIX_SINCE_VERSION) {
      for (uint32_t bug_id : kFixedBugIds) {
        zaura_shell_send_bug_fix(aura_shell_resource_, bug_id);
      }
      if (wl_resource_get_version(aura_shell_resource_) >=
          ZAURA_SHELL_ALL_BUG_FIXES_SENT_SINCE_VERSION) {
        zaura_shell_send_all_bug_fixes_sent(aura_shell_resource_);
      }
      wl_client_flush(wl_resource_get_client(aura_shell_resource_));
    }

    if (wl_resource_get_version(aura_shell_resource_) >=
        ZAURA_SHELL_WINDOW_CORNERS_RADII_SINCE_VERSION) {
      const int window_corner_radius =
          chromeos::features::IsRoundedWindowsEnabled()
              ? chromeos::features::RoundedWindowsRadius()
              : chromeos::kTopCornerRadiusWhenRestored;

      zaura_shell_send_window_corners_radii(
          aura_shell_resource_, window_corner_radius, window_corner_radius,
          chromeos::features::IsRoundedWindowsEnabled() ? window_corner_radius
                                                        : 0,
          chromeos::features::IsRoundedWindowsEnabled() ? window_corner_radius
                                                        : 0);
    }

    display->seat()->AddObserver(this, kAuraShellSeatObserverPriority);

    OnDesksChanged();
    OnDeskActivationChanged();
    OnOverviewModeChanged();
  }
  WaylandAuraShell(const WaylandAuraShell&) = delete;
  WaylandAuraShell& operator=(const WaylandAuraShell&) = delete;
  ~WaylandAuraShell() override {
    display::Screen::GetScreen()->RemoveObserver(this);
    ash::Shell::Get()->overview_controller()->RemoveObserver(this);
    ash::DesksController::Get()->RemoveObserver(this);
    if (seat_)
      seat_->RemoveObserver(this);
  }

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override {
    if (wl_resource_get_version(aura_shell_resource_) >=
        ZAURA_SHELL_LAYOUT_MODE_SINCE_VERSION) {
      if (state == display::TabletState::kInTabletMode) {
        zaura_shell_send_layout_mode(aura_shell_resource_,
                                     ZAURA_SHELL_LAYOUT_MODE_TABLET);
        wl_client_flush(wl_resource_get_client(aura_shell_resource_));
      } else if (state == display::TabletState::kExitingTabletMode) {
        zaura_shell_send_layout_mode(aura_shell_resource_,
                                     ZAURA_SHELL_LAYOUT_MODE_WINDOWED);
        wl_client_flush(wl_resource_get_client(aura_shell_resource_));
      }
    }
  }

  // ash::DesksController::Observer:
  void OnDeskAdded(const ash::Desk* desk, bool from_undo) override {
    OnDesksChanged();
  }
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

  // ash::OverviewObserver:
  void OnOverviewModeStartingAnimationComplete(bool canceled) override {
    if (!canceled) {
      OnOverviewModeChanged();
    }
  }
  void OnOverviewModeEndingAnimationComplete(bool canceled) override {
    if (!canceled) {
      OnOverviewModeChanged();
    }
  }

  // SeatObserver:
  void OnSurfaceFocused(Surface* gained_focus,
                        Surface* lost_focus,
                        bool has_focused_surface) override {
    FocusedSurfaceChanged(gained_focus, lost_focus, has_focused_surface);
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

  void OnOverviewModeChanged() {
    if (wl_resource_get_version(aura_shell_resource_) <
        ZAURA_SHELL_SET_OVERVIEW_MODE_SINCE_VERSION) {
      return;
    }

    const bool in_overview =
        ash::Shell::Get()->overview_controller()->InOverviewSession();
    if (in_overview) {
      zaura_shell_send_set_overview_mode(aura_shell_resource_);
    } else {
      zaura_shell_send_unset_overview_mode(aura_shell_resource_);
    }
  }

  void FocusedSurfaceChanged(Surface* gained_active_surface,
                             Surface* lost_active_surface,
                             bool has_focused_client) {
    if (wl_resource_get_version(aura_shell_resource_) <
        ZAURA_SHELL_ACTIVATED_SINCE_VERSION)
      return;

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

    if (gained_active_surface_resource == lost_active_surface_resource &&
        last_has_focused_client_ == has_focused_client) {
      return;
    }
    last_has_focused_client_ = has_focused_client;

    zaura_shell_send_activated(aura_shell_resource_,
                               gained_active_surface_resource,
                               lost_active_surface_resource);

    wl_client_flush(client);
  }

  // The aura shell resource associated with observer.
  const raw_ptr<wl_resource, DanglingUntriaged> aura_shell_resource_;
  const raw_ptr<Seat> seat_;

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

void aura_toplevel_set_window_corner_radii(wl_client* client,
                                           wl_resource* resource,
                                           uint32_t upper_left_radius,
                                           uint32_t upper_right_radius,
                                           uint32_t lower_right_radius,
                                           uint32_t lower_left_radius) {
  GetUserDataAs<AuraToplevel>(resource)->SetWindowCornersRadii(
      gfx::RoundedCornersF(upper_left_radius, upper_right_radius,
                           lower_right_radius, lower_left_radius));
}

void aura_toplevel_set_shadow_corners_radii(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t upper_left_radius,
                                            uint32_t upper_right_radius,
                                            uint32_t lower_right_radius,
                                            uint32_t lower_left_radius) {
  GetUserDataAs<AuraToplevel>(resource)->SetShadowCornersRadii(
      gfx::RoundedCornersF(upper_left_radius, upper_right_radius,
                           lower_right_radius, lower_left_radius));
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
  auto display_id = AuraOutputManager::GetDisplayIdForOutput(output);
  GetUserDataAs<AuraToplevel>(resource)->SetWindowBounds(x, y, width, height,
                                                         display_id);
}

void aura_toplevel_set_origin(wl_client* client,
                              wl_resource* resource,
                              int32_t x,
                              int32_t y,
                              wl_resource* output) {
  // TODO(b/247452928): Implement aura_toplevel.set_origin.
  NOTIMPLEMENTED();
}

void aura_toplevel_set_restore_info(wl_client* client,
                                    wl_resource* resource,
                                    int32_t restore_session_id,
                                    int32_t restore_window_id) {
  GetUserDataAs<AuraToplevel>(resource)->SetRestoreInfo(restore_session_id,
                                                        restore_window_id);
}

void aura_toplevel_set_system_modal(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraToplevel>(resource)->SetSystemModal(true);
}

void aura_toplevel_unset_system_modal(wl_client* client,
                                      wl_resource* resource) {
  GetUserDataAs<AuraToplevel>(resource)->SetSystemModal(false);
}

void aura_toplevel_set_float(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraToplevel>(resource)->SetFloatToLocation(
      ZAURA_TOPLEVEL_FLOAT_START_LOCATION_BOTTOM_RIGHT);
}

void aura_toplevel_set_float_to_location(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t location) {
  GetUserDataAs<AuraToplevel>(resource)->SetFloatToLocation(location);
}

void aura_toplevel_unset_float(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraToplevel>(resource)->UnsetFloat();
}

void aura_toplevel_set_snap_primary(wl_client* client,
                                    wl_resource* resource,
                                    uint32_t snap_ratio_as_uint) {
  float snap_ratio = base::bit_cast<float>(snap_ratio_as_uint);
  GetUserDataAs<AuraToplevel>(resource)->SetSnapPrimary(snap_ratio);
}

void aura_toplevel_set_snap_secondary(wl_client* client,
                                      wl_resource* resource,
                                      uint32_t snap_ratio_as_uint) {
  float snap_ratio = base::bit_cast<float>(snap_ratio_as_uint);
  GetUserDataAs<AuraToplevel>(resource)->SetSnapSecondary(snap_ratio);
}

void aura_toplevel_intent_to_snap(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t snap_direction) {
  GetUserDataAs<AuraToplevel>(resource)->IntentToSnap(snap_direction);
}

void aura_toplevel_unset_snap(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraToplevel>(resource)->UnsetSnap();
}

void aura_toplevel_set_restore_info_with_window_id_source(
    wl_client* client,
    wl_resource* resource,
    int32_t restore_session_id,
    const char* restore_window_id_source) {
  GetUserDataAs<AuraToplevel>(resource)->SetRestoreInfoWithWindowIdSource(
      restore_session_id, restore_window_id_source);
}

SurfaceFrameType AuraTopLevelDecorationType(uint32_t decoration_type) {
  switch (decoration_type) {
    case ZAURA_TOPLEVEL_DECORATION_TYPE_NONE:
      return SurfaceFrameType::NONE;
    case ZAURA_TOPLEVEL_DECORATION_TYPE_NORMAL:
      return SurfaceFrameType::NORMAL;
    case ZAURA_TOPLEVEL_DECORATION_TYPE_SHADOW:
      return SurfaceFrameType::SHADOW;
    default:
      VLOG(2) << "Unknown aura-toplevel decoration type: " << decoration_type;
      return SurfaceFrameType::NONE;
  }
}

void aura_toplevel_set_decoration(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t type) {
  GetUserDataAs<AuraToplevel>(resource)->SetDecoration(
      AuraTopLevelDecorationType(type));
}

void aura_toplevel_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

ui::ZOrderLevel AuraTopLevelZOrderLevel(uint32_t z_order_level) {
  switch (z_order_level) {
    case ZAURA_TOPLEVEL_Z_ORDER_LEVEL_NORMAL:
      return ui::ZOrderLevel::kNormal;
    case ZAURA_TOPLEVEL_Z_ORDER_LEVEL_FLOATING_WINDOW:
      return ui::ZOrderLevel::kFloatingWindow;
    case ZAURA_TOPLEVEL_Z_ORDER_LEVEL_FLOATING_UI_ELEMENT:
      return ui::ZOrderLevel::kFloatingUIElement;
    case ZAURA_TOPLEVEL_Z_ORDER_LEVEL_SECURITY_SURFACE:
      return ui::ZOrderLevel::kSecuritySurface;
  }

  NOTREACHED_IN_MIGRATION();
  return ui::ZOrderLevel::kNormal;
}

void aura_toplevel_set_z_order(wl_client* client,
                               wl_resource* resource,
                               uint32_t z_order) {
  GetUserDataAs<AuraToplevel>(resource)->SetZOrder(
      AuraTopLevelZOrderLevel(z_order));
}

void aura_toplevel_activate(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraToplevel>(resource)->Activate();
}

void aura_toplevel_deactivate(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraToplevel>(resource)->Deactivate();
}

void aura_toplevel_set_fullscreen_mode(wl_client* client,
                                       wl_resource* resource,
                                       uint32_t mode) {
  GetUserDataAs<AuraToplevel>(resource)->SetFullscreenMode(mode);
}

void aura_toplevel_set_scale_factor(wl_client* client,
                                    wl_resource* resource,
                                    uint32_t scale_factor_as_uint) {
  static_assert(sizeof(uint32_t) == sizeof(float),
                "Sizes much match for reinterpret cast to be meaningful");
  float scale_factor = base::bit_cast<float>(scale_factor_as_uint);
  GetUserDataAs<AuraToplevel>(resource)->SetScaleFactor(scale_factor);
}

void aura_toplevel_set_persistable(wl_client* client,
                                   wl_resource* resource,
                                   uint32_t persistable) {
  GetUserDataAs<AuraToplevel>(resource)->SetPersistable(
      persistable == ZAURA_TOPLEVEL_PERSISTABLE_PERSISTABLE);
}

void aura_toplevel_set_shape(wl_client* client,
                             wl_resource* resource,
                             wl_resource* region_resource) {
  GetUserDataAs<AuraToplevel>(resource)->SetShape(
      region_resource
          ? std::optional<cc::Region>(*GetUserDataAs<SkRegion>(region_resource))
          : std::nullopt);
}

void aura_toplevel_set_top_inset(wl_client* client,
                                 wl_resource* resource,
                                 int32_t top_inset) {
  GetUserDataAs<AuraToplevel>(resource)->SetTopInset(top_inset);
}

void aura_toplevel_ack_rotate_focus(wl_client* client,
                                    wl_resource* resource,
                                    uint32_t serial,
                                    uint32_t handled) {
  GetUserDataAs<AuraToplevel>(resource)->AckRotateFocus(serial, handled);
}

void aura_toplevel_set_can_maximize(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraToplevel>(resource)->SetCanMaximize(true);
}

void aura_toplevel_unset_can_maximize(wl_client* client,
                                      wl_resource* resource) {
  GetUserDataAs<AuraToplevel>(resource)->SetCanMaximize(false);
}

void aura_toplevel_set_can_fullscreen(wl_client* client,
                                      wl_resource* resource) {
  GetUserDataAs<AuraToplevel>(resource)->SetCanFullscreen(true);
}

void aura_toplevel_unset_can_fullscreen(wl_client* client,
                                        wl_resource* resource) {
  GetUserDataAs<AuraToplevel>(resource)->SetCanFullscreen(false);
}

const struct zaura_toplevel_interface aura_toplevel_implementation = {
    aura_toplevel_set_orientation_lock,
    aura_toplevel_surface_submission_in_pixel_coordinates,
    aura_toplevel_set_client_supports_window_bounds,
    aura_toplevel_set_window_bounds,
    aura_toplevel_set_restore_info,
    aura_toplevel_set_system_modal,
    aura_toplevel_unset_system_modal,
    aura_toplevel_set_restore_info_with_window_id_source,
    aura_toplevel_set_decoration,
    aura_toplevel_release,
    aura_toplevel_set_float,
    aura_toplevel_unset_float,
    aura_toplevel_set_z_order,
    aura_toplevel_set_origin,
    aura_toplevel_activate,
    aura_toplevel_deactivate,
    aura_toplevel_set_fullscreen_mode,
    aura_toplevel_set_scale_factor,
    aura_toplevel_set_snap_primary,
    aura_toplevel_set_snap_secondary,
    aura_toplevel_intent_to_snap,
    aura_toplevel_unset_snap,
    aura_toplevel_set_persistable,
    aura_toplevel_set_shape,
    aura_toplevel_set_top_inset,
    aura_toplevel_ack_rotate_focus,
    aura_toplevel_set_can_maximize,
    aura_toplevel_unset_can_maximize,
    aura_toplevel_set_can_fullscreen,
    aura_toplevel_unset_can_fullscreen,
    aura_toplevel_set_float_to_location,
    aura_toplevel_set_window_corner_radii,
    aura_toplevel_set_shadow_corners_radii};

void aura_popup_surface_submission_in_pixel_coordinates(wl_client* client,
                                                        wl_resource* resource) {
  GetUserDataAs<AuraPopup>(resource)
      ->SetClientSubmitsSurfacesInPixelCoordinates(true);
}

SurfaceFrameType AuraPopupDecorationType(uint32_t decoration_type) {
  switch (decoration_type) {
    case ZAURA_POPUP_DECORATION_TYPE_NONE:
      return SurfaceFrameType::NONE;
    case ZAURA_POPUP_DECORATION_TYPE_NORMAL:
      return SurfaceFrameType::NORMAL;
    case ZAURA_POPUP_DECORATION_TYPE_SHADOW:
      return SurfaceFrameType::SHADOW;
    default:
      VLOG(2) << "Unknown aura-popup decoration type: " << decoration_type;
      return SurfaceFrameType::NONE;
  }
}

void aura_popup_set_decoration(wl_client* client,
                               wl_resource* resource,
                               uint32_t type) {
  GetUserDataAs<AuraPopup>(resource)->SetDecoration(
      AuraPopupDecorationType(type));
}

void aura_popup_set_menu(wl_client* client, wl_resource* resource) {
  GetUserDataAs<AuraPopup>(resource)->SetMenu();
}

void aura_popup_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void aura_popup_set_scale_factor(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t scale_factor_as_uint) {
  float scale_factor = base::bit_cast<float>(scale_factor_as_uint);
  GetUserDataAs<AuraPopup>(resource)->SetScaleFactor(scale_factor);
}

const struct zaura_popup_interface aura_popup_implementation = {
    aura_popup_surface_submission_in_pixel_coordinates,
    aura_popup_set_decoration,
    aura_popup_set_menu,
    aura_popup_release,
    aura_popup_set_scale_factor,
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
          shell_surface_data.rotation_serial_tracker,
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

  auto aura_output =
      std::make_unique<AuraOutput>(aura_output_resource, display_handler);

  SetImplementation(aura_output_resource, &aura_output_implementation,
                    std::move(aura_output));
}

void aura_shell_surface_submission_in_pixel_coordinates(wl_client* client,
                                                        wl_resource* resource) {
  LOG(WARNING) << "Deprecated. The server doesn't support this request.";
}

void aura_shell_release(wl_client* client, wl_resource* resource) {
  // Nothing to do here.
}

const struct zaura_shell_interface aura_shell_implementation = {
    aura_shell_get_aura_surface,
    aura_shell_get_aura_output,
    aura_shell_surface_submission_in_pixel_coordinates,
    aura_shell_get_aura_toplevel,
    aura_shell_get_aura_popup,
    aura_shell_release,
};
}  // namespace

void bind_aura_shell(wl_client* client,
                     void* data,
                     uint32_t version,
                     uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zaura_shell_interface,
                         std::min(version, kZAuraShellVersion), id);

  Display* display = static_cast<Display*>(data);
  SetImplementation(resource, &aura_shell_implementation,
                    std::make_unique<WaylandAuraShell>(resource, display));
}

}  // namespace exo::wayland
