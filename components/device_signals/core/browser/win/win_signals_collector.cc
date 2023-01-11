// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/win/win_signals_collector.h"

#include <functional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/system_signals_service_host.h"
#include "components/device_signals/core/common/mojom/system_signals.mojom.h"
#include "components/device_signals/core/common/win/win_types.h"

namespace device_signals {

WinSignalsCollector::WinSignalsCollector(
    SystemSignalsServiceHost* system_service_host)
    : BaseSignalsCollector({
          {SignalName::kAntiVirus,
           base::BindRepeating(&WinSignalsCollector::GetAntiVirusSignal,
                               base::Unretained(this))},
          {SignalName::kHotfixes,
           base::BindRepeating(&WinSignalsCollector::GetHotfixSignal,
                               base::Unretained(this))},
      }),
      system_service_host_(system_service_host) {
  DCHECK(system_service_host_);
}

WinSignalsCollector::~WinSignalsCollector() = default;

void WinSignalsCollector::GetAntiVirusSignal(
    const SignalsAggregationRequest& request,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure) {
  auto* system_signals_service = system_service_host_->GetService();
  if (!system_signals_service) {
    AntiVirusSignalResponse av_response;
    av_response.collection_error = SignalCollectionError::kMissingSystemService;
    response.av_signal_response = std::move(av_response);
    std::move(done_closure).Run();
    return;
  }

  system_signals_service->GetAntiVirusSignals(base::BindOnce(
      &WinSignalsCollector::OnAntiVirusSignalCollected,
      weak_factory_.GetWeakPtr(), std::ref(response), std::move(done_closure)));
}

void WinSignalsCollector::OnAntiVirusSignalCollected(
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure,
    const std::vector<AvProduct>& av_products) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AntiVirusSignalResponse av_response;
  av_response.av_products = std::move(av_products);
  response.av_signal_response = std::move(av_response);

  std::move(done_closure).Run();
}

void WinSignalsCollector::GetHotfixSignal(
    const SignalsAggregationRequest& request,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure) {
  auto* system_signals_service = system_service_host_->GetService();
  if (!system_signals_service) {
    HotfixSignalResponse hotfix_response;
    hotfix_response.collection_error =
        SignalCollectionError::kMissingSystemService;
    response.hotfix_signal_response = std::move(hotfix_response);
    std::move(done_closure).Run();
    return;
  }

  system_signals_service->GetHotfixSignals(base::BindOnce(
      &WinSignalsCollector::OnHotfixSignalCollected, weak_factory_.GetWeakPtr(),
      std::ref(response), std::move(done_closure)));
}

void WinSignalsCollector::OnHotfixSignalCollected(
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure,
    const std::vector<InstalledHotfix>& hotfixes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HotfixSignalResponse hotfix_response;
  hotfix_response.hotfixes = std::move(hotfixes);
  response.hotfix_signal_response = std::move(hotfix_response);

  std::move(done_closure).Run();
}

}  // namespace device_signals
