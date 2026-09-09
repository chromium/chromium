// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_DIALER_WIN_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_DIALER_WIN_H_

#include <wrl/client.h>

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/functional/function_ref.h"
#include "chrome/updater/updater_scope.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace base {
class Time;
}  // namespace base

namespace mojo {
class PlatformChannelEndpoint;
}  // namespace mojo

namespace updater {

// Connect to the server.
void ConnectMojo(
    UpdaterScope scope,
    bool is_internal_service,
    base::Time deadline,
    base::OnceCallback<void(std::optional<mojo::PlatformChannelEndpoint>,
                            Microsoft::WRL::ComPtr<IUnknown>)>
        connected_callback);

// Prefers the admin-protected pipe, downgrading to the legacy one only for a
// server that does not serve it. `connect` and `is_pipe_missing` are injected
// so the selection can be tested without a privileged server.
std::optional<mojo::PlatformChannelEndpoint> SelectUpdateServiceEndpoint(
    base::FunctionRef<std::optional<mojo::PlatformChannelEndpoint>(
        mojo::NamedPlatformChannel::PipeNameType)> connect,
    base::FunctionRef<bool(mojo::NamedPlatformChannel::PipeNameType)>
        is_pipe_missing);

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_DIALER_WIN_H_
