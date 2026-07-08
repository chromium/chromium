// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_SIGNALS_MOCK_SIGNALS_SERVICE_H_
#define COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_SIGNALS_MOCK_SIGNALS_SERVICE_H_

#include "components/enterprise/device_trust/core/signals/signals_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors::test {

class MockSignalsService : public SignalsService {
 public:
  MockSignalsService();
  ~MockSignalsService() override;

  MOCK_METHOD(void, CollectSignals, (CollectSignalsCallback), (override));
};

}  // namespace enterprise_connectors::test

#endif  // COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_SIGNALS_MOCK_SIGNALS_SERVICE_H_
