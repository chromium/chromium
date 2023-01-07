// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_RECEIVER_SETUP_QUERIER_H_
#define COMPONENTS_MIRRORING_SERVICE_RECEIVER_SETUP_QUERIER_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {
class SimpleURLLoader;
}

namespace mirroring {

// The ReceiverSetupQuerier sends a message to the receiver in order to gather
// its setup information, especially build version and the receiver's friendly
// name. This can be used to apply model-specific mirroring tuning.
class COMPONENT_EXPORT(MIRRORING_SERVICE) ReceiverSetupQuerier {
 public:
  ReceiverSetupQuerier(
      const net::IPAddress& address,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> loader_factory);
  ReceiverSetupQuerier(const ReceiverSetupQuerier&) = delete;
  ReceiverSetupQuerier(ReceiverSetupQuerier&&) = delete;
  ReceiverSetupQuerier& operator=(const ReceiverSetupQuerier&) = delete;
  ReceiverSetupQuerier& operator=(ReceiverSetupQuerier&&) = delete;
  ~ReceiverSetupQuerier();

  // The build version of the receiver.
  const std::string& build_version() const { return build_version_; }

  // The friendly name (human readable) of the receiver.
  const std::string& friendly_name() const { return friendly_name_; }

 private:
  // Query the receiver for its current setup and uptime.
  void Query();

  // Callback for the url loader response to populate the query results.
  void ProcessResponse(std::unique_ptr<network::SimpleURLLoader> loader,
                       std::unique_ptr<std::string> response);

  const net::IPAddress address_;

  std::string build_version_;

  std::string friendly_name_;

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<ReceiverSetupQuerier> weak_factory_{this};
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_RECEIVER_SETUP_QUERIER_H_
