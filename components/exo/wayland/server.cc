// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/server.h"

#include <alpha-compositing-unstable-v1-server-protocol.h>
#include <aura-shell-server-protocol.h>
#include <chrome-color-management-server-protocol.h>
#include <content-type-v1-server-protocol.h>
#include <cursor-shapes-unstable-v1-server-protocol.h>
#include <extended-drag-unstable-v1-server-protocol.h>
#include <gaming-input-unstable-v2-server-protocol.h>
#include <grp.h>
#include <idle-inhibit-unstable-v1-server-protocol.h>
#include <input-timestamps-unstable-v1-server-protocol.h>
#include <keyboard-configuration-unstable-v1-server-protocol.h>
#include <keyboard-extension-unstable-v1-server-protocol.h>
#include <keyboard-shortcuts-inhibit-unstable-v1-server-protocol.h>
#include <linux-explicit-synchronization-unstable-v1-server-protocol.h>
#include <notification-shell-unstable-v1-server-protocol.h>
#include <overlay-prioritizer-server-protocol.h>
#include <pointer-constraints-unstable-v1-server-protocol.h>
#include <pointer-gestures-unstable-v1-server-protocol.h>
#include <presentation-time-server-protocol.h>
#include <relative-pointer-unstable-v1-server-protocol.h>
#include <remote-shell-unstable-v1-server-protocol.h>
#include <remote-shell-unstable-v2-server-protocol.h>
#include <secure-output-unstable-v1-server-protocol.h>
#include <single-pixel-buffer-v1-server-protocol.h>
#include <stylus-tools-unstable-v1-server-protocol.h>
#include <stylus-unstable-v2-server-protocol.h>
#include <surface-augmenter-server-protocol.h>
#include <sys/socket.h>
#include <text-input-extension-unstable-v1-server-protocol.h>
#include <text-input-unstable-v1-server-protocol.h>
#include <touchpad-haptics-unstable-v1-server-protocol.h>
#include <viewporter-server-protocol.h>
#include <vsync-feedback-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <xdg-decoration-unstable-v1-server-protocol.h>
#include <xdg-output-unstable-v1-server-protocol.h>
#include <xdg-shell-server-protocol.h>

#include <linux-dmabuf-unstable-v1-server-protocol.h>
#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/exo/display.h"
#include "components/exo/security_delegate.h"
#include "components/exo/wayland/content_type.h"
#include "components/exo/wayland/overlay_prioritizer.h"
#include "components/exo/wayland/serial_tracker.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/surface_augmenter.h"
#include "components/exo/wayland/wayland_display_output.h"
#include "components/exo/wayland/wayland_dmabuf_feedback_manager.h"
#include "components/exo/wayland/wayland_watcher.h"
#include "components/exo/wayland/weston_test.h"
#include "components/exo/wayland/wl_compositor.h"
#include "components/exo/wayland/wl_data_device_manager.h"
#include "components/exo/wayland/wl_output.h"
#include "components/exo/wayland/wl_seat.h"
#include "components/exo/wayland/wl_shell.h"
#include "components/exo/wayland/wl_shm.h"
#include "components/exo/wayland/wl_subcompositor.h"
#include "components/exo/wayland/wp_presentation.h"
#include "components/exo/wayland/wp_single_pixel_buffer.h"
#include "components/exo/wayland/wp_viewporter.h"
#include "components/exo/wayland/xdg_shell.h"
#include "components/exo/wayland/zaura_output_manager.h"
#include "components/exo/wayland/zaura_shell.h"
#include "components/exo/wayland/zcr_alpha_compositing.h"
#include "components/exo/wayland/zcr_color_manager.h"
#include "components/exo/wayland/zcr_cursor_shapes.h"
#include "components/exo/wayland/zcr_extended_drag.h"
#include "components/exo/wayland/zcr_gaming_input.h"
#include "components/exo/wayland/zcr_keyboard_configuration.h"
#include "components/exo/wayland/zcr_keyboard_extension.h"
#include "components/exo/wayland/zcr_notification_shell.h"
#include "components/exo/wayland/zcr_remote_shell.h"
#include "components/exo/wayland/zcr_remote_shell_v2.h"
#include "components/exo/wayland/zcr_secure_output.h"
#include "components/exo/wayland/zcr_stylus.h"
#include "components/exo/wayland/zcr_stylus_tools.h"
#include "components/exo/wayland/zcr_touchpad_haptics.h"
#include "components/exo/wayland/zcr_ui_controls.h"
#include "components/exo/wayland/zcr_vsync_feedback.h"
#include "components/exo/wayland/zwp_idle_inhibit_manager.h"
#include "components/exo/wayland/zwp_input_timestamps_manager.h"
#include "components/exo/wayland/zwp_keyboard_shortcuts_inhibit_manager.h"
#include "components/exo/wayland/zwp_linux_dmabuf.h"
#include "components/exo/wayland/zwp_linux_explicit_synchronization.h"
#include "components/exo/wayland/zwp_pointer_constraints.h"
#include "components/exo/wayland/zwp_pointer_gestures.h"
#include "components/exo/wayland/zwp_relative_pointer_manager.h"
#include "components/exo/wayland/zwp_text_input_manager.h"
#include "components/exo/wayland/zxdg_decoration_manager.h"
#include "components/exo/wayland/zxdg_output_manager.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/ozone/public/ozone_platform.h"

namespace exo {
namespace wayland {
namespace switches {

// This flag can be used to override the default wayland socket name. It is
// useful when another wayland server is already running and using the
// default name.
constexpr char kWaylandServerSocket[] = "wayland-server-socket";

}  // namespace switches

namespace {

// Default wayland socket name.
const base::FilePath::CharType kSocketName[] = FILE_PATH_LITERAL("wayland-0");

// Group used for wayland socket.
const char kWaylandSocketGroup[] = "wayland";

// Number of clients that can be waiting for accept() before we start refusing
// connections. This is *NOT* the maximum number of clients, just pending ones
// (see `man 2 listen`).
constexpr int kMaxPendingConnections = 128;

bool IsDrmAtomicAvailable() {
#if BUILDFLAG(IS_OZONE)
  auto& host_properties =
      ui::OzonePlatform::GetInstance()->GetPlatformRuntimeProperties();
  return host_properties.supports_overlays;
#else
  LOG(WARNING) << "Ozone disabled, cannot determine whether DrmAtomic is "
                  "present. Assuming it is not";
  return false;
#endif
}

void wayland_log(const char* fmt, va_list argp) {
  LOG(WARNING) << "libwayland: " << base::StringPrintV(fmt, argp);
}

int GetTextInputExtensionV1Version() {
  if (base::FeatureList::IsEnabled(
          ash::features::kExoExtendedConfirmComposition) &&
      base::FeatureList::IsEnabled(ash::features::kExoSurroundingTextOffset)) {
    // set_surrounding_text_offset_utf16 + new surrounding_text_support
    // strategy enabled once at version 10 was reverted (crbug.com/1451324).
    // Unfortunately, we have to disable confirm-composition in version 11
    // together, because of wayland's versioning system.
    //
    // Now, the new API to fix the issue is introduced in version 12.
    // We cannot enable confirm-composition only, because it will be hitting
    // the same issue at version 10. Thus, we'll set version 12 (including
    // all fixes + confirm-composition), or 9 (before everything).
    return 12;
  }

  return 9;
}

}  // namespace

bool Server::Open() {
  std::string socket_name = kSocketName;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kWaylandServerSocket)) {
    socket_name =
        command_line->GetSwitchValueASCII(switches::kWaylandServerSocket);
  }

  char* runtime_dir_str = getenv("XDG_RUNTIME_DIR");
  if (!runtime_dir_str) {
    LOG(ERROR) << "XDG_RUNTIME_DIR not set in the environment";
    return false;
  }
  base::FilePath socket_path =
      base::FilePath(runtime_dir_str).Append(socket_name);

  if (!socket_path.IsAbsolute()) {
    LOG(ERROR) << "Unable to create a wayland server. The provided path must "
                  "be absolute, got: "
               << socket_path;
    return false;
  }

  // On debugging chromeos-chrome on linux platform,
  // try to ensure the directory if missing.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    base::FilePath runtime_dir = socket_path.DirName();
    CHECK(base::DirectoryExists(runtime_dir) ||
          base::CreateDirectory(runtime_dir))
        << "Failed to create " << runtime_dir;
  }

  if (!AddSocket(socket_path.MaybeAsASCII().c_str())) {
    LOG(ERROR) << "Failed to add socket: " << socket_path;
    return false;
  }

  // Change permissions on the socket.
  struct group wayland_group;
  struct group* wayland_group_res = nullptr;
  char buf[10000];
  if (HANDLE_EINTR(getgrnam_r(kWaylandSocketGroup, &wayland_group, buf,
                              sizeof(buf), &wayland_group_res)) < 0) {
    PLOG(ERROR) << "getgrnam_r";
    return false;
  }
  if (wayland_group_res) {
    if (HANDLE_EINTR(chown(socket_path.MaybeAsASCII().c_str(), -1,
                           wayland_group.gr_gid)) < 0) {
      PLOG(ERROR) << "chown";
      return false;
    }
  } else {
    LOG(WARNING) << "Group '" << kWaylandSocketGroup << "' not found";
  }

  if (!base::SetPosixFilePermissions(socket_path, 0660)) {
    PLOG(ERROR) << "Could not set permissions: " << socket_path.value();
    return false;
  }
  return true;
}

bool Server::OpenFd(base::ScopedFD fd) {
  if (listen(fd.get(), kMaxPendingConnections) != 0) {
    PLOG(ERROR) << "listen";
    return false;
  }

  if (wl_display_add_socket_fd(wl_display_.get(), fd.get()) != 0) {
    PLOG(ERROR) << "Failed to add socket " << fd.get() << " to wl_display";
    return false;
  }

  // wl_display will only close() a socket that it successfully added, so is is
  // only safe to release() at this point
  std::ignore = fd.release();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Server, public:

Server::Server(Display* display,
               std::unique_ptr<SecurityDelegate> security_delegate)
    : display_(display), security_delegate_(std::move(security_delegate)) {
  wl_log_set_handler_server(wayland_log);

  wl_display_.reset(wl_display_create());
  SetSecurityDelegate(wl_display_.get(), security_delegate_.get());
}

void Server::Initialize() {
  serial_tracker_ = std::make_unique<SerialTracker>(wl_display_.get());
  rotation_serial_tracker_ = std::make_unique<SerialTracker>(wl_display_.get());
  wl_global_create(wl_display_.get(), &wl_compositor_interface,
                   kWlCompositorVersion, this, bind_compositor);
  wl_global_create(wl_display_.get(), &wl_shm_interface, 1, display_, bind_shm);
  wayland_feedback_manager_ =
      std::make_unique<WaylandDmabufFeedbackManager>(display_);
  if (wayland_feedback_manager_->GetVersionSupportedByPlatform() > 0) {
    wl_global_create(wl_display_.get(), &zwp_linux_dmabuf_v1_interface,
                     wayland_feedback_manager_->GetVersionSupportedByPlatform(),
                     wayland_feedback_manager_.get(), bind_linux_dmabuf);
  }

  // aura_output_manager needs to be registered before the wl_output globals to
  // ensure clients can bind to the aura_output_manager before any wl_outputs.
  // This is necessary to ensure aura_output_manager can send relevant output
  // events immediately after an output is bound to the client and before the
  // data in these events might be needed by the client.
  wl_global_create(wl_display_.get(), &zaura_output_manager_interface,
                   kZAuraOutputManagerVersion, this, bind_aura_output_manager);
  wl_global_create(wl_display_.get(), &wl_subcompositor_interface, 1, display_,
                   bind_subcompositor);
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    OnDisplayAdded(display);
  }
  wl_global_create(wl_display_.get(), &zcr_vsync_feedback_v1_interface, 1,
                   display_, bind_vsync_feedback);

  data_device_manager_data_ = std::make_unique<WaylandDataDeviceManager>(
      display_, serial_tracker_.get());
  wl_global_create(wl_display_.get(), &wl_data_device_manager_interface,
                   kWlDataDeviceManagerVersion, data_device_manager_data_.get(),
                   bind_data_device_manager);

  wl_global_create(wl_display_.get(), &surface_augmenter_interface,
                   kSurfaceAugmenterVersion, display_, bind_surface_augmenter);
  wl_global_create(
      wl_display_.get(), &wp_single_pixel_buffer_manager_v1_interface,
      kSinglePixelBufferVersion, display_, bind_single_pixel_buffer);
  wl_global_create(wl_display_.get(), &overlay_prioritizer_interface, 1,
                   display_, bind_overlay_prioritizer);
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
                     &zwp_linux_explicit_synchronization_v1_interface, 2,
                     display_, bind_linux_explicit_synchronization);
  }
  wl_global_create(wl_display_.get(), &zaura_shell_interface,
                   kZAuraShellVersion, display_, bind_aura_shell);
  wl_global_create(wl_display_.get(), &wl_shell_interface, 1, display_,
                   bind_shell);
  wl_global_create(wl_display_.get(), &wp_content_type_manager_v1_interface, 1,
                   display_, bind_content_type);
  wl_global_create(wl_display_.get(), &zcr_cursor_shapes_v1_interface, 1,
                   display_, bind_cursor_shapes);
  wl_global_create(wl_display_.get(), &zcr_gaming_input_v2_interface, 3,
                   display_, bind_gaming_input);
  wl_global_create(wl_display_.get(), &zcr_keyboard_configuration_v1_interface,
                   zcr_keyboard_configuration_v1_interface.version, display_,
                   bind_keyboard_configuration);
  wl_global_create(wl_display_.get(), &zcr_notification_shell_v1_interface, 1,
                   display_, bind_notification_shell);

  remote_shell_data_ = std::make_unique<WaylandRemoteShellData>(
      display_,
      WaylandRemoteShellData::OutputResourceProvider(base::BindRepeating(
          &Server::GetOutputResource, base::Unretained(this))));
  wl_global_create(wl_display_.get(), &zcr_remote_shell_v1_interface,
                   zcr_remote_shell_v1_interface.version,
                   remote_shell_data_.get(), bind_remote_shell);
  wl_global_create(wl_display_.get(), &zcr_remote_shell_v2_interface,
                   zcr_remote_shell_v2_interface.version,
                   remote_shell_data_.get(), bind_remote_shell_v2);

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
  wl_global_create(wl_display_.get(), &zcr_color_manager_v1_interface,
                   kZcrColorManagerVersion, this, bind_zcr_color_manager);
  wl_global_create(wl_display_.get(), &zxdg_decoration_manager_v1_interface, 1,
                   display_, bind_zxdg_decoration_manager);
  wl_global_create(wl_display_.get(), &zcr_extended_drag_v1_interface, 1,
                   display_, bind_extended_drag);
  wl_global_create(wl_display_.get(), &zxdg_output_manager_v1_interface, 3,
                   display_, bind_zxdg_output_manager);
  wl_global_create(wl_display_.get(), &zwp_idle_inhibit_manager_v1_interface, 1,
                   display_, bind_zwp_idle_inhibit_manager);

  weston_test_holder_ = std::make_unique<WestonTest>(this);
  ui_controls_holder_ = std::make_unique<UiControls>(this);

  zcr_keyboard_extension_data_ =
      std::make_unique<WaylandKeyboardExtension>(serial_tracker_.get());
  wl_global_create(wl_display_.get(), &zcr_keyboard_extension_v1_interface, 2,
                   zcr_keyboard_extension_data_.get(), bind_keyboard_extension);

  wl_global_create(wl_display_.get(),
                   &zwp_keyboard_shortcuts_inhibit_manager_v1_interface, 1,
                   display_, bind_keyboard_shortcuts_inhibit_manager);

  zwp_text_manager_data_ = std::make_unique<WaylandTextInputManager>(
      display_->seat()->xkb_tracker(), serial_tracker_.get());
  wl_global_create(wl_display_.get(), &zwp_text_input_manager_v1_interface, 1,
                   zwp_text_manager_data_.get(), bind_text_input_manager);

  zcr_text_input_extension_data_ =
      std::make_unique<WaylandTextInputExtension>();
  wl_global_create(wl_display_.get(), &zcr_text_input_extension_v1_interface,
                   GetTextInputExtensionV1Version(),
                   zcr_text_input_extension_data_.get(),
                   bind_text_input_extension);

  xdg_shell_data_ = std::make_unique<WaylandXdgShell>(
      display_, serial_tracker_.get(), rotation_serial_tracker_.get());
  wl_global_create(wl_display_.get(), &xdg_wm_base_interface, 3,
                   xdg_shell_data_.get(), bind_xdg_shell);

  wl_global_create(wl_display_.get(), &zcr_touchpad_haptics_v1_interface, 1,
                   display_, bind_touchpad_haptics);
}

void Server::Finalize(StartCallback callback, bool success) {
  // At this point, server creation was successful, so we should instantiate the
  // watcher.
  if (success) {
    wayland_watcher_ = std::make_unique<wayland::WaylandWatcher>(this);
  }
  std::move(callback).Run(success);
}

Server::~Server() {
  RemoveSecurityDelegate(wl_display_.get());
  // TODO(https://crbug.com/1124106): Investigate if we can eliminate Shutdown
  // methods.
  serial_tracker_->Shutdown();
}

// static
std::unique_ptr<Server> Server::Create(Display* display) {
  return Create(display, SecurityDelegate::GetDefaultSecurityDelegate());
}

// static
std::unique_ptr<Server> Server::Create(
    Display* display,
    std::unique_ptr<SecurityDelegate> security_delegate) {
  std::unique_ptr<Server> server(
      new Server(display, std::move(security_delegate)));
  server->Initialize();
  return server;
}

void Server::StartWithDefaultPath(StartCallback callback) {
  if (!Open()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  Finalize(std::move(callback), /*success=*/true);
}

void Server::StartWithFdAsync(base::ScopedFD fd, StartCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::MayBlock(),
      base::BindOnce(&Server::OpenFd, base::Unretained(this), std::move(fd)),
      base::BindOnce(&Server::Finalize, base::Unretained(this),
                     std::move(callback)));
}

bool Server::AddSocket(const std::string& name) {
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
  std::unique_ptr<WaylandDisplayOutput> output =
      std::move(outputs_[old_display.id()]);
  outputs_.erase(old_display.id());
  output.release()->OnDisplayRemoved();
}

wl_resource* Server::GetOutputResource(wl_client* client, int64_t display_id) {
  DCHECK_NE(display_id, display::kInvalidDisplayId);
  auto iter = outputs_.find(display_id);
  if (iter == outputs_.end()) {
    return nullptr;
  }
  return iter->second.get()->GetOutputResourceForClient(client);
}

void Server::AddWaylandOutput(int64_t id,
                              std::unique_ptr<WaylandDisplayOutput> output) {
  outputs_.insert(std::make_pair(id, std::move(output)));
}

}  // namespace wayland
}  // namespace exo
