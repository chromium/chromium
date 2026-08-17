// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_SIGNALS_DECORATORS_COMMON_MOCK_SIGNALS_DECORATOR_H_
#define COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_SIGNALS_DECORATORS_COMMON_MOCK_SIGNALS_DECORATOR_H_

#include "base/values.h"
#include "components/enterprise/device_trust/core/signals/decorators/common/signals_decorator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors::test {

class MockSignalsDecorator : public SignalsDecorator {
 public:
  MockSignalsDecorator();
  ~MockSignalsDecorator() override;

  MOCK_METHOD(void,
              Decorate,
              (base::DictValue&, base::OnceClosure),
              (override));
};

}  // namespace enterprise_connectors::test

#endif  // COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_SIGNALS_DECORATORS_COMMON_MOCK_SIGNALS_DECORATOR_H_
