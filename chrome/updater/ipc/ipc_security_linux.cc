// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/ipc_security.h"

#include <sys/types.h>
#include <unistd.h>

#include "components/named_mojo_ipc_server/connection_info.h"

namespace updater {

bool IsConnectionTrusted(
    const named_mojo_ipc_server::ConnectionInfo& connector) {
  return connector.credentials.uid == geteuid();
}

}  // namespace updater
