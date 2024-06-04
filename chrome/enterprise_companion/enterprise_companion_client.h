// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_CLIENT_H_
#define CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_CLIENT_H_

#include "mojo/public/cpp/platform/named_platform_channel.h"

// Utilities useful to clients of Chrome Enterprise Companion.

namespace enterprise_companion {

// Returns the server name for establishing IPC via NamedMojoIpcServer.
mojo::NamedPlatformChannel::ServerName GetServerName();

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_CLIENT_H_
