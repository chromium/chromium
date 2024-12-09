// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_events/address_form_event_logger.h"

#include <algorithm>
#include <iterator>
#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill::autofill_metrics {

namespace {

// Converts a set of `AutofillProfileRecordTypeCategory` to the corresponding
// `CategoryResolvedKeyMetricBucket`.
CategoryResolvedKeyMetricBucket ProfileCategoriesToMetricBucket(
    DenseSet<AutofillProfileRecordTypeCategory> categories) {
  if (categories.empty()) {
    return CategoryResolvedKeyMetricBucket::kNone;
  }
  if (categories.size() > 1) {
    return CategoryResolvedKeyMetricBucket::kMixed;
  }
  switch (*categories.begin()) {
    case AutofillProfileRecordTypeCategory::kLocalOrSyncable:
      return CategoryResolvedKeyMetricBucket::kLocalOrSyncable;
    case AutofillProfileRecordTypeCategory::kAccountChrome:
      return CategoryResolvedKeyMetricBucket::kAccountChrome;
    case AutofillProfileRecordTypeCategory::kAccountNonChrome:
      return CategoryResolvedKeyMetricBucket::kAccountNonChrome;
  }
}

}  // namespace

AddressFormEventLogger::AddressFormEventLogger(BrowserAutofillManager* owner)
    : FormEventLoggerBase("Address", owner) {}

AddressFormEventLogger::~AddressFormEventLogger() {
  for (const auto& [field_global_id, state] :
       fields_where_autofill_on_typing_was_shown_) {
    base::UmaHistogramBoolean(
        "Autofill.AddressSuggestionOnTypingAcceptance",
        state == AutofillOnTypingSuggestionState::kAccepted);
  }
}

void AddressFormEventLogger::UpdateProfileAvailabilityForReadiness(
    const std::vector<const AutofillProfile*>& profiles) {
  record_type_count_ = profiles.size();
  profile_categories_available_.clear();
  for (const AutofillProfile* profile : profiles) {
    profile_categories_available_.insert(GetCategoryOfProfile(*profile));
  }
}

void AddressFormEventLogger::OnDidFillFormFillingSuggestion(
    const AutofillProfile& profile,
    const FormStructure& form,
    const AutofillField& field,
    const AutofillTriggerSource trigger_source) {
  client().GetFormInteractionsUkmLogger().LogDidFillSuggestion(
      driver().GetPageUkmSourceId(), form, field);
  Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED, form);
  if (!has_logged_form_filling_suggestion_filled_) {
    has_logged_form_filling_suggestion_filled_ = true;
    Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, form);
  }
  base::RecordAction(
      base::UserMetricsAction("Autofill_FilledProfileSuggestion"));

  if (trigger_source != AutofillTriggerSource::kFastCheckout) {
    ++form_interaction_counts_.autofill_fills;
  }
  UpdateFlowId();

  profile_categories_filled_.insert(GetCategoryOfProfile(profile));
}

void AddressFormEventLogger::OnDidUndoAutofill() {
  has_logged_undo_after_fill_ = true;
  base::RecordAction(base::UserMetricsAction("Autofill_UndoAddressAutofill"));
}

void AddressFormEventLogger::OnDidShownAutofillOnTyping(
    FieldGlobalId field_global_id) {
  fields_where_autofill_on_typing_was_shown_[field_global_id] =
      AutofillOnTypingSuggestionState::kShown;
}

void AddressFormEventLogger::OnDidAcceptAutofillOnTyping(
    FieldGlobalId field_global_id) {
  CHECK(fields_where_autofill_on_typing_was_shown_.contains(field_global_id));
  fields_where_autofill_on_typing_was_shown_[field_global_id] =
      AutofillOnTypingSuggestionState::kAccepted;
}

void AddressFormEventLogger::OnLog(const std::string& name,
                                   FormEvent event,
                                   const FormStructure& form) const {
  uint32_t groups = data_util::DetermineGroups(form);
  base::UmaHistogramEnumeration(
      name + data_util::GetSuffixForProfileFormType(groups), event,
      NUM_FORM_EVENTS);
  if (data_util::ContainsAddress(groups) &&
      (data_util::ContainsPhone(groups) || data_util::ContainsEmail(groups))) {
    base::UmaHistogramEnumeration(name + ".AddressPlusContact", event,
                                  NUM_FORM_EVENTS);
  }
}

void AddressFormEventLogger::RecordPollSuggestions() {
  base::RecordAction(
      base::UserMetricsAction("Autofill_PolledProfileSuggestions"));
}

void AddressFormEventLogger::RecordParseForm() {
  base::RecordAction(base::UserMetricsAction("Autofill_ParsedProfileForm"));
}

void AddressFormEventLogger::RecordShowSuggestions() {
  base::RecordAction(
      base::UserMetricsAction("Autofill_ShowedProfileSuggestions"));
}

void AddressFormEventLogger::RecordFillingReadiness(LogBuffer& logs) const {
  FormEventLoggerBase::RecordFillingReadiness(logs);
  base::UmaHistogramEnumeration(
      "Autofill.Leipzig.FillingReadinessCategory",
      ProfileCategoriesToMetricBucket(profile_categories_available_));
}

void AddressFormEventLogger::RecordFillingAssistance(LogBuffer& logs) const {
  FormEventLoggerBase::RecordFillingAssistance(logs);
  base::UmaHistogramEnumeration(
      "Autofill.Leipzig.FillingAssistanceCategory",
      ProfileCategoriesToMetricBucket(profile_categories_filled_));
}

void AddressFormEventLogger::RecordFillingCorrectness(LogBuffer& logs) const {
  FormEventLoggerBase::RecordFillingCorrectness(logs);
  // Non-empty because correctness is only logged when an Autofill
  // suggestion was accepted.
  DCHECK(!profile_categories_filled_.empty());
  const std::string kBucket =
      profile_categories_filled_.size() == 1
          ? GetProfileCategorySuffix(*profile_categories_filled_.begin())
          : "Mixed";
  base::UmaHistogramBoolean("Autofill.Leipzig.FillingCorrectness." + kBucket,
                            !has_logged_edited_autofilled_field_);
}

void AddressFormEventLogger::LogUkmInteractedWithForm(
    FormSignature form_signature) {
  // Address Autofill has deprecated the concept of server addresses.
  client().GetFormInteractionsUkmLogger().LogInteractedWithForm(
      driver().GetPageUkmSourceId(),
      /*is_for_credit_card=*/false, record_type_count_,
      /*server_record_type_count=*/0, form_signature);
}

bool AddressFormEventLogger::HasLoggedDataToFillAvailable() const {
  return record_type_count_ > 0;
}

DenseSet<FormTypeNameForLogging>
AddressFormEventLogger::GetSupportedFormTypeNamesForLogging() const {
  return {FormTypeNameForLogging::kAddressForm,
          FormTypeNameForLogging::kEmailOnlyForm,
          FormTypeNameForLogging::kPostalAddressForm};
}

DenseSet<FormTypeNameForLogging> AddressFormEventLogger::GetFormTypesForLogging(
    const FormStructure& form) const {
  return GetAddressFormTypesForLogging(form);
}

}  // namespace autofill::autofill_metrics
