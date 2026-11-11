// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAM_TRANSPORT_PUBLIC_TRANSPORT_SERVICE_FACTORY_HELPER_H_
#define COMPONENTS_STREAM_TRANSPORT_PUBLIC_TRANSPORT_SERVICE_FACTORY_HELPER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/stream_transport/public/transport_service.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace stream_transport {

// Creates a concrete `TransportService` instance for a profile.
//
// `identity_manager` must outlive the returned service; the embedder's
// `KeyedService` factory (`TransportServiceFactory`) depends on
// `IdentityManagerFactory` to guarantee that.
std::unique_ptr<TransportService> CreateTransportService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    TransportServiceConfig config);

}  // namespace stream_transport

#endif  // COMPONENTS_STREAM_TRANSPORT_PUBLIC_TRANSPORT_SERVICE_FACTORY_HELPER_H_
