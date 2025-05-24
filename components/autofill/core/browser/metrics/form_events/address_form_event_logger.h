// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_ADDRESS_FORM_EVENT_LOGGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_ADDRESS_FORM_EVENT_LOGGER_H_

#include <map>
#include <set>
#include <string>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill::autofill_metrics {

// To measure the added value of kAccount profiles, the filling readiness and
// assistance metrics are split by profile category.
// Even for assistance, the `kMixed` case is possible, since the metric is
// emitted at navigation (rather than filling) time.
enum class CategoryResolvedKeyMetricBucket {
  kNone = 0,
  kLocalOrSyncable = 1,
  kAccountChrome = 2,
  kAccountNonChrome = 3,
  kMixed = 4,
  kMaxValue = kMixed
};

class AddressFormEventLogger : public FormEventLoggerBase {
 public:
  explicit AddressFormEventLogger(BrowserAutofillManager* owner);

  ~AddressFormEventLogger() override;

  void UpdateProfileAvailabilityForReadiness(
      const std::vector<const AutofillProfile*>& profiles);

  void OnDidFillFormFillingSuggestion(
      const AutofillProfile& profile,
      const FormStructure& form,
      const AutofillField& field,
      const AutofillTriggerSource trigger_source);

  void OnDidUndoAutofill();

  // `field_global_id` is the id of the field where at least one
  // `SuggestionType::kAddressEntryOnTyping` suggestion was shown.
  // `field_types_used` specifies the `FieldType` used to build each suggestion.
  // For the profiles used to build the shown suggestions,
  // `profile_last_used_time_per_guid` specifies the last time each of the
  // profiles was used.
  void OnDidShownAutofillOnTyping(
      FieldGlobalId field_global_id,
      FieldTypeSet field_types_used,
      std::map<std::string, base::TimeDelta> profile_last_used_time_per_guid);

  // `field_global_id` is the id of the field where a
  // `SuggestionType::kAddressEntryOnTyping` was accepted. `value` is the the
  // literal string used to fill the field.
  // `field_type_used_to_build_suggestion` is the autofill `FieldType` from
  // which `value` was derived from.
  // `profile_used_guid` specifies the profile used to build
  // the accepted suggestion.
  void OnDidAcceptAutofillOnTyping(
      FieldGlobalId field_global_id,
      const std::u16string& value,
      FieldType field_type_used_to_build_suggestion,
      const std::string profile_used_guid);
  void LogAutofillAddressOnTypingCorrectnessMetrics(const FormStructure& form);

 protected:
  void RecordPollSuggestions() override;
  void RecordParseForm() override;
  void RecordShowSuggestions() override;
  void OnLog(const std::string& name,
             FormEvent event,
             const FormStructure& form) const override;

  // Readiness, assistance and correctness metrics resolved by profile category.
  void RecordFillingReadiness(LogBuffer& logs) const override;
  void RecordFillingAssistance(LogBuffer& logs) const override;
  void RecordFillingCorrectness(LogBuffer& logs) const override;

  void LogUkmInteractedWithForm(FormSignature form_signature) override;

  bool HasLoggedDataToFillAvailable() const override;
  DenseSet<FormTypeNameForLogging> GetSupportedFormTypeNamesForLogging()
      const override;
  DenseSet<FormTypeNameForLogging> GetFormTypesForLogging(
      const FormStructure& form) const override;

 private:
  // All profile categories for which the user has at least one profile stored.
  DenseSet<AutofillProfileRecordTypeCategory> profile_categories_available_;
  // All profile categories for which the user has accepted at least one
  // suggestion.
  DenseSet<AutofillProfileRecordTypeCategory> profile_categories_filled_;
  // For fields where `SuggestionType::kAddressEntryOnTyping`
  // suggestions were shown, store the `FieldTypeSet` used to build the
  // suggestions keyed by the field global identifier.
  std::map<FieldGlobalId, FieldTypeSet>
      fields_where_autofill_on_typing_was_shown_;
  // For profiles that were used to build
  // `SuggestionType::kAddressEntryOnTyping` suggestions, store their last usage
  // time keyed by the profile identifier.
  std::map<std::string, base::TimeDelta>
      autofill_on_typing_suggestion_profile_last_used_time_per_guid_;
  // Stores the identifiers of those profiles that were used to build
  // `SuggestionType::kAddressEntryOnTyping` suggestions and were later
  // accepted.
  std::set<std::string> autofill_on_typing_suggestion_accepted_profile_used_;
  // For fields where `SuggestionType::kAddressEntryOnTyping` suggestions were
  // accepted, stored the filled value. This is used later
  // for correctness metrics emission.
  std::map<FieldGlobalId, std::u16string> autofill_on_typing_value_used_;

  size_t record_type_count_ = 0;
};

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_ADDRESS_FORM_EVENT_LOGGER_H_
