// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_DIALER_WIN_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_DIALER_WIN_H_

#include <windows.h>

#include <wrl/client.h>

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/functional/function_ref.h"
#include "base/types/expected.h"
#include "chrome/updater/updater_scope.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace base {
class Time;
}  // namespace base

namespace mojo {
class PlatformChannelEndpoint;
}  // namespace mojo

namespace updater {

// Why a connection attempt failed, which decides what `ConnectMojo` does next.
enum class ConnectToServerError {
  // A transient failure, such as the server still starting, a missing pipe, or
  // a process/token lookup error. `ConnectMojo` keeps retrying with backoff
  // until its (at most 30 second) deadline.
  kRetryable,
  // The server, or this client's access to it, failed a security check: the
  // pipe's DACL denied access, the server process is not privileged or
  // elevated, or COM denied activation. Retrying cannot succeed, so
  // `ConnectMojo` fails the whole connect sequence immediately.
  kRejected,
};

// Classifies a failed `CoCreateInstance` of the updater server. Permission
// errors are `kRejected`; everything else is `kRetryable`.
ConnectToServerError ClassifyDialError(HRESULT hr);

// Connect to the server.
void ConnectMojo(
    UpdaterScope scope,
    bool is_internal_service,
    base::Time deadline,
    base::OnceCallback<void(std::optional<mojo::PlatformChannelEndpoint>,
                            Microsoft::WRL::ComPtr<IUnknown>)>
        connected_callback);

// Connects to the named pipe for `server_name` and `pipe_name_type`, verifying
// server privilege (and elevation when `scope` is a system install). Only
// `ConnectMojo` uses this in production; it is exposed here for tests.
base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError>
ConnectToUpdateService(
    UpdaterScope scope,
    const mojo::NamedPlatformChannel::ServerName& server_name,
    mojo::NamedPlatformChannel::PipeNameType pipe_name_type);

// Prefers the admin-protected pipe, downgrading to the legacy one only for a
// server that does not serve it. `connect` and `is_pipe_missing` are injected
// so the selection can be tested without a privileged server.
base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError>
SelectUpdateServiceEndpoint(
    base::FunctionRef<
        base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError>(
            mojo::NamedPlatformChannel::PipeNameType)> connect,
    base::FunctionRef<bool(mojo::NamedPlatformChannel::PipeNameType)>
        is_pipe_missing);

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_DIALER_WIN_H_
