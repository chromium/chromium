// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/test_support/mock_transport_handler_factory.h"

namespace stream_transport {

MockTransportHandlerFactory::MockTransportHandlerFactory(
    const std::vector<PayloadType>& supported_types,
    FactoryId factory_id)
    : supported_types_(supported_types), factory_id_(factory_id) {}

MockTransportHandlerFactory::~MockTransportHandlerFactory() = default;

FactoryId MockTransportHandlerFactory::GetFactoryId() const {
  return factory_id_;
}

std::vector<PayloadType> MockTransportHandlerFactory::GetSupportedPayloadTypes()
    const {
  return supported_types_;
}

}  // namespace stream_transport
