// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_internal_proxy_mojo.h"

#include <memory>
#include <optional>
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
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/ipc/ipc_names.h"
#include "chrome/updater/ipc/update_service_dialer.h"
#include "chrome/updater/ipc/update_service_internal_proxy.h"
#include "chrome/updater/mojom/updater_service_internal.mojom.h"
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

#if BUILDFLAG(IS_WIN)
#include <wrl/client.h>
#endif  // BUILDFLAG(IS_WIN)

namespace updater {
namespace {

// The maximum amount of time to poll the server's socket for a connection. This
// can take a long time if the server is unable to start because it is blocked
// behind acquiring a prefs lock (for example if the active instance is busy).
constexpr base::TimeDelta kConnectionTimeout = base::Minutes(10);

}  // namespace

UpdateServiceInternalProxyMojoImpl::UpdateServiceInternalProxyMojoImpl(
    UpdaterScope scope)
    : scope_(scope) {}

void UpdateServiceInternalProxyMojoImpl::Run(
    base::OnceCallback<void(std::optional<RpcError>)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  remote_->Run(base::BindOnce(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindPostTaskToCurrentDefault(std::move(callback)),
          kErrorIpcDisconnect),
      std::nullopt));
}

void UpdateServiceInternalProxyMojoImpl::Hello(
    base::OnceCallback<void(std::optional<RpcError>)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureConnecting();
  remote_->Hello(base::BindOnce(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindPostTaskToCurrentDefault(std::move(callback)),
          kErrorIpcDisconnect),
      std::nullopt));
}

UpdateServiceInternalProxyMojoImpl::~UpdateServiceInternalProxyMojoImpl() =
    default;

void UpdateServiceInternalProxyMojoImpl::EnsureConnecting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (remote_) {
    return;
  }
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ConnectMojo, scope_, /*internal=*/true,
                     base::Time::Now() + kConnectionTimeout,
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &UpdateServiceInternalProxyMojoImpl::OnConnected, this,
                         remote_.BindNewPipeAndPassReceiver()))));
}

void UpdateServiceInternalProxyMojoImpl::OnDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  connection_.reset();
  remote_.reset();
}

#if BUILDFLAG(IS_WIN)
void UpdateServiceInternalProxyMojoImpl::OnConnected(
    mojo::PendingReceiver<mojom::UpdateServiceInternal> pending_receiver,
    std::optional<mojo::PlatformChannelEndpoint> endpoint,
    Microsoft::WRL::ComPtr<IUnknown> server) {
#else   // BUILDFLAG(IS_WIN)
void UpdateServiceInternalProxyMojoImpl::OnConnected(
    mojo::PendingReceiver<mojom::UpdateServiceInternal> pending_receiver,
    std::optional<mojo::PlatformChannelEndpoint> endpoint) {
#endif  // BUILDFLAG(IS_WIN)
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

#if BUILDFLAG(IS_WIN)
  server_ = server;
#endif  // BUILDFLAG(IS_WIN)

  // A weak pointer is used here to prevent remote_ from forming a reference
  // cycle with this object.
  remote_.set_disconnect_handler(
      base::BindOnce(&UpdateServiceInternalProxyMojoImpl::OnDisconnected,
                     weak_factory_.GetWeakPtr()));
}

#if BUILDFLAG(IS_WIN)
scoped_refptr<UpdateServiceInternal> CreateUpdateServiceInternalProxyMojo(
    UpdaterScope scope) {
#else   // BUILDFLAG(IS_WIN)
scoped_refptr<UpdateServiceInternal> CreateUpdateServiceInternalProxy(
    UpdaterScope scope) {
#endif  // BUILDFLAG(IS_WIN)
  return base::MakeRefCounted<UpdateServiceInternalProxy>(
      base::MakeRefCounted<UpdateServiceInternalProxyMojoImpl>(scope));
}

}  // namespace updater
