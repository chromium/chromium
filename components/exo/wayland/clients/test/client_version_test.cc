// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/clients/test/client_version_test.h"

#include <alpha-compositing-unstable-v1-server-protocol.h>
#include <cursor-shapes-unstable-v1-server-protocol.h>
#include <extended-drag-unstable-v1-server-protocol.h>
#include <gaming-input-unstable-v2-server-protocol.h>
#include <keyboard-configuration-unstable-v1-server-protocol.h>
#include <keyboard-extension-unstable-v1-server-protocol.h>
#include <notification-shell-unstable-v1-server-protocol.h>
#include <pointer-constraints-unstable-v1-server-protocol.h>
#include <pointer-gestures-unstable-v1-server-protocol.h>
#include <relative-pointer-unstable-v1-server-protocol.h>
#include <remote-shell-unstable-v1-server-protocol.h>
#include <secure-output-unstable-v1-server-protocol.h>
#include <stylus-tools-unstable-v1-server-protocol.h>
#include <stylus-unstable-v2-server-protocol.h>
#include <text-input-unstable-v1-server-protocol.h>
#include <viewporter-client-protocol.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <xdg-decoration-unstable-v1-server-protocol.h>
#include <xdg-shell-server-protocol.h>
#include <xdg-shell-unstable-v6-server-protocol.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/check_op.h"
#include "base/logging.h"

namespace exo {
namespace wayland {
namespace clients {
namespace {

struct Deleter {
  void operator()(void* obj) { free(obj); }
};

void RegistryHandler(void* data,
                     wl_registry* registry,
                     uint32_t id,
                     const char* interface,
                     uint32_t version) {
  ClientVersionTest::Globals* globals =
      static_cast<ClientVersionTest::Globals*>(data);
  if (globals->protocol_tested.length() == 0) {
    globals->protocols.push_back(interface);
    return;
  } else if (strcmp(interface, globals->protocol_tested.c_str()) == 0) {
    switch (globals->validity_type) {
      case ClientVersionTest::VersionValidityType::INVALID_NULL:
        version = 0;
        break;
      case ClientVersionTest::VersionValidityType::VALID_SKEWED:
        if (version > 1) {
          version = version - 1;
        }
        break;
      case ClientVersionTest::VersionValidityType::INVALID_UNSUPPORTED:
        version = version + 1;
        break;
      case ClientVersionTest::VersionValidityType::VALID_ADVERTISED:
      default:
        // Use advertised
        break;
    }
  }
  std::unordered_map<std::string, const struct wl_interface*> interfaces = {
      {"wl_compositor", &wl_compositor_interface},
      {"wl_shm", &wl_shm_interface},
      {"wl_shell", &wl_shell_interface},
      {"wl_seat", &wl_seat_interface},
      {"wp_presentation", &wp_presentation_interface},
      {"zaura_shell", &zaura_shell_interface},
      {"zwp_linux_dmabuf_v1", &zwp_linux_dmabuf_v1_interface},
      {"wl_subcompositor", &wl_subcompositor_interface},
      {"zwp_input_timestamps_manager_v1",
       &zwp_input_timestamps_manager_v1_interface},
      {"zwp_fullscreen_shell_v1", &zwp_fullscreen_shell_v1_interface},
      {"wl_output", &wl_output_interface},
      {"zwp_linux_explicit_synchronization_v1",
       &zwp_linux_explicit_synchronization_v1_interface},
      {"zcr_vsync_feedback_v1", &zcr_vsync_feedback_v1_interface},
      {"zcr_color_space_v1", &zcr_color_space_v1_interface},
      {"wl_data_device_manager", &wl_data_device_manager_interface},
      {"wp_viewporter", &wp_viewporter_interface},
      {"zxdg_shell_v6", &zxdg_shell_v6_interface},
      {"xdg_wm_base", &xdg_wm_base_interface},
      {"zwp_text_input_manager_v1", &zwp_text_input_manager_v1_interface},
      {"zcr_secure_output_v1", &zcr_secure_output_v1_interface},
      {"zcr_alpha_compositing_v1", &zcr_alpha_compositing_v1_interface},
      {"zcr_stylus_v2", &zcr_stylus_v2_interface},
      {"zcr_cursor_shapes_v1", &zcr_cursor_shapes_v1_interface},
      {"zcr_gaming_input_v2", &zcr_gaming_input_v2_interface},
      {"zcr_keyboard_configuration_v1",
       &zcr_keyboard_configuration_v1_interface},
      {"zcr_keyboard_extension_v1", &zcr_keyboard_extension_v1_interface},
      {"zcr_notification_shell_v1", &zcr_notification_shell_v1_interface},
      {"zcr_remote_shell_v1", &zcr_remote_shell_v1_interface},
      {"zcr_stylus_tools_v1", &zcr_stylus_tools_v1_interface},
      {"zwp_pointer_gestures_v1", &zwp_pointer_gestures_v1_interface},
      {"zwp_pointer_constraints_v1", &zwp_pointer_constraints_v1_interface},
      {"zwp_relative_pointer_manager_v1",
       &zwp_relative_pointer_manager_v1_interface},
      {"zxdg_decoration_manager_v1", &zxdg_decoration_manager_v1_interface},
      {"zcr_extended_drag_v1", &zcr_extended_drag_v1_interface},
  };
  if (strcmp(interface, "wl_compositor") == 0) {
    globals->compositor.reset(static_cast<wl_compositor*>(
        wl_registry_bind(registry, id, &wl_compositor_interface, version)));
  } else if (interfaces.find(interface) != interfaces.end()) {
    globals->ptrs.push_back(std::unique_ptr<void, Deleter>(
        wl_registry_bind(registry, id, interfaces[interface], version)));
  } else {
    LOG(ERROR) << "Unbound interface: " << interface;
  }
}

void RegistryRemover(void* data, wl_registry* registry, uint32_t id) {
  LOG(WARNING) << "Got a registry losing event for " << id;
}

wl_registry_listener g_registry_listener = {RegistryHandler, RegistryRemover};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ClientVersionTest::Globals, public:

ClientVersionTest::Globals::Globals() {}

ClientVersionTest::Globals::~Globals() {}

////////////////////////////////////////////////////////////////////////////////
// ClientVersionTest, public:

bool ClientVersionTest::TestProtocol(const std::string& protocol,
                                     VersionValidityType validity_type) {
  globals_.protocol_tested = protocol;
  globals_.validity_type = validity_type;
  std::unique_ptr<wl_display> display(wl_display_connect(nullptr));
  if (!display) {
    LOG(ERROR) << "wl_display_connect failed";
    return false;
  }
  std::unique_ptr<wl_registry> registry(wl_display_get_registry(display.get()));
  wl_registry_add_listener(registry.get(), &g_registry_listener, &globals_);

  wl_display_roundtrip(display.get());
  if (!globals_.compositor) {
    DCHECK_EQ(protocol.length(), 0u);
    return true;
  }

  std::unique_ptr<wl_surface> surface;
  surface.reset(static_cast<wl_surface*>(
      wl_compositor_create_surface(globals_.compositor.get())));
  wl_surface_attach(surface.get(), nullptr, 0, 0);
  wl_display_roundtrip(display.get());

  int err = wl_display_get_error(display.get());
  if (validity_type == VersionValidityType::VALID_SKEWED ||
      validity_type == VersionValidityType::VALID_ADVERTISED) {
    DCHECK_EQ(err, 0);
  } else {
    DCHECK_EQ(err, EPROTO);

    uint32_t errorcode;
    errorcode = wl_display_get_protocol_error(display.get(), nullptr, nullptr);
    DCHECK_EQ(errorcode, WL_DISPLAY_ERROR_INVALID_OBJECT);
  }
  globals_.ptrs.clear();
  globals_.compositor.reset(nullptr);
  surface.reset(nullptr);
  registry.reset(nullptr);
  display.reset(nullptr);
  return true;
}

const std::vector<std::string>& ClientVersionTest::Protocols() const {
  return globals_.protocols;
}
}  // namespace clients
}  // namespace wayland
}  // namespace exo
