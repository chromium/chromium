// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wl_seat.h"

#include "components/exo/keyboard.h"
#include "components/exo/pointer.h"
#include "components/exo/touch.h"
#include "components/exo/wayland/serial_tracker.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_keyboard_delegate.h"
#include "components/exo/wayland/wayland_pointer_delegate.h"
#include "components/exo/wayland/wayland_touch_delegate.h"
#include "ui/base/buildflags.h"

namespace exo::wayland {

namespace {

////////////////////////////////////////////////////////////////////////////////
// wl_pointer_interface:

void pointer_set_cursor(wl_client* client,
                        wl_resource* resource,
                        uint32_t serial,
                        wl_resource* surface_resource,
                        int32_t hotspot_x,
                        int32_t hotspot_y) {
  GetUserDataAs<Pointer>(resource)->SetCursor(
      surface_resource ? GetUserDataAs<Surface>(surface_resource) : nullptr,
      gfx::Point(hotspot_x, hotspot_y));
}

void pointer_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_pointer_interface pointer_implementation = {pointer_set_cursor,
                                                            pointer_release};

////////////////////////////////////////////////////////////////////////////////
// wl_keyboard_interface:

#if BUILDFLAG(USE_XKBCOMMON)

void keyboard_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_keyboard_interface keyboard_implementation = {keyboard_release};

#endif  // BUILDFLAG(USE_XKBCOMMON)
////////////////////////////////////////////////////////////////////////////////
// wl_touch_interface:

void touch_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_touch_interface touch_implementation = {touch_release};

////////////////////////////////////////////////////////////////////////////////
// wl_seat_interface:

void seat_get_pointer(wl_client* client, wl_resource* resource, uint32_t id) {
  auto* data = GetUserDataAs<WaylandSeat>(resource);

  wl_resource* pointer_resource = wl_resource_create(
      client, &wl_pointer_interface, wl_resource_get_version(resource), id);

  SetImplementation(
      pointer_resource, &pointer_implementation,
      std::make_unique<Pointer>(
          new WaylandPointerDelegate(pointer_resource, data->serial_tracker),
          data->seat));
}

void seat_get_keyboard(wl_client* client, wl_resource* resource, uint32_t id) {
#if BUILDFLAG(USE_XKBCOMMON)
  auto* data = GetUserDataAs<WaylandSeat>(resource);

  uint32_t version = wl_resource_get_version(resource);
  wl_resource* keyboard_resource =
      wl_resource_create(client, &wl_keyboard_interface, version, id);

  auto keyboard =
      std::make_unique<Keyboard>(std::make_unique<WaylandKeyboardDelegate>(
                                     keyboard_resource, data->serial_tracker),
                                 data->seat);
  SetImplementation(keyboard_resource, &keyboard_implementation,
                    std::move(keyboard));
#else
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(USE_XKBCOMMON)
}

void seat_get_touch(wl_client* client, wl_resource* resource, uint32_t id) {
  auto* data = GetUserDataAs<WaylandSeat>(resource);

  wl_resource* touch_resource = wl_resource_create(
      client, &wl_touch_interface, wl_resource_get_version(resource), id);

  SetImplementation(
      touch_resource, &touch_implementation,
      std::make_unique<Touch>(
          new WaylandTouchDelegate(touch_resource, data->serial_tracker),
          data->seat));
}

void seat_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_seat_interface seat_implementation = {
    seat_get_pointer, seat_get_keyboard, seat_get_touch, seat_release};

}  // namespace

void bind_seat(wl_client* client, void* data, uint32_t version, uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &wl_seat_interface, std::min(version, kWlSeatVersion), id);

  wl_resource_set_implementation(resource, &seat_implementation, data, nullptr);

  if (version >= WL_SEAT_NAME_SINCE_VERSION)
    wl_seat_send_name(resource, "default");
  uint32_t capabilities = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_TOUCH;

#if BUILDFLAG(USE_XKBCOMMON)
  capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
#endif  // BUILDFLAG(USE_XKBCOMMON)
  wl_seat_send_capabilities(resource, capabilities);
}

}  // namespace exo::wayland
