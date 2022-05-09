// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_AGGREGATOR_IMPL_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_AGGREGATOR_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/device_signals/core/browser/signals_aggregator.h"

namespace device_signals {

class SignalsCollector;

class SignalsAggregatorImpl : public SignalsAggregator {
 public:
  explicit SignalsAggregatorImpl(
      std::vector<std::unique_ptr<SignalsCollector>> collectors);

  SignalsAggregatorImpl(const SignalsAggregatorImpl&) = delete;
  SignalsAggregatorImpl& operator=(const SignalsAggregatorImpl&) = delete;

  ~SignalsAggregatorImpl() override;

  // SignalsAggregator:
  void GetSignals(const base::Value::Dict& parameters,
                  GetSignalsCallback callback) override;

 private:
  void OnSignalCollected(const std::string signal_name,
                         GetSignalsCallback callback,
                         base::Value value);

  std::vector<std::unique_ptr<SignalsCollector>> collectors_;

  base::WeakPtrFactory<SignalsAggregatorImpl> weak_factory_{this};
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_AGGREGATOR_IMPL_H_
