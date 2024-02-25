// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"

#include <utility>

#include "base/check.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"

namespace ash::mojo_service_manager {

// The security context of ash-chrome. This should be the default security
// context for the global connection.
constexpr char kAshSecurityContext[] = "u:r:cros_browser:s0";

namespace mojom = ::chromeos::mojo_service_manager::mojom;

FakeMojoServiceManager::ServiceState::ServiceState() = default;

FakeMojoServiceManager::ServiceState::~ServiceState() = default;

FakeMojoServiceManager::FakeMojoServiceManager() {
  SetServiceManagerRemoteForTesting(
      AddNewPipeAndPassRemote(kAshSecurityContext));
}

FakeMojoServiceManager::~FakeMojoServiceManager() {
  // Reset the connection before the fake service manager so the disconnect
  // handler won't be triggered.
  ResetServiceManagerConnection();
}

mojo::PendingRemote<mojom::ServiceManager>
FakeMojoServiceManager::AddNewPipeAndPassRemote(
    const std::string& security_context) {
  mojo::PendingRemote<mojom::ServiceManager> remote;
  receiver_set_.Add(this, remote.InitWithNewPipeAndPassReceiver(),
                    mojom::ProcessIdentity::New(security_context, 0, 0, 0));
  return remote;
}

void FakeMojoServiceManager::Register(
    const std::string& service_name,
    mojo::PendingRemote<mojom::ServiceProvider> service_provider) {
  auto it = service_map_.find(service_name);
  if (it == service_map_.end()) {
    auto [it_new, success] = service_map_.try_emplace(service_name);
    CHECK(success);
    it = it_new;
  }

  ServiceState& service_state = it->second;
  if (service_state.service_provider.is_bound()) {
    service_provider.ResetWithReason(
        static_cast<uint32_t>(mojom::ErrorCode::kServiceAlreadyRegistered),
        "The service: " + service_name + " has already been registered.");
    return;
  }
  service_state.service_provider.Bind(std::move(service_provider));
  service_state.service_provider.set_disconnect_handler(
      base::BindOnce(&FakeMojoServiceManager::ServiceProviderDisconnectHandler,
                     base::Unretained(this), service_name));

  const mojom::ProcessIdentityPtr& identity = receiver_set_.current_context();
  service_state.owner = identity.Clone();
  SendServiceEvent(mojom::ServiceEvent::New(
      mojom::ServiceEvent::Type::kRegistered, service_name, identity.Clone()));

  auto pending_requests = std::move(service_state.pending_requests);
  for (auto& [requester, receiver] : pending_requests) {
    // If a receiver become invalid before being posted, don't send it because
    // the mojo will complain about sending invalid handles and reset the
    // connection of service provider.
    if (!receiver.is_valid()) {
      continue;
    }
    service_state.service_provider->Request(std::move(requester),
                                            std::move(receiver));
  }
}

void FakeMojoServiceManager::Request(const std::string& service_name,
                                     std::optional<base::TimeDelta> /*timeout*/,
                                     mojo::ScopedMessagePipeHandle receiver) {
  auto it = service_map_.find(service_name);
  if (it == service_map_.end()) {
    auto [it_new, success] = service_map_.try_emplace(service_name);
    CHECK(success);
    it = it_new;
  }

  ServiceState& service_state = it->second;
  const mojom::ProcessIdentityPtr& identity = receiver_set_.current_context();
  if (service_state.service_provider.is_bound()) {
    service_state.service_provider->Request(identity.Clone(),
                                            std::move(receiver));
    return;
  }
  service_state.pending_requests.emplace_back(identity.Clone(),
                                              std::move(receiver));
}

void FakeMojoServiceManager::Query(const std::string& service_name,
                                   QueryCallback callback) {
  auto it = service_map_.find(service_name);
  if (it == service_map_.end()) {
    std::move(callback).Run(mojom::ErrorOrServiceState::NewError(
        mojom::Error::New(mojom::ErrorCode::kServiceNotFound,
                          "Cannot find service: " + service_name)));
    return;
  }

  const ServiceState& service_state = it->second;
  mojom::ServiceStatePtr state =
      service_state.service_provider.is_bound()
          ? mojom::ServiceState::NewRegisteredState(
                mojom::RegisteredServiceState::New(
                    /*owner=*/service_state.owner.Clone()))
          : mojom::ServiceState::NewUnregisteredState(
                mojom::UnregisteredServiceState::New());
  std::move(callback).Run(
      mojom::ErrorOrServiceState::NewState(std::move(state)));
}

void FakeMojoServiceManager::AddServiceObserver(
    mojo::PendingRemote<mojom::ServiceObserver> observer) {
  service_observers_.Add(std::move(observer));
}

void FakeMojoServiceManager::ServiceProviderDisconnectHandler(
    const std::string& service_name) {
  auto it = service_map_.find(service_name);
  CHECK(it != service_map_.end());
  ServiceState& service_state = it->second;
  service_state.service_provider.reset();
  mojom::ProcessIdentityPtr dispatcher;
  dispatcher.Swap(&service_state.owner);
  SendServiceEvent(
      mojom::ServiceEvent::New(mojom::ServiceEvent::Type::kUnRegistered,
                               service_name, std::move(dispatcher)));
}

void FakeMojoServiceManager::SendServiceEvent(mojom::ServiceEventPtr event) {
  for (const mojo::Remote<mojom::ServiceObserver>& remote :
       service_observers_) {
    remote->OnServiceEvent(event.Clone());
  }
}

}  // namespace ash::mojo_service_manager
