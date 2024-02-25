// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wp_single_pixel_buffer.h"

#include <cstdint>
#include <memory>

#include "components/exo/buffer.h"
#include "components/exo/sub_surface.h"
#include "components/exo/sub_surface_observer.h"
#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "components/exo/wayland/server_util.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/size.h"

namespace exo::wayland {
namespace {

////////////////////////////////////////////////////////////////////////////////
// wl_buffer_interface:

void buffer_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_buffer_interface buffer_implementation = {buffer_destroy};

////////////////////////////////////////////////////////////////////////////////
// single_pixel_buffer_interface:

void single_pixel_buffer_destroy_manager(wl_client* client,
                                         wl_resource* resource) {
  wl_resource_destroy(resource);
}

void HandleBufferReleaseCallback(wl_resource* resource) {
  wl_buffer_send_release(resource);
  wl_client_flush(wl_resource_get_client(resource));
}

void create_u32_rgba_buffer(wl_client* client,
                            wl_resource* resource,
                            uint32_t id,
                            uint32_t r,
                            uint32_t g,
                            uint32_t b,
                            uint32_t a) {
  double dividor = UINT32_MAX;
  // TODO: consider moving SolidColorBuffer to use premultiplied instead.
  float alpha = static_cast<float>(a / dividor);
  if (alpha != 0) {
    dividor /= alpha;
  }
  SkColor4f color = {static_cast<float>(r / dividor),
                     static_cast<float>(g / dividor),
                     static_cast<float>(b / dividor), alpha};
  std::unique_ptr<SolidColorBuffer> buffer =
      std::make_unique<SolidColorBuffer>(color, gfx::Size(1, 1));
  wl_resource* buffer_resource = wl_resource_create(
      client, &wl_buffer_interface, wl_resource_get_version(resource), id);

  buffer->set_release_callback(base::BindRepeating(
      &HandleBufferReleaseCallback, base::Unretained(buffer_resource)));

  SetImplementation(buffer_resource, &buffer_implementation, std::move(buffer));
}

const struct wp_single_pixel_buffer_manager_v1_interface
    single_pixel_buffer_implementation = {single_pixel_buffer_destroy_manager,
                                          create_u32_rgba_buffer};

}  // namespace

void bind_single_pixel_buffer(wl_client* client,
                              void* data,
                              uint32_t version,
                              uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wp_single_pixel_buffer_manager_v1_interface,
                         std::min(version, kSinglePixelBufferVersion), id);

  wl_resource_set_implementation(resource, &single_pixel_buffer_implementation,
                                 data, nullptr);
}

}  // namespace exo::wayland
