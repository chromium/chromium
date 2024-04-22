// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_dialer.h"

#include <sys/socket.h>
#include <sys/un.h>

#include <cstdio>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/linux/ipc_constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"

namespace updater {

// Start the update service by connecting to its activation socket. This will
// cause systemd to launch the service as the appropriate user.
bool DialUpdateService(UpdaterScope scope) {
  base::FilePath activation_socket_path = GetActivationSocketPath(scope);
  if (!base::PathExists(activation_socket_path)) {
    // If there's no activation socket present, abandon dialing.
    return false;
  }

  base::ScopedFD sock_fd(socket(AF_UNIX, SOCK_STREAM, 0));
  if (!sock_fd.is_valid()) {
    VPLOG(1) << "Could not create socket";
    return false;
  }

  struct sockaddr_un remote;
  remote.sun_family = AF_UNIX;
  snprintf(remote.sun_path, sizeof(remote.sun_path), "%s",
           activation_socket_path.AsUTF8Unsafe().c_str());
  int len = strlen(remote.sun_path) + sizeof(remote.sun_family);
  if (connect(sock_fd.get(), (struct sockaddr*)&remote, len) == -1) {
    VPLOG(1) << "Could not connect to activation socket";
    return false;
  }

  return true;
}

bool DialUpdateInternalService(UpdaterScope scope) {
  std::optional<base::FilePath> updater = GetUpdaterExecutablePath(scope);
  if (updater) {
    base::CommandLine command(*updater);
    command.AppendSwitch(kServerSwitch);
    command.AppendSwitchASCII(kServerServiceSwitch,
                              kServerUpdateServiceInternalSwitchValue);
    if (scope == UpdaterScope::kSystem) {
      command.AppendSwitch(kSystemSwitch);
    }
    base::LaunchProcess(command, {});
  }
  return true;
}

}  // namespace updater
