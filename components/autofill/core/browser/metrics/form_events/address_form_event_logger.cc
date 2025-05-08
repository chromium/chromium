// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_events/address_form_event_logger.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/levenshtein_distance.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
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
  // Once a `SuggestionType::kAutofillAddressOnTyping` suggestion
  // is accepted, we remove it from
  // `fields_where_autofill_on_typing_was_shown_`. Therefore for
  // the remaining fields, log that they were not accepted
  for (const auto& [field_global_id, field_types_used] :
       fields_where_autofill_on_typing_was_shown_) {
    base::UmaHistogramBoolean("Autofill.AddressSuggestionOnTypingAcceptance",
                              false);
    for (FieldType field_type : field_types_used) {
      base::UmaHistogramSparse(
          "Autofill.AddressSuggestionOnTypingAcceptance.PerFieldType",
          GetBucketForAcceptanceMetricsGroupedByFieldType(
              field_type, /*suggestion_accepted=*/false));
    }
  }

  // Log information about `SuggestionType::kAutofillAddressOnTyping`
  // suggestions and profile usage.
  for (auto [guid, last_used_time] :
       autofill_on_typing_suggestion_profile_last_used_time_per_guid_) {
    base::UmaHistogramCounts1000(
        "Autofill.AddressSuggestionOnTypingShown.DaysSinceLastUse.Profile",
        last_used_time.InDays());
  }

  for (const std::string& profile_accepted_guid :
       autofill_on_typing_suggestion_accepted_profile_used_) {
    base::UmaHistogramCounts1000(
        "Autofill.AddressSuggestionOnTypingAccepted.DaysSinceLastUse.Profile",
        autofill_on_typing_suggestion_profile_last_used_time_per_guid_
            [profile_accepted_guid]
                .InDays());
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

  FieldType field_type = field.Type().GetStorableType();
  field_types_with_shown_suggestions_.erase(field_type);
  field_types_with_accepted_suggestions_.insert(field_type);

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
    FieldGlobalId field_global_id,
    FieldTypeSet field_types_used,
    std::map<std::string, base::TimeDelta> profile_last_used_time_per_guid) {
  if (fields_where_autofill_on_typing_was_shown_.contains(field_global_id)) {
    fields_where_autofill_on_typing_was_shown_[field_global_id].insert_all(
        field_types_used);
  } else {
    fields_where_autofill_on_typing_was_shown_[field_global_id] =
        field_types_used;
  }
  for (auto [guid, last_used_time] : profile_last_used_time_per_guid) {
    autofill_on_typing_suggestion_profile_last_used_time_per_guid_[guid] =
        last_used_time;
  }
}

void AddressFormEventLogger::OnDidAcceptAutofillOnTyping(
    FieldGlobalId field_global_id,
    const std::u16string& value,
    FieldType field_type_used_to_build_suggestion,
    const std::string profile_used_guid) {
  if (value.empty()) {
    return;
  }

  CHECK(fields_where_autofill_on_typing_was_shown_.contains(field_global_id));
  CHECK(autofill_on_typing_suggestion_profile_last_used_time_per_guid_.contains(
      profile_used_guid));
  autofill_on_typing_value_used_[field_global_id] = value;
  base::UmaHistogramBoolean("Autofill.AddressSuggestionOnTypingAcceptance",
                            true);
  for (FieldType field_type :
       fields_where_autofill_on_typing_was_shown_[field_global_id]) {
    base::UmaHistogramSparse(
        "Autofill.AddressSuggestionOnTypingAcceptance.PerFieldType",
        GetBucketForAcceptanceMetricsGroupedByFieldType(
            field_type, /*suggestion_accepted=*/field_type ==
                            field_type_used_to_build_suggestion));
  }
  // Stores the accepted profile and log on destruction as a way to avoid
  // logging acceptance multiple times for the same profile.
  autofill_on_typing_suggestion_accepted_profile_used_.insert(
      profile_used_guid);
  fields_where_autofill_on_typing_was_shown_.erase(field_global_id);
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

void AddressFormEventLogger::LogAutofillAddressOnTypingCorrectnessMetrics(
    const FormStructure& form) {
  const std::vector<std::unique_ptr<AutofillField>>& submitted_form_fields =
      form.fields();

  // For each field in the submitted form, record its value.
  auto submitted_fields_values =
      base::MakeFlatMap<FieldGlobalId, std::u16string>(
          submitted_form_fields, {},
          [](const std::unique_ptr<AutofillField>& field) {
            return std::make_pair(field->global_id(), field->value());
          });
  // Used to delete fields for which correctness was logged from
  // `autofill_on_typing_value_used_`.
  std::set<FieldGlobalId> logged_correctness_for_field;
  for (const auto& [field_global_id, filled_value] :
       autofill_on_typing_value_used_) {
    if (submitted_fields_values.contains(field_global_id)) {
      const std::u16string submitted_value =
          submitted_fields_values.at(field_global_id);
      base::UmaHistogramBoolean(
          "Autofill.EditedAutofilledFieldAtSubmission.AddressOnTyping",
          filled_value == submitted_fields_values.at(field_global_id));
      logged_correctness_for_field.insert(field_global_id);
      size_t filled_value_and_submitted_value_distance =
          base::LevenshteinDistance(filled_value, submitted_value);
      base::UmaHistogramCounts100(
          "Autofill.EditedDistanceAutofilledFieldAtSubmission.AddressOnTyping",
          filled_value_and_submitted_value_distance);

      int edited_percentage = 100 *
                                filled_value_and_submitted_value_distance /
                                filled_value.length();
      base::UmaHistogramCounts100(
          "Autofill.EditedPercentageAutofilledFieldAtSubmission."
          "AddressOnTyping",
          edited_percentage);
    }
  }

  // Remove from `autofill_on_typing_value_used_` fields for which correctness
  // metrics were logged.
  for (const FieldGlobalId field : logged_correctness_for_field) {
    autofill_on_typing_value_used_.erase(field);
  }
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
