// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_SERVER_H_
#define COMPONENTS_EXO_WAYLAND_SERVER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/exo/wayland/scoped_wl.h"
#include "ui/display/display_observer.h"

struct wl_resource;
struct wl_client;

namespace exo {
class SecurityDelegate;
class Display;

namespace wayland {

class SerialTracker;
class UiControls;
struct WaylandDataDeviceManager;
class WaylandDisplayOutput;
struct WaylandKeyboardExtension;
struct WaylandSeat;
struct WaylandTextInputExtension;
struct WaylandTextInputManager;
struct WaylandXdgShell;
struct WaylandZxdgShell;
struct WaylandRemoteShellData;
class WaylandDmabufFeedbackManager;
class WestonTest;
class WaylandWatcher;

// This class is a thin wrapper around a Wayland display server. All Wayland
// requests are dispatched into the given Exosphere display.
class Server : public display::DisplayObserver {
 public:
  using StartCallback =
      base::OnceCallback<void(bool, const base::FilePath& path)>;

  Server(Display* display, std::unique_ptr<SecurityDelegate> security_delegate);

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  ~Server() override;

  // Creates a Wayland display server that clients can connect to using the
  // default socket name.
  static std::unique_ptr<Server> Create(Display* display);

  // As above, but with the given |security_delegate|.
  static std::unique_ptr<Server> Create(
      Display* display,
      std::unique_ptr<SecurityDelegate> security_delegate);

  // In cases where the server was started asynchronously, this helper can be
  // used to delete it asynchronously as well.
  static void DestroyAsync(std::unique_ptr<Server> server);

  // TODO(b/270254359): deprecate go/secure-exo-ids in favour of
  // go/securer-exo-ids.
  void StartAsync(StartCallback callback);
  void StartWithDefaultPath(StartCallback callback);
  void StartWithFdAsync(base::ScopedFD fd, StartCallback callback);

  void Initialize();

  bool Open(bool default_path);

  bool OpenFd(base::ScopedFD fd);

  void Finalize(StartCallback callback, bool success);

  // Returns the file descriptor associated with the server.
  int GetFileDescriptor() const;

  // This function dispatches events. This must be called on a thread for
  // which it's safe to access the Exosphere display that this server was
  // created for. The |timeout| argument specifies the amount of time that
  // Dispatch() should block waiting for the file descriptor to become ready.
  void Dispatch(base::TimeDelta timeout);

  // Send all buffered events to the clients.
  void Flush();

  // Overridden from display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;

  wl_resource* GetOutputResource(wl_client* client, int64_t display_id);

  Display* GetDisplay() { return display_; }

  // Public version of the protected accessor below, to be used in tests.
  wl_display* GetWaylandDisplayForTesting() const {
    return GetWaylandDisplay();
  }

  // Returns the path to the wayland socket used by this server. Returns "" if
  // StarTWithDefaultPath() hasn't been called, or StartWithFd() was called.
  const base::FilePath& socket_path() const { return socket_path_; }

 protected:
  friend class UiControls;
  friend class WestonTest;
  void AddWaylandOutput(int64_t id,
                        std::unique_ptr<WaylandDisplayOutput> output);
  wl_display* GetWaylandDisplay() const { return wl_display_.get(); }

 private:
  friend class ScopedEventDispatchDisabler;

  // This adds a Unix socket to the Wayland display server which can be used
  // by clients to connect to the display server.
  bool AddSocket(const std::string& name);

  // This has the server's socket inside it, so it must be deleted last.
  base::ScopedTempDir socket_dir_;
  const raw_ptr<Display, ExperimentalAsh> display_;
  std::unique_ptr<SecurityDelegate> security_delegate_;
  // Deleting wl_display depends on SerialTracker.
  std::unique_ptr<SerialTracker> serial_tracker_;
  std::unique_ptr<wl_display, WlDisplayDeleter> wl_display_;
  base::flat_map<int64_t, std::unique_ptr<WaylandDisplayOutput>> outputs_;
  std::unique_ptr<WaylandDataDeviceManager> data_device_manager_data_;
  std::unique_ptr<WaylandSeat> seat_data_;
  display::ScopedDisplayObserver display_observer_{this};
  std::unique_ptr<wayland::WaylandWatcher> wayland_watcher_;
  base::FilePath socket_path_;
  std::unique_ptr<WaylandDmabufFeedbackManager> wayland_feedback_manager_;

  std::unique_ptr<WaylandKeyboardExtension> zcr_keyboard_extension_data_;
  std::unique_ptr<WaylandTextInputManager> zwp_text_manager_data_;
  std::unique_ptr<WaylandTextInputExtension> zcr_text_input_extension_data_;
  std::unique_ptr<WaylandZxdgShell> zxdg_shell_data_;
  std::unique_ptr<WaylandXdgShell> xdg_shell_data_;
  std::unique_ptr<WaylandRemoteShellData> remote_shell_data_;
  std::unique_ptr<WestonTest> weston_test_holder_;
  std::unique_ptr<UiControls> ui_controls_holder_;
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_SERVER_H_
