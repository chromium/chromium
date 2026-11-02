// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/ipc_security.h"

#include <windows.h>

#include <optional>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/win/access_token.h"
#include "components/named_mojo_ipc_server/connection_info.h"

namespace enterprise_companion {

bool IsTrustedIntegrityLevel(DWORD integrity_level) {
  // A caller at High integrity or above is an elevated administrator or
  // SYSTEM, and so at least as privileged as this process; an administrator
  // to SYSTEM transition is not a security boundary. Anything below that, or a
  // token with no integrity level, is untrusted.
  return integrity_level != MAXDWORD &&
         integrity_level >= SECURITY_MANDATORY_HIGH_RID;
}

IpcTrustDecider CreateIpcTrustDecider() {
  return base::BindRepeating(
      [](const named_mojo_ipc_server::ConnectionInfo& info) {
        if (!info.process.IsValid()) {
          VLOG(1) << "IPC caller " << info.pid
                  << " is untrusted: its process could not be opened.";
          return false;
        }
        const std::optional<base::win::AccessToken> token =
            base::win::AccessToken::FromProcess(info.process.Handle());
        if (!token) {
          VPLOG(1) << "IPC caller " << info.pid
                   << " is untrusted: its token could not be read.";
          return false;
        }
        const DWORD integrity_level = token->IntegrityLevel();
        const bool trusted = IsTrustedIntegrityLevel(integrity_level);
        VLOG_IF(1, !trusted)
            << "IPC caller " << info.pid << " is untrusted: integrity level 0x"
            << std::hex << integrity_level << ".";
        return trusted;
      });
}

}  // namespace enterprise_companion
