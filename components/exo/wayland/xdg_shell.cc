// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/xdg_shell.h"

#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <xdg-decoration-unstable-v1-server-protocol.h>
#include <xdg-shell-server-protocol.h>

#include <optional>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/exo/display.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/wayland/serial_tracker.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_display_observer.h"
#include "components/exo/wayland/wayland_positioner.h"
#include "components/exo/xdg_shell_surface.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace exo {
namespace wayland {
namespace {

////////////////////////////////////////////////////////////////////////////////
// xdg_positioner_interface:

void xdg_positioner_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_positioner_set_size(wl_client* client,
                             wl_resource* resource,
                             int32_t width,
                             int32_t height) {
  if (width < 1 || height < 1) {
    wl_resource_post_error(resource, XDG_POSITIONER_ERROR_INVALID_INPUT,
                           "width and height must be positive and non-zero");
    return;
  }

  GetUserDataAs<WaylandPositioner>(resource)->SetSize(gfx::Size(width, height));
}

void xdg_positioner_set_anchor_rect(wl_client* client,
                                    wl_resource* resource,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width,
                                    int32_t height) {
  if (width < 1 || height < 1) {
    wl_resource_post_error(resource, XDG_POSITIONER_ERROR_INVALID_INPUT,
                           "width and height must be positive and non-zero");
    return;
  }

  GetUserDataAs<WaylandPositioner>(resource)->SetAnchorRect(
      gfx::Rect(x, y, width, height));
}

void xdg_positioner_set_anchor(wl_client* client,
                               wl_resource* resource,
                               uint32_t anchor) {
  GetUserDataAs<WaylandPositioner>(resource)->SetAnchor(anchor);
}

void xdg_positioner_set_gravity(wl_client* client,
                                wl_resource* resource,
                                uint32_t gravity) {
  GetUserDataAs<WaylandPositioner>(resource)->SetGravity(gravity);
}

void xdg_positioner_set_constraint_adjustment(wl_client* client,
                                              wl_resource* resource,
                                              uint32_t adjustment) {
  GetUserDataAs<WaylandPositioner>(resource)->SetAdjustment(adjustment);
}

void xdg_positioner_set_offset(wl_client* client,
                               wl_resource* resource,
                               int32_t x,
                               int32_t y) {
  GetUserDataAs<WaylandPositioner>(resource)->SetOffset(gfx::Vector2d(x, y));
}

// Version 3 xdg_positioner
// Empty since Weston doesn't implement this.
// Ref:
// https://gitlab.freedesktop.org/wayland/weston/-/blob/main/libweston/desktop/xdg-shell.c#L312
void xdg_positioner_set_reactive(wl_client* client, wl_resource* resource) {}

void xdg_positioner_set_parent_size(wl_client* client,
                                    wl_resource* resource,
                                    int32_t parent_width,
                                    int32_t parent_height) {}

void xdg_positioner_set_parent_configure(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t serial) {}

const struct xdg_positioner_interface xdg_positioner_implementation = {
    xdg_positioner_destroy,         xdg_positioner_set_size,
    xdg_positioner_set_anchor_rect, xdg_positioner_set_anchor,
    xdg_positioner_set_gravity,     xdg_positioner_set_constraint_adjustment,
    xdg_positioner_set_offset,      xdg_positioner_set_reactive,
    xdg_positioner_set_parent_size, xdg_positioner_set_parent_configure};

////////////////////////////////////////////////////////////////////////////////
// xdg_toplevel_interface:

int XdgToplevelResizeComponent(uint32_t edges) {
  switch (edges) {
    case XDG_TOPLEVEL_RESIZE_EDGE_TOP:
      return HTTOP;
    case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM:
      return HTBOTTOM;
    case XDG_TOPLEVEL_RESIZE_EDGE_LEFT:
      return HTLEFT;
    case XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT:
      return HTTOPLEFT;
    case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT:
      return HTBOTTOMLEFT;
    case XDG_TOPLEVEL_RESIZE_EDGE_RIGHT:
      return HTRIGHT;
    case XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT:
      return HTTOPRIGHT;
    case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT:
      return HTBOTTOMRIGHT;
    default:
      return HTBOTTOMRIGHT;
  }
}

using XdgSurfaceConfigureCallback = base::RepeatingCallback<void(
    const gfx::Size& size,
    chromeos::WindowStateType state_type,
    bool resizing,
    bool activated,
    std::optional<chromeos::WindowStateType> restore_state_type)>;

uint32_t HandleXdgSurfaceConfigureCallback(
    wl_resource* resource,
    SerialTracker* serial_tracker,
    const XdgSurfaceConfigureCallback& callback,
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
  callback.Run(bounds.size(), state_type, resizing, activated,
               restore_state_type);
  xdg_surface_send_configure(resource, serial);
  wl_client_flush(wl_resource_get_client(resource));
  return serial;
}

// Wrapper around shell surface that allows us to handle the case where the
// xdg surface resource is destroyed before the toplevel resource.
class WaylandToplevel : public aura::WindowObserver {
 public:
  WaylandToplevel(wl_resource* xdg_toplevel_resource,
                  wl_resource* xdg_surface_resource)
      : xdg_toplevel_resource_(xdg_toplevel_resource),
        xdg_surface_resource_(xdg_surface_resource),
        shell_surface_data_(
            GetUserDataAs<WaylandXdgSurface>(xdg_surface_resource)) {
    shell_surface_data_->shell_surface->host_window()->AddObserver(this);
    shell_surface_data_->shell_surface->set_close_callback(base::BindRepeating(
        &WaylandToplevel::OnClose, weak_ptr_factory_.GetWeakPtr()));
    shell_surface_data_->shell_surface->set_configure_callback(
        base::BindRepeating(
            &HandleXdgSurfaceConfigureCallback, xdg_surface_resource,
            shell_surface_data_->serial_tracker,
            base::BindRepeating(&WaylandToplevel::OnConfigure,
                                weak_ptr_factory_.GetWeakPtr())));
  }

  WaylandToplevel(const WaylandToplevel&) = delete;
  WaylandToplevel& operator=(const WaylandToplevel&) = delete;

  ~WaylandToplevel() override {
    if (shell_surface_data_)
      shell_surface_data_->shell_surface->host_window()->RemoveObserver(this);
  }

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window->RemoveObserver(this);
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

  void SetTitle(const std::u16string& title) {
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

  void SetFullscreen(bool fullscreen,
                     int64_t display_id = display::kInvalidDisplayId) {
    if (shell_surface_data_)
      shell_surface_data_->shell_surface->SetFullscreen(fullscreen, display_id);
  }

  void Minimize() {
    if (shell_surface_data_)
      shell_surface_data_->shell_surface->Minimize();
  }

  void SetFrame(SurfaceFrameType type) {
    if (shell_surface_data_)
      shell_surface_data_->shell_surface->OnSetFrame(type);
  }

  ShellSurfaceData GetShellSurfaceData() {
    return ShellSurfaceData(shell_surface_data_->shell_surface.get(),
                            shell_surface_data_->serial_tracker,
                            shell_surface_data_->rotation_serial_tracker,
                            xdg_surface_resource_);
  }

 private:
  void OnClose() {
    xdg_toplevel_send_close(xdg_toplevel_resource_);
    wl_client_flush(wl_resource_get_client(xdg_toplevel_resource_));
  }

  static void AddState(wl_array* states, xdg_toplevel_state state) {
    xdg_toplevel_state* value = static_cast<xdg_toplevel_state*>(
        wl_array_add(states, sizeof(xdg_toplevel_state)));
    DCHECK(value);
    *value = state;
  }

  void OnConfigure(
      const gfx::Size& size,
      chromeos::WindowStateType state_type,
      bool resizing,
      bool activated,
      std::optional<chromeos::WindowStateType> restore_state_type) {
    wl_array states;
    wl_array_init(&states);
    if (state_type == chromeos::WindowStateType::kMaximized)
      AddState(&states, XDG_TOPLEVEL_STATE_MAXIMIZED);
    // TODO(crbug.com/40197882): Pinned states need to be handled properly.
    if (IsFullscreenOrPinnedWindowStateType(state_type)) {
      AddState(&states, XDG_TOPLEVEL_STATE_FULLSCREEN);
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
    xdg_toplevel_send_configure(xdg_toplevel_resource_, size.width(),
                                size.height(), &states);
    wl_array_release(&states);
  }

  const raw_ptr<wl_resource> xdg_toplevel_resource_;
  const raw_ptr<wl_resource, DanglingUntriaged> xdg_surface_resource_;
  raw_ptr<WaylandXdgSurface> shell_surface_data_;
  base::WeakPtrFactory<WaylandToplevel> weak_ptr_factory_{this};
};

void xdg_toplevel_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_toplevel_set_parent(wl_client* client,
                             wl_resource* resource,
                             wl_resource* parent) {
  WaylandToplevel* parent_surface = nullptr;
  if (parent)
    parent_surface = GetUserDataAs<WaylandToplevel>(parent);

  GetUserDataAs<WaylandToplevel>(resource)->SetParent(parent_surface);
}

void xdg_toplevel_set_title(wl_client* client,
                            wl_resource* resource,
                            const char* title) {
  GetUserDataAs<WaylandToplevel>(resource)->SetTitle(
      std::u16string(base::UTF8ToUTF16(title)));
}

void xdg_toplevel_set_app_id(wl_client* client,
                             wl_resource* resource,
                             const char* app_id) {
  GetUserDataAs<WaylandToplevel>(resource)->SetApplicationId(app_id);
}

void xdg_toplevel_show_window_menu(wl_client* client,
                                   wl_resource* resource,
                                   wl_resource* seat,
                                   uint32_t serial,
                                   int32_t x,
                                   int32_t y) {
  NOTIMPLEMENTED();
}

void xdg_toplevel_move(wl_client* client,
                       wl_resource* resource,
                       wl_resource* seat,
                       uint32_t serial) {
  GetUserDataAs<WaylandToplevel>(resource)->Move();
}

void xdg_toplevel_resize(wl_client* client,
                         wl_resource* resource,
                         wl_resource* seat,
                         uint32_t serial,
                         uint32_t edges) {
  GetUserDataAs<WaylandToplevel>(resource)->Resize(
      XdgToplevelResizeComponent(edges));
}

void xdg_toplevel_set_max_size(wl_client* client,
                               wl_resource* resource,
                               int32_t width,
                               int32_t height) {
  GetUserDataAs<WaylandToplevel>(resource)->SetMaximumSize(
      gfx::Size(width, height));
}

void xdg_toplevel_set_min_size(wl_client* client,
                               wl_resource* resource,
                               int32_t width,
                               int32_t height) {
  GetUserDataAs<WaylandToplevel>(resource)->SetMinimumSize(
      gfx::Size(width, height));
}

void xdg_toplevel_set_maximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<WaylandToplevel>(resource)->Maximize();
}

void xdg_toplevel_unset_maximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<WaylandToplevel>(resource)->Restore();
}

void xdg_toplevel_set_fullscreen(wl_client* client,
                                 wl_resource* resource,
                                 wl_resource* output) {
  int64_t display_id = output
                           ? GetUserDataAs<WaylandDisplayHandler>(output)->id()
                           : display::kInvalidDisplayId;
  GetUserDataAs<WaylandToplevel>(resource)->SetFullscreen(true, display_id);
}

void xdg_toplevel_unset_fullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<WaylandToplevel>(resource)->SetFullscreen(false);
}

void xdg_toplevel_set_minimized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<WaylandToplevel>(resource)->Minimize();
}

const struct xdg_toplevel_interface xdg_toplevel_implementation = {
    xdg_toplevel_destroy,          xdg_toplevel_set_parent,
    xdg_toplevel_set_title,        xdg_toplevel_set_app_id,
    xdg_toplevel_show_window_menu, xdg_toplevel_move,
    xdg_toplevel_resize,           xdg_toplevel_set_max_size,
    xdg_toplevel_set_min_size,     xdg_toplevel_set_maximized,
    xdg_toplevel_unset_maximized,  xdg_toplevel_set_fullscreen,
    xdg_toplevel_unset_fullscreen, xdg_toplevel_set_minimized};

class WaylandXdgToplevelDecoration {
 public:
  WaylandXdgToplevelDecoration(wl_resource* resource,
                               wl_resource* toplevel_resource)
      : resource_(resource),
        top_level_(GetUserDataAs<WaylandToplevel>(toplevel_resource)) {}

  WaylandXdgToplevelDecoration(const WaylandXdgToplevelDecoration&) = delete;
  WaylandXdgToplevelDecoration& operator=(const WaylandXdgToplevelDecoration&) =
      delete;

  uint32_t decoration_mode() const { return default_mode_; }
  void SetDecorationMode(uint32_t mode) {
    if (default_mode_ != mode) {
      default_mode_ = mode;
      OnConfigure(mode);
    }
  }

 private:
  void OnConfigure(uint32_t mode) {
    switch (mode) {
      case ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE:
        top_level_->SetFrame(SurfaceFrameType::NONE);
        break;
      case ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE:
        top_level_->SetFrame(SurfaceFrameType::NORMAL);
        break;
    }
    zxdg_toplevel_decoration_v1_send_configure(resource_, mode);
  }

  const raw_ptr<wl_resource> resource_;
  raw_ptr<WaylandToplevel, DanglingUntriaged> top_level_;
  // Keeps track of the xdg-decoration mode on server side.
  uint32_t default_mode_ = ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
};

////////////////////////////////////////////////////////////////////////////////
// xdg_popup_interface:

// Wrapper around shell surface that allows us to handle the case where the
// xdg surface resource is destroyed before the popup resource.
class WaylandPopup : aura::WindowObserver {
 public:
  WaylandPopup(wl_resource* resource, wl_resource* surface_resource)
      : resource_(resource),
        surface_resource_(surface_resource),
        shell_surface_data_(
            GetUserDataAs<WaylandXdgSurface>(surface_resource)) {
    shell_surface_data_->shell_surface->host_window()->AddObserver(this);
    shell_surface_data_->shell_surface->set_close_callback(base::BindRepeating(
        &WaylandPopup::OnClose, weak_ptr_factory_.GetWeakPtr()));
    shell_surface_data_->shell_surface->set_configure_callback(
        base::BindRepeating(
            &HandleXdgSurfaceConfigureCallback, surface_resource,
            shell_surface_data_->serial_tracker,
            base::BindRepeating(&WaylandPopup::OnConfigure,
                                weak_ptr_factory_.GetWeakPtr())));
  }

  WaylandPopup(const WaylandPopup&) = delete;
  WaylandPopup& operator=(const WaylandPopup&) = delete;

  ~WaylandPopup() override {
    if (shell_surface_data_)
      shell_surface_data_->shell_surface->host_window()->RemoveObserver(this);
  }

  ShellSurfaceBase* GetShellSurface() {
    return shell_surface_data_->shell_surface.get();
  }

  void Grab() {
    if (!shell_surface_data_) {
      wl_resource_post_error(resource_.get(), XDG_POPUP_ERROR_INVALID_GRAB,
                             "the surface has already been destroyed");
      return;
    }
    if (shell_surface_data_->shell_surface->GetWidget()) {
      wl_resource_post_error(resource_.get(), XDG_POPUP_ERROR_INVALID_GRAB,
                             "grab must be called before construction");
      return;
    }
    shell_surface_data_->shell_surface->Grab();
  }

  void Reposition(WaylandPositioner* positioner, uint32_t token) {
    if (wl_resource_get_version(resource_) <
        XDG_POPUP_REPOSITIONED_SINCE_VERSION) {
      return;
    }
    xdg_popup_send_repositioned(resource_, token);

    display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(
            shell_surface_data_->shell_surface->GetWidget()
                ->parent()
                ->GetNativeWindow());
    gfx::Rect work_area = display.work_area();
    wm::ConvertRectFromScreen(shell_surface_data_->shell_surface->GetWidget()
                                  ->parent()
                                  ->GetNativeWindow(),
                              &work_area);
    WaylandPositioner::Result position = positioner->CalculateBounds(work_area);
    gfx::Point origin = position.origin;
    views::View::ConvertPointToScreen(
        shell_surface_data_->shell_surface->GetWidget()
            ->parent()
            ->widget_delegate()
            ->GetContentsView(),
        &origin);

    shell_surface_data_->shell_surface->SetOrigin(origin);
    shell_surface_data_->shell_surface->SetSize(position.size);

    xdg_popup_send_configure(resource_, position.origin.x(),
                             position.origin.y(), position.size.width(),
                             position.size.height());
    xdg_surface_send_configure(
        surface_resource_, shell_surface_data_->serial_tracker->GetNextSerial(
                               SerialTracker::EventType::OTHER_EVENT));
  }

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window->RemoveObserver(this);
    shell_surface_data_ = nullptr;
  }

 private:
  void OnClose() {
    xdg_popup_send_popup_done(resource_);
    wl_client_flush(wl_resource_get_client(resource_));
  }

  void OnConfigure(
      const gfx::Size& size,
      chromeos::WindowStateType state_type,
      bool resizing,
      bool activated,
      std::optional<chromeos::WindowStateType> restore_state_type) {
    // Nothing to do here as popups don't have additional configure state.
  }

  const raw_ptr<wl_resource> resource_;
  const raw_ptr<wl_resource, DanglingUntriaged> surface_resource_;
  raw_ptr<WaylandXdgSurface> shell_surface_data_;
  base::WeakPtrFactory<WaylandPopup> weak_ptr_factory_{this};
};

void xdg_popup_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_popup_grab(wl_client* client,
                    wl_resource* resource,
                    wl_resource* seat,
                    uint32_t serial) {
  GetUserDataAs<WaylandPopup>(resource)->Grab();
}

void xdg_popup_reposition(wl_client* client,
                          wl_resource* resource,
                          wl_resource* positioner,
                          uint32_t token) {
  GetUserDataAs<WaylandPopup>(resource)->Reposition(
      GetUserDataAs<WaylandPositioner>(positioner), token);
}

const struct xdg_popup_interface xdg_popup_implementation = {
    xdg_popup_destroy, xdg_popup_grab, xdg_popup_reposition};

////////////////////////////////////////////////////////////////////////////////
// xdg_surface_interface:

void xdg_surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_surface_get_toplevel(wl_client* client,
                              wl_resource* xdg_surface_resource,
                              uint32_t id) {
  auto* shell_surface_data =
      GetUserDataAs<WaylandXdgSurface>(xdg_surface_resource);
  if (shell_surface_data->shell_surface->GetEnabled()) {
    wl_resource_post_error(xdg_surface_resource,
                           XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED,
                           "surface has already been constructed");
    return;
  }

  shell_surface_data->shell_surface->SetCanMinimize(true);
  shell_surface_data->shell_surface->SetEnabled(true);

  wl_resource* xdg_toplevel_resource =
      wl_resource_create(client, &xdg_toplevel_interface, 1, id);

  SetImplementation(xdg_toplevel_resource, &xdg_toplevel_implementation,
                    std::make_unique<WaylandToplevel>(xdg_toplevel_resource,
                                                      xdg_surface_resource));
}

void xdg_surface_get_popup(wl_client* client,
                           wl_resource* resource,
                           uint32_t id,
                           wl_resource* parent_resource,
                           wl_resource* positioner_resource) {
  auto* shell_surface_data = GetUserDataAs<WaylandXdgSurface>(resource);
  if (shell_surface_data->shell_surface->GetEnabled()) {
    wl_resource_post_error(resource, XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED,
                           "surface has already been constructed");
    return;
  }

  if (!parent_resource) {
    wl_resource_post_error(resource, XDG_SURFACE_ERROR_NOT_CONSTRUCTED,
                           "popup parent not supplied");
    return;
  }

  auto* parent_data = GetUserDataAs<WaylandXdgSurface>(parent_resource);
  if (!parent_data->shell_surface->GetWidget()) {
    wl_resource_post_error(resource, XDG_SURFACE_ERROR_NOT_CONSTRUCTED,
                           "popup parent not constructed");
    return;
  }

  if (shell_surface_data->shell_surface->GetWidget()) {
    wl_resource_post_error(resource, XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED,
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
  WaylandPositioner::Result position = positioner->CalculateBounds(work_area);

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

  wl_resource* xdg_popup_resource = wl_resource_create(
      client, &xdg_popup_interface, wl_resource_get_version(resource), id);

  SetImplementation(
      xdg_popup_resource, &xdg_popup_implementation,
      std::make_unique<WaylandPopup>(xdg_popup_resource, resource));

  // We send the configure event here as this event needs x,y coordinates
  // relative to the parent window.
  xdg_popup_send_configure(xdg_popup_resource, position.origin.x(),
                           position.origin.y(), position.size.width(),
                           position.size.height());
}

void xdg_surface_set_window_geometry(wl_client* client,
                                     wl_resource* resource,
                                     int32_t x,
                                     int32_t y,
                                     int32_t width,
                                     int32_t height) {
  GetUserDataAs<WaylandXdgSurface>(resource)->shell_surface->SetGeometry(
      gfx::Rect(x, y, width, height));
}

void xdg_surface_ack_configure(wl_client* client,
                               wl_resource* resource,
                               uint32_t serial) {
  GetUserDataAs<WaylandXdgSurface>(resource)
      ->shell_surface->AcknowledgeConfigure(serial);
}

const struct xdg_surface_interface xdg_surface_implementation = {
    xdg_surface_destroy, xdg_surface_get_toplevel, xdg_surface_get_popup,
    xdg_surface_set_window_geometry, xdg_surface_ack_configure};

////////////////////////////////////////////////////////////////////////////////
// xdg_wm_base_interface:

void xdg_wm_base_destroy(wl_client* client, wl_resource* resource) {
  // Nothing to do here.
}

void xdg_wm_base_create_positioner(wl_client* client,
                                   wl_resource* resource,
                                   uint32_t id) {
  wl_resource* positioner_resource = wl_resource_create(
      client, &xdg_positioner_interface, wl_resource_get_version(resource), id);

  SetImplementation(positioner_resource, &xdg_positioner_implementation,
                    std::make_unique<WaylandPositioner>());
}

void xdg_wm_base_get_xdg_surface(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t id,
                                 wl_resource* surface) {
  auto* data = GetUserDataAs<WaylandXdgShell>(resource);
  std::unique_ptr<XdgShellSurface> shell_surface =
      data->display->CreateXdgShellSurface(GetUserDataAs<Surface>(surface));
  if (!shell_surface) {
    wl_resource_post_error(resource, XDG_WM_BASE_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  // Xdg shell surfaces are initially disabled and needs to be explicitly mapped
  // before they are enabled and can become visible.
  shell_surface->SetEnabled(false);

  shell_surface->SetSecurityDelegate(GetSecurityDelegate(client));

  std::unique_ptr<WaylandXdgSurface> wayland_shell_surface =
      std::make_unique<WaylandXdgSurface>(std::move(shell_surface),
                                          data->serial_tracker,
                                          data->rotation_serial_tracker);

  wl_resource* xdg_surface_resource = wl_resource_create(
      client, &xdg_surface_interface, wl_resource_get_version(resource), id);

  SetImplementation(xdg_surface_resource, &xdg_surface_implementation,
                    std::move(wayland_shell_surface));
}

void xdg_wm_base_pong(wl_client* client,
                      wl_resource* resource,
                      uint32_t serial) {
  NOTIMPLEMENTED();
}

const struct xdg_wm_base_interface xdg_wm_base_implementation = {
    xdg_wm_base_destroy, xdg_wm_base_create_positioner,
    xdg_wm_base_get_xdg_surface, xdg_wm_base_pong};

////////////////////////////////////////////////////////////////////////////////
// Top level decoration
void toplevel_decoration_handle_destroy(wl_client* client,
                                        wl_resource* resource) {
  wl_resource_destroy(resource);
}

void toplevel_decoration_handle_set_mode(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t mode) {
  GetUserDataAs<WaylandXdgToplevelDecoration>(resource)->SetDecorationMode(
      mode);
}

void toplevel_decoration_handle_unset_mode(wl_client* client,
                                           wl_resource* resource) {
  NOTIMPLEMENTED();
}

const struct zxdg_toplevel_decoration_v1_interface toplevel_decoration_impl = {
    .destroy = toplevel_decoration_handle_destroy,
    .set_mode = toplevel_decoration_handle_set_mode,
    .unset_mode = toplevel_decoration_handle_unset_mode,
};

// Decoration manager
void decoration_manager_handle_destroy(wl_client* client,
                                       wl_resource* manager_resource) {
  wl_resource_destroy(manager_resource);
}

void decoration_manager_handle_get_toplevel_decoration(
    wl_client* client,
    wl_resource* manager_resource,
    uint32_t id,
    wl_resource* toplevel_resource) {
  uint32_t version = wl_resource_get_version(manager_resource);
  wl_resource* decoration_resource = wl_resource_create(
      client, &zxdg_toplevel_decoration_v1_interface, version, id);
  if (!decoration_resource) {
    wl_client_post_no_memory(client);
    return;
  }

  auto xdg_toplevel_decoration = std::make_unique<WaylandXdgToplevelDecoration>(
      decoration_resource, toplevel_resource);

  SetImplementation(decoration_resource, &toplevel_decoration_impl,
                    std::move(xdg_toplevel_decoration));
}

static const struct zxdg_decoration_manager_v1_interface
    decoration_manager_impl = {
        .destroy = decoration_manager_handle_destroy,
        .get_toplevel_decoration =
            decoration_manager_handle_get_toplevel_decoration,
};

}  // namespace

WaylandXdgSurface ::WaylandXdgSurface(
    std::unique_ptr<XdgShellSurface> shell_surface,
    SerialTracker* const serial_tracker,
    SerialTracker* const rotation_serial_tracker)
    : shell_surface(std::move(shell_surface)),
      serial_tracker(serial_tracker),
      rotation_serial_tracker(rotation_serial_tracker) {}

WaylandXdgSurface::~WaylandXdgSurface() = default;

void bind_zxdg_decoration_manager(wl_client* client,
                                  void* data,
                                  uint32_t version,
                                  uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zxdg_decoration_manager_v1_interface, version, id);

  wl_resource_set_implementation(resource, &decoration_manager_impl, data,
                                 nullptr);
}

void bind_xdg_shell(wl_client* client,
                    void* data,
                    uint32_t version,
                    uint32_t id) {
  wl_resource* resource = wl_resource_create(client, &xdg_wm_base_interface,
                                             std::min(3u, version), id);

  wl_resource_set_implementation(resource, &xdg_wm_base_implementation, data,
                                 nullptr);
}

ShellSurfaceData GetShellSurfaceFromToplevelResource(wl_resource* resource) {
  auto* toplevel = GetUserDataAs<WaylandToplevel>(resource);
  return toplevel->GetShellSurfaceData();
}

ShellSurfaceBase* GetShellSurfaceFromPopupResource(wl_resource* resource) {
  auto* popup = GetUserDataAs<WaylandPopup>(resource);
  return popup->GetShellSurface();
}

}  // namespace wayland
}  // namespace exo
