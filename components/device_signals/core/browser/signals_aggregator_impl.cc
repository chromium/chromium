// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/signals_aggregator_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/device_signals/core/browser/signals_collector.h"

namespace device_signals {

SignalsAggregatorImpl::SignalsAggregatorImpl(
    std::vector<std::unique_ptr<SignalsCollector>> collectors)
    : collectors_(std::move(collectors)) {}

SignalsAggregatorImpl::~SignalsAggregatorImpl() = default;

void SignalsAggregatorImpl::GetSignals(const base::Value::Dict& parameters,
                                       GetSignalsCallback callback) {
  // TODO(b/229737384): Add CBCM-managed check.
  if (parameters.empty()) {
    std::move(callback).Run(base::Value::Dict());
    return;
  }

  // Request for collection of multiple signals is not yet supported. Only the
  // first signal will be returned.
  DCHECK(parameters.size() == 1);

  std::pair<const std::string&, const base::Value&> signal_request =
      *parameters.begin();
  for (const auto& collector : collectors_) {
    for (const auto& supported_signal : collector->GetSupportedSignalNames()) {
      if (base::EqualsCaseInsensitiveASCII(supported_signal,
                                           signal_request.first)) {
        // Current collector supports collecting the current signal.
        auto return_callback =
            base::BindOnce(&SignalsAggregatorImpl::OnSignalCollected,
                           weak_factory_.GetWeakPtr(), signal_request.first,
                           std::move(callback));
        collector->GetSignal(signal_request.first, signal_request.second,
                             std::move(return_callback));
        return;
      }
    }
  }

  // Not a supported signal.
  std::move(callback).Run(base::Value::Dict());
}

void SignalsAggregatorImpl::OnSignalCollected(const std::string signal_name,
                                              GetSignalsCallback callback,
                                              base::Value value) {
  base::Value::Dict return_value;
  return_value.Set(signal_name, std::move(value));
  std::move(callback).Run(std::move(return_value));
}

}  // namespace device_signals
