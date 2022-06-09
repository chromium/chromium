// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_WIN_WIN_SIGNALS_COLLECTOR_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_WIN_WIN_SIGNALS_COLLECTOR_H_

#include <map>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/device_signals/core/browser/signals_collector.h"
#include "components/device_signals/core/common/win/win_types.h"

namespace base {
class Value;
}  // namespace base

namespace device_signals {

class SystemSignalsServiceHost;

// Collector in charge of collecting device signals that are either specific to
// Windows, or require a specific Windows-only implementation.
class WinSignalsCollector : public SignalsCollector {
 public:
  explicit WinSignalsCollector(SystemSignalsServiceHost* system_service_host);

  ~WinSignalsCollector() override;

  WinSignalsCollector(const WinSignalsCollector&) = delete;
  WinSignalsCollector& operator=(const WinSignalsCollector&) = delete;

  // SignalsCollector:
  const std::unordered_set<std::string> GetSupportedSignalNames() override;
  void GetSignal(const std::string& signal_name,
                 const base::Value& params,
                 GetSignalCallback callback) override;

 private:
  // Collection function for the AV signal. `params` is ignored since AV signal
  // does not require parameters. `callback` will be invoked when the signal
  // values are available, or when something went wrong.
  void GetAntiVirusSignal(const base::Value& params,
                          GetSignalCallback callback);

  // Invoked when the SystemSignalsService returns the collected AV signals as
  // `av_products`. Will convert the structs to base::Value and invoke
  // `callback`.
  void OnAntiVirusSignalCollected(GetSignalCallback callback,
                                  const std::vector<AvProduct>& av_products);

  // Collection function for the Hotfix signal. `params` is ignored since Hotfix
  // signal does not require parameters. `callback` will be invoked when the
  // signal values are available, or when something went wrong.
  void GetHotfixSignal(const base::Value& params, GetSignalCallback callback);

  // Invoked when the SystemSignalsService returns the collected Hotfix signals
  // as `hotfixes`. Will convert the structs to base::Value and invoke
  // `callback`.
  void OnHotfixSignalCollected(GetSignalCallback callback,
                               const std::vector<InstalledHotfix>& hotfixes);

  // Instance used to retrieve a pointer to a SystemSignalsService instance.
  base::raw_ptr<SystemSignalsServiceHost> system_service_host_;

  // Map used to forward signal collection requests to the right function keyed
  // from a given signal name.
  std::map<const std::string,
           base::RepeatingCallback<void(const base::Value&, GetSignalCallback)>>
      signals_collection_map_;

  base::WeakPtrFactory<WinSignalsCollector> weak_factory_{this};
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_WIN_WIN_SIGNALS_COLLECTOR_H_
