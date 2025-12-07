// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_dialer.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/ipc/ipc_names.h"
#include "chrome/updater/updater_scope.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"

namespace updater {

namespace {

void ConnectMojoImpl(
    UpdaterScope scope,
    bool is_internal_service,
    int tries,
    base::Time deadline,
    base::OnceCallback<void(std::optional<mojo::PlatformChannelEndpoint>)>
        connected_callback) {
  if (base::Time::Now() > deadline) {
    VLOG(1) << "Failed to connect to remote mojo service, is_internal_service: "
            << is_internal_service << ", scope: " << scope
            << ", connection timed out.";
    std::move(connected_callback).Run(std::nullopt);
    return;
  }

  auto endpoint = [&]() -> std::optional<mojo::PlatformChannelEndpoint> {
    if (tries == 1 && !(is_internal_service ? DialUpdateInternalService(scope)
                                            : DialUpdateService(scope))) {
      return std::nullopt;
    }

    return named_mojo_ipc_server::ConnectToServer(
        {.server_name = is_internal_service
                            ? GetUpdateServiceInternalServerName(scope)
                            : GetUpdateServiceServerName(scope)});
  }();

  if (!endpoint) {
    VLOG(1) << "Failed to connect to remote mojo service, is_internal_service: "
            << is_internal_service << ", scope: " << scope
            << ", no updater exists.";
    std::move(connected_callback).Run(std::nullopt);
    return;
  }

  if (endpoint->is_valid()) {
    std::move(connected_callback).Run(std::move(endpoint));
    return;
  }

  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ConnectMojoImpl, scope, is_internal_service, tries + 1,
                     deadline, std::move(connected_callback)),
      base::Milliseconds(30 * tries));
}

}  // namespace

void ConnectMojo(
    UpdaterScope scope,
    bool is_internal_service,
    base::Time deadline,
    base::OnceCallback<void(std::optional<mojo::PlatformChannelEndpoint>)>
        connected_callback) {
  ConnectMojoImpl(scope, is_internal_service, /*tries=*/0, deadline,
                  std::move(connected_callback));
}

}  // namespace updater
