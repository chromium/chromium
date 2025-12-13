// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_dialer.h"

#include <windows.h>

#include <wrl/client.h>

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/ipc/ipc_names.h"
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
// `Microsoft::WRL::ComPtr<IUnknown>` if the service can be created.
Microsoft::WRL::ComPtr<IUnknown> DialUpdateService(UpdaterScope scope,
                                                   bool is_internal_service) {
  Microsoft::WRL::ComPtr<IUnknown> server;
  const CLSID clsid = is_internal_service ? GetUpdaterInternalClsid(scope)
                                          : GetUpdaterClsid(scope);
  const HRESULT hr = ::CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER,
                                        IID_PPV_ARGS(&server));
  if (FAILED(hr)) {
    VLOG(2) << "::CoCreateInstance failed: " << StringFromGuid(clsid) << ": "
            << std::hex << hr;
    return {};
  }

  return server;
}

void ConnectMojoImpl(
    UpdaterScope scope,
    bool is_internal_service,
    int tries,
    base::Time deadline,
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

  Microsoft::WRL::ComPtr<IUnknown> server;
  auto endpoint = [&]() -> std::optional<mojo::PlatformChannelEndpoint> {
    Microsoft::WRL::ComPtr<IUnknown> result =
        DialUpdateService(scope, is_internal_service);
    if (!result) {
      return std::nullopt;
    }

    server = result;
    return named_mojo_ipc_server::ConnectToServer({
        .server_name = is_internal_service
                           ? GetUpdateServiceInternalServerName(scope)
                           : GetUpdateServiceServerName(scope),
        .allow_impersonation = true,
    });
  }();

  if (tries >= 1 && !endpoint) {
    VLOG(1) << "Failed to connect to remote mojo service, is_internal_service: "
            << is_internal_service << ", scope: " << scope;
    std::move(connected_callback).Run(std::nullopt, {});
    return;
  }

  if (endpoint && endpoint->is_valid()) {
    std::move(connected_callback).Run(std::move(endpoint), server);
    return;
  }

  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ConnectMojoImpl, scope, is_internal_service, tries + 1,
                     deadline, std::move(connected_callback)),
      base::Milliseconds(200 * tries));
}

}  // namespace

void ConnectMojo(
    UpdaterScope scope,
    bool is_internal_service,
    base::Time deadline,
    base::OnceCallback<void(std::optional<mojo::PlatformChannelEndpoint>,
                            Microsoft::WRL::ComPtr<IUnknown>)>
        connected_callback) {
  ConnectMojoImpl(scope, is_internal_service, /*tries=*/0, deadline,
                  std::move(connected_callback));
}

}  // namespace updater
