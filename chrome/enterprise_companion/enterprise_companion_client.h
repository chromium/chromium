// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_CLIENT_H_
#define CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_CLIENT_H_

#include <memory>
#include <optional>

#include "base/time/time.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/isolated_connection.h"

// Utilities useful to clients of Chrome Enterprise Companion App.

namespace base {
class Clock;
}

namespace enterprise_companion {

// Controls the transmission of usage stats (i.e. crash reports).
extern const char kEnableUsageStatsSwitch[];

extern const char kInstallIfNeededSwitch[];

// Returns the server name for establishing IPC via NamedMojoIpcServer.
mojo::NamedPlatformChannel::ServerName GetServerName();

// Connects to the IPC server. `callback` is answered on the calling sequence,
// the IsolationConnection and Remote may be null/invalid if the service could
// not be reached. The returned `Remote` is bound to the calling sequence.
void ConnectToServer(
    base::OnceCallback<void(std::unique_ptr<mojo::IsolatedConnection>,
                            mojo::Remote<mojom::EnterpriseCompanion>)> callback,
    const mojo::NamedPlatformChannel::ServerName& server_name =
        GetServerName());

// Connects to the IPC server, attempting to launch the installed Chrome
// Enterprise Companion App if necessary. This function repeatedly attempts
// to connect to the server until `timeout` has expired, launching the
// application at most once. `callback` is answered on the calling sequence, the
// IsolationConnection and Remote may be null/invalid if the service could not
// be reached. The returned `Remote` is bound to the calling sequence.
// If the companion app is not already running, `enable_usagestats` controls
// whether the transmission of usage stats is enabled for the lifetime of the
// process.
void ConnectAndLaunchServer(
    const base::Clock* clock,
    base::TimeDelta timeout,
    bool enable_usagestats,
    base::OnceCallback<void(std::unique_ptr<mojo::IsolatedConnection>,
                            mojo::Remote<mojom::EnterpriseCompanion>)> callback,
    const mojo::NamedPlatformChannel::ServerName& server_name =
        GetServerName());

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_CLIENT_H_
