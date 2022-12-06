// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_internal_proxy_posix.h"

#include "chrome/updater/ipc/ipc_names.h"
#include "chrome/updater/updater_scope.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "mojo/public/cpp/platform/platform_channel.h"

namespace updater {

mojo::PlatformChannelEndpoint ConnectMojo(UpdaterScope scope, int retries) {
  return named_mojo_ipc_server::ConnectToServer(
      GetUpdateServiceInternalServerName(scope));
}

}  // namespace updater
