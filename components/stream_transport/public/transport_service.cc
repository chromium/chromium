// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/public/transport_service.h"

namespace stream_transport {

TransportServiceConfig::TransportServiceConfig() = default;
TransportServiceConfig::TransportServiceConfig(const TransportServiceConfig&) =
    default;
TransportServiceConfig& TransportServiceConfig::operator=(
    const TransportServiceConfig&) = default;
TransportServiceConfig::TransportServiceConfig(TransportServiceConfig&&) =
    default;
TransportServiceConfig& TransportServiceConfig::operator=(
    TransportServiceConfig&&) = default;
TransportServiceConfig::~TransportServiceConfig() = default;

TransportService::TransportService() = default;

TransportService::~TransportService() = default;

}  // namespace stream_transport
