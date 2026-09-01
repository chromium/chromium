// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_metrics_tracker.h"

#include <algorithm>
#include <string_view>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"

namespace autofill {

namespace {

void TryRecordLatency(std::optional<base::TimeTicks>& previous_event_time,
                      std::optional<base::TimeTicks>& current_event_time,
                      std::string_view histogram_name) {
  base::TimeTicks now = base::TimeTicks::Now();
  if (previous_event_time.has_value()) {
    base::TimeDelta latency = now - *previous_event_time;
    previous_event_time.reset();
    if (latency <= OtpMetricsTracker::kFieldDetectionTimeout) {
      // `kNotificationExpirationDuration` is currently smaller than
      // `kFieldDetectionTimeout`, but `std::min` is used in case either value
      // changes in the future.
      base::UmaHistogramCustomTimes(
          histogram_name, latency, base::Milliseconds(10),
          std::min(OtpMetricsTracker::kFieldDetectionTimeout,
                   one_time_tokens::kNotificationExpirationDuration),
          50);
      return;
    }
  }
  current_event_time = now;
}

}  // namespace

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
  TryRecordLatency(tickle_time_, field_detection_time_,
                   kTickleToFieldDetectionLatencyHistogram);
}

void OtpMetricsTracker::OnTickleReceived(
    one_time_tokens::OneTimeTokenSource source) {
  if (!base::FeatureList::IsEnabled(features::kAutofillGmailOtp)) {
    return;
  }
  TryRecordLatency(field_detection_time_, tickle_time_,
                   kFieldDetectionToTickleLatencyHistogram);
}

}  // namespace autofill
