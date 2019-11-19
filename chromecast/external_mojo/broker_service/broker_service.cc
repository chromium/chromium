// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/external_mojo/broker_service/broker_service.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "chromecast/external_mojo/public/cpp/common.h"
#include "chromecast/external_mojo/public/cpp/external_mojo_broker.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace chromecast {
namespace external_mojo {

namespace {

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

BrokerService::BrokerService(service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)) {
  io_thread_ = std::make_unique<base::Thread>("external_mojo");
  io_thread_->StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));

  std::vector<std::string> external_services_to_proxy;
  const service_manager::Manifest& manifest = GetManifest();
  for (const auto& sub_manifest : manifest.packaged_services) {
    external_services_to_proxy.push_back(sub_manifest.service_name);
  }
  broker_ = base::SequenceBound<ExternalMojoBroker>(io_thread_->task_runner(),
                                                    GetBrokerPath());
  broker_.Post(FROM_HERE, &ExternalMojoBroker::InitializeChromium,
               service_binding_.GetConnector()->Clone(),
               external_services_to_proxy);
}

BrokerService::~BrokerService() {
  broker_.Reset();
  io_thread_.reset();
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
          .Build()
          .Amend(MakePackagedServices(GetExternalManifests()))};
  return *manifest;
}

}  // namespace external_mojo
}  // namespace chromecast
