// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/settings_signals_collector.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "components/device_signals/core/browser/settings_client.h"
#include "components/device_signals/core/browser/signals_types.h"

namespace device_signals {

SettingsSignalsCollector::SettingsSignalsCollector(
    std::unique_ptr<SettingsClient> settings_client)
    : BaseSignalsCollector({
          {SignalName::kSystemSettings,
           base::BindRepeating(&SettingsSignalsCollector::GetSettingsSignal,
                               base::Unretained(this))},
      }),
      settings_client_(std::move(settings_client)) {
  DCHECK(settings_client_);
}

SettingsSignalsCollector::~SettingsSignalsCollector() = default;

void SettingsSignalsCollector::GetSettingsSignal(
    const SignalsAggregationRequest& request,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure) {
  if (request.settings_signal_parameters.empty()) {
    SettingsResponse signal_response;
    signal_response.collection_error =
        SignalCollectionError::kMissingParameters;
    response.settings_response = std::move(signal_response);
    std::move(done_closure).Run();
    return;
  }

  settings_client_->GetSettings(
      request.settings_signal_parameters,
      base::BindOnce(&SettingsSignalsCollector::OnSettingsSignalCollected,
                     weak_factory_.GetWeakPtr(), std::ref(response),
                     std::move(done_closure)));
}

void SettingsSignalsCollector::OnSettingsSignalCollected(
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure,
    const std::vector<SettingsItem>& settings_items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SettingsResponse signal_response;
  signal_response.settings_items = std::move(settings_items);
  response.settings_response = std::move(signal_response);
  std::move(done_closure).Run();
}

}  // namespace device_signals
