// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_phish_guard_gateway.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_phish_guard_delegate.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/logging/log_macros.h"

namespace autofill {

OtpPhishGuardGateway::PendingCheck::PendingCheck(LocalFrameToken frame,
                                                 VerdictCallback callback)
    : frame(frame), callback(std::move(callback)) {}

OtpPhishGuardGateway::PendingCheck::PendingCheck(PendingCheck&&) noexcept =
    default;

OtpPhishGuardGateway::PendingCheck&
OtpPhishGuardGateway::PendingCheck::operator=(PendingCheck&&) noexcept =
    default;

OtpPhishGuardGateway::PendingCheck::~PendingCheck() = default;

OtpPhishGuardGateway::OtpPhishGuardGateway(AutofillClient& client)
    : client_(client) {}

OtpPhishGuardGateway::~OtpPhishGuardGateway() = default;

void OtpPhishGuardGateway::Check(LocalFrameToken frame,
                                 VerdictCallback callback) {
  pending_checks_.emplace(frame, std::move(callback));
  DrainPendingChecks();
}

void OtpPhishGuardGateway::DrainPendingChecks() {
  if (dispatch_state_ != DispatchState::kIdle) {
    return;
  }

  base::WeakPtr<OtpPhishGuardGateway> self = weak_ptr_factory_.GetWeakPtr();
  while (self && !pending_checks_.empty()) {
    PendingCheck next = std::move(pending_checks_.front());
    pending_checks_.pop();

    if (OtpPhishGuardDelegate* delegate = client_->GetOtpPhishGuardDelegate()) {
      dispatch_state_ = DispatchState::kWaitingForVerdict;
      in_flight_start_time_ = base::TimeTicks::Now();
      in_flight_callback_ = std::move(next.callback);
      base::UmaHistogramBoolean(kCheckPerformedHistogram, true);
      LOG_AF(client_->GetCurrentLogManager())
          << LoggingScope::kOneTimeTokens
          << "PhishGuard check initiated for OTP token delivery.";
      in_flight_timeout_timer_.Start(
          FROM_HERE, kCheckTimeout,
          base::BindOnce(&OtpPhishGuardGateway::OnPhishGuardCheckCompleted,
                         in_flight_weak_ptr_factory_.GetWeakPtr(),
                         std::nullopt));
      delegate->StartOtpPhishGuardCheck(
          next.frame,
          base::BindOnce(
              [](base::WeakPtr<OtpPhishGuardGateway> self,
                 bool is_phishing_site) {
                if (self) {
                  self->OnPhishGuardCheckCompleted(is_phishing_site);
                }
              },
              in_flight_weak_ptr_factory_.GetWeakPtr()));
      // If `delegate` completed synchronously, `OnPhishGuardCheckCompleted`
      // already drained remaining checks; otherwise wait for the async verdict.
      return;
    }

    dispatch_state_ = DispatchState::kRunningCallback;
    base::UmaHistogramBoolean(kCheckPerformedHistogram, false);
    base::UmaHistogramEnumeration(kVerdictHistogram,
                                  OneTimeTokensPhishGuardVerdict::kUnknown);
    LOG_AF(client_->GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "PhishGuard delegate unavailable; verdict: "
        << OneTimeTokensPhishGuardVerdictToStringView(
               OneTimeTokensPhishGuardVerdict::kUnknown);
    std::move(next.callback).Run(OneTimeTokensPhishGuardVerdict::kUnknown);
    if (!self) {
      return;
    }
    dispatch_state_ = DispatchState::kIdle;
  }
}

void OtpPhishGuardGateway::OnPhishGuardCheckCompleted(
    std::optional<bool> is_phishing_site) {
  in_flight_weak_ptr_factory_.InvalidateWeakPtrs();
  in_flight_timeout_timer_.Stop();
  dispatch_state_ = DispatchState::kRunningCallback;
  base::UmaHistogramTimes(kLatencyHistogram,
                          base::TimeTicks::Now() - in_flight_start_time_);
  const OneTimeTokensPhishGuardVerdict verdict =
      !is_phishing_site.has_value()
          ? OneTimeTokensPhishGuardVerdict::kUnknown
          : (*is_phishing_site ? OneTimeTokensPhishGuardVerdict::kPhishing
                               : OneTimeTokensPhishGuardVerdict::kNotPhishing);
  LOG_AF(client_->GetCurrentLogManager())
      << LoggingScope::kOneTimeTokens
      << "PhishGuard check completed with verdict: "
      << OneTimeTokensPhishGuardVerdictToStringView(verdict);
  base::UmaHistogramEnumeration(kVerdictHistogram, verdict);
  VerdictCallback callback = std::move(in_flight_callback_);
  base::WeakPtr<OtpPhishGuardGateway> self = weak_ptr_factory_.GetWeakPtr();
  std::move(callback).Run(verdict);
  if (!self) {
    return;
  }
  dispatch_state_ = DispatchState::kIdle;
  DrainPendingChecks();
}

}  // namespace autofill
