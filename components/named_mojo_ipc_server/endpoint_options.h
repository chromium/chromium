// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAMED_MOJO_IPC_SERVER_ENDPOINT_OPTIONS_H_
#define COMPONENTS_NAMED_MOJO_IPC_SERVER_ENDPOINT_OPTIONS_H_

#include <stdint.h>

#include <string>
#include <variant>

#include "build/build_config.h"
#include "mojo/public/c/system/invitation.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

#if BUILDFLAG(IS_WIN)
#include <string>
#endif

namespace named_mojo_ipc_server {

// Options used by NamedMojoIpcServer to start the server endpoint.
struct EndpointOptions {
  using MessagePipeId = std::variant<std::monostate, uint64_t, std::string>;

  // DEPRECATED: New callers should not use an isolated connection. Pass a
  // valid message pipe ID instead.
  static constexpr std::monostate kUseIsolatedConnection;

  EndpointOptions();
  EndpointOptions(mojo::NamedPlatformChannel::ServerName server_name,
                  const MessagePipeId& message_pipe_id);
  EndpointOptions(mojo::NamedPlatformChannel::ServerName server_name,
                  const MessagePipeId& message_pipe_id,
                  MojoSendInvitationFlags extra_send_invitation_flags);
#if BUILDFLAG(IS_WIN)
  EndpointOptions(mojo::NamedPlatformChannel::ServerName server_name,
                  const MessagePipeId& message_pipe_id,
                  MojoSendInvitationFlags extra_send_invitation_flags,
                  std::wstring security_descriptor);
#endif
  EndpointOptions(EndpointOptions&&);
  EndpointOptions(const EndpointOptions&);
  ~EndpointOptions();

  // The server name to start the NamedPlatformChannel. Must not be empty.
  mojo::NamedPlatformChannel::ServerName server_name;

  // The message pipe ID (either a number or a string). If provided, the client
  // must call ExtractMessagePipe() with the same ID. If not provided (or
  // kUseIsolatedConnection is used), the client must connect using an isolated
  // connection.
  // Note that using an isolated connection is DEPRECATED and new callers should
  // always pass a valid message pipe ID.
  MessagePipeId message_pipe_id = kUseIsolatedConnection;

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
