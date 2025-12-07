// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_BASE_SIGNALS_COLLECTOR_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_BASE_SIGNALS_COLLECTOR_H_

#include <unordered_map>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "components/device_signals/core/browser/signals_collector.h"
#include "components/device_signals/core/browser/system_signals_service_host.h"

namespace device_signals {

class SystemSignalsServiceHost;

class BaseSignalsCollector
    : public SignalsCollector,
      public device_signals::SystemSignalsServiceHost::Observer {
 public:
  ~BaseSignalsCollector() override;

  // SignalsCollector:
  bool IsSignalSupported(SignalName signal_name) override;
  const std::unordered_set<SignalName> GetSupportedSignalNames() override;
  void GetSignal(SignalName signal_name,
                 UserPermission permission,
                 const SignalsAggregationRequest& request,
                 SignalsAggregationResponse& response,
                 base::OnceClosure done_closure) override;

  // SystemSignalsServiceHost::Observer
  void OnServiceDisconnect() override;

 protected:
  using GetSignalCallback =
      base::RepeatingCallback<void(UserPermission permission,
                                   const SignalsAggregationRequest&,
                                   SignalsAggregationResponse&,
                                   base::OnceClosure)>;

  explicit BaseSignalsCollector(
      std::unordered_map<SignalName, GetSignalCallback>
          signals_collection_map);

  // This should only be called to also observe `system_service_host`.
  BaseSignalsCollector(
      std::unordered_map<SignalName, GetSignalCallback> signals_collection_map,
      SystemSignalsServiceHost* system_service_host);

  // Adds the `done_closure` to the map of pending callbacks and returns the
  // callback id.
  int AddPendingCallback(base::OnceClosure done_closure);

  // Both invokes and removes the pending callback with the given
  // `callback_id` from the map of tracked callbacks.
  void RunPendingCallback(int callback_id);

  // Retrieves an instance of the SystemSignalsService.
  device_signals::mojom::SystemSignalsService* GetService();

 private:
  // Map used to forward signal collection requests to the right function keyed
  // from a given signal name.
  std::unordered_map<SignalName, GetSignalCallback>
      signals_collection_map_;

  // Map of callbacks waiting for a response from the systems signals
  // service.
  std::unordered_map<int, base::OnceClosure> pending_callbacks_;

  // Tracks the next callback ID for a pending callback.
  int next_pending_callback_id_ = 0;

  // Instance used to retrieve a pointer to a SystemSignalsService instance.
  raw_ptr<SystemSignalsServiceHost> system_service_host_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_BASE_SIGNALS_COLLECTOR_H_
