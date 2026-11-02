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
#include "components/policy/core/common/policy_types.h"
#include "mojo/public/c/system/invitation.h"
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

  void FetchPolicies(policy::PolicyFetchReason reason,
                     FetchPoliciesCallback callback) override {
    std::move(callback).Run(
        EnterpriseCompanionStatus(ApplicationError::kIpcCallerNotAllowed)
            .ToMojomStatus());
  }
};

// Returns a copy of `options` configured so that the trust Mojo extends to each
// connection matches the stub `trust_decider` selects for it.
named_mojo_ipc_server::EndpointOptions ApplyTrustDecider(
    named_mojo_ipc_server::EndpointOptions options,
    const IpcTrustDecider& trust_decider) {
#if BUILDFLAG(IS_WIN)
  // Callers which are not trusted are declared untrusted to Mojo. This prevents
  // them from transmitting objects whose safety depends on the caller's good
  // behavior, notably handles it claims to have already duplicated into this
  // process.
  DCHECK(!options.send_invitation_flags_callback);
  options.include_peer_process_info = true;
  options.send_invitation_flags_callback = base::BindRepeating(
      [](const IpcTrustDecider& trust_decider,
         const named_mojo_ipc_server::ConnectionInfo& info) {
        return trust_decider.Run(info)
                   ? MOJO_SEND_INVITATION_FLAG_NONE
                   : MOJO_SEND_INVITATION_FLAG_UNTRUSTED_PROCESS;
      },
      trust_decider);
#endif
  return options;
}

// Manages the NamedMojoIpcServer and forwards calls to the underlying service.
class Stub final : public mojom::EnterpriseCompanion {
 public:
  Stub(std::unique_ptr<EnterpriseCompanionService> service,
       const named_mojo_ipc_server::EndpointOptions& options,
       IpcTrustDecider trust_decider,
       base::RepeatingClosure endpoint_created_listener_for_testing)
      : service_(std::move(service)),
        server_(ApplyTrustDecider(options, trust_decider),
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
      server_.set_on_server_endpoint_created_callback(
          endpoint_created_listener_for_testing);
    }
    server_.StartServer();
  }

  // Overrides for mojom::EnterpriseCompanion.
  void Shutdown(ShutdownCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    service_->Shutdown(
        base::BindOnce(std::move(callback),
                       EnterpriseCompanionStatus::Success().ToMojomStatus()));
  }

  void FetchPolicies(policy::PolicyFetchReason reason,
                     FetchPoliciesCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    service_->FetchPolicies(
        reason, base::BindOnce([](const EnterpriseCompanionStatus& status) {
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
