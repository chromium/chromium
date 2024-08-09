// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_SERVICE_STUB_H_
#define CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_SERVICE_STUB_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/enterprise_companion/enterprise_companion_client.h"
#include "chrome/enterprise_companion/ipc_security.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace enterprise_companion {
namespace mojom {
class EnterpriseCompanion;
}

class EnterpriseCompanionService;

// Provides the options used to instantiate the NamedMojoIpcServer.
named_mojo_ipc_server::EndpointOptions CreateServerEndpointOptions(
    const mojo::NamedPlatformChannel::ServerName& server_name);

// Creates a stub that receives RPC calls from the client and delegates them to
// an `EnterpriseCompanionService`. The stub creates and manages a
// `NamedMojoIpcServer` to listen for and broker new Mojo connections with
// clients.
std::unique_ptr<mojom::EnterpriseCompanion>
CreateEnterpriseCompanionServiceStub(
    std::unique_ptr<EnterpriseCompanionService> service,
    const named_mojo_ipc_server::EndpointOptions& options =
        CreateServerEndpointOptions(GetServerName()),
    IpcTrustDecider trust_decider = CreateIpcTrustDecider(),
    base::RepeatingClosure endpoint_created_listener_for_testing =
        base::DoNothing());

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_SERVICE_STUB_H_
