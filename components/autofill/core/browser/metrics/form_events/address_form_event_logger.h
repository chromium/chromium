// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_ADDRESS_FORM_EVENT_LOGGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_ADDRESS_FORM_EVENT_LOGGER_H_

#include <map>
#include <string>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
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

  void OnDidShownAutofillOnTyping(FieldGlobalId field_global_id);

  void OnDidAcceptAutofillOnTyping(FieldGlobalId field_global_id);

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
  enum class AutofillOnTypingSuggestionState {
    kShown,
    kAccepted,
  };

  // All profile categories for which the user has at least one profile stored.
  DenseSet<AutofillProfileRecordTypeCategory> profile_categories_available_;
  // All profile categories for which the user has accepted at least one
  // suggestion.
  DenseSet<AutofillProfileRecordTypeCategory> profile_categories_filled_;
  // For fields where `SuggestionType::kAddressEntryOnTyping`
  // suggestions were shown, defined whether the user accepted the suggestion.
  std::map<FieldGlobalId, AutofillOnTypingSuggestionState>
      fields_where_autofill_on_typing_was_shown_;
  size_t record_type_count_ = 0;
};

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_ADDRESS_FORM_EVENT_LOGGER_H_
