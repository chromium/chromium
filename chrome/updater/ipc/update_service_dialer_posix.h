// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_DIALER_POSIX_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_DIALER_POSIX_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "chrome/updater/updater_scope.h"

namespace base {
class Time;
}  // namespace base

namespace mojo {
class PlatformChannelEndpoint;
}  // namespace mojo

namespace updater {

// Start the update service. Returns false if the service can't be reached.
[[nodiscard]] bool DialUpdateService(UpdaterScope scope);

// Start the update internal service. Returns false if the service can't be
// reached.
[[nodiscard]] bool DialUpdateInternalService(UpdaterScope scope);

// Connect to the server.
void ConnectMojo(
    UpdaterScope scope,
    bool is_internal_service,
    base::Time deadline,
    base::OnceCallback<void(std::optional<mojo::PlatformChannelEndpoint>)>
        connected_callback);

}  // namespace updater
#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_DIALER_POSIX_H_
