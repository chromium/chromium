// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/endpoint_options.h"

namespace named_mojo_ipc_server {

EndpointOptions::EndpointOptions() = default;

EndpointOptions::EndpointOptions(
    mojo::NamedPlatformChannel::ServerName server_name,
    const MessagePipeId& message_pipe_id)
    : EndpointOptions(server_name,
                      message_pipe_id,
                      MOJO_SEND_INVITATION_FLAG_NONE) {}

EndpointOptions::EndpointOptions(
    mojo::NamedPlatformChannel::ServerName server_name,
    const MessagePipeId& message_pipe_id,
    MojoSendInvitationFlags extra_send_invitation_flags)
    : server_name(server_name),
      message_pipe_id(message_pipe_id),
      extra_send_invitation_flags(extra_send_invitation_flags) {}

#if BUILDFLAG(IS_WIN)
EndpointOptions::EndpointOptions(
    mojo::NamedPlatformChannel::ServerName server_name,
    const MessagePipeId& message_pipe_id,
    MojoSendInvitationFlags extra_send_invitation_flags,
    std::wstring security_descriptor)
    : server_name(server_name),
      message_pipe_id(message_pipe_id),
      extra_send_invitation_flags(extra_send_invitation_flags),
      security_descriptor(security_descriptor) {}
#endif

EndpointOptions::EndpointOptions(EndpointOptions&&) = default;
EndpointOptions::EndpointOptions(const EndpointOptions&) = default;
EndpointOptions::~EndpointOptions() = default;

}  // namespace named_mojo_ipc_server
