// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_CHROMIUM_SERVICE_H_
#define CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_CHROMIUM_SERVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chromecast/external_mojo/public/mojom/connector.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace service_manager {
class Connector;
class Service;
}  // namespace service_manager

namespace chromecast {
namespace external_service_support {
class ExternalConnector;

// Provides a wrapper for a Chromium ServiceManager-based service to run in
// an external (non-Chromium) process.
class ChromiumServiceWrapper : public external_mojo::mojom::ExternalService {
 public:
  ChromiumServiceWrapper(
      ExternalConnector* connector,
      service_manager::mojom::ServicePtr service_ptr,
      std::unique_ptr<service_manager::Service> chromium_service,
      const std::string& service_name);
  ~ChromiumServiceWrapper() override;

 private:
  // external_mojo::mojom::ExternalService implementation:
  void OnBindInterface(const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  const service_manager::mojom::ServicePtr service_ptr_;
  const std::unique_ptr<service_manager::Service> chromium_service_;

  mojo::Receiver<external_mojo::mojom::ExternalService> service_receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromiumServiceWrapper);
};

// Creates a ServiceRequest (analogous to one created by Chromium
// ServiceManager) for use in creating Chromium Mojo services in an external
// process. |service_ptr| will be filled in with a pointer for the service,
// which should be bassed to ChromiumServiceWrapper's constructor. |identity| is
// the desired identity of the service to be created (ie, what will be returned
// from ServiceBinding::identity() once the service binding is created). If you
// don't care about the identity, just use the default.
service_manager::mojom::ServiceRequest CreateChromiumServiceRequest(
    ExternalConnector* connector,
    service_manager::mojom::ServicePtr* service_ptr,
    service_manager::Identity identity = service_manager::Identity());

// Creates a service_manager::Connector instance from an external service
// ExternalConnector.
std::unique_ptr<service_manager::Connector> CreateChromiumConnector(
    ExternalConnector* connector);

// Convenience helper for services that only take a ServiceRequest param in the
// constructor. The |name| is the desired service name.
template <typename T>
std::unique_ptr<ChromiumServiceWrapper> CreateChromiumService(
    ExternalConnector* connector,
    const std::string& name) {
  service_manager::mojom::ServicePtr service_ptr;
  auto request = CreateChromiumServiceRequest(connector, &service_ptr);
  return std::make_unique<ChromiumServiceWrapper>(
      connector, std::move(service_ptr),
      std::make_unique<T>(std::move(request)), name);
}

}  // namespace external_service_support
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_CHROMIUM_SERVICE_H_
