// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_internal_proxy_posix.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/ipc/ipc_names.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "mojo/public/cpp/platform/platform_channel.h"

namespace updater {

mojo::PlatformChannelEndpoint ConnectMojo(UpdaterScope scope, int retries) {
  if (retries == 1) {
    // Launch a server process.
    absl::optional<base::FilePath> updater = GetUpdaterExecutablePath(scope);
    if (updater) {
      base::CommandLine command(*updater);
      command.AppendSwitch(kServerSwitch);
      command.AppendSwitchASCII(kServerServiceSwitch,
                                kServerUpdateServiceInternalSwitchValue);
      if (scope == UpdaterScope::kSystem) {
        command.AppendSwitch(kSystemSwitch);
      }
      command.AppendSwitch(kEnableLoggingSwitch);
      command.AppendSwitchASCII(kLoggingModuleSwitch,
                                kLoggingModuleSwitchValue);
      base::LaunchProcess(command, {});
    }
  }

  return named_mojo_ipc_server::ConnectToServer(
      GetUpdateServiceInternalServerName(scope));
}

}  // namespace updater
