// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_MOCK_TRANSPORT_HANDLER_FACTORY_H_
#define COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_MOCK_TRANSPORT_HANDLER_FACTORY_H_

#include <memory>
#include <vector>

#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_handler_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_actuator {

class TransportSession;

class MockTransportHandlerFactory : public TransportHandlerFactory {
 public:
  explicit MockTransportHandlerFactory(
      const std::vector<PayloadType>& supported_types,
      FactoryId factory_id = FactoryId::kUnset);
  ~MockTransportHandlerFactory() override;

  FactoryId GetFactoryId() const override;
  std::vector<PayloadType> GetSupportedPayloadTypes() const override;

  MOCK_METHOD(std::unique_ptr<TransportHandler>,
              OnNewSession,
              (TransportSession*),
              (override));

 private:
  const std::vector<PayloadType> supported_types_;
  const FactoryId factory_id_;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_MOCK_TRANSPORT_HANDLER_FACTORY_H_
