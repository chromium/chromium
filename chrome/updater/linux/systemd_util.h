// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_LINUX_SYSTEMD_UTIL_H_
#define CHROME_UPDATER_LINUX_SYSTEMD_UTIL_H_

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

// If the updater is started as a systemd service it can be launched by
// activity on a dedicated "activation" socket. This provides a means for
// unprivileged users to request systemd to start the update service as root.
// While the activation socket is not used for any IPC, the daemon process must
// accept connections. Otherwise, when the server shuts down normally, systemd
// will detect that there are pending connections on the socket and restart the
// server indefinitely.
// Thus, the |SystemdService| class is a utility which accepts connections on
// the activation socket (if one was provided to this process) and hangs up
// immediately.
class SystemdService {
 public:
  // SystemdService must be created on a thread capable of blocking IO. It is
  // suggested that this object be wrapped in a |base::SequenceBound|.
  SystemdService();
  SystemdService(const SystemdService&) = delete;
  SystemdService operator=(const SystemdService&) = delete;
  ~SystemdService();

 private:
  // Function signature for sd_listen_fds.
  using ListenFDsFunction = int (*)(int);

  void OnSocketReadable();

  SEQUENCE_CHECKER(sequence_checker_);
  // Note that |server_socket_| must outlive |read_watcher_controller_|;
  // otherwise a bad file descriptor error will occur at destruction.
  base::ScopedFD server_socket_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<base::FileDescriptorWatcher::Controller>
      read_watcher_controller_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<SystemdService> weak_factory_{this};
};

// Installs two systemd units: A service which points to the updater launcher
// and a socket which can activate the updater. When clients connect to the
// socket, systemd will ensure that the service is running. This allows non-root
// users to start the updater server as root for system-scope installations.
// Returns true on success.
[[nodiscard]] bool InstallSystemdUnits(UpdaterScope scope);

// Perform a best-effort uninstallation of systemd units. Returns true on
// success.
bool UninstallSystemdUnits(UpdaterScope scope);

// Checks for the existence of any updater systemd units.
[[nodiscard]] bool SystemdUnitsInstalled(UpdaterScope scope);
}  // namespace updater

#endif  // CHROME_UPDATER_LINUX_SYSTEMD_UTIL_H_
