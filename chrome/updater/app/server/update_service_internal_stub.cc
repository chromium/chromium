// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/update_service_internal_stub.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "chrome/updater/ipc/ipc_names.h"
#include "chrome/updater/ipc/ipc_security.h"
#include "chrome/updater/updater_scope.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace updater {
namespace {

// Forwards calls permitted from untrusted callers (e.g., unelevated `--wake
// --system` clients); any new `mojom::UpdateServiceInternal` method must
// explicitly decide whether untrusted callers are allowed.
class UpdateServiceInternalStubUntrusted : public mojom::UpdateServiceInternal {
 public:
  explicit UpdateServiceInternalStubUntrusted(
      mojom::UpdateServiceInternal* impl)
      : impl_(impl) {}
  UpdateServiceInternalStubUntrusted(
      const UpdateServiceInternalStubUntrusted&) = delete;
  UpdateServiceInternalStubUntrusted& operator=(
      const UpdateServiceInternalStubUntrusted&) = delete;

  // updater::mojom::UpdateServiceInternal
  void Run(RunCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    impl_->Run(std::move(callback));
  }

  void Hello(HelloCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    impl_->Hello(std::move(callback));
  }

 private:
  raw_ptr<mojom::UpdateServiceInternal> impl_;
  SEQUENCE_CHECKER(sequence_checker_);
};

base::RepeatingCallback<
    mojom::UpdateServiceInternal*(const named_mojo_ipc_server::ConnectionInfo&)>
MakeImplProvider(mojom::UpdateServiceInternal* interface,
                 mojom::UpdateServiceInternal* filter) {
  return base::BindRepeating(
      [](mojom::UpdateServiceInternal* interface,
         mojom::UpdateServiceInternal* filter,
         const named_mojo_ipc_server::ConnectionInfo& info) {
        return IsConnectionTrusted(info) ? interface : filter;
      },
      interface, filter);
}

named_mojo_ipc_server::EndpointOptions CreateEndpointOptions(
    UpdaterScope scope) {
#if BUILDFLAG(IS_WIN)
  if (IsSystemInstall(scope) &&
      base::win::GetVersion() >= base::win::Version::WIN10_RS3) {
    return CreateProtectedServerEndpointOptions(
        scope, GetUpdateServiceInternalServerName(scope));
  }
#endif
  return CreateServerEndpointOptions(scope,
                                     GetUpdateServiceInternalServerName(scope));
}

}  // namespace

UpdateServiceInternalStub::UpdateServiceInternalStub(
    scoped_refptr<updater::UpdateServiceInternal> impl,
    UpdaterScope scope,
    base::RepeatingClosure task_start_listener,
    base::RepeatingClosure task_end_listener)
    : filter_(std::make_unique<UpdateServiceInternalStubUntrusted>(this)),
      server_(CreateEndpointOptions(scope),
              MakeImplProvider(this, filter_.get())),
      impl_(impl),
      task_start_listener_(task_start_listener),
      task_end_listener_(task_end_listener) {
  server_.set_disconnect_handler(base::BindRepeating(
      [] { VLOG(1) << "UpdateServiceInternal client disconnected."; }));
  server_.StartServer();
}

UpdateServiceInternalStub::~UpdateServiceInternalStub() = default;

void UpdateServiceInternalStub::Run(RunCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->Run(std::move(callback).Then(task_end_listener_));
}

void UpdateServiceInternalStub::Hello(HelloCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_start_listener_.Run();
  impl_->Hello(std::move(callback).Then(task_end_listener_));
}

}  // namespace updater
