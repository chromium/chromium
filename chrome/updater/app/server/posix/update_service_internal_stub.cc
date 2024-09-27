// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/posix/update_service_internal_stub.h"

#include <iterator>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/ipc/ipc_names.h"
#include "chrome/updater/ipc/ipc_security.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"

namespace updater {

UpdateServiceInternalStub::UpdateServiceInternalStub(
    scoped_refptr<updater::UpdateServiceInternal> impl,
    UpdaterScope scope,
    base::RepeatingClosure task_start_listener,
    base::RepeatingClosure task_end_listener)
    : server_({GetUpdateServiceInternalServerName(scope),
               named_mojo_ipc_server::EndpointOptions::kUseIsolatedConnection},
              base::BindRepeating(
                  [](mojom::UpdateServiceInternal* interface,
                     const named_mojo_ipc_server::ConnectionInfo& info) {
                    return interface;
                  },
                  this)),
      impl_(impl),
      task_start_listener_(task_start_listener),
      task_end_listener_(task_end_listener) {
  server_.set_disconnect_handler(
      base::BindRepeating(&UpdateServiceInternalStub::OnClientDisconnected,
                          base::Unretained(this)));
  server_.StartServer();
}

UpdateServiceInternalStub::~UpdateServiceInternalStub() = default;

void UpdateServiceInternalStub::OnClientDisconnected() {
  VLOG(1) << "UpdateServiceInternal receiver disconnected: "
          << server_.current_receiver();
}

void UpdateServiceInternalStub::Run(RunCallback callback) {
  task_start_listener_.Run();
  impl_->Run(std::move(callback).Then(task_end_listener_));
}

void UpdateServiceInternalStub::Hello(HelloCallback callback) {
  task_start_listener_.Run();
  impl_->Hello(std::move(callback).Then(task_end_listener_));
}

}  // namespace updater
