// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_BASE_SIGNALS_COLLECTOR_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_BASE_SIGNALS_COLLECTOR_H_

#include <unordered_map>

#include "base/functional/callback.h"
#include "components/device_signals/core/browser/signals_collector.h"

namespace device_signals {

class BaseSignalsCollector : public SignalsCollector {
 public:
  ~BaseSignalsCollector() override;

  // SignalsCollector:
  bool IsSignalSupported(SignalName signal_name) override;
  const std::unordered_set<SignalName> GetSupportedSignalNames() override;
  void GetSignal(SignalName signal_name,
                 const SignalsAggregationRequest& request,
                 SignalsAggregationResponse& response,
                 base::OnceClosure done_closure) override;

 protected:
  using GetSignalCallback =
      base::RepeatingCallback<void(const SignalsAggregationRequest&,
                                   SignalsAggregationResponse&,
                                   base::OnceClosure)>;

  explicit BaseSignalsCollector(
      std::unordered_map<const SignalName, GetSignalCallback>
          signals_collection_map);

 private:
  // Map used to forward signal collection requests to the right function keyed
  // from a given signal name.
  std::unordered_map<const SignalName, GetSignalCallback>
      signals_collection_map_;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_BASE_SIGNALS_COLLECTOR_H_
