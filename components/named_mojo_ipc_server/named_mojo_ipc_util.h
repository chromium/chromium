// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_UTIL_H_
#define COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_UTIL_H_

#include <string_view>

#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace named_mojo_ipc_server {

// Creates a server name that is independent to the working directory, i.e.
// it resolves to the same channel no matter which working directory you are
// running the binary from.
mojo::NamedPlatformChannel::ServerName
WorkingDirectoryIndependentServerNameFromUTF8(std::string_view name);

}  // namespace named_mojo_ipc_server

#endif  // COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_UTIL_H_
