// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_SERVER_CLIENT_UTIL_H_
#define COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_SERVER_CLIENT_UTIL_H_

#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"

namespace named_mojo_ipc_server {

// A platform-agnostic helper method to connect to a NamedMojoIpcServer.
// This calls into |mojo::NamedPlatformChannel::ConnectToServer| with
// additional brokerage steps on platforms that require them.
[[nodiscard]] mojo::PlatformChannelEndpoint ConnectToServer(
    const mojo::NamedPlatformChannel::ServerName& server_name);

}  // namespace named_mojo_ipc_server

#endif  // COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_SERVER_CLIENT_UTIL_H_
