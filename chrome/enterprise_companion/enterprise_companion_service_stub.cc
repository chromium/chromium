// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/enterprise_companion_service_stub.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/enterprise_companion_service.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace enterprise_companion {
namespace {

class UntrustedCallerStub final : public mojom::EnterpriseCompanion {
 public:
  UntrustedCallerStub() = default;

  // Overrides for mojom::EnterpriseCompanion.
  void Shutdown(ShutdownCallback callback) override {
    std::move(callback).Run(
        EnterpriseCompanionStatus(ApplicationError::kIpcCallerNotAllowed)
            .ToMojomStatus());
  }

  void FetchPolicies(FetchPoliciesCallback callback) override {
    std::move(callback).Run(
        EnterpriseCompanionStatus(ApplicationError::kIpcCallerNotAllowed)
            .ToMojomStatus());
  }
};

// Manages the NamedMojoIpcServer and forwards calls to the underlying service.
class Stub final : public mojom::EnterpriseCompanion {
 public:
  Stub(std::unique_ptr<EnterpriseCompanionService> service,
       const named_mojo_ipc_server::EndpointOptions& options,
       IpcTrustDecider trust_decider,
       base::RepeatingClosure endpoint_created_listener_for_testing)
      : service_(std::move(service)),
        server_(options,
                base::BindRepeating(
                    [](IpcTrustDecider trust_decider,
                       mojom::EnterpriseCompanion* stub,
                       mojom::EnterpriseCompanion* untrusted_stub,
                       const named_mojo_ipc_server::ConnectionInfo&
                           connection_info) {
                      return trust_decider.Run(connection_info)
                                 ? stub
                                 : untrusted_stub;
                    },
                    trust_decider,
                    base::Unretained(this),
                    base::Unretained(untrusted_stub_.get()))) {
    server_.set_disconnect_handler(base::BindRepeating(
        [] { VLOG(1) << "EnterpriseCompanion client disconnected"; }));
    if (endpoint_created_listener_for_testing) {
      server_.set_on_server_endpoint_created_callback_for_testing(
          endpoint_created_listener_for_testing);
    }
    server_.StartServer();
  }
  ~Stub() override = default;

  // Overrides for mjom::EnterpriseCompanion.
  void Shutdown(ShutdownCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    service_->Shutdown(
        base::BindOnce(std::move(callback),
                       EnterpriseCompanionStatus::Success().ToMojomStatus()));
  }

  void FetchPolicies(FetchPoliciesCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    service_->FetchPolicies(
        base::BindOnce([](const EnterpriseCompanionStatus& status) {
          return status.ToMojomStatus();
        }).Then(std::move(callback)));
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<UntrustedCallerStub> untrusted_stub_ =
      std::make_unique<UntrustedCallerStub>();
  std::unique_ptr<EnterpriseCompanionService> service_;
  named_mojo_ipc_server::NamedMojoIpcServer<mojom::EnterpriseCompanion> server_;
};

}  // namespace

named_mojo_ipc_server::EndpointOptions CreateServerEndpointOptions(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  named_mojo_ipc_server::EndpointOptions options{
      server_name,
      named_mojo_ipc_server::EndpointOptions::kUseIsolatedConnection};
#if BUILDFLAG(IS_WIN)
  options.security_descriptor =
      GetGlobalConstants()->NamedPipeSecurityDescriptor();
#endif
  return options;
}

// Creates a stub that receives RPC calls from the client and delegates them to
// an `EnterpriseCompanionService`. The stub creates and manages a
// `NamedMojoIpcServer` to listen for and broker new Mojo connections with
// clients.
std::unique_ptr<mojom::EnterpriseCompanion>
CreateEnterpriseCompanionServiceStub(
    std::unique_ptr<EnterpriseCompanionService> service,
    const named_mojo_ipc_server::EndpointOptions& options,
    IpcTrustDecider trust_decider,
    base::RepeatingClosure endpoint_created_listener_for_testing) {
  return std::make_unique<Stub>(std::move(service), options, trust_decider,
                                endpoint_created_listener_for_testing);
}

}  // namespace enterprise_companion
