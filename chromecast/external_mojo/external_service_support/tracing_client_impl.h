// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_TRACING_CLIENT_IMPL_H_
#define CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_TRACING_CLIENT_IMPL_H_

#include "chromecast/external_mojo/external_service_support/tracing_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/mojom/tracing_service.mojom.h"

namespace chromecast {
namespace external_service_support {
class ExternalConnector;

// TracingClient implementation for connecting to a tracing::TracingService
// instance through an external mojo broker.
class TracingClientImpl : public TracingClient {
 public:
  explicit TracingClientImpl(ExternalConnector* connector);
  ~TracingClientImpl() override;

 private:
  void TracingServiceDisconnected();

  mojo::Remote<tracing::mojom::TracingService> tracing_service_;

  ExternalConnector* const connector_;

  TracingClientImpl(const TracingClientImpl&) = delete;
  TracingClientImpl& operator=(const TracingClientImpl&) = delete;
};

}  // namespace external_service_support
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_TRACING_CLIENT_IMPL_H_
