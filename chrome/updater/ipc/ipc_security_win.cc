// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/ipc_security.h"

#include "chrome/updater/updater_scope.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace updater {

bool IsConnectionTrusted(
    const named_mojo_ipc_server::ConnectionInfo& connector) {
  // IPC callers on Windows are authenticated via the DACL applied to the stub's
  // named pipe (see `CreateServerEndpointOptions` below).
  // TODO(crbug.com/456542123): Set to `true` for system after the client proxy
  // allows impersonation and the server stub gates method calls based on the
  // client's integrity levels.
  return !IsSystemInstall();
}

named_mojo_ipc_server::EndpointOptions CreateServerEndpointOptions(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  named_mojo_ipc_server::EndpointOptions options{
      server_name,
      named_mojo_ipc_server::EndpointOptions::kUseIsolatedConnection};

  if (IsSystemInstall()) {
    // A DACL to grant:
    // GA = Generic All
    // access to:
    // SY = LOCAL_SYSTEM
    // BA = BUILTIN_ADMINISTRATORS
    // GR = Generic Read
    // GW = Generic Write
    // access to:
    // AU = AUTHENTICATED_USERS
    options.security_descriptor = L"D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;AU)";
  }

  return options;
}

}  // namespace updater
