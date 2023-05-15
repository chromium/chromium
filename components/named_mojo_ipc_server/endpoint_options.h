// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAMED_MOJO_IPC_SERVER_ENDPOINT_OPTIONS_H_
#define COMPONENTS_NAMED_MOJO_IPC_SERVER_ENDPOINT_OPTIONS_H_

#include "build/build_config.h"
#include "mojo/public/c/system/invitation.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include <string>
#endif

namespace named_mojo_ipc_server {

// Options used by NamedMojoIpcServer to start the server endpoint.
struct EndpointOptions {
  // DEPRECATED: New callers should not use an isolated connection. Pass a
  // valid message pipe ID instead.
  static constexpr absl::optional<uint64_t> kUseIsolatedConnection =
      absl::nullopt;

  // The server name to start the NamedPlatformChannel. Must not be empty.
  mojo::NamedPlatformChannel::ServerName server_name;

  // The message pipe ID. If provided, the client must call ExtractMessagePipe()
  // with the same ID. If not provided (or kUseIsolatedConnection is used), the
  // client must connect using an isolated connection.
  // Note that using an isolated connection is DEPRECATED and new callers should
  // always pass a valid message pipe ID.
  absl::optional<uint64_t> message_pipe_id;

  // Extra flags added when sending the outgoing invitation.
  MojoSendInvitationFlags extra_send_invitation_flags =
      MOJO_SEND_INVITATION_FLAG_NONE;

#if BUILDFLAG(IS_WIN)
  // If non-empty, a security descriptor to use when creating the pipe. If
  // empty, a default security descriptor will be used.
  std::wstring security_descriptor;
#endif
};

}  // namespace named_mojo_ipc_server

#endif  // COMPONENTS_NAMED_MOJO_IPC_SERVER_ENDPOINT_OPTIONS_H_
