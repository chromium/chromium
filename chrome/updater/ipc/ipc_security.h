// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_IPC_SECURITY_H_
#define CHROME_UPDATER_IPC_IPC_SECURITY_H_

#include "build/build_config.h"
#include "chrome/updater/updater_scope.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#include "mojo/public/c/system/invitation.h"
#endif

namespace named_mojo_ipc_server {
struct ConnectionInfo;
struct EndpointOptions;
}

namespace updater {

// Returns true if the client identified by `connector` is the current user.
bool IsConnectionTrusted(
    const named_mojo_ipc_server::ConnectionInfo& connector);

// Creates the options for instantiating the `NamedMojoIpcServer` serving
// `scope`. How the endpoint is secured is derived from `scope`, which callers
// must also derive `server_name` from, so that a pipe's name and its
// protection cannot disagree.
//
// In production these never diverge: the only caller is `UpdateServiceStub`,
// whose scope is `App::updater_scope()`, which is initialized from
// `GetUpdaterScope()` - the same value the parameterless `IsSystemInstall()`
// reads. Passing it explicitly keeps the decision an input rather than ambient
// process state, matches `CreateProtectedServerEndpointOptions()` below, and
// lets tests cover both scopes without mutating the process command line.
named_mojo_ipc_server::EndpointOptions CreateServerEndpointOptions(
    UpdaterScope scope,
    const mojo::NamedPlatformChannel::ServerName& server_name);

#if BUILDFLAG(IS_WIN)
// Like above, but for the pipe under "ProtectedPrefix\Administrators", where
// only Administrators and LocalSystem may create pipes. System scope only.
named_mojo_ipc_server::EndpointOptions CreateProtectedServerEndpointOptions(
    UpdaterScope scope,
    const mojo::NamedPlatformChannel::ServerName& server_name);

// The extra invitation flags a system install's server uses for a caller,
// chosen by the caller's token integrity level: a caller at High integrity or
// above (an elevated administrator or SYSTEM) gets none, any other caller is
// declared untrusted. `MAXDWORD` denotes a token with no integrity level, or
// one that could not be read, and is treated as untrusted.
MojoSendInvitationFlags SendInvitationFlagsForIntegrityLevel(
    DWORD integrity_level);

// `SendInvitationFlagsForIntegrityLevel()` applied to the token of
// `info.process`, which must have been opened by the server (see
// `EndpointOptions::include_peer_process_info`).
MojoSendInvitationFlags SendInvitationFlagsForCaller(
    const named_mojo_ipc_server::ConnectionInfo& info);
#endif

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_IPC_SECURITY_H_
