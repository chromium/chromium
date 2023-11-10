// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_POSIX_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_POSIX_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/app/server/posix/mojom/updater_service_internal.mojom.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/updater_scope.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace mojo {
class PlatformChannelEndpoint;
class IsolatedConnection;
}  // namespace mojo

namespace updater {

using RpcError = int;

class UpdateServiceInternalProxyImpl
    : public base::RefCountedThreadSafe<UpdateServiceInternalProxyImpl> {
 public:
  // Creates an UpdateServiceInternalProxyImpl which is not bound to a remote.
  // It establishes a connection lazily and can be used immediately.
  explicit UpdateServiceInternalProxyImpl(UpdaterScope scope);

  void Run(base::OnceCallback<void(std::optional<RpcError>)> callback);
  void Hello(base::OnceCallback<void(std::optional<RpcError>)> callback);

 private:
  friend class base::RefCountedThreadSafe<UpdateServiceInternalProxyImpl>;
  ~UpdateServiceInternalProxyImpl();

  void EnsureConnecting();
  void OnDisconnected();
  void OnConnected(
      mojo::PendingReceiver<mojom::UpdateServiceInternal> pending_receiver,
      std::optional<mojo::PlatformChannelEndpoint> endpoint);

  SEQUENCE_CHECKER(sequence_checker_);
  const UpdaterScope scope_;
  std::unique_ptr<mojo::IsolatedConnection> connection_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::UpdateServiceInternal> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtrFactory<UpdateServiceInternalProxyImpl> weak_factory_{this};
};

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_POSIX_H_
