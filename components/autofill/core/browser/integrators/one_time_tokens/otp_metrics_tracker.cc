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
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"

namespace autofill {

namespace {

// Returns true if a correlated previous event was found and metrics were
// recorded.
bool TryRecordTickleMetrics(std::optional<base::TimeTicks>& previous_event_time,
                            std::optional<base::TimeTicks>& current_event_time,
                            std::string_view latency_histogram_name,
                            one_time_tokens::TickleArrival arrival_type) {
  base::TimeTicks now = base::TimeTicks::Now();
  if (previous_event_time.has_value()) {
    base::TimeDelta latency = now - *previous_event_time;
    previous_event_time.reset();
    if (latency <= OtpMetricsTracker::kFieldDetectionTimeout) {
      // `kNotificationExpirationDuration` is currently smaller than
      // `kFieldDetectionTimeout`, but `std::min` is used in case either value
      // changes in the future.
      base::UmaHistogramCustomTimes(
          latency_histogram_name, latency, base::Milliseconds(10),
          std::min(OtpMetricsTracker::kFieldDetectionTimeout,
                   one_time_tokens::kNotificationExpirationDuration),
          50);
      base::UmaHistogramEnumeration(one_time_tokens::kTickleArrivalHistogram,
                                    arrival_type);
      return true;
    }
  }
  current_event_time = now;
  return false;
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

void OtpMetricsTracker::OnOtpFieldDetected(
    FormGlobalId form_id,
    std::vector<FieldGlobalId> field_ids,
    base::WeakPtr<AutofillManager> autofill_manager) {
  if (!base::FeatureList::IsEnabled(features::kAutofillGmailOtp)) {
    return;
  }
  // Ignore subsequent detections for a form that has already completed its
  // outcome lifecycle (e.g. tickle matched or timeout elapsed).
  if (last_handled_form_id_.has_value() && *last_handled_form_id_ == form_id) {
    return;
  }
  tickle_timeout_timer_.Stop();
  form_id_ = form_id;
  field_ids_ = std::move(field_ids);
  autofill_manager_ = std::move(autofill_manager);

  bool matched_existing_tickle = TryRecordTickleMetrics(
      tickle_time_, field_detection_time_,
      kTickleToFieldDetectionLatencyHistogram,
      one_time_tokens::TickleArrival::kBeforeFieldDetection);

  if (matched_existing_tickle) {
    // Tickle arrived before the form loaded (pre-arrival), so it arrived
    // before any user interaction on this form.
    base::UmaHistogramEnumeration(
        one_time_tokens::kTickleFormOutcomeHistogram,
        one_time_tokens::TickleFormOutcome::kTickleBeforeUserInteraction);
    form_outcome_timeout_timer_.Stop();
    ResetPendingFormState();
  } else if (!form_outcome_timeout_timer_.IsRunning()) {
    form_outcome_timeout_timer_.Start(
        FROM_HERE, one_time_tokens::kNotificationExpirationDuration,
        base::BindOnce(&OtpMetricsTracker::OnFormOutcomeTimeout,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void OtpMetricsTracker::OnTickleReceived(
    one_time_tokens::OneTimeTokenSource source) {
  if (!base::FeatureList::IsEnabled(features::kAutofillGmailOtp)) {
    return;
  }
  bool matched_existing_field = TryRecordTickleMetrics(
      field_detection_time_, tickle_time_,
      kFieldDetectionToTickleLatencyHistogram,
      one_time_tokens::TickleArrival::kAfterFieldDetection);

  if (matched_existing_field) {
    if (form_outcome_timeout_timer_.IsRunning()) {
      form_outcome_timeout_timer_.Stop();
      if (IsOtpFieldEmptyAndUnedited()) {
        base::UmaHistogramEnumeration(
            one_time_tokens::kTickleFormOutcomeHistogram,
            one_time_tokens::TickleFormOutcome::kTickleBeforeUserInteraction);
      } else {
        // If the field contains user input, the form is no longer found in the
        // cache, or the AutofillManager/frame was destroyed (e.g. the user
        // manually typed and submitted the form, or navigated away), the tickle
        // arrived too late to assist the user.
        base::UmaHistogramEnumeration(
            one_time_tokens::kTickleFormOutcomeHistogram,
            one_time_tokens::TickleFormOutcome::kTickleAfterUserInteraction);
      }
      ResetPendingFormState();
    }
  } else {
    // If no OTP field was detected yet (or the previous field timed out), start
    // a timer to record `kWithoutFieldDetection` if no field appears before
    // expiration.
    tickle_timeout_timer_.Start(
        FROM_HERE, one_time_tokens::kNotificationExpirationDuration,
        base::BindOnce(&OtpMetricsTracker::OnTickleTimeout,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void OtpMetricsTracker::OnTickleTimeout() {
  base::UmaHistogramEnumeration(
      one_time_tokens::kTickleArrivalHistogram,
      one_time_tokens::TickleArrival::kWithoutFieldDetection);
  tickle_time_.reset();
}

void OtpMetricsTracker::OnFormOutcomeTimeout() {
  base::UmaHistogramEnumeration(
      one_time_tokens::kTickleFormOutcomeHistogram,
      one_time_tokens::TickleFormOutcome::kNoTickleReceived);
  ResetPendingFormState();
}

void OtpMetricsTracker::ResetPendingFormState() {
  last_handled_form_id_ = form_id_;
  form_id_.reset();
  field_ids_.clear();
  autofill_manager_.reset();
}

bool OtpMetricsTracker::IsOtpFieldEmptyAndUnedited() const {
  if (!autofill_manager_ || !form_id_.has_value() || field_ids_.empty()) {
    return false;
  }
  const FormStructure* form = autofill_manager_->FindCachedFormById(*form_id_);
  if (!form) {
    return false;
  }
  for (FieldGlobalId field_id : field_ids_) {
    const AutofillField* field = form->GetFieldById(field_id);
    if (!field || !field->value().empty() ||
        field->all_modifiers().contains(FieldModifier::kUser)) {
      return false;
    }
  }
  return true;
}

}  // namespace autofill
