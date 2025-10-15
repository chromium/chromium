// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/base_signals_collector.h"

#include <unordered_set>
#include <utility>

#include "base/functional/callback.h"
#include "components/device_signals/core/browser/metrics_utils.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/system_signals_service_host.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/device_signals/core/common/signals_features.h"

namespace device_signals {

BaseSignalsCollector::BaseSignalsCollector(
    std::unordered_map<SignalName, GetSignalCallback> signals_collection_map)
    : signals_collection_map_(std::move(signals_collection_map)) {
  CHECK(!signals_collection_map_.empty());
}

BaseSignalsCollector::BaseSignalsCollector(
    std::unordered_map<SignalName, GetSignalCallback> signals_collection_map,
    SystemSignalsServiceHost* system_service_host)
    : signals_collection_map_(std::move(signals_collection_map)),
      system_service_host_(system_service_host) {
  CHECK(!signals_collection_map_.empty());
  CHECK(system_service_host_);

  if (enterprise_signals::features::
          IsSystemSignalCollectionImprovementEnabled()) {
    system_service_host_->AddObserver(this);
  }
}

BaseSignalsCollector::~BaseSignalsCollector() {
  if (enterprise_signals::features::
          IsSystemSignalCollectionImprovementEnabled() &&
      system_service_host_) {
    system_service_host_->RemoveObserver(this);
  }
}

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
                                     UserPermission permission,
                                     const SignalsAggregationRequest& request,
                                     SignalsAggregationResponse& response,
                                     base::OnceClosure done_closure) {
  if (!IsSignalSupported(signal_name)) {
    response.top_level_error = SignalCollectionError::kUnsupported;
    std::move(done_closure).Run();
    return;
  }

  if (permission != UserPermission::kGranted &&
      permission != UserPermission::kMissingConsent) {
    std::move(done_closure).Run();
    return;
  }

  signals_collection_map_[signal_name].Run(permission, request, response,
                                           std::move(done_closure));
}

void BaseSignalsCollector::OnServiceDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LogSystemSignalCollectionDisconnect(pending_callbacks_.size());
  auto it = pending_callbacks_.begin();
  while (it != pending_callbacks_.end()) {
    base::OnceClosure cb = std::move(it->second);
    it = pending_callbacks_.erase(it);
    std::move(cb).Run();
  }
}

int BaseSignalsCollector::AddPendingCallback(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  next_pending_callback_id_++;
  pending_callbacks_.emplace(next_pending_callback_id_,
                             std::move(done_closure));
  return next_pending_callback_id_;
}

void BaseSignalsCollector::RunPendingCallback(int callback_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = pending_callbacks_.find(callback_id);
  if (it == pending_callbacks_.end()) {
    LogSystemSignalCollectionMissingPendingCallback();
    return;
  }

  base::OnceClosure cb = std::move(it->second);
  pending_callbacks_.erase(it);
  std::move(cb).Run();
}

device_signals::mojom::SystemSignalsService*
BaseSignalsCollector::GetService() {
  if (!system_service_host_) {
    return nullptr;
  }

  return system_service_host_->GetService();
}

}  // namespace device_signals
