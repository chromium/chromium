// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/server.h"

#include <alpha-compositing-unstable-v1-server-protocol.h>
#include <aura-shell-server-protocol.h>
#include <color-space-unstable-v1-server-protocol.h>
#include <cursor-shapes-unstable-v1-server-protocol.h>
#include <extended-drag-unstable-v1-server-protocol.h>
#include <gaming-input-unstable-v2-server-protocol.h>
#include <grp.h>
#include <input-timestamps-unstable-v1-server-protocol.h>
#include <keyboard-configuration-unstable-v1-server-protocol.h>
#include <keyboard-extension-unstable-v1-server-protocol.h>
#include <linux-explicit-synchronization-unstable-v1-server-protocol.h>
#include <notification-shell-unstable-v1-server-protocol.h>
#include <pointer-constraints-unstable-v1-server-protocol.h>
#include <pointer-gestures-unstable-v1-server-protocol.h>
#include <presentation-time-server-protocol.h>
#include <relative-pointer-unstable-v1-server-protocol.h>
#include <remote-shell-unstable-v1-server-protocol.h>
#include <secure-output-unstable-v1-server-protocol.h>
#include <stylus-tools-unstable-v1-server-protocol.h>
#include <stylus-unstable-v2-server-protocol.h>
#include <text-input-unstable-v1-server-protocol.h>
#include <viewporter-server-protocol.h>
#include <vsync-feedback-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <xdg-decoration-unstable-v1-server-protocol.h>
#include <xdg-shell-server-protocol.h>
#include <xdg-shell-unstable-v6-server-protocol.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/posix/eintr_wrapper.h"
#include "components/exo/display.h"
#include "components/exo/wayland/serial_tracker.h"
#include "components/exo/wayland/wayland_display_output.h"
#include "components/exo/wayland/wl_compositor.h"
#include "components/exo/wayland/wl_data_device_manager.h"
#include "components/exo/wayland/wl_output.h"
#include "components/exo/wayland/wl_seat.h"
#include "components/exo/wayland/wl_shm.h"
#include "components/exo/wayland/wl_subcompositor.h"
#include "components/exo/wayland/wp_presentation.h"
#include "components/exo/wayland/wp_viewporter.h"
#include "components/exo/wayland/zaura_shell.h"
#include "components/exo/wayland/zcr_alpha_compositing.h"
#include "components/exo/wayland/zcr_secure_output.h"
#include "components/exo/wayland/zcr_stylus.h"
#include "components/exo/wayland/zcr_vsync_feedback.h"
#include "components/exo/wayland/zwp_linux_explicit_synchronization.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

#if defined(OS_CHROMEOS)
#include "base/system/sys_info.h"
#include "components/exo/wayland/wl_shell.h"
#include "components/exo/wayland/xdg_shell.h"
#include "components/exo/wayland/zcr_color_space.h"
#include "components/exo/wayland/zcr_cursor_shapes.h"
#include "components/exo/wayland/zcr_extended_drag.h"
#include "components/exo/wayland/zcr_gaming_input.h"
#include "components/exo/wayland/zcr_keyboard_configuration.h"
#include "components/exo/wayland/zcr_keyboard_extension.h"
#include "components/exo/wayland/zcr_notification_shell.h"
#include "components/exo/wayland/zcr_remote_shell.h"
#include "components/exo/wayland/zcr_stylus_tools.h"
#include "components/exo/wayland/zwp_input_timestamps_manager.h"
#include "components/exo/wayland/zwp_pointer_constraints.h"
#include "components/exo/wayland/zwp_pointer_gestures.h"
#include "components/exo/wayland/zwp_relative_pointer_manager.h"
#include "components/exo/wayland/zwp_text_input_manager.h"
#include "components/exo/wayland/zxdg_decoration_manager.h"
#include "components/exo/wayland/zxdg_shell.h"
#endif

#if defined(USE_OZONE)
#include <linux-dmabuf-unstable-v1-server-protocol.h>
#include "components/exo/wayland/zwp_linux_dmabuf.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(USE_FULLSCREEN_SHELL)
#include <fullscreen-shell-unstable-v1-server-protocol.h>
#include "components/exo/wayland/zwp_fullscreen_shell.h"
#endif

namespace exo {
namespace wayland {
namespace switches {

// This flag can be used to override the default wayland socket name. It is
// useful when another wayland server is already running and using the
// default name.
constexpr char kWaylandServerSocket[] = "wayland-server-socket";
}

namespace {

// Default wayland socket name.
const base::FilePath::CharType kSocketName[] = FILE_PATH_LITERAL("wayland-0");

// Group used for wayland socket.
const char kWaylandSocketGroup[] = "wayland";

bool IsDrmAtomicAvailable() {
#if defined(USE_OZONE)
  auto& host_properties =
      ui::OzonePlatform::GetInstance()->GetInitializedHostProperties();
  return host_properties.supports_overlays;
#else
  LOG(WARNING) << "Ozone disabled, cannot determine whether DrmAtomic is "
                  "present. Assuming it is not";
  return false;
#endif
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Server, public:

Server::Server(Display* display)
    : display_(display), wl_display_(wl_display_create()) {
  serial_tracker_ = std::make_unique<SerialTracker>(wl_display_.get());
  wl_global_create(wl_display_.get(), &wl_compositor_interface,
                   kWlCompositorVersion, this, bind_compositor);
  wl_global_create(wl_display_.get(), &wl_shm_interface, 1, display_, bind_shm);
#if defined(USE_OZONE)
  wl_global_create(wl_display_.get(), &zwp_linux_dmabuf_v1_interface,
                   kZwpLinuxDmabufVersion, display_, bind_linux_dmabuf);
#endif
  wl_global_create(wl_display_.get(), &wl_subcompositor_interface, 1, display_,
                   bind_subcompositor);
  display::Screen::GetScreen()->AddObserver(this);
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays())
    OnDisplayAdded(display);
  wl_global_create(wl_display_.get(), &zcr_vsync_feedback_v1_interface, 1,
                   display_, bind_vsync_feedback);

  data_device_manager_data_ = std::make_unique<WaylandDataDeviceManager>(
      display_, serial_tracker_.get());
  wl_global_create(wl_display_.get(), &wl_data_device_manager_interface,
                   kWlDataDeviceManagerVersion, data_device_manager_data_.get(),
                   bind_data_device_manager);

  wl_global_create(wl_display_.get(), &wp_viewporter_interface, 1, display_,
                   bind_viewporter);
  wl_global_create(wl_display_.get(), &wp_presentation_interface, 1, display_,
                   bind_presentation);
  wl_global_create(wl_display_.get(), &zcr_secure_output_v1_interface, 1,
                   display_, bind_secure_output);
  wl_global_create(wl_display_.get(), &zcr_alpha_compositing_v1_interface, 1,
                   display_, bind_alpha_compositing);
  wl_global_create(wl_display_.get(), &zcr_stylus_v2_interface,
                   zcr_stylus_v2_interface.version, display_, bind_stylus_v2);

  seat_data_ =
      std::make_unique<WaylandSeat>(display_->seat(), serial_tracker_.get());
  wl_global_create(wl_display_.get(), &wl_seat_interface, kWlSeatVersion,
                   seat_data_.get(), bind_seat);

  if (IsDrmAtomicAvailable()) {
    // The release fence needed by linux-explicit-sync comes from DRM-atomic.
    // If DRM atomic is not supported, linux-explicit-sync interface is
    // disabled.
    wl_global_create(wl_display_.get(),
                     &zwp_linux_explicit_synchronization_v1_interface, 1,
                     display_, bind_linux_explicit_synchronization);
  }
  wl_global_create(wl_display_.get(), &zaura_shell_interface,
                   kZAuraShellVersion, display_, bind_aura_shell);
#if defined(OS_CHROMEOS)
  wl_global_create(wl_display_.get(), &wl_shell_interface, 1, display_,
                   bind_shell);
  wl_global_create(wl_display_.get(), &zcr_cursor_shapes_v1_interface, 1,
                   display_, bind_cursor_shapes);
  wl_global_create(wl_display_.get(), &zcr_gaming_input_v2_interface, 2,
                   display_, bind_gaming_input);
  wl_global_create(wl_display_.get(), &zcr_keyboard_configuration_v1_interface,
                   zcr_keyboard_configuration_v1_interface.version, display_,
                   bind_keyboard_configuration);
  wl_global_create(wl_display_.get(), &zcr_keyboard_extension_v1_interface, 1,
                   display_, bind_keyboard_extension);
  wl_global_create(wl_display_.get(), &zcr_notification_shell_v1_interface, 1,
                   display_, bind_notification_shell);
  wl_global_create(wl_display_.get(), &zcr_remote_shell_v1_interface,
                   zcr_remote_shell_v1_interface.version, display_,
                   bind_remote_shell);
  wl_global_create(wl_display_.get(), &zcr_stylus_tools_v1_interface, 1,
                   display_, bind_stylus_tools);
  wl_global_create(wl_display_.get(),
                   &zwp_input_timestamps_manager_v1_interface, 1, display_,
                   bind_input_timestamps_manager);
  wl_global_create(wl_display_.get(), &zwp_pointer_gestures_v1_interface, 1,
                   display_, bind_pointer_gestures);
  wl_global_create(wl_display_.get(), &zwp_pointer_constraints_v1_interface, 1,
                   display_, bind_pointer_constraints);
  wl_global_create(wl_display_.get(),
                   &zwp_relative_pointer_manager_v1_interface, 1, display_,
                   bind_relative_pointer_manager);
  wl_global_create(wl_display_.get(), &zcr_color_space_v1_interface, 1,
                   display_, bind_color_space);
  wl_global_create(wl_display_.get(), &zxdg_decoration_manager_v1_interface, 1,
                   display_, bind_zxdg_decoration_manager);
  wl_global_create(wl_display_.get(), &zcr_extended_drag_v1_interface, 1,
                   display_, bind_extended_drag);

  zwp_text_manager_data_ = std::make_unique<WaylandTextInputManager>(
      display_->seat()->xkb_tracker(), serial_tracker_.get());
  wl_global_create(wl_display_.get(), &zwp_text_input_manager_v1_interface, 1,
                   zwp_text_manager_data_.get(), bind_text_input_manager);

  zxdg_shell_data_ =
      std::make_unique<WaylandZxdgShell>(display_, serial_tracker_.get());
  wl_global_create(wl_display_.get(), &zxdg_shell_v6_interface, 1,
                   zxdg_shell_data_.get(), bind_zxdg_shell_v6);

  xdg_shell_data_ =
      std::make_unique<WaylandXdgShell>(display_, serial_tracker_.get());
  wl_global_create(wl_display_.get(), &xdg_wm_base_interface, 1,
                   xdg_shell_data_.get(), bind_xdg_shell);
#endif

#if defined(USE_FULLSCREEN_SHELL)
  wl_global_create(wl_display_.get(), &zwp_fullscreen_shell_v1_interface, 1,
                   display_, bind_fullscreen_shell);
#endif
}

Server::~Server() {
  display::Screen::GetScreen()->RemoveObserver(this);
  // TODO(https://crbug.com/1124106): Investigate if we can eliminate Shutdown
  // methods.
  serial_tracker_->Shutdown();
  display_->Shutdown();
}

// static
std::unique_ptr<Server> Server::Create(Display* display) {
  std::unique_ptr<Server> server(new Server(display));

  char* runtime_dir_str = getenv("XDG_RUNTIME_DIR");
  if (!runtime_dir_str) {
    LOG(ERROR) << "XDG_RUNTIME_DIR not set in the environment";
    return nullptr;
  }

  const base::FilePath runtime_dir(runtime_dir_str);
#if defined(OS_CHROMEOS)
  // On debugging chromeos-chrome on linux platform,
  // try to ensure the directory if missing.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    CHECK(base::DirectoryExists(runtime_dir) ||
          base::CreateDirectory(runtime_dir))
        << "Failed to create XDG_RUNTIME_DIR";
  }
#endif  // defined(OS_CHROMEOS)

  std::string socket_name(kSocketName);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kWaylandServerSocket)) {
    socket_name =
        command_line->GetSwitchValueASCII(switches::kWaylandServerSocket);
  }

  if (!server->AddSocket(socket_name.c_str())) {
    LOG(ERROR) << "Failed to add socket: " << socket_name;
    return nullptr;
  }

  base::FilePath socket_path = base::FilePath(runtime_dir).Append(socket_name);

  // Change permissions on the socket.
  struct group wayland_group;
  struct group* wayland_group_res = nullptr;
  char buf[10000];
  if (HANDLE_EINTR(getgrnam_r(kWaylandSocketGroup, &wayland_group, buf,
                              sizeof(buf), &wayland_group_res)) < 0) {
    PLOG(ERROR) << "getgrnam_r";
    return nullptr;
  }
  if (wayland_group_res) {
    if (HANDLE_EINTR(chown(socket_path.MaybeAsASCII().c_str(), -1,
                           wayland_group.gr_gid)) < 0) {
      PLOG(ERROR) << "chown";
      return nullptr;
    }
  } else {
    LOG(WARNING) << "Group '" << kWaylandSocketGroup << "' not found";
  }

  if (!base::SetPosixFilePermissions(socket_path, 0660)) {
    PLOG(ERROR) << "Could not set permissions: " << socket_path.value();
    return nullptr;
  }

  return server;
}

bool Server::AddSocket(const std::string name) {
  DCHECK(!name.empty());
  return !wl_display_add_socket(wl_display_.get(), name.c_str());
}

int Server::GetFileDescriptor() const {
  wl_event_loop* event_loop = wl_display_get_event_loop(wl_display_.get());
  DCHECK(event_loop);
  return wl_event_loop_get_fd(event_loop);
}

void Server::Dispatch(base::TimeDelta timeout) {
  wl_event_loop* event_loop = wl_display_get_event_loop(wl_display_.get());
  DCHECK(event_loop);
  wl_event_loop_dispatch(event_loop, timeout.InMilliseconds());
}

void Server::Flush() {
  wl_display_flush_clients(wl_display_.get());
}

void Server::OnDisplayAdded(const display::Display& new_display) {
  auto output = std::make_unique<WaylandDisplayOutput>(new_display.id());
  output->set_global(wl_global_create(wl_display_.get(), &wl_output_interface,
                                      kWlOutputVersion, output.get(),
                                      bind_output));
  DCHECK_EQ(outputs_.count(new_display.id()), 0u);
  outputs_.insert(std::make_pair(new_display.id(), std::move(output)));
}

void Server::OnDisplayRemoved(const display::Display& old_display) {
  DCHECK_EQ(outputs_.count(old_display.id()), 1u);
  outputs_.erase(old_display.id());
}

wl_resource* Server::GetOutputResource(wl_client* client, int64_t display_id) {
  DCHECK_NE(display_id, display::kInvalidDisplayId);
  auto iter = outputs_.find(display_id);
  if (iter == outputs_.end())
    return nullptr;
  return iter->second.get()->GetOutputResourceForClient(client);
}

}  // namespace wayland
}  // namespace exo
