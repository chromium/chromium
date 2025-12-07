// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_MOJO_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_MOJO_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/ipc/update_service_internal_proxy_impl.h"
#include "chrome/updater/mojom/updater_service_internal.mojom.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/updater_scope.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_WIN)
#include <wrl/client.h>
#endif  // BUILDFLAG(IS_WIN)

namespace mojo {
class PlatformChannelEndpoint;
class IsolatedConnection;
}  // namespace mojo

namespace updater {

class UpdateServiceInternalProxyMojoImpl
    : public UpdateServiceInternalProxyImpl {
 public:
  // Creates an UpdateServiceInternalProxyMojoImpl which is not bound to a
  // remote. It establishes a connection lazily and can be used immediately.
  explicit UpdateServiceInternalProxyMojoImpl(UpdaterScope scope);

  void Run(base::OnceCallback<void(std::optional<RpcError>)> callback) override;
  void Hello(
      base::OnceCallback<void(std::optional<RpcError>)> callback) override;

 private:
  ~UpdateServiceInternalProxyMojoImpl() override;

  void EnsureConnecting();
  void OnDisconnected();

#if BUILDFLAG(IS_WIN)
  void OnConnected(
      mojo::PendingReceiver<mojom::UpdateServiceInternal> pending_receiver,
      std::optional<mojo::PlatformChannelEndpoint> endpoint,
      Microsoft::WRL::ComPtr<IUnknown> server);
#else   // BUILDFLAG(IS_WIN)
  void OnConnected(
      mojo::PendingReceiver<mojom::UpdateServiceInternal> pending_receiver,
      std::optional<mojo::PlatformChannelEndpoint> endpoint);
#endif  // BUILDFLAG(IS_WIN)

  SEQUENCE_CHECKER(sequence_checker_);
  const UpdaterScope scope_;
  std::unique_ptr<mojo::IsolatedConnection> connection_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::UpdateServiceInternal> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);

#if BUILDFLAG(IS_WIN)
  Microsoft::WRL::ComPtr<IUnknown> server_;
#endif  // BUILDFLAG(IS_WIN)

  base::WeakPtrFactory<UpdateServiceInternalProxyMojoImpl> weak_factory_{this};
};

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_MOJO_H_
