// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/signals_aggregator_impl.h"

#include <functional>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/device_signals/core/browser/metrics_utils.h"
#include "components/device_signals/core/browser/signals_collector.h"
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
      NOTREACHED();
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
  CHECK(response);
  std::move(callback).Run(std::move(*response));
}

}  // namespace

SignalsAggregatorImpl::SignalsAggregatorImpl(
    UserPermissionService* permission_service,
    std::vector<std::unique_ptr<SignalsCollector>> collectors)
    : permission_service_(permission_service),
      collectors_(std::move(collectors)) {
  CHECK(permission_service_);
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

  const auto permission =
      permission_service_->CanUserCollectSignals(user_context);
  LogUserPermissionChecked(permission);
  if (permission != UserPermission::kGranted) {
    RespondWithError(PermissionToError(permission), std::move(callback));
    return;
  }

  auto response = std::make_unique<SignalsAggregationResponse>();
  auto* response_ptr = response.get();
  auto done_closure = base::BindOnce(OnSignalRetrieved, std::move(response),
                                     std::move(callback));
  GetSignal(*request.signal_names.begin(), permission, std::move(request),
            response_ptr, std::move(done_closure));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

void SignalsAggregatorImpl::GetSignals(const SignalsAggregationRequest& request,
                                       GetSignalsCallback callback) {
  LogSignalsCountRequested(request.signal_names.size());
  if (request.signal_names.empty()) {
    std::move(callback).Run(SignalsAggregationResponse());
    return;
  }

  const auto permission = (request.trigger == Trigger::kSignalsReport)
                              ? permission_service_->CanCollectReportSignals()
                              : permission_service_->CanCollectSignals();
  LogUserPermissionChecked(permission);
  if (permission != UserPermission::kGranted &&
      permission != UserPermission::kMissingConsent) {
    RespondWithError(PermissionToError(permission), std::move(callback));
    return;
  }

  // Create `response` on the heap to prevent memory access errors from
  // occurring in the collectors.
  auto response = std::make_unique<SignalsAggregationResponse>();
  auto* response_ptr = response.get();
  auto barrier_closure = base::BarrierClosure(
      request.signal_names.size(),
      base::BindOnce(OnSignalRetrieved, std::move(response),
                     std::move(callback)));
  for (const auto signal_name : request.signal_names) {
    GetSignal(signal_name, permission, request, response_ptr, barrier_closure);
  }
}

void SignalsAggregatorImpl::GetSignal(SignalName signal_name,
                                      UserPermission permission,
                                      const SignalsAggregationRequest& request,
                                      SignalsAggregationResponse* response,
                                      base::OnceClosure done_closure) {
  LogSignalCollectionRequested(signal_name);
  for (const auto& collector : collectors_) {
    if (!collector->IsSignalSupported(signal_name)) {
      // Signal is not supported by current collector.
      continue;
    }

    // Signal is supported by current collector.
    collector->GetSignal(signal_name, permission, request, *response,
                         std::move(done_closure));
    return;
  }

  // Not a supported signal.
  std::move(done_closure).Run();
}

}  // namespace device_signals
