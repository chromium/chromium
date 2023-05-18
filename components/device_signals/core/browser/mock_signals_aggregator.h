// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_SIGNALS_AGGREGATOR_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_SIGNALS_AGGREGATOR_H_

#include "base/functional/callback.h"
#include "components/device_signals/core/browser/signals_aggregator.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device_signals {

class MockSignalsAggregator : public SignalsAggregator {
 public:
  MockSignalsAggregator();
  ~MockSignalsAggregator() override;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  MOCK_METHOD(void,
              GetSignalsForUser,
              (const UserContext&,
               const SignalsAggregationRequest&,
               SignalsAggregator::GetSignalsCallback),
              (override));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  MOCK_METHOD(void,
              GetSignals,
              (const SignalsAggregationRequest&,
               SignalsAggregator::GetSignalsCallback),
              (override));
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_SIGNALS_AGGREGATOR_H_
