// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_SIGNALS_MOCK_SIGNALS_FILTERER_H_
#define COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_SIGNALS_MOCK_SIGNALS_FILTERER_H_

#include "components/enterprise/device_trust/core/signals/signals_filterer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors::test {

class MockSignalsFilterer : public SignalsFilterer {
 public:
  MockSignalsFilterer();
  ~MockSignalsFilterer() override;

  MOCK_METHOD(void, Filter, (base::DictValue & signals), (override));
};

}  // namespace enterprise_connectors::test

#endif  // COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_SIGNALS_MOCK_SIGNALS_FILTERER_H_
