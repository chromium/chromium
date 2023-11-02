// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_SERVICE_CONNECTOR_H_
#define CHROMECAST_BROWSER_SERVICE_CONNECTOR_H_

#include "base/types/id_type.h"
#include "chromecast/common/mojom/service_connector.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromecast {

class ServiceConnector;

// An opaque identifier type so that bound ServiceConnector endpoints can reason
// about who's making connection requests.
//
// We don't use an enum because the definition of these IDs is split across
// public and internal sources.
using ServiceConnectorClientId = base::IdType32<ServiceConnector>;

// Something in browser process itself (e.g. CastAudioManager)
extern const ServiceConnectorClientId kBrowserProcessClientId;

// The Media Service hosted by Content.
extern const ServiceConnectorClientId kMediaServiceClientId;

// Browser-side implementation of the ServiceConnector mojom interface to route
// interface binding requests to various Cast-related services on behalf of
// clients both inside and outside of the browser process.
class ServiceConnector : public mojom::ServiceConnector {
 public:
  ServiceConnector();
  ServiceConnector(const ServiceConnector&) = delete;
  ServiceConnector& operator=(const ServiceConnector&) = delete;
  ~ServiceConnector() override;

  // Connects a new pipe to the global ServiceConnector instance and returns its
  // PendingRemote. Callable from any thread.
  //
  // |client_id| indicates the identity of the client that will ultimately use
  // the returned ServiceConnector endpoint.
  static mojo::PendingRemote<mojom::ServiceConnector> MakeRemote(
      ServiceConnectorClientId client_id);

  // Binds a receiver to the global ServiceConnector. Callable from any thread.
  // |client_id| indicates the identity of the client holding the other end of
  // the ServiceConnector pipe.
  static void BindReceiver(
      ServiceConnectorClientId client_id,
      mojo::PendingReceiver<mojom::ServiceConnector> receiver);

  // mojom::ServiceConnector implementation:
  void Connect(const std::string& service_name,
               mojo::GenericPendingReceiver receiver) override;

 private:
  mojo::ReceiverSet<mojom::ServiceConnector, ServiceConnectorClientId>
      receivers_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_SERVICE_CONNECTOR_H_
