// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/external_mojo/broker_service/broker_service.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "chromecast/external_mojo/public/cpp/common.h"
#include "chromecast/external_mojo/public/cpp/external_mojo_broker.h"
#include "chromecast/external_mojo/public/mojom/connector.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace chromecast {
namespace external_mojo {

namespace {

BrokerService* g_instance = nullptr;

std::vector<service_manager::Manifest>& GetExternalManifests() {
  static std::vector<service_manager::Manifest> manifests;
  return manifests;
}

service_manager::Manifest MakePackagedServices(
    std::vector<service_manager::Manifest> manifests) {
  service_manager::Manifest packaged;
  for (auto& manifest : manifests) {
    // External services can only access what the external Mojo broker service
    // can access, so require all services that external services require.
    for (const auto& capability_entry : manifest.required_capabilities) {
      const auto& service_name = capability_entry.first;
      const auto& capability_names = capability_entry.second;
      for (const auto& capability_name : capability_names) {
        packaged.required_capabilities[service_name].insert(capability_name);
      }
    }

    // External services that expose capabilities have an internal proxy service
    // registered with ServiceManager.
    if (!manifest.exposed_capabilities.empty()) {
      // The external Mojo broker service needs to require a capability from
      // each external service in order to register the internal proxy service
      // for it.
      packaged.required_capabilities[manifest.service_name].insert(
          manifest.exposed_capabilities.begin()->first);
      packaged.packaged_services.emplace_back(std::move(manifest));
    }
  }
  return packaged;
}

}  // namespace

BrokerService::BrokerService(service_manager::Connector* connector) {
  DCHECK(!g_instance);
  g_instance = this;
  io_thread_ = std::make_unique<base::Thread>("external_mojo");
  io_thread_->StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));

  std::vector<std::string> external_services_to_proxy;
  const service_manager::Manifest& manifest = GetManifest();
  for (const auto& sub_manifest : manifest.packaged_services) {
    external_services_to_proxy.push_back(sub_manifest.service_name);
  }
  bundle_.AddBinder(base::BindRepeating(&BrokerService::BindConnector,
                                        base::Unretained(this)));
  broker_ = base::SequenceBound<ExternalMojoBroker>(io_thread_->task_runner(),
                                                    GetBrokerPath());
  broker_.AsyncCall(&ExternalMojoBroker::InitializeChromium)
      .WithArgs(connector->Clone(), external_services_to_proxy);
}

BrokerService::~BrokerService() {
  broker_.Reset();
  io_thread_.reset();
  g_instance = nullptr;
}

// static
BrokerService* BrokerService::GetInstance() {
  return g_instance;
}

// static
void BrokerService::ServiceRequestHandler(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  if (!g_instance) {
    return;
  }
  g_instance->BindServiceRequest(std::move(receiver));
}

// static
void BrokerService::AddExternalServiceManifest(
    service_manager::Manifest manifest) {
  GetExternalManifests().push_back(std::move(manifest));
}

// static
const service_manager::Manifest& BrokerService::GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(BrokerService::kServiceName)
          .WithDisplayName("External Mojo Broker Service")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithSandboxType("none")
                           .CanRegisterOtherServiceInstances(true)
                           .Build())
          .ExposeCapability(
              "connector_factory",
              std::set<const char*>{
                  "chromecast.external_mojo.mojom.ExternalConnector",
              })
          .Build()
          .Amend(MakePackagedServices(GetExternalManifests()))};
  return *manifest;
}

void BrokerService::OnConnect(const service_manager::BindSourceInfo& source,
                              const std::string& interface_name,
                              mojo::ScopedMessagePipeHandle interface_pipe) {
  bundle_.BindInterface(interface_name, std::move(interface_pipe));
}

void BrokerService::BindConnector(
    mojo::PendingReceiver<mojom::ExternalConnector> receiver) {
  broker_.AsyncCall(&ExternalMojoBroker::BindConnector)
      .WithArgs(std::move(receiver));
}

mojo::PendingRemote<mojom::ExternalConnector> BrokerService::CreateConnector() {
  mojo::PendingRemote<mojom::ExternalConnector> connector_remote;
  BindConnector(connector_remote.InitWithNewPipeAndPassReceiver());
  return connector_remote;
}

void BrokerService::BindServiceRequest(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  if (service_receiver_.is_bound()) {
    LOG(INFO) << "BrokerService is re-binding to the Service Manager.";
    service_receiver_.Close();
  }
  service_receiver_.Bind(std::move(receiver));
}

}  // namespace external_mojo
}  // namespace chromecast
