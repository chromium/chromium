// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/signals_aggregator_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/device_signals/core/browser/signals_collector.h"
#include "components/device_signals/core/browser/user_context.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/device_signals/core/common/signals_constants.h"

namespace device_signals {

namespace {

std::string PermissionToError(const UserPermission permission) {
  switch (permission) {
    case UserPermission::kUnaffiliated:
      return errors::kUnaffiliatedUser;
    case UserPermission::kMissingConsent:
      return errors::kConsentRequired;
    case UserPermission::kConsumerUser:
    case UserPermission::kUnknownUser:
      return errors::kUnsupported;
    case UserPermission::kGranted:
      NOTREACHED();
      return "";
  }
}

}  // namespace

SignalsAggregatorImpl::SignalsAggregatorImpl(
    UserPermissionService* permission_service,
    std::vector<std::unique_ptr<SignalsCollector>> collectors)
    : permission_service_(permission_service),
      collectors_(std::move(collectors)) {
  DCHECK(permission_service_);
}

SignalsAggregatorImpl::~SignalsAggregatorImpl() = default;

void SignalsAggregatorImpl::GetSignals(const UserContext& user_context,
                                       base::Value::Dict parameters,
                                       GetSignalsCallback callback) {
  if (parameters.empty()) {
    std::move(callback).Run(base::Value(errors::kUnsupported));
    return;
  }

  // Request for collection of multiple signals is not yet supported. Only the
  // first signal will be returned.
  DCHECK(parameters.size() == 1);

  permission_service_->CanCollectSignals(
      user_context,
      base::BindOnce(&SignalsAggregatorImpl::OnUserPermissionChecked,
                     weak_factory_.GetWeakPtr(), std::move(parameters),
                     std::move(callback)));
}

void SignalsAggregatorImpl::OnUserPermissionChecked(
    base::Value::Dict parameters,
    GetSignalsCallback callback,
    const UserPermission user_permission) {
  if (user_permission != UserPermission::kGranted) {
    std::move(callback).Run(base::Value(PermissionToError(user_permission)));
    return;
  }

  std::pair<const std::string&, const base::Value&> signal_request =
      *parameters.begin();
  for (const auto& collector : collectors_) {
    const auto signals_set = collector->GetSupportedSignalNames();
    if (signals_set.find(signal_request.first) == signals_set.end()) {
      // Signal is not supported by current collector.
      continue;
    }

    // Signal is supported by current collector.
    auto return_callback = base::BindOnce(
        &SignalsAggregatorImpl::OnSignalCollected, weak_factory_.GetWeakPtr(),
        signal_request.first, std::move(callback));
    collector->GetSignal(signal_request.first, signal_request.second,
                         std::move(return_callback));
    return;
  }

  // Not a supported signal.
  std::move(callback).Run(base::Value(errors::kUnsupported));
}

void SignalsAggregatorImpl::OnSignalCollected(const std::string signal_name,
                                              GetSignalsCallback callback,
                                              base::Value value) {
  base::Value::Dict return_value;
  return_value.Set(signal_name, std::move(value));
  std::move(callback).Run(base::Value(std::move(return_value)));
}

}  // namespace device_signals
