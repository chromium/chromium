// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_SIGNALS_COLLECTOR_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_SIGNALS_COLLECTOR_H_

#include "base/callback.h"
#include "base/values.h"
#include "components/device_signals/core/browser/signals_collector.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device_signals {

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

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_SIGNALS_COLLECTOR_H_
