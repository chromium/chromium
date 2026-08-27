// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_metrics_tracker.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"

namespace autofill {

OtpMetricsTracker::OtpMetricsTracker(
    one_time_tokens::OneTimeTokenService* one_time_token_service)
    : one_time_token_service_(one_time_token_service) {
  if (one_time_token_service_) {
    tickle_subscription_ = one_time_token_service_->SubscribeToTickles(
        one_time_tokens::OneTimeTokenSource::kGmail, base::Time::Max(),
        base::BindRepeating(&OtpMetricsTracker::OnTickleReceived,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

OtpMetricsTracker::~OtpMetricsTracker() = default;

void OtpMetricsTracker::OnOtpFieldDetected() {
  // TODO(crbug.com/548326101): Handle OTP field detection and correlate with
  // tickle arrivals.
}

void OtpMetricsTracker::OnTickleReceived(
    one_time_tokens::OneTimeTokenSource source) {
  // TODO(crbug.com/548326101): Record metrics when tickles arrive (both with
  // and without detected OTP fields).
}

}  // namespace autofill
