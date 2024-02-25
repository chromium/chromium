// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_IPC_SECURITY_H_
#define CHROME_UPDATER_IPC_IPC_SECURITY_H_

#include "chrome/updater/updater_scope.h"

namespace named_mojo_ipc_server {
struct ConnectionInfo;
}

namespace updater {

// Returns true iff the client identified by `connector` is the current user.
bool IsConnectionTrusted(
    const named_mojo_ipc_server::ConnectionInfo& connector);

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_IPC_SECURITY_H_
