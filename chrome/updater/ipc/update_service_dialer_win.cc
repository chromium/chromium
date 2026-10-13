// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_dialer.h"

#include <windows.h>

#include <wrl/client.h>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/win/access_token.h"
#include "base/win/scoped_handle.h"
#include "base/win/sid.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/ipc/ipc_names.h"
#include "chrome/updater/ipc/update_service_dialer_win.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"

namespace updater {

namespace {

CLSID GetUpdaterClsid(UpdaterScope scope) {
  return IsSystemInstall(scope) ? __uuidof(UpdaterSystemClass)
                                : __uuidof(UpdaterUserClass);
}

CLSID GetUpdaterInternalClsid(UpdaterScope scope) {
  return IsSystemInstall(scope) ? __uuidof(UpdaterInternalSystemClass)
                                : __uuidof(UpdaterInternalUserClass);
}

// Start the update service or the update internal service. Returns a valid
// `Microsoft::WRL::ComPtr<IUnknown>` if the service can be created, or the
// classified activation error otherwise.
base::expected<Microsoft::WRL::ComPtr<IUnknown>, ConnectToServerError>
DialUpdateService(UpdaterScope scope, bool is_internal_service) {
  Microsoft::WRL::ComPtr<IUnknown> server;
  const CLSID clsid = is_internal_service ? GetUpdaterInternalClsid(scope)
                                          : GetUpdaterClsid(scope);
  const HRESULT hr = ::CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER,
                                        IID_PPV_ARGS(&server));
  if (FAILED(hr)) {
    VLOG(2) << "::CoCreateInstance failed: " << StringFromGuid(clsid) << ": "
            << std::hex << hr;
    return base::unexpected(ClassifyDialError(hr));
  }

  return server;
}

base::expected<void, ConnectToServerError> CheckServerElevated(
    HANDLE pipe_handle,
    mojo::NamedPlatformChannel::PipeNameType pipe_name_type) {
  DWORD pid = 0;
  if (!::GetNamedPipeServerProcessId(pipe_handle, &pid)) {
    PLOG(ERROR) << "Failed to get named pipe server process ID";
    return base::unexpected(ConnectToServerError::kRetryable);
  }

  base::win::ScopedHandle process(
      ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
  if (!process.is_valid()) {
    if (::GetLastError() != ERROR_ACCESS_DENIED) {
      PLOG(ERROR) << "Failed to open named pipe server process " << pid;
      return base::unexpected(ConnectToServerError::kRetryable);
    }
    // Only the protected pipe's DACL denies other users new instances, so only
    // there does the pipe owner identify the server.
    if (pipe_name_type !=
            mojo::NamedPlatformChannel::PipeNameType::kAdminProtected ||
        !mojo::NamedPlatformChannel::IsPipeOwnerPrivileged(pipe_handle)) {
      return base::unexpected(ConnectToServerError::kRejected);
    }
    return base::ok();
  }

  std::optional<base::win::AccessToken> server_token =
      base::win::AccessToken::FromProcess(process.get());
  if (!server_token) {
    LOG(ERROR) << "Failed to get access token for server process " << pid;
    return base::unexpected(ConnectToServerError::kRetryable);
  }

  const base::win::Sid owner = server_token->Owner();
  if (owner != base::win::Sid(base::win::WellKnownSid::kLocalSystem) &&
      owner !=
          base::win::Sid(base::win::WellKnownSid::kBuiltinAdministrators)) {
    return base::unexpected(ConnectToServerError::kRejected);
  }
  return base::ok();
}

// `::WaitNamedPipeW` reports ERROR_FILE_NOT_FOUND only when NPFS has no pipe
// registered under the name, and ERROR_PATH_NOT_FOUND when the prefix
// directory itself is absent; a pipe that exists but is busy reports
// ERROR_SEM_TIMEOUT instead.
bool IsPipeMissing(const mojo::NamedPlatformChannel::ServerName& server_name,
                   mojo::NamedPlatformChannel::PipeNameType pipe_name_type) {
  const std::wstring pipe_name =
      mojo::NamedPlatformChannel::GetPipeNameFromServerName(server_name,
                                                            pipe_name_type);
  if (::WaitNamedPipeW(pipe_name.c_str(), 1)) {
    return false;
  }
  const DWORD error = ::GetLastError();
  return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

void ConnectMojoImpl(
    UpdaterScope scope,
    bool is_internal_service,
    int tries,
    base::Time deadline,
    Microsoft::WRL::ComPtr<IUnknown> server,
    base::OnceCallback<void(std::optional<mojo::PlatformChannelEndpoint>,
                            Microsoft::WRL::ComPtr<IUnknown>)>
        connected_callback) {
  if (base::Time::Now() > deadline) {
    VLOG(1) << "Failed to connect to remote mojo service, is_internal_service: "
            << is_internal_service << ", scope: " << scope
            << ", connection timed out.";
    std::move(connected_callback).Run(std::nullopt, {});
    return;
  }

  base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError> result =
      [&]
      -> base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError> {
    // Dial once per connect sequence and hold the reference across retries.
    // Releasing it between attempts can drop the server's WRL module object
    // count to zero, at which point the `Module<OutOfProc>` release notifier
    // calls `AppServerWin::Stop`, which revokes the class objects and shuts
    // the server down; re-dialing then activates yet another server process.
    // For user installs, COM launches a new server process once the class
    // objects are revoked. For system installs, the server runs as a Windows
    // service, and `AppServerWin::Stop` first reports `SERVICE_STOP_PENDING`
    // so that the SCM hands new activations to a fresh service process. Since
    // the reference is never re-acquired, a server that dies while a client is
    // connecting fails this sequence; the proxy reconnects from scratch on
    // the next RPC.
    if (!server) {
      base::expected<Microsoft::WRL::ComPtr<IUnknown>, ConnectToServerError>
          dialed = DialUpdateService(scope, is_internal_service);
      if (!dialed.has_value()) {
        return base::unexpected(dialed.error());
      }
      server = std::move(*dialed);
    }

    const mojo::NamedPlatformChannel::ServerName server_name =
        is_internal_service ? GetUpdateServiceInternalServerName(scope)
                            : GetUpdateServiceServerName(scope);

    if (IsSystemInstall(scope) && !is_internal_service) {
      return SelectUpdateServiceEndpoint(
          [&](mojo::NamedPlatformChannel::PipeNameType pipe_name_type) {
            return ConnectToUpdateService(scope, server_name, pipe_name_type);
          },
          [&](mojo::NamedPlatformChannel::PipeNameType pipe_name_type) {
            return IsPipeMissing(server_name, pipe_name_type);
          });
    }

    return ConnectToUpdateService(
        scope, server_name, mojo::NamedPlatformChannel::PipeNameType::kDefault);
  }();

  if (result.has_value()) {
    std::move(connected_callback).Run(std::move(*result), std::move(server));
    return;
  }

  if (result.error() == ConnectToServerError::kRejected) {
    VLOG(1) << "Failed to connect to remote mojo service, is_internal_service: "
            << is_internal_service << ", scope: " << scope
            << ", server rejected.";
    std::move(connected_callback).Run(std::nullopt, {});
    return;
  }

  // `server` is still null only if `CoCreateInstance` failed with a retryable
  // error, and every earlier attempt must have failed the same way, so `tries`
  // counts failed dials. A failed dial is not waiting for a server that is
  // already starting, so the time-bounded wait below does not apply; keep the
  // previous limit of two attempts.
  constexpr int kMaxDialAttempts = 2;
  if (!server && tries + 1 >= kMaxDialAttempts) {
    VLOG(1) << "Failed to activate remote mojo service, is_internal_service: "
            << is_internal_service << ", scope: " << scope;
    std::move(connected_callback).Run(std::nullopt, {});
    return;
  }

  // Keep retrying until `deadline`, which is clamped in `ConnectMojo`. A retry
  // does not relaunch anything; it only waits for the server that is already
  // starting to publish its pipe, so back off linearly up to a cap.
  constexpr base::TimeDelta kRetryBackoffStep = base::Milliseconds(200);
  constexpr base::TimeDelta kMaxRetryBackoff = base::Seconds(1);
  const base::TimeDelta delay =
      std::min(kRetryBackoffStep * (tries + 1), kMaxRetryBackoff);
  VLOG(2) << "Failed to connect to remote mojo service, is_internal_service: "
          << is_internal_service << ", scope: " << scope << ", retrying in "
          << delay;
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ConnectMojoImpl, scope, is_internal_service, tries + 1,
                     deadline, std::move(server),
                     std::move(connected_callback)),
      delay);
}

}  // namespace

ConnectToServerError ClassifyDialError(HRESULT hr) {
  // DCOM reports a caller that lacks launch or activation permission for the
  // class as `E_ACCESSDENIED`. That is a configuration or identity problem,
  // not a transient one, so retrying cannot help.
  return hr == E_ACCESSDENIED ? ConnectToServerError::kRejected
                              : ConnectToServerError::kRetryable;
}

base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError>
ConnectToUpdateService(
    UpdaterScope scope,
    const mojo::NamedPlatformChannel::ServerName& server_name,
    mojo::NamedPlatformChannel::PipeNameType pipe_name_type) {
  mojo::NamedPlatformChannel::Options options;
  options.server_name = server_name;
  options.allow_impersonation = true;
  options.verify_server_privilege = true;
  options.pipe_name_type = pipe_name_type;
  mojo::PlatformChannelEndpoint connected_endpoint =
      named_mojo_ipc_server::ConnectToServer(options);
  if (!connected_endpoint.is_valid()) {
    // `NamedPlatformChannel::ConnectToServer` preserves `ERROR_ACCESS_DENIED`
    // in `GetLastError()` when either `CreateFileW` is denied by the pipe's
    // DACL or `VerifyServerPrivilege` rejects the server process.
    return base::unexpected(::GetLastError() == ERROR_ACCESS_DENIED
                                ? ConnectToServerError::kRejected
                                : ConnectToServerError::kRetryable);
  }
  if (IsSystemInstall(scope)) {
    if (base::expected<void, ConnectToServerError> elevated =
            CheckServerElevated(
                connected_endpoint.platform_handle().GetHandle().get(),
                pipe_name_type);
        !elevated.has_value()) {
      return base::unexpected(elevated.error());
    }
  }
  return connected_endpoint;
}

base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError>
SelectUpdateServiceEndpoint(
    base::FunctionRef<
        base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError>(
            mojo::NamedPlatformChannel::PipeNameType)> connect,
    base::FunctionRef<bool(mojo::NamedPlatformChannel::PipeNameType)>
        is_pipe_missing) {
  constexpr auto kProtected =
      mojo::NamedPlatformChannel::PipeNameType::kAdminProtected;
  constexpr auto kDefault = mojo::NamedPlatformChannel::PipeNameType::kDefault;

  base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError> result =
      connect(kProtected);
  if (result.has_value() || result.error() == ConnectToServerError::kRejected) {
    return result;
  }

  // A server serving the legacy pipe but not the protected one either predates
  // it or failed to create it; both want the fallback. While the server is
  // still starting neither pipe exists yet, so retry instead of downgrading.
  if (is_pipe_missing(kProtected) && !is_pipe_missing(kDefault)) {
    return connect(kDefault);
  }
  return base::unexpected(ConnectToServerError::kRetryable);
}

void ConnectMojo(
    UpdaterScope scope,
    bool is_internal_service,
    base::Time deadline,
    base::OnceCallback<void(std::optional<mojo::PlatformChannelEndpoint>,
                            Microsoft::WRL::ComPtr<IUnknown>)>
        connected_callback) {
  // Bound the whole connect sequence, not each attempt. Giving up releases
  // the activation reference, which can shut down a server that is still
  // starting, so wait long enough for a slow server to publish its pipe, but
  // not for the caller's full `deadline`.
  constexpr base::TimeDelta kMaxConnectDuration = base::Seconds(30);
  ConnectMojoImpl(scope, is_internal_service, /*tries=*/0,
                  std::min(deadline, base::Time::Now() + kMaxConnectDuration),
                  /*server=*/{}, std::move(connected_callback));
}

}  // namespace updater
