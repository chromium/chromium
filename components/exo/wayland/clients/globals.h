// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_CLIENTS_GLOBALS_H_
#define COMPONENTS_EXO_WAYLAND_CLIENTS_GLOBALS_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "components/exo/wayland/clients/client_helper.h"

namespace exo::wayland::clients {

struct Globals {
  Globals();
  ~Globals();

  // Initializes the members. `versions` optionally specify the maximum
  // versions to use for corresponding interfaces.
  void Init(wl_display* display,
            base::flat_map<std::string, uint32_t> in_requested_versions);

  std::unique_ptr<wl_registry> registry;

  std::unique_ptr<wl_output> output;
  std::unique_ptr<wl_compositor> compositor;
  std::unique_ptr<wl_shm> shm;
  std::unique_ptr<wp_presentation> presentation;
  std::unique_ptr<zwp_linux_dmabuf_v1> linux_dmabuf;
  std::unique_ptr<wl_shell> shell;
  std::unique_ptr<wl_seat> seat;
  std::unique_ptr<wl_subcompositor> subcompositor;
  std::unique_ptr<wl_touch> touch;
  std::unique_ptr<zaura_shell> aura_shell;
  std::unique_ptr<zaura_output> aura_output;
  std::unique_ptr<zxdg_shell_v6> xdg_shell_v6;
  std::unique_ptr<xdg_wm_base> xdg_wm_base;
  std::unique_ptr<zwp_fullscreen_shell_v1> fullscreen_shell;
  std::unique_ptr<zwp_input_timestamps_manager_v1> input_timestamps_manager;
  std::unique_ptr<zwp_linux_explicit_synchronization_v1>
      linux_explicit_synchronization;
  std::unique_ptr<zcr_vsync_feedback_v1> vsync_feedback;
  std::unique_ptr<zcr_color_manager_v1> color_manager;
  std::unique_ptr<zcr_stylus_v2> stylus;
  std::unique_ptr<zcr_remote_shell_v1> cr_remote_shell_v1;
  std::unique_ptr<zcr_remote_shell_v2> cr_remote_shell_v2;

  base::flat_map<std::string, uint32_t> requested_versions;
};

}  // namespace exo::wayland::clients

#endif  // COMPONENTS_EXO_WAYLAND_CLIENTS_GLOBALS_H_
