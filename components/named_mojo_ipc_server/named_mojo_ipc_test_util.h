// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_TEST_UTIL_H_
#define COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_TEST_UTIL_H_

#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace named_mojo_ipc_server::test {

// Generates a random server name for unittests.
mojo::NamedPlatformChannel::ServerName GenerateRandomServerName();

}  // namespace named_mojo_ipc_server::test

#endif  // COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_TEST_UTIL_H_
