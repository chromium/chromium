// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/public/transport_service_factory_helper.h"

#include <memory>
#include <utility>

#include "components/stream_transport/internal/transport_service_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace stream_transport {

std::unique_ptr<TransportService> CreateTransportService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    TransportServiceConfig config) {
  return std::make_unique<TransportServiceImpl>(
      std::move(url_loader_factory), identity_manager, std::move(config));
}

}  // namespace stream_transport
