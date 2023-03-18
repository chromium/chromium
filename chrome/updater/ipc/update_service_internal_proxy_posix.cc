// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_internal_proxy_posix.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "chrome/updater/app/server/posix/mojom/updater_service_internal.mojom.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/ipc/ipc_names.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

// The maximum amount of time to poll the server's socket for a connection. This
// can take a long time if the server is unable to start because it is blocked
// behind acquiring a prefs lock (for example if the active instance is busy).
constexpr base::TimeDelta kConnectionTimeout = base::Minutes(10);

// Connect to the server.
// `retries` is 0 for the first try, 1 for the first retry, etc.
mojo::PlatformChannelEndpoint ConnectMojo(UpdaterScope scope, int retries) {
  if (retries == 1) {
    // Launch a server process.
    absl::optional<base::FilePath> updater = GetUpdaterExecutablePath(scope);
    if (updater) {
      base::CommandLine command(*updater);
      command.AppendSwitch(kServerSwitch);
      command.AppendSwitchASCII(kServerServiceSwitch,
                                kServerUpdateServiceInternalSwitchValue);
      if (scope == UpdaterScope::kSystem) {
        command.AppendSwitch(kSystemSwitch);
      }
      command.AppendSwitch(kEnableLoggingSwitch);
      command.AppendSwitchASCII(kLoggingModuleSwitch,
                                kLoggingModuleSwitchValue);
      base::LaunchProcess(command, {});
    }
  }

  return named_mojo_ipc_server::ConnectToServer(
      GetUpdateServiceInternalServerName(scope));
}

void Connect(
    UpdaterScope scope,
    int tries,
    base::Time deadline,
    base::OnceCallback<void(absl::optional<mojo::PlatformChannelEndpoint>)>
        connected_callback) {
  if (base::Time::Now() > deadline) {
    LOG(ERROR) << "Failed to connect to UpdateServiceInternal remote. "
                  "Connection timed out.";
    std::move(connected_callback).Run(absl::nullopt);
    return;
  }

  mojo::PlatformChannelEndpoint endpoint = ConnectMojo(scope, tries);

  if (endpoint.is_valid()) {
    std::move(connected_callback).Run(std::move(endpoint));
    return;
  }

  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&Connect, scope, tries + 1, deadline,
                     std::move(connected_callback)),
      base::Milliseconds(30 * tries));
}

}  // namespace

UpdateServiceInternalProxy::UpdateServiceInternalProxy(UpdaterScope scope)
    : scope_(scope) {}

void UpdateServiceInternalProxy::Run(base::OnceClosure callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  remote_->Run(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void UpdateServiceInternalProxy::Hello(base::OnceClosure callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  remote_->Hello(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindPostTaskToCurrentDefault(std::move(callback))));
}

UpdateServiceInternalProxy::~UpdateServiceInternalProxy() = default;

void UpdateServiceInternalProxy::EnsureConnecting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (remote_) {
    return;
  }
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&Connect, scope_, 0,
                     base::Time::Now() + kConnectionTimeout,
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &UpdateServiceInternalProxy::OnConnected, this,
                         remote_.BindNewPipeAndPassReceiver()))));
}

void UpdateServiceInternalProxy::OnDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  connection_.reset();
  remote_.reset();
}

void UpdateServiceInternalProxy::OnConnected(
    mojo::PendingReceiver<mojom::UpdateServiceInternal> pending_receiver,
    absl::optional<mojo::PlatformChannelEndpoint> endpoint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!endpoint) {
    VLOG(2) << "No endpoint received.";
    remote_.reset();
    return;
  }

  auto connection = std::make_unique<mojo::IsolatedConnection>();
  // Connect `remote_` to the RPC server by fusing its message pipe to the one
  // created by `IsolatedConnection::Connect`.
  if (!mojo::FusePipes(
          std::move(pending_receiver),
          mojo::PendingRemote<mojom::UpdateServiceInternal>(
              connection->Connect(std::move(endpoint.value())), 0))) {
    LOG(ERROR) << "Failed to fuse Mojo pipes for RPC.";
    remote_.reset();
    return;
  }

  connection_ = std::move(connection);

  // A weak pointer is used here to prevent remote_ from forming a reference
  // cycle with this object.
  remote_.set_disconnect_handler(base::BindOnce(
      &UpdateServiceInternalProxy::OnDisconnected, weak_factory_.GetWeakPtr()));
}

scoped_refptr<UpdateServiceInternal> CreateUpdateServiceInternalProxy(
    UpdaterScope scope) {
  return base::MakeRefCounted<UpdateServiceInternalProxy>(scope);
}

}  // namespace updater
