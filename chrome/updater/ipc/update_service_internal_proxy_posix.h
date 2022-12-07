// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_POSIX_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_POSIX_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/app/server/posix/mojom/updater_service_internal.mojom.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/updater_scope.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {
class PlatformChannelEndpoint;
class IsolatedConnection;
}  // namespace mojo

namespace updater {

class UpdateServiceInternalProxy : public UpdateServiceInternal {
 public:
  // Creates an UpdateServiceInternalProxy which is not bound to a remote. It
  // establishes a connection lazily and can be used immediately.
  explicit UpdateServiceInternalProxy(UpdaterScope scope);

  // Overrides for UpdateServiceInternal.
  void Run(base::OnceClosure callback) override;
  void Hello(base::OnceClosure callback) override;

 private:
  ~UpdateServiceInternalProxy() override;

  void EnsureConnecting();
  void OnDisconnected();
  void OnConnected(
      mojo::PendingReceiver<mojom::UpdateServiceInternal> pending_receiver,
      absl::optional<mojo::PlatformChannelEndpoint> endpoint);

  SEQUENCE_CHECKER(sequence_checker_);
  const UpdaterScope scope_;
  std::unique_ptr<mojo::IsolatedConnection> connection_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::UpdateServiceInternal> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_SERVICE_INTERNAL_PROXY_POSIX_H_
