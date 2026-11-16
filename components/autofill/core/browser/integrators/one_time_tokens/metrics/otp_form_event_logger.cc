// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/metrics/otp_form_event_logger.h"

#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_metrics_tracker.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_util.h"
#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/signatures.h"
#include "components/metrics/profile_metrics_service.h"

namespace autofill::autofill_metrics {

OtpFormEventLogger::OtpFormEventLogger(BrowserAutofillManager* owner)
    : FormEventLoggerBase("OneTimePassword", owner) {}

OtpFormEventLogger::~OtpFormEventLogger() = default;

void OtpFormEventLogger::OnDidShowSuggestions(
    const FormStructure& form,
    const AutofillField& field,
    base::TimeTicks form_parsed_timestamp,
    bool off_the_record,
    base::span<const Suggestion> suggestions) {
  CHECK(field.Type().GetTypes().contains(ONE_TIME_CODE));
  FormEventLoggerBase::OnDidShowSuggestions(form, field, ONE_TIME_CODE,
                                            form_parsed_timestamp,
                                            off_the_record, suggestions);
}

void OtpFormEventLogger::OnDestroyed() {
  if (!identified_form_types_.empty()) {
    is_eligible_for_gmail_otps_ = IsEligibleForGmailOtps();
  }
  FormEventLoggerBase::OnDestroyed();
}

void OtpFormEventLogger::OnDidFillOtpSuggestion(const FormStructure& form,
                                                const AutofillField& field) {
  Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED, form);
  if (!has_logged_form_filling_suggestion_filled_) {
    has_logged_form_filling_suggestion_filled_ = true;
    Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, form);
  }
  client().GetFormInteractionsUkmLogger().LogDidFillSuggestion(
      driver().GetPageUkmSourceId(), form, field, /*record_type=*/std::nullopt);
  ++form_interaction_counts_.autofill_fills;
}

void OtpFormEventLogger::RecordParseForm() {
  base::RecordAction(base::UserMetricsAction("Autofill_ParsedOtpForm"));
}

void OtpFormEventLogger::RecordShowSuggestions() {
  base::RecordAction(base::UserMetricsAction("Autofill_ShowedOtpSuggestions"));
}

bool OtpFormEventLogger::HasLoggedDataToFillAvailable() const {
  return otp_for_filling_existed_;
}

void OtpFormEventLogger::LogUkmInteractedWithForm(
    FormSignature form_signature) {
  client().GetFormInteractionsUkmLogger().LogInteractedWithForm(
      driver().GetPageUkmSourceId(),
      /*is_for_credit_card=*/false,
      /*local_record_type_count=*/otp_for_filling_existed_ ? 1 : 0,
      /*server_record_type_count=*/0, form_signature);
}

void OtpFormEventLogger::OnOtpAvailable() {
  otp_for_filling_existed_ = true;
}

DenseSet<FormTypeNameForLogging>
OtpFormEventLogger::GetSupportedFormTypeNamesForLogging() const {
  return {FormTypeNameForLogging::kOneTimePasswordForm};
}

DenseSet<FormTypeNameForLogging> OtpFormEventLogger::GetFormTypesForLogging(
    const FormStructure& form,
    AutocompleteUnrecognizedBehavior ac_unrecognized_behavior) const {
  return GetOneTimePasswordTypesForLogging(form, ac_unrecognized_behavior);
}

void OtpFormEventLogger::RecordInteractionAfterParsedAsType(
    LogBuffer& logs) const {
  FormEventLoggerBase::RecordInteractionAfterParsedAsType(logs);
  RecordGmailHistogram("Autofill.Funnel.InteractionAfterParsedAsType.",
                       has_logged_interacted_);
}

void OtpFormEventLogger::RecordSuggestionAfterInteraction(
    LogBuffer& logs) const {
  FormEventLoggerBase::RecordSuggestionAfterInteraction(logs);
  RecordGmailHistogram("Autofill.Funnel.SuggestionAfterInteraction.",
                       has_logged_suggestions_shown_);
}

void OtpFormEventLogger::RecordFillAfterSuggestion(LogBuffer& logs) const {
  FormEventLoggerBase::RecordFillAfterSuggestion(logs);
  RecordGmailHistogram("Autofill.Funnel.FillAfterSuggestion.",
                       has_logged_form_filling_suggestion_filled_);
}

void OtpFormEventLogger::RecordSubmissionAfterFill(LogBuffer& logs) const {
  FormEventLoggerBase::RecordSubmissionAfterFill(logs);
  RecordGmailHistogram("Autofill.Funnel.SubmissionAfterFill.",
                       has_logged_will_submit_);
}

void OtpFormEventLogger::RecordFormSubmission(LogBuffer& logs) const {
  FormEventLoggerBase::RecordFormSubmission(logs);
  RecordGmailHistogram(
      has_logged_form_filling_suggestion_filled_
          ? "Autofill.KeyMetrics.FormSubmission.Autofilled."
          : "Autofill.KeyMetrics.FormSubmission.NotAutofilled.",
      has_logged_will_submit_);
}

void OtpFormEventLogger::RecordFillingReadiness(LogBuffer& logs) const {
  FormEventLoggerBase::RecordFillingReadiness(logs);
  RecordGmailKeyMetric("FillingReadiness", HasLoggedDataToFillAvailable());
}

void OtpFormEventLogger::RecordFillingAcceptance(LogBuffer& logs) const {
  FormEventLoggerBase::RecordFillingAcceptance(logs);
  RecordGmailKeyMetric("FillingAcceptance",
                       has_logged_form_filling_suggestion_filled_);
}

void OtpFormEventLogger::RecordFillingCorrectness(LogBuffer& logs) const {
  FormEventLoggerBase::RecordFillingCorrectness(logs);
  RecordGmailKeyMetric("FillingCorrectness",
                       !has_logged_edited_autofilled_field_);
}

void OtpFormEventLogger::RecordFillingAssistance(LogBuffer& logs) const {
  FormEventLoggerBase::RecordFillingAssistance(logs);
  RecordGmailKeyMetric("FillingAssistance",
                       has_logged_form_filling_suggestion_filled_);
}

bool OtpFormEventLogger::IsEligibleForGmailOtps() const {
  const AutofillClient& client = owner_->client();
  const PrefService* prefs = client.GetPrefs();
  return base::FeatureList::IsEnabled(
             features::kAutofillShowGmailOtpSuggestions) &&
         prefs && prefs::IsAutofillGmailOtpFillingEnabled(prefs) &&
         client.GetGmailOtpBackend() &&
         OtpMetricsTracker::IsEligibleForGmailOtps(client.GetIdentityManager());
}

void OtpFormEventLogger::RecordGmailKeyMetric(std::string_view metric,
                                              bool value) const {
  if (!is_eligible_for_gmail_otps_) {
    return;
  }
  metrics::ProfileMetricsService* profile_metrics_service =
      owner_->client().GetProfileMetricsService();
  CHECK(profile_metrics_service);
  profile_metrics_service->UmaHistogramBoolean(
      base::StrCat({"Autofill.KeyMetrics.", metric,
                    ".OneTimePassword.GmailEligibleClients"}),
      value);
}

void OtpFormEventLogger::RecordGmailHistogram(std::string_view histogram_prefix,
                                              bool value) const {
  if (!is_eligible_for_gmail_otps_) {
    return;
  }
  base::UmaHistogramBoolean(
      base::StrCat({histogram_prefix, "OneTimePassword.GmailEligibleClients"}),
      value);
}

}  // namespace autofill::autofill_metrics
