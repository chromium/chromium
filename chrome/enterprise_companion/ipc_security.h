// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_IPC_SECURITY_H_
#define CHROME_ENTERPRISE_COMPANION_IPC_SECURITY_H_

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "components/named_mojo_ipc_server/connection_info.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

namespace enterprise_companion {

// Returns true if IPC caller is allowed.
using IpcTrustDecider =
    base::RepeatingCallback<bool(const named_mojo_ipc_server::ConnectionInfo&)>;

IpcTrustDecider CreateIpcTrustDecider();

#if BUILDFLAG(IS_WIN)
// Returns true if a caller whose token has `integrity_level` is trusted. Only
// callers at High integrity or above (elevated administrators and SYSTEM) are
// trusted. `MAXDWORD`, which indicates that the token has no integrity level,
// is not trusted.
bool IsTrustedIntegrityLevel(DWORD integrity_level);
#endif

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_IPC_SECURITY_H_
