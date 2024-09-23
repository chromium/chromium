// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/signals_aggregator_impl.h"

#include <functional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/device_signals/core/browser/metrics_utils.h"
#include "components/device_signals/core/browser/signals_collector.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/user_context.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/device_signals/core/common/signals_constants.h"

namespace device_signals {

namespace {

SignalCollectionError PermissionToError(const UserPermission permission) {
  switch (permission) {
    case UserPermission::kUnaffiliated:
      return SignalCollectionError::kUnaffiliatedUser;
    case UserPermission::kMissingConsent:
      return SignalCollectionError::kConsentRequired;
    case UserPermission::kConsumerUser:
    case UserPermission::kUnknownUser:
    case UserPermission::kMissingUser:
      return SignalCollectionError::kInvalidUser;
    case UserPermission::kGranted:
      NOTREACHED_IN_MIGRATION();
      ABSL_FALLTHROUGH_INTENDED;
    case UserPermission::kUnsupported:
      return SignalCollectionError::kUnsupported;
  }
}

void RespondWithError(SignalCollectionError error,
                      SignalsAggregator::GetSignalsCallback callback) {
  SignalsAggregationResponse response;
  response.top_level_error = error;
  std::move(callback).Run(std::move(response));
}

void OnSignalRetrieved(std::unique_ptr<SignalsAggregationResponse> response,
                       SignalsAggregator::GetSignalsCallback callback) {
  DCHECK(response);
  std::move(callback).Run(std::move(*response));
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
void SignalsAggregatorImpl::GetSignalsForUser(
    const UserContext& user_context,
    const SignalsAggregationRequest& request,
    GetSignalsCallback callback) {
  // Request for collection of multiple signals is not yet supported. Only the
  // first signal will be returned.
  if (request.signal_names.size() != 1) {
    RespondWithError(SignalCollectionError::kUnsupported, std::move(callback));
    return;
  }

  LogSignalCollectionRequested(*request.signal_names.begin());

  const auto permission =
      permission_service_->CanUserCollectSignals(user_context);
  GetSignalsWithPermission(permission, std::move(request), std::move(callback));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

void SignalsAggregatorImpl::GetSignals(const SignalsAggregationRequest& request,
                                       GetSignalsCallback callback) {
  // Request for collection of multiple signals is not yet supported. Only the
  // first signal will be returned.
  if (request.signal_names.size() != 1) {
    RespondWithError(SignalCollectionError::kUnsupported, std::move(callback));
    return;
  }

  LogSignalCollectionRequested(*request.signal_names.begin());

  const auto permission = permission_service_->CanCollectSignals();
  GetSignalsWithPermission(permission, std::move(request), std::move(callback));
}

void SignalsAggregatorImpl::GetSignalsWithPermission(
    const UserPermission user_permission,
    const SignalsAggregationRequest& request,
    GetSignalsCallback callback) {
  LogUserPermissionChecked(user_permission);
  if (user_permission != UserPermission::kGranted) {
    RespondWithError(PermissionToError(user_permission), std::move(callback));
    return;
  }

  SignalName signal_name = *request.signal_names.begin();
  for (const auto& collector : collectors_) {
    if (!collector->IsSignalSupported(signal_name)) {
      // Signal is not supported by current collector.
      continue;
    }

    // Create `response` on the heap to prevent memory access errors from
    // occurring in the collectors.
    auto response = std::make_unique<SignalsAggregationResponse>();
    SignalsAggregationResponse* response_ptr = response.get();

    // Signal is supported by current collector.
    auto done_closure = base::BindOnce(&OnSignalRetrieved, std::move(response),
                                       std::move(callback));
    collector->GetSignal(signal_name, request, *response_ptr,
                         std::move(done_closure));
    return;
  }

  // Not a supported signal.
  RespondWithError(SignalCollectionError::kUnsupported, std::move(callback));
}

}  // namespace device_signals
