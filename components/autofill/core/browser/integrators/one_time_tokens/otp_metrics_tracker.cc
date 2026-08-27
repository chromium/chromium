// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_metrics_tracker.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"
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
  if (!base::FeatureList::IsEnabled(features::kAutofillGmailOtp)) {
    return;
  }
  field_detection_time_ = base::TimeTicks::Now();
}

void OtpMetricsTracker::OnTickleReceived(
    one_time_tokens::OneTimeTokenSource source) {
  if (!base::FeatureList::IsEnabled(features::kAutofillGmailOtp)) {
    return;
  }
  if (field_detection_time_.has_value()) {
    base::TimeDelta latency = base::TimeTicks::Now() - *field_detection_time_;
    if (latency <= kFieldDetectionTimeout) {
      base::UmaHistogramCustomTimes(
          kFieldDetectionToTickleLatencyHistogram, latency,
          base::Milliseconds(10),
          std::min(kFieldDetectionTimeout,
                   one_time_tokens::kNotificationExpirationDuration),
          50);
    }
    field_detection_time_.reset();
  }
}

}  // namespace autofill
