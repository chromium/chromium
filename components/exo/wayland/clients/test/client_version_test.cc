// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/clients/test/client_version_test.h"

#include <alpha-compositing-unstable-v1-client-protocol.h>
#include <aura-output-management-server-protocol.h>
#include <aura-shell-server-protocol.h>
#include <chrome-color-management-server-protocol.h>
#include <content-type-v1-server-protocol.h>
#include <cursor-shapes-unstable-v1-server-protocol.h>
#include <extended-drag-unstable-v1-server-protocol.h>
#include <gaming-input-unstable-v2-server-protocol.h>
#include <idle-inhibit-unstable-v1-server-protocol.h>
#include <keyboard-configuration-unstable-v1-server-protocol.h>
#include <keyboard-extension-unstable-v1-server-protocol.h>
#include <keyboard-shortcuts-inhibit-unstable-v1-server-protocol.h>
#include <notification-shell-unstable-v1-server-protocol.h>
#include <overlay-prioritizer-server-protocol.h>
#include <pointer-constraints-unstable-v1-server-protocol.h>
#include <pointer-gestures-unstable-v1-server-protocol.h>
#include <relative-pointer-unstable-v1-server-protocol.h>
#include <remote-shell-unstable-v1-server-protocol.h>
#include <remote-shell-unstable-v2-server-protocol.h>
#include <secure-output-unstable-v1-server-protocol.h>
#include <stylus-tools-unstable-v1-server-protocol.h>
#include <stylus-unstable-v2-server-protocol.h>
#include <surface-augmenter-server-protocol.h>
#include <text-input-extension-unstable-v1-server-protocol.h>
#include <text-input-unstable-v1-server-protocol.h>
#include <touchpad-haptics-unstable-v1-server-protocol.h>
#include <ui-controls-unstable-v1-server-protocol.h>
#include <viewporter-client-protocol.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <xdg-decoration-unstable-v1-server-protocol.h>
#include <xdg-shell-server-protocol.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/check_op.h"
#include "base/logging.h"

#include "components/exo/wayland/clients/client_helper.h"

namespace exo::wayland::clients {
namespace {

struct Globals {
  Globals() = default;
  ~Globals() = default;

  std::unique_ptr<wl_compositor> compositor;
  std::vector<std::string> protocols;
  std::string protocol_tested;
  ClientVersionTest::VersionValidityType validity_type =
      ClientVersionTest::VersionValidityType::VALID_ADVERTISED;
  std::unique_ptr<surface_augmenter> surface_augmenter;
  std::unique_ptr<overlay_prioritizer> overlay_prioritizer;
  std::unique_ptr<wl_shm> wl_shm;
  std::unique_ptr<wl_shell> wl_shell;
  std::unique_ptr<wl_seat> wl_seat;
  std::unique_ptr<wp_presentation> wp_presentation;
  std::unique_ptr<zaura_output_manager> zaura_output_manager;
  std::unique_ptr<zaura_output_manager_v2> zaura_output_manager_v2;
  std::unique_ptr<zaura_shell> zaura_shell;
  std::unique_ptr<zwp_linux_dmabuf_v1> zwp_linux_dmabuf_v1;
  std::unique_ptr<wl_subcompositor> wl_subcompositor;
  std::unique_ptr<zwp_input_timestamps_manager_v1>
      zwp_input_timestamps_manager_v1;
  std::unique_ptr<zwp_fullscreen_shell_v1> zwp_fullscreen_shell_v1;
  std::unique_ptr<wl_output> wl_output;
  std::unique_ptr<zwp_linux_explicit_synchronization_v1>
      zwp_linux_explicit_synchronization_v1;
  std::unique_ptr<zcr_vsync_feedback_v1> zcr_vsync_feedback_v1;
  std::unique_ptr<wl_data_device_manager> wl_data_device_manager;
  std::unique_ptr<wp_content_type_manager_v1> wp_content_type_manager_v1;
  std::unique_ptr<wp_fractional_scale_manager_v1>
      wp_fractional_scale_manager_v1;
  std::unique_ptr<wp_viewporter> wp_viewporter;
  std::unique_ptr<xdg_wm_base> xdg_wm_base;
  std::unique_ptr<zwp_text_input_manager_v1> zwp_text_input_manager_v1;
  std::unique_ptr<zcr_secure_output_v1> zcr_secure_output_v1;
  std::unique_ptr<zcr_alpha_compositing_v1> zcr_alpha_compositing_v1;
  std::unique_ptr<zcr_stylus_v2> zcr_stylus_v2;
  std::unique_ptr<zcr_color_manager_v1> zcr_color_manager_v1;
  std::unique_ptr<zcr_cursor_shapes_v1> zcr_cursor_shapes_v1;
  std::unique_ptr<zcr_gaming_input_v2> zcr_gaming_input_v2;
  std::unique_ptr<zcr_text_input_extension_v1> zcr_text_input_extension_v1;
  std::unique_ptr<zcr_keyboard_configuration_v1> zcr_keyboard_configuration_v1;
  std::unique_ptr<zcr_keyboard_extension_v1> zcr_keyboard_extension_v1;
  std::unique_ptr<zwp_keyboard_shortcuts_inhibit_manager_v1>
      zwp_keyboard_shortcuts_inhibit_manager_v1;
  std::unique_ptr<zcr_notification_shell_v1> zcr_notification_shell_v1;
  std::unique_ptr<zcr_remote_shell_v1> zcr_remote_shell_v1;
  std::unique_ptr<zcr_remote_shell_v2> zcr_remote_shell_v2;
  std::unique_ptr<zcr_stylus_tools_v1> zcr_stylus_tools_v1;
  std::unique_ptr<zcr_touchpad_haptics_v1> zcr_touchpad_haptics_v1;
  std::unique_ptr<zcr_ui_controls_v1> zcr_ui_controls_v1;
  std::unique_ptr<zwp_pointer_gestures_v1> zwp_pointer_gestures_v1;
  std::unique_ptr<zwp_pointer_constraints_v1> zwp_pointer_constraints_v1;
  std::unique_ptr<zwp_relative_pointer_manager_v1>
      zwp_relative_pointer_manager_v1;
  std::unique_ptr<zxdg_decoration_manager_v1> zxdg_decoration_manager_v1;
  std::unique_ptr<zcr_extended_drag_v1> zcr_extended_drag_v1;
  std::unique_ptr<zxdg_output_manager_v1> zxdg_output_manager_v1;
  std::unique_ptr<zwp_idle_inhibit_manager_v1> zwp_idle_inhibit_manager_v1;
  std::unique_ptr<wp_single_pixel_buffer_manager_v1>
      wp_single_pixel_buffer_manager_v1;
};

typedef void (*InterfaceRegistryCallback)(Globals*,
                                          wl_registry*,
                                          uint32_t,
                                          uint32_t);

#define REGISTRY_CALLBACK(field, t)                                         \
  {                                                                         \
#t, [](Globals* globals, wl_registry* registry,                           \
           uint32_t id, uint32_t version) {                                   \
      globals->field.reset(static_cast<t*>(                                   \
        wl_registry_bind(registry, id, &(t##_interface), version)));          \
    } \
  }

void RegistryHandler(void* data,
                     wl_registry* registry,
                     uint32_t id,
                     const char* interface,
                     uint32_t version) {
  Globals* globals = static_cast<Globals*>(data);
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

  std::unordered_map<std::string, InterfaceRegistryCallback>
      interfaces_callbacks = {
          REGISTRY_CALLBACK(compositor, wl_compositor),
          REGISTRY_CALLBACK(wl_shm, wl_shm),
          REGISTRY_CALLBACK(wl_shell, wl_shell),
          REGISTRY_CALLBACK(wl_seat, wl_seat),
          REGISTRY_CALLBACK(wp_presentation, wp_presentation),
          REGISTRY_CALLBACK(zaura_output_manager, zaura_output_manager),
          REGISTRY_CALLBACK(zaura_output_manager_v2, zaura_output_manager_v2),
          REGISTRY_CALLBACK(zaura_shell, zaura_shell),
          REGISTRY_CALLBACK(zwp_linux_dmabuf_v1, zwp_linux_dmabuf_v1),
          REGISTRY_CALLBACK(wl_subcompositor, wl_subcompositor),
          REGISTRY_CALLBACK(zwp_input_timestamps_manager_v1,
                            zwp_input_timestamps_manager_v1),
          REGISTRY_CALLBACK(zwp_fullscreen_shell_v1, zwp_fullscreen_shell_v1),
          REGISTRY_CALLBACK(wl_output, wl_output),
          REGISTRY_CALLBACK(zwp_linux_explicit_synchronization_v1,
                            zwp_linux_explicit_synchronization_v1),
          REGISTRY_CALLBACK(zcr_vsync_feedback_v1, zcr_vsync_feedback_v1),
          REGISTRY_CALLBACK(wl_data_device_manager, wl_data_device_manager),
          REGISTRY_CALLBACK(wp_content_type_manager_v1,
                            wp_content_type_manager_v1),
          REGISTRY_CALLBACK(wp_fractional_scale_manager_v1,
                            wp_fractional_scale_manager_v1),
          REGISTRY_CALLBACK(wp_single_pixel_buffer_manager_v1,
                            wp_single_pixel_buffer_manager_v1),
          REGISTRY_CALLBACK(wp_viewporter, wp_viewporter),
          REGISTRY_CALLBACK(xdg_wm_base, xdg_wm_base),
          REGISTRY_CALLBACK(zwp_text_input_manager_v1,
                            zwp_text_input_manager_v1),
          REGISTRY_CALLBACK(zcr_secure_output_v1, zcr_secure_output_v1),
          REGISTRY_CALLBACK(zcr_alpha_compositing_v1, zcr_alpha_compositing_v1),
          REGISTRY_CALLBACK(zcr_stylus_v2, zcr_stylus_v2),
          REGISTRY_CALLBACK(zcr_color_manager_v1, zcr_color_manager_v1),
          REGISTRY_CALLBACK(zcr_cursor_shapes_v1, zcr_cursor_shapes_v1),
          REGISTRY_CALLBACK(zcr_gaming_input_v2, zcr_gaming_input_v2),
          REGISTRY_CALLBACK(zcr_keyboard_configuration_v1,
                            zcr_keyboard_configuration_v1),
          REGISTRY_CALLBACK(zcr_keyboard_extension_v1,
                            zcr_keyboard_extension_v1),
          REGISTRY_CALLBACK(zwp_keyboard_shortcuts_inhibit_manager_v1,
                            zwp_keyboard_shortcuts_inhibit_manager_v1),
          REGISTRY_CALLBACK(zcr_notification_shell_v1,
                            zcr_notification_shell_v1),
          REGISTRY_CALLBACK(zcr_remote_shell_v1, zcr_remote_shell_v1),
          REGISTRY_CALLBACK(zcr_remote_shell_v2, zcr_remote_shell_v2),
          REGISTRY_CALLBACK(zcr_stylus_tools_v1, zcr_stylus_tools_v1),
          REGISTRY_CALLBACK(zcr_text_input_extension_v1,
                            zcr_text_input_extension_v1),
          REGISTRY_CALLBACK(zcr_touchpad_haptics_v1, zcr_touchpad_haptics_v1),
          REGISTRY_CALLBACK(zcr_ui_controls_v1, zcr_ui_controls_v1),
          REGISTRY_CALLBACK(zwp_pointer_gestures_v1, zwp_pointer_gestures_v1),
          REGISTRY_CALLBACK(zwp_pointer_constraints_v1,
                            zwp_pointer_constraints_v1),
          REGISTRY_CALLBACK(zwp_relative_pointer_manager_v1,
                            zwp_relative_pointer_manager_v1),
          REGISTRY_CALLBACK(zxdg_decoration_manager_v1,
                            zxdg_decoration_manager_v1),
          REGISTRY_CALLBACK(zcr_extended_drag_v1, zcr_extended_drag_v1),
          REGISTRY_CALLBACK(zxdg_output_manager_v1, zxdg_output_manager_v1),
          REGISTRY_CALLBACK(surface_augmenter, surface_augmenter),
          REGISTRY_CALLBACK(overlay_prioritizer, overlay_prioritizer),
          REGISTRY_CALLBACK(zwp_idle_inhibit_manager_v1,
                            zwp_idle_inhibit_manager_v1),
      };
  if (interfaces_callbacks.find(interface) != interfaces_callbacks.end()) {
    interfaces_callbacks[interface](globals, registry, id, version);
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
// ClientVersionTest, public:

void ClientVersionTest::TestProtocol(const std::string& protocol,
                                     VersionValidityType validity_type) {
  auto globals = std::make_unique<Globals>();
  globals->protocol_tested = protocol;
  globals->validity_type = validity_type;
  std::unique_ptr<wl_display> display(wl_display_connect(nullptr));
  CHECK(display) << "wl_display_connect failed";

  std::unique_ptr<wl_registry> registry(wl_display_get_registry(display.get()));
  wl_registry_add_listener(registry.get(), &g_registry_listener, globals.get());

  wl_display_roundtrip(display.get());
  CHECK(globals->compositor) << "no compositor bound";

  std::unique_ptr<wl_surface> surface;
  surface.reset(static_cast<wl_surface*>(
      wl_compositor_create_surface(globals->compositor.get())));
  wl_surface_attach(surface.get(), nullptr, 0, 0);
  wl_display_roundtrip(display.get());

  int err = wl_display_get_error(display.get());
  if (validity_type == VersionValidityType::VALID_SKEWED ||
      validity_type == VersionValidityType::VALID_ADVERTISED) {
    DCHECK_EQ(err, 0) << "Couldn't bind valid version of " << protocol;
  } else {
    DCHECK_EQ(err, EPROTO) << "Invalid version bound for " << protocol;

    uint32_t errorcode;
    errorcode = wl_display_get_protocol_error(display.get(), nullptr, nullptr);
    DCHECK_EQ(errorcode, WL_DISPLAY_ERROR_INVALID_OBJECT)
        << "Invalid error code for invalid version of " << protocol;
  }

  globals.reset();
}

const std::vector<std::string> ClientVersionTest::Protocols() {
  Globals globals;
  std::unique_ptr<wl_display> display(wl_display_connect(nullptr));
  std::unique_ptr<wl_registry> registry(wl_display_get_registry(display.get()));
  wl_registry_add_listener(registry.get(), &g_registry_listener, &globals);
  wl_display_roundtrip(display.get());
  return std::move(globals.protocols);
}
}  // namespace exo::wayland::clients
