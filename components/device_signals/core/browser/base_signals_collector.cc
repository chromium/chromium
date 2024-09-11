// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/base_signals_collector.h"

#include <unordered_set>
#include <utility>

#include "base/functional/callback.h"
#include "components/device_signals/core/browser/signals_types.h"

namespace device_signals {

BaseSignalsCollector::BaseSignalsCollector(
    std::unordered_map<const SignalName, GetSignalCallback>
        signals_collection_map)
    : signals_collection_map_(std::move(signals_collection_map)) {
  DCHECK(!signals_collection_map_.empty());
}

BaseSignalsCollector::~BaseSignalsCollector() = default;

bool BaseSignalsCollector::IsSignalSupported(SignalName signal_name) {
  const auto it = signals_collection_map_.find(signal_name);
  return it != signals_collection_map_.end();
}

const std::unordered_set<SignalName>
BaseSignalsCollector::GetSupportedSignalNames() {
  std::unordered_set<SignalName> supported_signals;
  for (const auto& collection_pair : signals_collection_map_) {
    supported_signals.insert(collection_pair.first);
  }

  return supported_signals;
}

void BaseSignalsCollector::GetSignal(SignalName signal_name,
                                     const SignalsAggregationRequest& request,
                                     SignalsAggregationResponse& response,
                                     base::OnceClosure done_closure) {
  if (!IsSignalSupported(signal_name)) {
    response.top_level_error = SignalCollectionError::kUnsupported;
    std::move(done_closure).Run();
    return;
  }

  signals_collection_map_[signal_name].Run(request, response,
                                           std::move(done_closure));
}

}  // namespace device_signals
