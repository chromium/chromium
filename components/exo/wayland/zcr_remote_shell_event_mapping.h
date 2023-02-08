// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZCR_REMOTE_SHELL_EVENT_MAPPING_H_
#define COMPONENTS_EXO_WAYLAND_ZCR_REMOTE_SHELL_EVENT_MAPPING_H_

#include <stdint.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "base/functional/callback.h"

// These structs are populated with function pointers and values from the v1 and
// v2 remote shell protocol.
struct WaylandRemoteOutputEventMapping {
  void (*send_identification_data)(struct wl_resource* resource_,
                                   struct wl_array* identification_data);
  void (*send_display_id)(struct wl_resource* resource_,
                          uint32_t display_id_hi,
                          uint32_t display_id_lo);
  void (*send_port)(struct wl_resource* resource_, uint32_t port);
  void (*send_insets)(struct wl_resource* resource_,
                      int32_t inset_left,
                      int32_t inset_top,
                      int32_t inset_right,
                      int32_t inset_bottom);
  void (*send_stable_insets)(struct wl_resource* resource_,
                             int32_t stable_inset_left,
                             int32_t stable_inset_top,
                             int32_t stable_inset_right,
                             int32_t stable_inset_bottom);
  void (*send_systemui_behavior)(struct wl_resource* resource_,
                                 int32_t systemui_behavior);
  int system_ui_behavior_since_version;
  int stable_insets_since_version;
};

struct WaylandRemoteShellEventMapping {
  void (*send_window_geometry_changed)(struct wl_resource* resource_,
                                       int32_t x,
                                       int32_t y,
                                       int32_t width,
                                       int32_t height);
  void (*send_change_zoom_level)(struct wl_resource* resource_, int32_t change);
  void (*send_state_type_changed)(struct wl_resource* resource_,
                                  uint32_t state_type);
  void (*send_bounds_changed_in_output)(struct wl_resource* resource_,
                                        struct wl_resource* output,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height,
                                        uint32_t bounds_change_reason);
  void (*send_bounds_changed)(struct wl_resource* resource_,
                              uint32_t display_id_hi,
                              uint32_t display_id_lo,
                              int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height,
                              uint32_t bounds_change_reason);
  void (*send_activated)(struct wl_resource* resource_,
                         struct wl_resource* gained_active,
                         struct wl_resource* lost_active);
  void (*send_desktop_focus_state_changed)(struct wl_resource* resource_,
                                           uint32_t focus_state);
  void (*send_workspace_info)(struct wl_resource* resource_,
                              uint32_t display_id_hi,
                              uint32_t display_id_lo,
                              int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height,
                              int32_t inset_left,
                              int32_t inset_top,
                              int32_t inset_right,
                              int32_t inset_bottom,
                              int32_t stable_inset_left,
                              int32_t stable_inset_top,
                              int32_t stable_inset_right,
                              int32_t stable_inset_bottom,
                              int32_t systemui_visibility,
                              int32_t transform,
                              uint32_t is_internal,
                              struct wl_array* identification_data);
  void (*send_drag_finished)(struct wl_resource* resource_,
                             int32_t x,
                             int32_t y,
                             int32_t canceled);
  void (*send_drag_started)(struct wl_resource* resource_, uint32_t direction);
  void (*send_layout_mode)(struct wl_resource* resource_, uint32_t layout_mode);
  void (*send_default_device_scale_factor)(struct wl_resource* resource_,
                                           int32_t scale);
  void (*send_configure)(struct wl_resource* resource_, uint32_t layout_mode);

  int bounds_changed_in_output_since_version;
  int desktop_focus_state_changed_since_version;
  int layout_mode_since_version;
  int default_device_scale_factor_since_version;
  int change_zoom_level_since_version;
  int send_workspace_info_since_version;
  int set_use_default_scale_cancellation_since_version;
  bool has_bounds_change_reason_float;
};

#endif  // COMPONENTS_EXO_WAYLAND_ZCR_REMOTE_SHELL_EVENT_MAPPING_H_
