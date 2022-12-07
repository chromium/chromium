// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/posix/update_service_internal_stub.h"

#include <sys/types.h>
#include <unistd.h>

#include "components/named_mojo_ipc_server/connection_info.h"

namespace updater {

bool ConnectionHasSamePrivilege(
    const named_mojo_ipc_server::ConnectionInfo& connector) {
  return connector.credentials.uid == geteuid();
}

}  // namespace updater
