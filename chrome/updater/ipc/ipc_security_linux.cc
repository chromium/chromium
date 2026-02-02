// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/ipc_security.h"

#include <sys/types.h>
#include <unistd.h>

#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace updater {

bool IsConnectionTrusted(
    const named_mojo_ipc_server::ConnectionInfo& connector) {
  return connector.credentials.uid == geteuid();
}

named_mojo_ipc_server::EndpointOptions CreateServerEndpointOptions(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  named_mojo_ipc_server::EndpointOptions options;
  options.server_name = server_name;
  options.message_pipe_id =
      named_mojo_ipc_server::EndpointOptions::kUseIsolatedConnection;
  options.require_same_peer_user = false;
  return options;
}

}  // namespace updater
