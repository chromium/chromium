// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/linux/update_service_internal_proxy.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/app/server/linux/mojom/updater_service_internal.mojom.h"
#include "chrome/updater/linux/ipc_constants.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/isolated_connection.h"

namespace updater {

class UpdateServiceInternalProxyImpl
    : public base::RefCountedThreadSafe<UpdateServiceInternalProxyImpl> {
 public:
  explicit UpdateServiceInternalProxyImpl(
      UpdaterScope /*scope*/,
      std::unique_ptr<mojo::IsolatedConnection> connection,
      mojo::Remote<mojom::UpdateServiceInternal> remote)
      : connection_(std::move(connection)), remote_(std::move(remote)) {}

  void Run(base::OnceClosure callback) {
    remote_->Run(
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback)));
  }

  void Hello(base::OnceClosure callback) {
    remote_->Hello(
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback)));
  }

 private:
  friend class base::RefCountedThreadSafe<UpdateServiceInternalProxyImpl>;
  virtual ~UpdateServiceInternalProxyImpl() = default;

  std::unique_ptr<mojo::IsolatedConnection> connection_;
  mojo::Remote<mojom::UpdateServiceInternal> remote_;
};

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
  absl::optional<base::FilePath> socket_path =
      GetActiveDutyInternalSocketPath(scope, base::Version(kUpdaterVersion));
  if (!socket_path)
    return nullptr;

  mojo::PlatformChannelEndpoint endpoint;

  // TODO(1382127): Avoid blocking the calling thread.
  base::Time deadline = base::Time::NowFromSystemTime() + base::Seconds(3);
  do {
    endpoint = mojo::NamedPlatformChannel::ConnectToServer(
        socket_path->MaybeAsASCII());
    base::PlatformThread::Sleep(base::Milliseconds(100));
  } while (!endpoint.is_valid() && base::Time::NowFromSystemTime() < deadline);

  if (!endpoint.is_valid()) {
    LOG(ERROR) << "Failed to connect to UpdateServiceInternal remote. "
                  "Connection timed out.";
    return nullptr;
  }

  std::unique_ptr<mojo::IsolatedConnection> connection =
      std::make_unique<mojo::IsolatedConnection>();

  mojo::Remote<mojom::UpdateServiceInternal> remote(
      mojo::PendingRemote<mojom::UpdateServiceInternal>(
          connection->Connect(std::move(endpoint)), 0));
  remote.set_disconnect_handler(base::BindOnce([]() {
    LOG(ERROR) << "UpdateService remote has unexpectedly disconnected.";
  }));

  return CreateUpdateServiceInternalProxy(scope, std::move(connection),
                                          std::move(remote));
}

scoped_refptr<UpdateServiceInternal> CreateUpdateServiceInternalProxy(
    UpdaterScope scope,
    std::unique_ptr<mojo::IsolatedConnection> connection,
    mojo::Remote<mojom::UpdateServiceInternal> remote) {
  return base::MakeRefCounted<UpdateServiceInternalProxy>(
      scope, std::move(connection), std::move(remote));
}

}  // namespace updater
