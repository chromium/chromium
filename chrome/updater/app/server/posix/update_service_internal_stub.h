// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_POSIX_UPDATE_SERVICE_INTERNAL_STUB_H_
#define CHROME_UPDATER_APP_SERVER_POSIX_UPDATE_SERVICE_INTERNAL_STUB_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/updater/app/server/posix/mojom/updater_service_internal.mojom.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"

namespace updater {

// Receives RPC calls from the client and delegates them to an
// UpdateServiceInternal. The stub creates and manages a `NamedMojoIpcServer`
// to listen for and broker new Mojo connections with clients.
class UpdateServiceInternalStub : public mojom::UpdateServiceInternal {
 public:
  // Create an UpdateServiceInternalStub which forwards calls to `impl`. Opens
  // a NamedMojoIpcServer which listens on a socket whose name is decided by
  // `scope`.
  UpdateServiceInternalStub(scoped_refptr<updater::UpdateServiceInternal> impl,
                            UpdaterScope scope,
                            base::RepeatingClosure task_start_listener,
                            base::RepeatingClosure task_end_listener);
  UpdateServiceInternalStub(const UpdateServiceInternalStub&) = delete;
  UpdateServiceInternalStub& operator=(const UpdateServiceInternalStub&) =
      delete;
  ~UpdateServiceInternalStub() override;

  // updater::mojom::UpdateServiceInternal
  void Run(RunCallback callback) override;
  void Hello(HelloCallback callback) override;

 private:
  void OnClientDisconnected();

  named_mojo_ipc_server::NamedMojoIpcServer<mojom::UpdateServiceInternal>
      server_;
  scoped_refptr<updater::UpdateServiceInternal> impl_;
  base::RepeatingClosure task_start_listener_;
  base::RepeatingClosure task_end_listener_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_POSIX_UPDATE_SERVICE_INTERNAL_STUB_H_
