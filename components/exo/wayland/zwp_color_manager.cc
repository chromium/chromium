// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zwp_color_manager.h"

#include <color-management-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>

#include "base/notreached.h"

namespace exo {
namespace wayland {

namespace {

////////////////////////////////////////////////////////////////////////////////
// zwp_color_management_output_v1_interface:

void color_space_get_information(struct wl_client* client,
                                 struct wl_resource* resource) {
  NOTIMPLEMENTED();
}

void color_space_destroy(struct wl_client* client,
                         struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zwp_color_space_v1_interface color_space_v1_implementation = {
    color_space_get_information, color_space_destroy};

////////////////////////////////////////////////////////////////////////////////
// zwp_color_management_output_v1_interface:

void color_management_output_get_color_space(struct wl_client* client,
                                             struct wl_resource* resource,
                                             uint32_t id) {
  wl_resource* color_space_resource =
      wl_resource_create(client, &zwp_color_space_v1_interface, 1, id);

  wl_resource_set_implementation(color_space_resource,
                                 &color_space_v1_implementation,
                                 /*data=*/nullptr, /*destroy=*/nullptr);
}

void color_management_output_destroy(struct wl_client* client,
                                     struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zwp_color_management_output_v1_interface
    color_management_output_v1_implementation = {
        color_management_output_get_color_space,
        color_management_output_destroy};

////////////////////////////////////////////////////////////////////////////////
// zwp_color_management_surface_v1_interface:

void color_management_surface_set_alpha_mode(struct wl_client* client,
                                             struct wl_resource* resource,
                                             uint32_t alpha_mode) {
  NOTIMPLEMENTED();
}

void color_management_surface_set_extended_dynamic_range(
    struct wl_client* client,
    struct wl_resource* resource,
    uint32_t value) {
  NOTIMPLEMENTED();
}
void color_management_surface_set_color_space(struct wl_client* client,
                                              struct wl_resource* resource,
                                              struct wl_resource* color_space,
                                              uint32_t render_intent) {
  NOTIMPLEMENTED();
}
void color_management_surface_set_default_color_space(
    struct wl_client* client,
    struct wl_resource* resource) {
  NOTIMPLEMENTED();
}
void color_management_surface_destroy(struct wl_client* client,
                                      struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zwp_color_management_surface_v1_interface
    color_management_surface_v1_implementation = {
        color_management_surface_set_alpha_mode,
        color_management_surface_set_extended_dynamic_range,
        color_management_surface_set_color_space,
        color_management_surface_set_default_color_space,
        color_management_surface_destroy};

////////////////////////////////////////////////////////////////////////////////
// zwp_color_manager_v1_interface:

void color_manager_create_color_space_from_icc(struct wl_client* client,
                                               struct wl_resource* resource,
                                               uint32_t id,
                                               int32_t icc) {
  NOTIMPLEMENTED();
}

void color_manager_create_color_space_from_names(struct wl_client* client,
                                                 struct wl_resource* resource,
                                                 uint32_t id,
                                                 uint32_t eotf,
                                                 uint32_t chromaticity,
                                                 uint32_t whitepoint) {
  wl_resource* color_space_resource =
      wl_resource_create(client, &zwp_color_space_v1_interface, 1, id);

  wl_resource_set_implementation(color_space_resource,
                                 &color_space_v1_implementation,
                                 /*data=*/nullptr, /*destroy=*/nullptr);
}

void color_manager_create_color_space_from_params(struct wl_client* client,
                                                  struct wl_resource* resource,
                                                  uint32_t id,
                                                  uint32_t eotf,
                                                  uint32_t primary_r_x,
                                                  uint32_t primary_r_y,
                                                  uint32_t primary_g_x,
                                                  uint32_t primary_g_y,
                                                  uint32_t primary_b_x,
                                                  uint32_t primary_b_y,
                                                  uint32_t white_point_x,
                                                  uint32_t white_point_y) {
  NOTIMPLEMENTED();
}

void color_manager_get_color_management_output(struct wl_client* client,
                                               struct wl_resource* resource,
                                               uint32_t id,
                                               struct wl_resource* output) {
  wl_resource* color_management_output_resource = wl_resource_create(
      client, &zwp_color_management_output_v1_interface, 1, id);

  wl_resource_set_implementation(color_management_output_resource,
                                 &color_management_output_v1_implementation,
                                 /*data=*/nullptr, /*destroy=*/nullptr);
}

void color_manager_get_color_management_surface(struct wl_client* client,
                                                struct wl_resource* resource,
                                                uint32_t id,
                                                struct wl_resource* surface) {
  wl_resource* color_management_surface_resource = wl_resource_create(
      client, &zwp_color_management_surface_v1_interface, 1, id);

  wl_resource_set_implementation(color_management_surface_resource,
                                 &color_management_surface_v1_implementation,
                                 /*data=*/nullptr, /*destroy=*/nullptr);
}

void color_manager_destroy(struct wl_client* client,
                           struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zwp_color_manager_v1_interface color_manager_v1_implementation = {
    color_manager_create_color_space_from_icc,
    color_manager_create_color_space_from_names,
    color_manager_create_color_space_from_params,
    color_manager_get_color_management_output,
    color_manager_get_color_management_surface,
    color_manager_destroy};
}  // namespace

void bind_zwp_color_manager(wl_client* client,
                            void* data,
                            uint32_t version,
                            uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zwp_color_manager_v1_interface, version, id);

  wl_resource_set_implementation(resource, &color_manager_v1_implementation,
                                 data, /*destroy=*/nullptr);
}

}  // namespace wayland
}  // namespace exo
