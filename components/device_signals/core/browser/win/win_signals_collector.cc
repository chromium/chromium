// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/win/win_signals_collector.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/values.h"
#include "components/device_signals/core/browser/system_signals_service_host.h"
#include "components/device_signals/core/common/mojom/system_signals.mojom.h"
#include "components/device_signals/core/common/signals_constants.h"

namespace device_signals {

WinSignalsCollector::WinSignalsCollector(
    SystemSignalsServiceHost* system_service_host)
    : system_service_host_(system_service_host),
      signals_collection_map_({
          {names::kAntiVirusInfo,
           base::BindRepeating(&WinSignalsCollector::GetAntiVirusSignal,
                               base::Unretained(this))},
          {names::kInstalledHotfixes,
           base::BindRepeating(&WinSignalsCollector::GetHotfixSignal,
                               base::Unretained(this))},
      }) {
  DCHECK(system_service_host_);
}

WinSignalsCollector::~WinSignalsCollector() = default;

const std::unordered_set<std::string>
WinSignalsCollector::GetSupportedSignalNames() {
  std::unordered_set<std::string> supported_signals;
  for (const auto& collection_pair : signals_collection_map_) {
    supported_signals.insert(collection_pair.first);
  }

  return supported_signals;
}

void WinSignalsCollector::GetSignal(const std::string& signal_name,
                                    const base::Value& params,
                                    GetSignalCallback callback) {
  const auto it = signals_collection_map_.find(signal_name);
  if (it == signals_collection_map_.end()) {
    std::move(callback).Run(base::Value(errors::kUnsupported));
    return;
  }

  it->second.Run(params, std::move(callback));
}

void WinSignalsCollector::GetAntiVirusSignal(const base::Value& params,
                                             GetSignalCallback callback) {
  auto* system_signals_service = system_service_host_->GetService();
  if (!system_signals_service) {
    std::move(callback).Run(base::Value(errors::kMissingSystemService));
    return;
  }

  system_signals_service->GetAntiVirusSignals(
      base::BindOnce(&WinSignalsCollector::OnAntiVirusSignalCollected,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WinSignalsCollector::OnAntiVirusSignalCollected(
    GetSignalCallback callback,
    const std::vector<AvProduct>& av_products) {
  base::Value::List av_values;
  for (const auto& av_product : av_products) {
    av_values.Append(av_product.ToValue());
  }

  std::move(callback).Run(base::Value(std::move(av_values)));
}

void WinSignalsCollector::GetHotfixSignal(const base::Value& params,
                                          GetSignalCallback callback) {
  auto* system_signals_service = system_service_host_->GetService();
  if (!system_signals_service) {
    std::move(callback).Run(base::Value(errors::kMissingSystemService));
    return;
  }

  system_signals_service->GetHotfixSignals(
      base::BindOnce(&WinSignalsCollector::OnHotfixSignalCollected,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WinSignalsCollector::OnHotfixSignalCollected(
    GetSignalCallback callback,
    const std::vector<InstalledHotfix>& hotfixes) {
  base::Value::List hotfix_values;
  for (const auto& hotfix : hotfixes) {
    hotfix_values.Append(hotfix.ToValue());
  }

  std::move(callback).Run(base::Value(std::move(hotfix_values)));
}

}  // namespace device_signals
