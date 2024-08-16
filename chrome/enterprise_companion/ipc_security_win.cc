// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/ipc_security.h"

#include "base/functional/bind.h"
#include "components/named_mojo_ipc_server/connection_info.h"

namespace enterprise_companion {

IpcTrustDecider CreateIpcTrustDecider() {
  return base::BindRepeating(
      [](const named_mojo_ipc_server::ConnectionInfo& connector) {
        // IPC callers on Windows are authenticated via the DACL applied to the
        // stub's named pipe. Thus, any caller that can reach us is trusted.
        return true;
      });
}

}  // namespace enterprise_companion
