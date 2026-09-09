// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_IPC_SECURITY_H_
#define CHROME_UPDATER_IPC_IPC_SECURITY_H_

#include "build/build_config.h"
#include "chrome/updater/updater_scope.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace named_mojo_ipc_server {
struct ConnectionInfo;
struct EndpointOptions;
}

namespace updater {

// Returns true if the client identified by `connector` is the current user.
bool IsConnectionTrusted(
    const named_mojo_ipc_server::ConnectionInfo& connector);

// Creates the options for instantiating the `NamedMojoIpcServer`.
named_mojo_ipc_server::EndpointOptions CreateServerEndpointOptions(
    const mojo::NamedPlatformChannel::ServerName& server_name);

#if BUILDFLAG(IS_WIN)
// Like above, but for the pipe under "ProtectedPrefix\Administrators", where
// only Administrators and LocalSystem may create pipes. System scope only.
named_mojo_ipc_server::EndpointOptions CreateProtectedServerEndpointOptions(
    UpdaterScope scope,
    const mojo::NamedPlatformChannel::ServerName& server_name);
#endif

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_IPC_SECURITY_H_
