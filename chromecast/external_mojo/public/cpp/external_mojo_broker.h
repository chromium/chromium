// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_EXTERNAL_MOJO_PUBLIC_CPP_EXTERNAL_MOJO_BROKER_H_
#define CHROMECAST_EXTERNAL_MOJO_PUBLIC_CPP_EXTERNAL_MOJO_BROKER_H_

#include <memory>
#include <string>
#include <vector>

#include "chromecast/external_mojo/public/mojom/connector.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromecast {
namespace external_mojo {

// Manages connections from Mojo services in external processes. May be used
// either in a standalone broker process, or embedded into a Chromium process.
class ExternalMojoBroker {
 public:
  // |in_process_only_services| names services which may only be registered or
  // bound by connectors created via CreateConnector()/BindConnector(); clients
  // connecting via the broker socket cannot register or bind to them.
  explicit ExternalMojoBroker(
      const std::string& broker_path,
      std::vector<std::string> in_process_only_services = {});

  ExternalMojoBroker(const ExternalMojoBroker&) = delete;
  ExternalMojoBroker& operator=(const ExternalMojoBroker&) = delete;

  ~ExternalMojoBroker();

  // Initializes the embedded into a Chromium process (eg in cast_shell).
  // |connector| is the ServiceManager connector within the Chromium process.
  // |external_services_to_proxy| is a list of the names of external services
  // that should be made accessible to Mojo services running within Chromium.
  void InitializeChromium(
      std::unique_ptr<service_manager::Connector> connector,
      const std::vector<std::string>& external_services_to_proxy);

  // Creates/binds a connector for use by code hosted in the same process as
  // the broker.
  mojo::PendingRemote<mojom::ExternalConnector> CreateConnector();
  void BindConnector(mojo::PendingReceiver<mojom::ExternalConnector> receiver);

  // Binds a connector for a client connecting from outside the broker process.
  // Such clients cannot register or bind to |in_process_only_services|.
  void BindExternalConnector(
      mojo::PendingReceiver<mojom::ExternalConnector> receiver);

 private:
  class ConnectorImpl;
  class ReadWatcher;

  std::unique_ptr<ConnectorImpl> connector_;
  std::unique_ptr<ReadWatcher> read_watcher_;
};

}  // namespace external_mojo
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_PUBLIC_CPP_EXTERNAL_MOJO_BROKER_H_
