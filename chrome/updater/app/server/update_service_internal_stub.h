// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_UPDATE_SERVICE_INTERNAL_STUB_H_
#define CHROME_UPDATER_APP_SERVER_UPDATE_SERVICE_INTERNAL_STUB_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/mojom/updater_service_internal.mojom.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"

namespace updater {

// Receives RPC calls from the client and delegates them to an
// UpdateServiceInternal. The stub creates and manages a `NamedMojoIpcServer`
// to listen for and broker new Mojo connections with clients.
class UpdateServiceInternalStub : public mojom::UpdateServiceInternal {
 public:
  // Creates an `UpdateServiceInternalStub` which forwards calls to `impl`.
  // Opens a `NamedMojoIpcServer` listening on the endpoint for `scope` (using
  // the admin-protected pipe on Windows 10 RS3+ system installs).
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
  // `filter_` must outlive `server_`.
  std::unique_ptr<mojom::UpdateServiceInternal> filter_;
  named_mojo_ipc_server::NamedMojoIpcServer<mojom::UpdateServiceInternal>
      server_;
  scoped_refptr<updater::UpdateServiceInternal> impl_;
  base::RepeatingClosure task_start_listener_;
  base::RepeatingClosure task_end_listener_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_UPDATE_SERVICE_INTERNAL_STUB_H_
