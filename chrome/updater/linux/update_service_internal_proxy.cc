// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/linux/update_service_internal_proxy.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/app/server/linux/mojom/updater_service_internal.mojom.h"
#include "chrome/updater/linux/ipc_constants.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/isolated_connection.h"

namespace updater {
namespace {

// The maximum amount of time to poll the server's socket for a connection.
constexpr base::TimeDelta kConnectionTimeout = base::Seconds(3);

}  // namespace

class UpdateServiceInternalProxyImpl
    : public base::RefCountedThreadSafe<UpdateServiceInternalProxyImpl> {
 public:
  explicit UpdateServiceInternalProxyImpl(UpdaterScope scope) {
    base::ThreadPool::CreateSequencedTaskRunner({})->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&UpdateServiceInternalProxyImpl::Connect, scope),
        base::BindOnce(&UpdateServiceInternalProxyImpl::OnConnected,
                       weak_factory_.GetWeakPtr(),
                       remote_.BindNewPipeAndPassReceiver()));
  }

  UpdateServiceInternalProxyImpl(
      UpdaterScope /*scope*/,
      std::unique_ptr<mojo::IsolatedConnection> connection,
      mojo::Remote<mojom::UpdateServiceInternal> remote)
      : connection_(std::move(connection)), remote_(std::move(remote)) {}

  void Run(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::OnceClosure wrapped_callback =
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback));

    if (!remote_)
      return;

    remote_->Run(std::move(wrapped_callback));
  }

  void Hello(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::OnceClosure wrapped_callback =
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback));

    if (!remote_)
      return;

    remote_->Hello(std::move(wrapped_callback));
  }

 private:
  friend class base::RefCountedThreadSafe<UpdateServiceInternalProxyImpl>;
  virtual ~UpdateServiceInternalProxyImpl() = default;

  void OnDisconnected() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    LOG(ERROR) << "UpdateService remote has unexpectedly disconnected.";
    connection_.reset();
    remote_.reset();
  }

  static absl::optional<mojo::PlatformChannelEndpoint> Connect(
      UpdaterScope scope) {
    absl::optional<base::FilePath> socket_path =
        GetActiveDutyInternalSocketPath(scope, base::Version(kUpdaterVersion));
    if (!socket_path) {
      LOG(ERROR) << "Failed to get socket path.";
      return absl::nullopt;
    }

    mojo::PlatformChannelEndpoint endpoint;

    base::Time deadline = base::Time::NowFromSystemTime() + kConnectionTimeout;
    do {
      endpoint = mojo::NamedPlatformChannel::ConnectToServer(
          socket_path->MaybeAsASCII());
      base::PlatformThread::Sleep(base::Milliseconds(100));
    } while (!endpoint.is_valid() &&
             base::Time::NowFromSystemTime() < deadline);

    if (!endpoint.is_valid()) {
      LOG(ERROR) << "Failed to connect to UpdateServiceInternal remote. "
                    "Connection timed out.";
      return absl::nullopt;
    }

    return std::move(endpoint);
  }

  void OnConnected(
      mojo::PendingReceiver<mojom::UpdateServiceInternal> pending_receiver,
      absl::optional<mojo::PlatformChannelEndpoint> endpoint) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!endpoint) {
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
    remote_.set_disconnect_handler(
        base::BindOnce(&UpdateServiceInternalProxyImpl::OnDisconnected,
                       weak_factory_.GetWeakPtr()));
  }

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<mojo::IsolatedConnection> connection_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::UpdateServiceInternal> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<UpdateServiceInternalProxyImpl> weak_factory_{this};
};

UpdateServiceInternalProxy::UpdateServiceInternalProxy(UpdaterScope scope)
    : impl_(base::MakeRefCounted<UpdateServiceInternalProxyImpl>(scope)) {}

UpdateServiceInternalProxy::UpdateServiceInternalProxy(
    UpdaterScope scope,
    std::unique_ptr<mojo::IsolatedConnection> connection,
    mojo::Remote<mojom::UpdateServiceInternal> remote)
    : impl_(base::MakeRefCounted<UpdateServiceInternalProxyImpl>(
          scope,
          std::move(connection),
          std::move(remote))) {}

UpdateServiceInternalProxy::~UpdateServiceInternalProxy() = default;

void UpdateServiceInternalProxy::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->Run(OnCurrentSequence(std::move(callback)));
}

void UpdateServiceInternalProxy::Hello(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  impl_->Hello(OnCurrentSequence(std::move(callback)));
}

scoped_refptr<UpdateServiceInternal> CreateUpdateServiceInternalProxy(
    UpdaterScope scope) {
  return base::MakeRefCounted<UpdateServiceInternalProxy>(scope);
}

scoped_refptr<UpdateServiceInternal> CreateUpdateServiceInternalProxy(
    UpdaterScope scope,
    std::unique_ptr<mojo::IsolatedConnection> connection,
    mojo::Remote<mojom::UpdateServiceInternal> remote) {
  return base::MakeRefCounted<UpdateServiceInternalProxy>(
      scope, std::move(connection), std::move(remote));
}

}  // namespace updater
