// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SETTINGS_SIGNALS_COLLECTOR_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SETTINGS_SIGNALS_COLLECTOR_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/device_signals/core/browser/base_signals_collector.h"

namespace device_signals {

struct SettingsItem;
class SettingsClient;

// Collector in charge of collecting device signals that live in the settings
// (registry on windows, PLIST on mac).
class SettingsSignalsCollector : public BaseSignalsCollector {
 public:
  explicit SettingsSignalsCollector(
      std::unique_ptr<SettingsClient> settings_client);

  ~SettingsSignalsCollector() override;

  SettingsSignalsCollector(const SettingsSignalsCollector&) = delete;
  SettingsSignalsCollector& operator=(const SettingsSignalsCollector&) = delete;

 private:
  // Collection function for the Settings signal. `request` must contain
  // the required parameters for this signal. `response` will be passed along
  // and the signal values will be set on it when available. `done_closure` will
  // be invoked when signal collection is complete.
  void GetSettingsSignal(const SignalsAggregationRequest& request,
                         SignalsAggregationResponse& response,
                         base::OnceClosure done_closure);

  // Invoked when the SettingsClient returns the collected Settings (PLIST/REG)
  // items' signals as `settings_items`. Will update `response` with the
  // signal collection outcome, and invoke `done_closure` to asynchronously
  // notify the caller of the completion of this request.
  void OnSettingsSignalCollected(
      SignalsAggregationResponse& response,
      base::OnceClosure done_closure,
      const std::vector<SettingsItem>& settings_items);

  // Instance of SettingsClient responsible for actual signals collection
  std::unique_ptr<SettingsClient> settings_client_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SettingsSignalsCollector> weak_factory_{this};
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SETTINGS_SIGNALS_COLLECTOR_H_
