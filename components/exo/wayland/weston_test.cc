// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/weston_test.h"

#include <wayland-server-core.h>
#include <weston-test-server-protocol.h>
#include <algorithm>

#include "base/notreached.h"

namespace exo {
namespace wayland {
namespace {

static void weston_test_move_surface(struct wl_client* client,
                                     struct wl_resource* resource,
                                     struct wl_resource* surface_resource,
                                     int32_t x,
                                     int32_t y) {
  NOTIMPLEMENTED();
}

static void weston_test_move_pointer(struct wl_client* client,
                                     struct wl_resource* resource,
                                     struct wl_resource* surface_resource,
                                     uint32_t tv_sec_hi,
                                     uint32_t tv_sec_lo,
                                     uint32_t tv_nsec,
                                     int32_t x,
                                     int32_t y) {
  NOTIMPLEMENTED();
}

static void weston_test_send_button(struct wl_client* client,
                                    struct wl_resource* resource,
                                    uint32_t tv_sec_hi,
                                    uint32_t tv_sec_lo,
                                    uint32_t tv_nsec,
                                    int32_t button,
                                    uint32_t state) {
  NOTIMPLEMENTED();
}

static void weston_test_reset_pointer(struct wl_client* client,
                                      struct wl_resource* resource) {
  NOTIMPLEMENTED();
}

static void weston_test_send_axis(struct wl_client* client,
                                  struct wl_resource* resource,
                                  uint32_t tv_sec_hi,
                                  uint32_t tv_sec_lo,
                                  uint32_t tv_nsec,
                                  uint32_t axis,
                                  wl_fixed_t value) {
  NOTIMPLEMENTED();
}

static void weston_test_activate_surface(struct wl_client* client,
                                         struct wl_resource* resource,
                                         struct wl_resource* surface_resource) {
  NOTIMPLEMENTED();
}

static void weston_test_send_key(struct wl_client* client,
                                 struct wl_resource* resource,
                                 uint32_t tv_sec_hi,
                                 uint32_t tv_sec_lo,
                                 uint32_t tv_nsec,
                                 uint32_t key,
                                 uint32_t state) {
  NOTIMPLEMENTED();
}

static void weston_test_device_release(struct wl_client* client,
                                       struct wl_resource* resource,
                                       const char* device) {
  NOTIMPLEMENTED();
}

static void weston_test_device_add(struct wl_client* client,
                                   struct wl_resource* resource,
                                   const char* device) {
  NOTIMPLEMENTED();
}

static void weston_test_capture_screenshot(struct wl_client* client,
                                           struct wl_resource* resource,
                                           struct wl_resource* output,
                                           struct wl_resource* buffer) {
  NOTIMPLEMENTED();
}

static void weston_test_send_touch(struct wl_client* client,
                                   struct wl_resource* resource,
                                   uint32_t tv_sec_hi,
                                   uint32_t tv_sec_lo,
                                   uint32_t tv_nsec,
                                   int32_t touch_id,
                                   wl_fixed_t x,
                                   wl_fixed_t y,
                                   uint32_t touch_type) {
  NOTIMPLEMENTED();
}

const struct weston_test_interface weston_test_implementation = {
    weston_test_move_surface, weston_test_move_pointer,
    weston_test_send_button,  weston_test_reset_pointer,
    weston_test_send_axis,    weston_test_activate_surface,
    weston_test_send_key,     weston_test_device_release,
    weston_test_device_add,   weston_test_capture_screenshot,
    weston_test_send_touch};

}  // namespace

void bind_weston_test(wl_client* client,
                      void* data,
                      uint32_t version,
                      uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &weston_test_interface, version, id);

  wl_resource_set_implementation(resource, &weston_test_implementation, nullptr,
                                 nullptr);
}

}  // namespace wayland
}  // namespace exo
