// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/external_mojo/external_service_support/fake_external_connector.h"

#include <utility>

#include "chromecast/external_mojo/external_service_support/external_service.h"

namespace chromecast {
namespace external_service_support {

FakeExternalConnector::FakeExternalConnector() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FakeExternalConnector::FakeExternalConnector(
    mojo::PendingRemote<external_mojo::mojom::TestExternalConnector> remote)
    : parent_(std::move(remote)) {}

FakeExternalConnector::~FakeExternalConnector() = default;

base::CallbackListSubscription
FakeExternalConnector::AddConnectionErrorCallback(
    base::RepeatingClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::CallbackListSubscription();
}

void FakeExternalConnector::RegisterService(const std::string& service_name,
                                            ExternalService* service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RegisterService(service_name, service->GetReceiver());
}

void FakeExternalConnector::RegisterService(
    const std::string& service_name,
    mojo::PendingRemote<external_mojo::mojom::ExternalService> service_remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  services_.emplace(service_name, std::move(service_remote));
}

void FakeExternalConnector::RegisterServices(
    const std::vector<std::string>& service_names,
    const std::vector<ExternalService*>& services) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(service_names.size() == services.size());
  for (size_t i = 0; i < services.size(); ++i) {
    RegisterService(service_names[i], services[i]);
  }
}

void FakeExternalConnector::RegisterServices(
    std::vector<chromecast::external_mojo::mojom::ServiceInstanceInfoPtr>
        service_instances_info) {}

void FakeExternalConnector::QueryServiceList(
    base::OnceCallback<void(
        std::vector<chromecast::external_mojo::mojom::ExternalServiceInfoPtr>)>
        callback) {}

void FakeExternalConnector::BindInterface(
    const std::string& service_name,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe,
    bool async) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (parent_.is_bound()) {
    parent_->BindInterfaceInternal(service_name, interface_name,
                                   std::move(interface_pipe));
    return;
  }
  if (!services_.count(service_name)) {
    return;
  }
  services_[service_name]->OnBindInterface(interface_name,
                                           std::move(interface_pipe));
}

std::unique_ptr<external_service_support::ExternalConnector>
FakeExternalConnector::Clone() {
  mojo::PendingRemote<external_mojo::mojom::TestExternalConnector> remote;
  child_receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return std::make_unique<FakeExternalConnector>(std::move(remote));
}

mojo::PendingRemote<external_mojo::mojom::ExternalConnector>
FakeExternalConnector::RequestConnector() {
  return mojo::PendingRemote<external_mojo::mojom::ExternalConnector>();
}

void FakeExternalConnector::SendChromiumConnectorRequest(
    mojo::ScopedMessagePipeHandle request) {}

void FakeExternalConnector::BindInterfaceInternal(
    const std::string& service_name,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  BindInterface(service_name, interface_name, std::move(interface_pipe));
}

}  // namespace external_service_support
}  // namespace chromecast
