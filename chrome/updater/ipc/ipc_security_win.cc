// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/ipc_security.h"

#include "components/named_mojo_ipc_server/connection_info.h"

namespace updater {

bool IsConnectionTrusted(
    const named_mojo_ipc_server::ConnectionInfo& connector) {
  // IPC callers on Windows are authenticated via the DACL applied to the stub's
  // named pipe. Thus, any caller that can reach us is trusted.
  return true;
}

}  // namespace updater
