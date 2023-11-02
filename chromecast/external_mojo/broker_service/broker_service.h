// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_EXTERNAL_MOJO_BROKER_SERVICE_BROKER_SERVICE_H_
#define CHROMECAST_EXTERNAL_MOJO_BROKER_SERVICE_BROKER_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/threading/sequence_bound.h"
#include "chromecast/external_mojo/public/mojom/connector.mojom.h"
#include "chromecast/mojo/interface_bundle.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace base {
class SequencedTaskRunner;
class Thread;
}  // namespace base

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromecast {
namespace external_mojo {
class ExternalMojoBroker;

// A Mojo service (intended to run within cast_shell or some other Chromium
// ServiceManager environment) that allows Mojo services built into external
// processes to interoperate with the Mojo services within cast_shell.
class BrokerService : public ::service_manager::Service {
 public:
  static BrokerService* GetInstance();
  static void ServiceRequestHandler(
      mojo::PendingReceiver<service_manager::mojom::Service> receiver);

  static constexpr char const* kServiceName = "external_mojo_broker";

  // Adds a manifest for an external Mojo service (ie, one that is running in
  // a non-Chromium process). A manifest is only needed for external services
  // that bind to Mojo services within cast_shell, or for external services that
  // are bound to (used) by internal Mojo services. All external manifests must
  // be added before GetExternalMojoBrokerManifest() is called (otherwise they
  // will not be included in the broker manifest, and so the relevant
  // permissions will not be set correctly).
  static void AddExternalServiceManifest(service_manager::Manifest manifest);

  // Returns the manifest for this service.
  static const service_manager::Manifest& GetManifest();

  explicit BrokerService(service_manager::Connector* connector);

  BrokerService(const BrokerService&) = delete;
  BrokerService& operator=(const BrokerService&) = delete;

  ~BrokerService() override;

  // ::service_manager::Service implementation:
  void OnConnect(const service_manager::BindSourceInfo& source,
                 const std::string& interface_name,
                 mojo::ScopedMessagePipeHandle interface_pipe) override;

  // Dispenses a connector for use in a remote process. The remote process must
  // already belong to the same process network as the BrokerService.
  mojo::PendingRemote<mojom::ExternalConnector> CreateConnector();

 private:
  void BindServiceRequest(
      mojo::PendingReceiver<service_manager::mojom::Service> receiver);

  void BindConnector(mojo::PendingReceiver<mojom::ExternalConnector> receiver);

  service_manager::ServiceReceiver service_receiver_{this};

  std::unique_ptr<base::Thread> io_thread_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  InterfaceBundle bundle_;

  base::SequenceBound<ExternalMojoBroker> broker_;
};

}  // namespace external_mojo
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_BROKER_SERVICE_BROKER_SERVICE_H_
