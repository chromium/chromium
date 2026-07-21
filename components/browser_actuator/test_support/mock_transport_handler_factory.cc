// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/test_support/mock_transport_handler_factory.h"

namespace browser_actuator {

MockTransportHandlerFactory::MockTransportHandlerFactory(
    const std::vector<PayloadType>& supported_types)
    : supported_types_(supported_types) {}

MockTransportHandlerFactory::~MockTransportHandlerFactory() = default;

std::vector<PayloadType> MockTransportHandlerFactory::GetSupportedPayloadTypes()
    const {
  return supported_types_;
}

}  // namespace browser_actuator
