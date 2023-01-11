// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_COLLECTOR_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_COLLECTOR_H_

#include <unordered_set>

#include "base/functional/callback_forward.h"

namespace device_signals {

enum class SignalName;
struct SignalsAggregationRequest;
struct SignalsAggregationResponse;

class SignalsCollector {
 public:
  virtual ~SignalsCollector() = default;

  // Returns true if `signal_name` is part of the set of signals supported by
  // this collector.
  virtual bool IsSignalSupported(SignalName signal_name) = 0;

  // Returns the set of signal names that this collector can collect.
  virtual const std::unordered_set<SignalName> GetSupportedSignalNames() = 0;

  // Collects the signal named `signal_name` using parameters in `request`
  // (if needed), sets the collected values on `response` and invokes
  // `done_closure` when the signal is collected. `response` is owned by the
  // caller who is responsible for keeping the value alive while the signal is
  // being collected.
  virtual void GetSignal(SignalName signal_name,
                         const SignalsAggregationRequest& request,
                         SignalsAggregationResponse& response,
                         base::OnceClosure done_closure) = 0;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_COLLECTOR_H_
