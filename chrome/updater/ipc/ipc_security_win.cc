// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/ipc_security.h"

#include <windows.h>

#include <optional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/process/process.h"
#include "base/win/access_token.h"
#include "chrome/updater/get_updater_scope.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "mojo/public/c/system/invitation.h"
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

MojoSendInvitationFlags SendInvitationFlagsForIntegrityLevel(
    DWORD integrity_level) {
  // A caller at High integrity or above is an elevated administrator or
  // SYSTEM, and so at least as privileged as this process; an administrator
  // to SYSTEM transition is not a security boundary. Anything below that,
  // or a token with no integrity level, is declared untrusted.
  return integrity_level != MAXDWORD &&
                 integrity_level >= SECURITY_MANDATORY_HIGH_RID
             ? MOJO_SEND_INVITATION_FLAG_NONE
             : MOJO_SEND_INVITATION_FLAG_UNTRUSTED_PROCESS;
}

MojoSendInvitationFlags SendInvitationFlagsForCaller(
    const named_mojo_ipc_server::ConnectionInfo& info) {
  // Fail closed: a caller whose token cannot be read is declared untrusted.
  const std::optional<base::win::AccessToken> token =
      info.process.IsValid()
          ? base::win::AccessToken::FromProcess(info.process.Handle())
          : std::nullopt;
  return SendInvitationFlagsForIntegrityLevel(token ? token->IntegrityLevel()
                                                    : MAXDWORD);
}

named_mojo_ipc_server::EndpointOptions CreateServerEndpointOptions(
    UpdaterScope scope,
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  named_mojo_ipc_server::EndpointOptions options{
      server_name,
      named_mojo_ipc_server::EndpointOptions::kUseIsolatedConnection};

  if (IsSystemInstall(scope)) {
    // This server runs as SYSTEM while the DACL below lets any authenticated
    // user connect. A caller below High integrity is strictly less privileged
    // than this process, and is declared untrusted so that the connection
    // refuses objects whose safety depends on the caller's good behavior,
    // notably handles it claims to have already duplicated into this process.
    // See `Transport::CanAcceptReceiverOwnedHandles()`. Such a caller cannot
    // open this process and so never pre-duplicates legitimately; nothing it
    // could do before is lost.
    //
    // The declaration is made per caller rather than for the endpoint so that
    // elevated callers - administrators and SYSTEM - keep today's behavior.
    // They can open this process and pre-duplicate, and callers built before
    // this change do so, including during the connection handshake itself;
    // declaring them untrusted would refuse the connection outright. Elevated
    // callers built after this change declare the server elevated instead and
    // send handles for this process to duplicate; see
    // `UpdateServiceProxyMojoImpl::OnConnected()`.
    //
    // A user install has no such boundary - server and callers run as the same
    // user - so no caller is declared untrusted there.
    options.include_peer_process_info = true;
    options.send_invitation_flags_callback =
        base::BindRepeating(&SendInvitationFlagsForCaller);

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

named_mojo_ipc_server::EndpointOptions CreateProtectedServerEndpointOptions(
    UpdaterScope scope,
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  CHECK(IsSystemInstall(scope));
  named_mojo_ipc_server::EndpointOptions options =
      CreateServerEndpointOptions(scope, server_name);
  options.pipe_name_type =
      mojo::NamedPlatformChannel::PipeNameType::kAdminProtected;

  // A DACL to grant:
  // GA = Generic All
  // access to:
  // SY = LOCAL_SYSTEM
  // BA = BUILTIN_ADMINISTRATORS
  // 0x12019B = GENERIC_READ | GENERIC_WRITE excluding
  //            FILE_CREATE_PIPE_INSTANCE (0x4), which would let a client add
  //            an instance under this name and inherit its privileged owner
  // access to:
  // AU = AUTHENTICATED_USERS
  options.security_descriptor =
      L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;0x12019B;;;AU)";
  return options;
}

}  // namespace updater
