// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_WIN_WIN_SIGNALS_COLLECTOR_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_WIN_WIN_SIGNALS_COLLECTOR_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/device_signals/core/browser/base_signals_collector.h"

namespace device_signals {

struct AvProduct;
struct InstalledHotfix;
class SystemSignalsServiceHost;

// Collector in charge of collecting device signals that are either specific to
// Windows, or require a specific Windows-only implementation.
class WinSignalsCollector : public BaseSignalsCollector {
 public:
  explicit WinSignalsCollector(SystemSignalsServiceHost* system_service_host);

  ~WinSignalsCollector() override;

  WinSignalsCollector(const WinSignalsCollector&) = delete;
  WinSignalsCollector& operator=(const WinSignalsCollector&) = delete;

 private:
  // Collection function for the Antivirus signal. `request` is ignored since AV
  // signal does not require parameters. `response` will be passed along and the
  // signal values will be set on it when available. `done_closure` will be
  // invoked when signal collection is complete.
  void GetAntiVirusSignal(const SignalsAggregationRequest& request,
                          SignalsAggregationResponse& response,
                          base::OnceClosure done_closure);

  // Invoked when the SystemSignalsService returns the collected AV signals as
  // `av_products`. Will update `response` with the signal collection outcome,
  // and invoke `done_closure` to asynchronously notify the caller of the
  // completion of this request.
  void OnAntiVirusSignalCollected(SignalsAggregationResponse& response,
                                  base::OnceClosure done_closure,
                                  const std::vector<AvProduct>& av_products);

  // Collection function for the Hotfix signal. `request` is ignored since
  // Hotfix signal does not require parameters. `response` will be passed along
  // and the signal values will be set on it when available. `done_closure` will
  // be invoked when signal collection is complete.
  void GetHotfixSignal(const SignalsAggregationRequest& request,
                       SignalsAggregationResponse& response,
                       base::OnceClosure done_closure);

  // Invoked when the SystemSignalsService returns the collected Hotfix signals
  // as `hotfixes`. Will update `response` with the signal collection outcome,
  // and invoke `done_closure` to asynchronously notify the caller of the
  // completion of this request.
  void OnHotfixSignalCollected(SignalsAggregationResponse& response,
                               base::OnceClosure done_closure,
                               const std::vector<InstalledHotfix>& hotfixes);

  // Instance used to retrieve a pointer to a SystemSignalsService instance.
  raw_ptr<SystemSignalsServiceHost> system_service_host_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<WinSignalsCollector> weak_factory_{this};
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_WIN_WIN_SIGNALS_COLLECTOR_H_
