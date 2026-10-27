// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_TRANSPORT_MOCK_TRANSPORT_SERVICE_H_
#define COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_TRANSPORT_MOCK_TRANSPORT_SERVICE_H_

#include <string_view>

#include "components/browser_actuator/public/transport/transport_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_actuator::transport {

class MockTransportService : public TransportService {
 public:
  MockTransportService();
  ~MockTransportService() override;

  MOCK_METHOD(TransportChannel*, GetChannel, (), (override));
  MOCK_METHOD(TransportSession*,
              CreateSession,
              (std::string_view session_id),
              (override));
  MOCK_METHOD(TransportSession*,
              GetSession,
              (std::string_view session_id),
              (override));
  MOCK_METHOD(SessionStreamRecorderFactory*,
              GetStreamRecorderFactory,
              (),
              (override));
};

}  // namespace browser_actuator::transport

#endif  // COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_TRANSPORT_MOCK_TRANSPORT_SERVICE_H_
