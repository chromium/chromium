// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/shell_client_data.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "components/exo/wayland/test/client_util.h"
#include "extended-drag-unstable-v1-client-protocol.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace exo::wayland::test {

namespace {

// xdg_shell listener callbacks:
void OnXdgToplevelClose(void* data, struct xdg_toplevel* toplevel) {
  static_cast<ShellClientData*>(data)->Close();
}

// zcr_remote_surface_v2 listener callbacks:
void OnRemoteSurfaceClose(void* data, zcr_remote_surface_v2*) {
  static_cast<ShellClientData*>(data)->Close();
}

gfx::PointF ToPointF(wl_fixed_t x, wl_fixed_t y) {
  return gfx::PointF(wl_fixed_to_double(x), wl_fixed_to_double(y));
}

// wl_pointer_listener callbacks:
void OnEnter(void* data,
             wl_pointer* obj,
             uint32_t serial,
             wl_surface* surface,
             wl_fixed_t surface_x,
             wl_fixed_t surface_y) {
  auto* listener = static_cast<ShellClientData*>(data)->input_listener();
  if (listener) {
    listener->OnEnter(serial, surface, ToPointF(surface_x, surface_y));
  }
}

void OnLeave(void* data,
             wl_pointer* obj,
             uint32_t serial,
             wl_surface* surface) {
  auto* listener = static_cast<ShellClientData*>(data)->input_listener();
  if (listener) {
    listener->OnLeave(serial, surface);
  }
}

void OnMotion(void* data,
              wl_pointer* obj,
              uint32_t time,
              wl_fixed_t surface_x,
              wl_fixed_t surface_y) {
  auto* listener = static_cast<ShellClientData*>(data)->input_listener();
  if (listener) {
    listener->OnMotion(ToPointF(surface_x, surface_y));
  }
}

void OnButton(void* data,
              wl_pointer* obj,
              uint32_t serial,
              uint32_t time,
              uint32_t button,
              uint32_t state) {
  auto* listener = static_cast<ShellClientData*>(data)->input_listener();
  if (listener) {
    if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
      listener->OnButtonPressed(serial, button);
    } else {
      listener->OnButtonReleased(serial, button);
    }
  }
}

// wl_tocuh_listener callback.
void OnTouchDown(void* data,
                 wl_touch* touch,
                 uint32_t serial,
                 uint32_t time,
                 struct wl_surface* surface,
                 int32_t id,
                 wl_fixed_t x,
                 wl_fixed_t y) {
  auto* listener = static_cast<ShellClientData*>(data)->input_listener();
  if (listener) {
    listener->OnTouchDown(serial, surface, id, ToPointF(x, y));
  }
}

void OnTouchUp(void* data,
               wl_touch* touch,
               uint32_t serial,
               uint32_t time,
               int32_t id) {
  auto* listener = static_cast<ShellClientData*>(data)->input_listener();
  if (listener) {
    listener->OnTouchUp(serial, id);
  }
}

void OnDataOffer(void* data,
                 wl_data_device* data_device,
                 wl_data_offer* offer) {
  static_cast<ShellClientData*>(data)->set_data_offer(
      std::unique_ptr<wl_data_offer, decltype(&wl_data_offer_destroy)>(
          offer, &wl_data_offer_destroy));
}

}  // namespace

ShellClientData::ShellClientData(test::TestClient* client)
    : client_(client),
      pointer_(nullptr, &wl_pointer_destroy),
      touch_(nullptr, &wl_touch_destroy),
      surface_(wl_compositor_create_surface(client->compositor()),
               &wl_surface_destroy),
      xdg_surface_(nullptr, &xdg_surface_destroy),
      xdg_toplevel_(nullptr, &xdg_toplevel_destroy),
      aura_toplevel_(nullptr, &zaura_toplevel_destroy),
      remote_surface_(nullptr, &zcr_remote_surface_v2_destroy),
      data_device_(nullptr, &wl_data_device_destroy),
      data_source_(nullptr, &wl_data_source_destroy),
      data_offer_(nullptr, &wl_data_offer_destroy) {
  data_device_.reset(wl_data_device_manager_get_data_device(
      client_->globals().data_device_manager.get(),
      client_->globals().seat.get()));
  constexpr wl_data_device_listener kDataDeviceListener = {
      .data_offer = &OnDataOffer,
      .enter = [](void* data, wl_data_device* data_device, uint32_t serial,
                  wl_surface* surface, wl_fixed_t x, wl_fixed_t y,
                  wl_data_offer* offer) {},
      .leave = [](void*, wl_data_device*) {},
      .motion = [](void*, wl_data_device*, uint32_t, wl_fixed_t, wl_fixed_t) {},
      .drop = [](void*, wl_data_device*) {},
      .selection = [](void*, wl_data_device*, wl_data_offer*) {},
  };
  wl_data_device_add_listener(data_device_.get(), &kDataDeviceListener, this);

  pointer_.reset(wl_seat_get_pointer(client_->globals().seat.get()));
  constexpr wl_pointer_listener kPointerListener = {
      .enter = &OnEnter,
      .leave = &OnLeave,
      .motion = &OnMotion,
      .button = &OnButton,
      .axis = [](void*, wl_pointer*, uint32_t, uint32_t, wl_fixed_t) {},
      .frame = [](void*, wl_pointer*) {},
      .axis_source = [](void*, wl_pointer*, uint32_t) {},
      .axis_stop = [](void*, wl_pointer*, uint32_t, uint32_t) {},
      .axis_discrete = [](void*, wl_pointer*, uint32_t, int32_t) {},
      .axis_value120 = [](void*, wl_pointer*, uint32_t, int32_t) {},
  };
  wl_pointer_add_listener(pointer_.get(), &kPointerListener, this);

  touch_.reset(wl_seat_get_touch(client_->globals().seat.get()));
  constexpr wl_touch_listener kTouchListener = {
      .down = &OnTouchDown,
      .up = &OnTouchUp,
      .motion = [](void*, wl_touch*, uint32_t, int32_t, wl_fixed_t,
                   wl_fixed_t) {},
      .frame = [](void*, wl_touch*) {},
      .cancel = [](void*, wl_touch*) {},
      .shape = [](void*, wl_touch*, int32_t, wl_fixed_t, wl_fixed_t) {},
      .orientation = [](void*, wl_touch*, int32_t, wl_fixed_t) {},
  };
  wl_touch_add_listener(touch_.get(), &kTouchListener, this);
}

ShellClientData::~ShellClientData() {
  Close();
}

void ShellClientData::CreateXdgToplevel() {
  constexpr xdg_toplevel_listener kXdgToplevelListener = {
      [](void*, xdg_toplevel*, int32_t, int32_t, wl_array*) {},
      &OnXdgToplevelClose,
      [](void*, xdg_toplevel*, int32_t, int32_t) {},
      [](void*, xdg_toplevel*, wl_array*) {},
  };

  xdg_surface_.reset(
      xdg_wm_base_get_xdg_surface(client_->xdg_wm_base(), surface_.get()));
  xdg_toplevel_.reset(xdg_surface_get_toplevel(xdg_surface_.get()));
  xdg_toplevel_add_listener(xdg_toplevel_.get(), &kXdgToplevelListener, this);

  aura_toplevel_.reset(zaura_shell_get_aura_toplevel_for_xdg_toplevel(
      client_->aura_shell(), xdg_toplevel_.get()));
}

void ShellClientData::CreateRemoteSurface() {
  constexpr zcr_remote_surface_v2_listener kRemoteSurfaceV2Listener = {
      &OnRemoteSurfaceClose,
      [](void*, zcr_remote_surface_v2*, uint32_t) {},
      [](void*, zcr_remote_surface_v2*, int, int, int, int) {},
      [](void*, zcr_remote_surface_v2*, uint32_t, uint32_t, int32_t, int32_t,
         int32_t, int32_t, uint32_t) {},
      [](void*, zcr_remote_surface_v2*, uint32_t) {},
      [](void*, zcr_remote_surface_v2*, int32_t, int32_t, int32_t) {},
      [](void*, zcr_remote_surface_v2*, int32_t) {},
      [](void*, zcr_remote_surface_v2*, wl_output*, int32_t, int32_t, int32_t,
         int32_t, uint32_t) {},
  };

  remote_surface_.reset(zcr_remote_shell_v2_get_remote_surface(
      client_->cr_remote_shell_v2(), surface_.get(),
      ZCR_REMOTE_SHELL_V2_CONTAINER_DEFAULT));
  zcr_remote_surface_v2_add_listener(remote_surface_.get(),
                                     &kRemoteSurfaceV2Listener, this);
}

void ShellClientData::Pin() {
  zcr_remote_surface_v2_pin(remote_surface_.get(), /*trusted=*/true);
}

void ShellClientData::UnsetSnap() {
  zaura_toplevel_unset_snap(aura_toplevel_.get());
}

void ShellClientData::CreateAndAttachBuffer(const gfx::Size& size) {
  buffer_ = client_->shm_buffer_factory()->CreateBuffer(0, size.width(),
                                                        size.height());
  wl_surface_attach(surface_.get(), buffer_->resource(), 0, 0);
}

void ShellClientData::Commit() {
  wl_surface_commit(surface_.get());
}

void ShellClientData::DestroySurface() {
  zcr_remote_surface_v2_destroy(remote_surface_.release());
  wl_surface_destroy(surface_.release());
}

void ShellClientData::RequestWindowBounds(const gfx::Rect& bounds,
                                          wl_output* target_output) {
  zaura_toplevel_set_window_bounds(aura_toplevel_.get(), bounds.x(), bounds.y(),
                                   bounds.width(), bounds.height(),
                                   target_output);
}

void ShellClientData::StartDrag(uint32_t serial) {
  DCHECK(!data_source_);
  data_source_.reset(wl_data_device_manager_create_data_source(
      client_->globals().data_device_manager.get()));
  constexpr wl_data_source_listener kDataSourceListener = {
      .target = [](void* data, wl_data_source*, const char*) {},
      .send = [](void* data, wl_data_source*, const char*, int32_t) {},
      .cancelled =
          [](void* data, wl_data_source*) {
            static_cast<ShellClientData*>(data)->DestroyDataSource();
          },
      .dnd_drop_performed =
          [](void* data, wl_data_source*) {
            static_cast<ShellClientData*>(data)->DestroyDataSource();
          },
      .dnd_finished = [](void* data, wl_data_source*) {},
      .action = [](void* data, wl_data_source*, uint32_t) {},
  };
  wl_data_source_add_listener(data_source_.get(), &kDataSourceListener, this);

  wl_data_device_start_drag(data_device_.get(), data_source_.get(),
                            surface_.get(), nullptr, serial);
}

void ShellClientData::DestroyDataSource() {
  data_source_.reset();
}

void ShellClientData::Close() {
  close_called_ = true;
  if (surface_) {
    wl_surface_attach(surface_.get(), nullptr, 0, 0);
  }
  if (buffer_) {
    wl_buffer_destroy(buffer_->GetResourceAndRelease());
  }
  buffer_.reset();
  xdg_toplevel_.reset();
  xdg_surface_.reset();
  aura_toplevel_.reset();
  remote_surface_.reset();
  surface_.reset();
  pointer_.reset();
  touch_.reset();
  data_offer_.reset();
  data_source_.reset();
  data_device_.reset();
}

ResourceKey ShellClientData::GetSurfaceResourceKey() const {
  return test::client_util::GetResourceKey(surface_.get());
}

}  // namespace exo::wayland::test
