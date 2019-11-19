// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zxdg_shell.h"

#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <xdg-shell-unstable-v6-server-protocol.h>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/display.h"
#include "components/exo/wayland/serial_tracker.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_positioner.h"
#include "components/exo/xdg_shell_surface.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace exo {
namespace wayland {
namespace {

////////////////////////////////////////////////////////////////////////////////
// xdg_positioner_interface:

void xdg_positioner_v6_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_positioner_v6_set_size(wl_client* client,
                                wl_resource* resource,
                                int32_t width,
                                int32_t height) {
  if (width < 1 || height < 1) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "width and height must be positive and non-zero");
    return;
  }

  GetUserDataAs<WaylandPositioner>(resource)->SetSize(gfx::Size(width, height));
}

void xdg_positioner_v6_set_anchor_rect(wl_client* client,
                                       wl_resource* resource,
                                       int32_t x,
                                       int32_t y,
                                       int32_t width,
                                       int32_t height) {
  if (width < 1 || height < 1) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "width and height must be positive and non-zero");
    return;
  }

  GetUserDataAs<WaylandPositioner>(resource)->SetAnchorRect(
      gfx::Rect(x, y, width, height));
}

void xdg_positioner_v6_set_anchor(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t anchor) {
  if (((anchor & ZXDG_POSITIONER_V6_ANCHOR_LEFT) &&
       (anchor & ZXDG_POSITIONER_V6_ANCHOR_RIGHT)) ||
      ((anchor & ZXDG_POSITIONER_V6_ANCHOR_TOP) &&
       (anchor & ZXDG_POSITIONER_V6_ANCHOR_BOTTOM))) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "same-axis values are not allowed");
    return;
  }

  GetUserDataAs<WaylandPositioner>(resource)->SetAnchor(anchor);
}

void xdg_positioner_v6_set_gravity(wl_client* client,
                                   wl_resource* resource,
                                   uint32_t gravity) {
  if (((gravity & ZXDG_POSITIONER_V6_GRAVITY_LEFT) &&
       (gravity & ZXDG_POSITIONER_V6_GRAVITY_RIGHT)) ||
      ((gravity & ZXDG_POSITIONER_V6_GRAVITY_TOP) &&
       (gravity & ZXDG_POSITIONER_V6_GRAVITY_BOTTOM))) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "same-axis values are not allowed");
    return;
  }

  GetUserDataAs<WaylandPositioner>(resource)->SetGravity(gravity);
}

void xdg_positioner_v6_set_constraint_adjustment(wl_client* client,
                                                 wl_resource* resource,
                                                 uint32_t adjustment) {
  GetUserDataAs<WaylandPositioner>(resource)->SetAdjustment(adjustment);
}

void xdg_positioner_v6_set_offset(wl_client* client,
                                  wl_resource* resource,
                                  int32_t x,
                                  int32_t y) {
  GetUserDataAs<WaylandPositioner>(resource)->SetOffset(gfx::Vector2d(x, y));
}

const struct zxdg_positioner_v6_interface xdg_positioner_v6_implementation = {
    xdg_positioner_v6_destroy,
    xdg_positioner_v6_set_size,
    xdg_positioner_v6_set_anchor_rect,
    xdg_positioner_v6_set_anchor,
    xdg_positioner_v6_set_gravity,
    xdg_positioner_v6_set_constraint_adjustment,
    xdg_positioner_v6_set_offset};

////////////////////////////////////////////////////////////////////////////////
// xdg_toplevel_interface:

int XdgToplevelV6ResizeComponent(uint32_t edges) {
  switch (edges) {
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP:
      return HTTOP;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM:
      return HTBOTTOM;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_LEFT:
      return HTLEFT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_LEFT:
      return HTTOPLEFT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_LEFT:
      return HTBOTTOMLEFT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_RIGHT:
      return HTRIGHT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_RIGHT:
      return HTTOPRIGHT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_RIGHT:
      return HTBOTTOMRIGHT;
    default:
      return HTBOTTOMRIGHT;
  }
}

using XdgSurfaceConfigureCallback =
    base::Callback<void(const gfx::Size& size,
                        ash::WindowStateType state_type,
                        bool resizing,
                        bool activated)>;

uint32_t HandleXdgSurfaceV6ConfigureCallback(
    wl_resource* resource,
    SerialTracker* serial_tracker,
    const XdgSurfaceConfigureCallback& callback,
    const gfx::Size& size,
    ash::WindowStateType state_type,
    bool resizing,
    bool activated,
    const gfx::Vector2d& origin_offset) {
  uint32_t serial =
      serial_tracker->GetNextSerial(SerialTracker::EventType::OTHER_EVENT);
  callback.Run(size, state_type, resizing, activated);
  zxdg_surface_v6_send_configure(resource, serial);
  wl_client_flush(wl_resource_get_client(resource));
  return serial;
}

struct WaylandXdgSurface {
  WaylandXdgSurface(std::unique_ptr<XdgShellSurface> shell_surface,
                    SerialTracker* const serial_tracker)
      : shell_surface(std::move(shell_surface)),
        serial_tracker(serial_tracker) {}

  std::unique_ptr<XdgShellSurface> shell_surface;

  // Owned by Server, which always outlives this surface.
  SerialTracker* const serial_tracker;

  DISALLOW_COPY_AND_ASSIGN(WaylandXdgSurface);
};

// Wrapper around shell surface that allows us to handle the case where the
// xdg surface resource is destroyed before the toplevel resource.
class WaylandToplevel : public aura::WindowObserver {
 public:
  WaylandToplevel(wl_resource* resource, wl_resource* surface_resource)
      : resource_(resource),
        shell_surface_data_(
            GetUserDataAs<WaylandXdgSurface>(surface_resource)) {
    shell_surface_data_->shell_surface->host_window()->AddObserver(this);
    shell_surface_data_->shell_surface->set_close_callback(
        base::Bind(&WaylandToplevel::OnClose, weak_ptr_factory_.GetWeakPtr()));
    shell_surface_data_->shell_surface->set_configure_callback(
        base::Bind(&HandleXdgSurfaceV6ConfigureCallback, surface_resource,
                   shell_surface_data_->serial_tracker,
                   base::Bind(&WaylandToplevel::OnConfigure,
                              weak_ptr_factory_.GetWeakPtr())));
  }
  ~WaylandToplevel() override {
    if (shell_surface_data_)
      shell_surface_data_->shell_surface->host_window()->RemoveObserver(this);
  }

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    shell_surface_data_ = nullptr;
  }

  void SetParent(WaylandToplevel* parent) {
    if (!shell_surface_data_)
      return;

    if (!parent) {
      shell_surface_data_->shell_surface->SetParent(nullptr);
      return;
    }

    // This is a no-op if parent is not mapped.
    if (parent->shell_surface_data_ &&
        parent->shell_surface_data_->shell_surface->GetWidget())
      shell_surface_data_->shell_surface->SetParent(
          parent->shell_surface_data_->shell_surface.get());
  }

  void SetTitle(const base::string16& title) {
    if (shell_surface_data_)
      shell_surface_data_->shell_surface->SetTitle(title);
  }

  void SetApplicationId(const char* application_id) {
    if (shell_surface_data_)
      shell_surface_data_->shell_surface->SetApplicationId(application_id);
  }

  void Move() {
    if (shell_surface_data_)
      shell_surface_data_->shell_surface->StartMove();
  }

  void Resize(int component) {
    if (!shell_surface_data_)
      return;

    if (component != HTNOWHERE)
      shell_surface_data_->shell_surface->StartResize(component);
  }

  void SetMaximumSize(const gfx::Size& size) {
    if (shell_surface_data_)
      shell_surface_data_->shell_surface->SetMaximumSize(size);
  }

  void SetMinimumSize(const gfx::Size& size) {
    if (shell_surface_data_)
      shell_surface_data_->shell_surface->SetMinimumSize(size);
  }

  void Maximize() {
    if (shell_surface_data_)
      shell_surface_data_->shell_surface->Maximize();
  }

  void Restore() {
    if (shell_surface_data_)
      shell_surface_data_->shell_surface->Restore();
  }

  void SetFullscreen(bool fullscreen) {
    if (shell_surface_data_)
      shell_surface_data_->shell_surface->SetFullscreen(fullscreen);
  }

  void Minimize() {
    if (shell_surface_data_)
      shell_surface_data_->shell_surface->Minimize();
  }

 private:
  void OnClose() {
    zxdg_toplevel_v6_send_close(resource_);
    wl_client_flush(wl_resource_get_client(resource_));
  }

  static void AddState(wl_array* states, zxdg_toplevel_v6_state state) {
    zxdg_toplevel_v6_state* value = static_cast<zxdg_toplevel_v6_state*>(
        wl_array_add(states, sizeof(zxdg_toplevel_v6_state)));
    DCHECK(value);
    *value = state;
  }

  void OnConfigure(const gfx::Size& size,
                   ash::WindowStateType state_type,
                   bool resizing,
                   bool activated) {
    wl_array states;
    wl_array_init(&states);
    if (state_type == ash::WindowStateType::kMaximized)
      AddState(&states, ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED);
    if (state_type == ash::WindowStateType::kFullscreen)
      AddState(&states, ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN);
    if (resizing)
      AddState(&states, ZXDG_TOPLEVEL_V6_STATE_RESIZING);
    if (activated)
      AddState(&states, ZXDG_TOPLEVEL_V6_STATE_ACTIVATED);
    zxdg_toplevel_v6_send_configure(resource_, size.width(), size.height(),
                                    &states);
    wl_array_release(&states);
  }

  wl_resource* const resource_;
  WaylandXdgSurface* shell_surface_data_;
  base::WeakPtrFactory<WaylandToplevel> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WaylandToplevel);
};

void xdg_toplevel_v6_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_toplevel_v6_set_parent(wl_client* client,
                                wl_resource* resource,
                                wl_resource* parent) {
  WaylandToplevel* parent_surface = nullptr;
  if (parent)
    parent_surface = GetUserDataAs<WaylandToplevel>(parent);

  GetUserDataAs<WaylandToplevel>(resource)->SetParent(parent_surface);
}

void xdg_toplevel_v6_set_title(wl_client* client,
                               wl_resource* resource,
                               const char* title) {
  GetUserDataAs<WaylandToplevel>(resource)->SetTitle(
      base::string16(base::UTF8ToUTF16(title)));
}

void xdg_toplevel_v6_set_app_id(wl_client* client,
                                wl_resource* resource,
                                const char* app_id) {
  GetUserDataAs<WaylandToplevel>(resource)->SetApplicationId(app_id);
}

void xdg_toplevel_v6_show_window_menu(wl_client* client,
                                      wl_resource* resource,
                                      wl_resource* seat,
                                      uint32_t serial,
                                      int32_t x,
                                      int32_t y) {
  NOTIMPLEMENTED();
}

void xdg_toplevel_v6_move(wl_client* client,
                          wl_resource* resource,
                          wl_resource* seat,
                          uint32_t serial) {
  GetUserDataAs<WaylandToplevel>(resource)->Move();
}

void xdg_toplevel_v6_resize(wl_client* client,
                            wl_resource* resource,
                            wl_resource* seat,
                            uint32_t serial,
                            uint32_t edges) {
  GetUserDataAs<WaylandToplevel>(resource)->Resize(
      XdgToplevelV6ResizeComponent(edges));
}

void xdg_toplevel_v6_set_max_size(wl_client* client,
                                  wl_resource* resource,
                                  int32_t width,
                                  int32_t height) {
  GetUserDataAs<WaylandToplevel>(resource)->SetMaximumSize(
      gfx::Size(width, height));
}

void xdg_toplevel_v6_set_min_size(wl_client* client,
                                  wl_resource* resource,
                                  int32_t width,
                                  int32_t height) {
  GetUserDataAs<WaylandToplevel>(resource)->SetMinimumSize(
      gfx::Size(width, height));
}

void xdg_toplevel_v6_set_maximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<WaylandToplevel>(resource)->Maximize();
}

void xdg_toplevel_v6_unset_maximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<WaylandToplevel>(resource)->Restore();
}

void xdg_toplevel_v6_set_fullscreen(wl_client* client,
                                    wl_resource* resource,
                                    wl_resource* output) {
  GetUserDataAs<WaylandToplevel>(resource)->SetFullscreen(true);
}

void xdg_toplevel_v6_unset_fullscreen(wl_client* client,
                                      wl_resource* resource) {
  GetUserDataAs<WaylandToplevel>(resource)->SetFullscreen(false);
}

void xdg_toplevel_v6_set_minimized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<WaylandToplevel>(resource)->Minimize();
}

const struct zxdg_toplevel_v6_interface xdg_toplevel_v6_implementation = {
    xdg_toplevel_v6_destroy,          xdg_toplevel_v6_set_parent,
    xdg_toplevel_v6_set_title,        xdg_toplevel_v6_set_app_id,
    xdg_toplevel_v6_show_window_menu, xdg_toplevel_v6_move,
    xdg_toplevel_v6_resize,           xdg_toplevel_v6_set_max_size,
    xdg_toplevel_v6_set_min_size,     xdg_toplevel_v6_set_maximized,
    xdg_toplevel_v6_unset_maximized,  xdg_toplevel_v6_set_fullscreen,
    xdg_toplevel_v6_unset_fullscreen, xdg_toplevel_v6_set_minimized};

////////////////////////////////////////////////////////////////////////////////
// xdg_popup_interface:

// Wrapper around shell surface that allows us to handle the case where the
// xdg surface resource is destroyed before the popup resource.
class WaylandPopup : aura::WindowObserver {
 public:
  WaylandPopup(wl_resource* resource, wl_resource* surface_resource)
      : resource_(resource),
        shell_surface_data_(
            GetUserDataAs<WaylandXdgSurface>(surface_resource)) {
    shell_surface_data_->shell_surface->host_window()->AddObserver(this);
    shell_surface_data_->shell_surface->set_close_callback(
        base::Bind(&WaylandPopup::OnClose, weak_ptr_factory_.GetWeakPtr()));
    shell_surface_data_->shell_surface->set_configure_callback(
        base::Bind(&HandleXdgSurfaceV6ConfigureCallback, surface_resource,
                   shell_surface_data_->serial_tracker,
                   base::Bind(&WaylandPopup::OnConfigure,
                              weak_ptr_factory_.GetWeakPtr())));
  }
  ~WaylandPopup() override {
    if (shell_surface_data_)
      shell_surface_data_->shell_surface->host_window()->RemoveObserver(this);
  }

  void Grab() {
    if (!shell_surface_data_) {
      wl_resource_post_error(resource_, ZXDG_POPUP_V6_ERROR_INVALID_GRAB,
                             "the surface has already been destroyed");
      return;
    }
    if (shell_surface_data_->shell_surface->GetWidget()) {
      wl_resource_post_error(resource_, ZXDG_POPUP_V6_ERROR_INVALID_GRAB,
                             "grab must be called before construction");
      return;
    }
    shell_surface_data_->shell_surface->Grab();
  }

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    shell_surface_data_ = nullptr;
  }

 private:
  void OnClose() {
    zxdg_popup_v6_send_popup_done(resource_);
    wl_client_flush(wl_resource_get_client(resource_));
  }

  void OnConfigure(const gfx::Size& size,
                   ash::WindowStateType state_type,
                   bool resizing,
                   bool activated) {
    // Nothing to do here as popups don't have additional configure state.
  }

  wl_resource* const resource_;
  WaylandXdgSurface* shell_surface_data_;
  base::WeakPtrFactory<WaylandPopup> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WaylandPopup);
};

void xdg_popup_v6_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_popup_v6_grab(wl_client* client,
                       wl_resource* resource,
                       wl_resource* seat,
                       uint32_t serial) {
  GetUserDataAs<WaylandPopup>(resource)->Grab();
}

const struct zxdg_popup_v6_interface xdg_popup_v6_implementation = {
    xdg_popup_v6_destroy, xdg_popup_v6_grab};

////////////////////////////////////////////////////////////////////////////////
// xdg_surface_interface:

void xdg_surface_v6_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_surface_v6_get_toplevel(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t id) {
  auto* shell_surface_data = GetUserDataAs<WaylandXdgSurface>(resource);
  if (shell_surface_data->shell_surface->GetEnabled()) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_ALREADY_CONSTRUCTED,
                           "surface has already been constructed");
    return;
  }

  shell_surface_data->shell_surface->SetCanMinimize(true);
  shell_surface_data->shell_surface->SetEnabled(true);

  wl_resource* xdg_toplevel_resource =
      wl_resource_create(client, &zxdg_toplevel_v6_interface, 1, id);

  SetImplementation(
      xdg_toplevel_resource, &xdg_toplevel_v6_implementation,
      std::make_unique<WaylandToplevel>(xdg_toplevel_resource, resource));
}

void xdg_surface_v6_get_popup(wl_client* client,
                              wl_resource* resource,
                              uint32_t id,
                              wl_resource* parent_resource,
                              wl_resource* positioner_resource) {
  auto* shell_surface_data = GetUserDataAs<WaylandXdgSurface>(resource);
  if (shell_surface_data->shell_surface->GetEnabled()) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_ALREADY_CONSTRUCTED,
                           "surface has already been constructed");
    return;
  }

  auto* parent_data = GetUserDataAs<WaylandXdgSurface>(parent_resource);
  if (!parent_data->shell_surface->GetWidget()) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_NOT_CONSTRUCTED,
                           "popup parent not constructed");
    return;
  }

  if (shell_surface_data->shell_surface->GetWidget()) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_ALREADY_CONSTRUCTED,
                           "get_popup is called after constructed");
    return;
  }

  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          parent_data->shell_surface->GetWidget()->GetNativeWindow());
  gfx::Rect work_area = display.work_area();
  wm::ConvertRectFromScreen(
      parent_data->shell_surface->GetWidget()->GetNativeWindow(), &work_area);

  // Try layout using parent's flip state.
  WaylandPositioner* positioner =
      GetUserDataAs<WaylandPositioner>(positioner_resource);
  WaylandPositioner::Result position = positioner->CalculatePosition(
      work_area, parent_data->shell_surface->x_flipped(),
      parent_data->shell_surface->y_flipped());

  // Remember the new flip state for its child popups.
  shell_surface_data->shell_surface->set_x_flipped(position.x_flipped);
  shell_surface_data->shell_surface->set_y_flipped(position.y_flipped);

  // |position| is relative to the parent's contents view origin, and |origin|
  // is in screen coordinates.
  gfx::Point origin = position.origin;
  views::View::ConvertPointToScreen(parent_data->shell_surface->GetWidget()
                                        ->widget_delegate()
                                        ->GetContentsView(),
                                    &origin);
  shell_surface_data->shell_surface->SetOrigin(origin);
  shell_surface_data->shell_surface->SetSize(position.size);
  shell_surface_data->shell_surface->DisableMovement();
  shell_surface_data->shell_surface->SetActivatable(false);
  shell_surface_data->shell_surface->SetCanMinimize(false);
  shell_surface_data->shell_surface->SetParent(
      parent_data->shell_surface.get());
  shell_surface_data->shell_surface->SetPopup();
  shell_surface_data->shell_surface->SetEnabled(true);

  wl_resource* xdg_popup_resource =
      wl_resource_create(client, &zxdg_popup_v6_interface, 1, id);

  SetImplementation(
      xdg_popup_resource, &xdg_popup_v6_implementation,
      std::make_unique<WaylandPopup>(xdg_popup_resource, resource));

  // We send the configure event here as this event needs x,y coordinates
  // relative to the parent window.
  zxdg_popup_v6_send_configure(xdg_popup_resource, position.origin.x(),
                               position.origin.y(), position.size.width(),
                               position.size.height());
}

void xdg_surface_v6_set_window_geometry(wl_client* client,
                                        wl_resource* resource,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height) {
  GetUserDataAs<WaylandXdgSurface>(resource)->shell_surface->SetGeometry(
      gfx::Rect(x, y, width, height));
}

void xdg_surface_v6_ack_configure(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t serial) {
  GetUserDataAs<WaylandXdgSurface>(resource)
      ->shell_surface->AcknowledgeConfigure(serial);
}

const struct zxdg_surface_v6_interface xdg_surface_v6_implementation = {
    xdg_surface_v6_destroy, xdg_surface_v6_get_toplevel,
    xdg_surface_v6_get_popup, xdg_surface_v6_set_window_geometry,
    xdg_surface_v6_ack_configure};

////////////////////////////////////////////////////////////////////////////////
// xdg_shell_interface:

void xdg_shell_v6_destroy(wl_client* client, wl_resource* resource) {
  // Nothing to do here.
}

void xdg_shell_v6_create_positioner(wl_client* client,
                                    wl_resource* resource,
                                    uint32_t id) {
  wl_resource* positioner_resource =
      wl_resource_create(client, &zxdg_positioner_v6_interface, 1, id);

  SetImplementation(positioner_resource, &xdg_positioner_v6_implementation,
                    std::make_unique<WaylandPositioner>());
}

void xdg_shell_v6_get_xdg_surface(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  wl_resource* surface) {
  auto* data = GetUserDataAs<WaylandXdgShell>(resource);
  std::unique_ptr<XdgShellSurface> shell_surface =
      data->display->CreateXdgShellSurface(GetUserDataAs<Surface>(surface));
  if (!shell_surface) {
    wl_resource_post_error(resource, ZXDG_SHELL_V6_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  // Xdg shell v6 surfaces are initially disabled and needs to be explicitly
  // mapped before they are enabled and can become visible.
  shell_surface->SetEnabled(false);

  std::unique_ptr<WaylandXdgSurface> wayland_shell_surface =
      std::make_unique<WaylandXdgSurface>(std::move(shell_surface),
                                          data->serial_tracker);

  wl_resource* xdg_surface_resource =
      wl_resource_create(client, &zxdg_surface_v6_interface, 1, id);

  SetImplementation(xdg_surface_resource, &xdg_surface_v6_implementation,
                    std::move(wayland_shell_surface));
}

void xdg_shell_v6_pong(wl_client* client,
                       wl_resource* resource,
                       uint32_t serial) {
  NOTIMPLEMENTED();
}

const struct zxdg_shell_v6_interface xdg_shell_v6_implementation = {
    xdg_shell_v6_destroy, xdg_shell_v6_create_positioner,
    xdg_shell_v6_get_xdg_surface, xdg_shell_v6_pong};

}  // namespace

void bind_xdg_shell_v6(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zxdg_shell_v6_interface, 1, id);

  wl_resource_set_implementation(resource, &xdg_shell_v6_implementation, data,
                                 nullptr);
}

}  // namespace wayland
}  // namespace exo
