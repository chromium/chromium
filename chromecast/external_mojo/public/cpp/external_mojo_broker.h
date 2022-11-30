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
  explicit ExternalMojoBroker(const std::string& broker_path);

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

  mojo::PendingRemote<mojom::ExternalConnector> CreateConnector();

  void BindConnector(mojo::PendingReceiver<mojom::ExternalConnector> receiver);

 private:
  class ConnectorImpl;
  class ReadWatcher;

  std::unique_ptr<ConnectorImpl> connector_;
  std::unique_ptr<ReadWatcher> read_watcher_;
};

}  // namespace external_mojo
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_PUBLIC_CPP_EXTERNAL_MOJO_BROKER_H_
