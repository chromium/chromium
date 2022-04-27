// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_SIGNALS_MOCK_SIGNALS_COLLECTOR_H_
#define COMPONENTS_ENTERPRISE_BROWSER_SIGNALS_MOCK_SIGNALS_COLLECTOR_H_

#include "base/callback.h"
#include "base/values.h"
#include "components/enterprise/browser/signals/signals_collector.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_signals {

class MockSignalsCollector : public SignalsCollector {
 public:
  MockSignalsCollector();
  ~MockSignalsCollector() override;

  MOCK_METHOD(const std::vector<std::string>,
              GetSupportedSignalNames,
              (),
              (override));
  MOCK_METHOD(void,
              GetSignal,
              (const std::string&,
               const base::Value&,
               SignalsCollector::GetSignalCallback),
              (override));
};

}  // namespace enterprise_signals

#endif  // COMPONENTS_ENTERPRISE_BROWSER_SIGNALS_MOCK_SIGNALS_COLLECTOR_H_
