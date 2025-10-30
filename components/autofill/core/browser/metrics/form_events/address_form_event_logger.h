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

// A superset of `AutofillProfileRecordTypeCategory` that additionally contains
// a value `kMixed`. It is used to measure key metrics, broken down by the
// `AutofillProfileRecordTypeCategory`. This helps to answer questions like,
// "what is the acceptance of local addresses?" or "what percentage of users are
// only ready because of home and work addresses?".
// Since key metrics are only emitted on navigation, it is possible that users
// are ready/were assisted/accepted profiles of different record type
// categories. This case is represented by the kMixed enum value.
enum class CategoryResolvedKeyMetricBucket {
  kNone = 0,
  kLocalOrSyncable = 1,
  kAccountChrome = 2,
  kAccountNonChrome = 3,
  kMixed = 4,
  kAccountHome = 5,
  kAccountWork = 6,
  kAccountNameEmail = 7,
  kMaxValue = kAccountNameEmail
};

class AddressFormEventLogger : public FormEventLoggerBase {
 public:
  explicit AddressFormEventLogger(BrowserAutofillManager* owner);

  ~AddressFormEventLogger() override;

  void UpdateProfileAvailabilityForReadiness(
      const std::vector<const AutofillProfile*>& profiles);

  void OnDidShowSuggestions(const FormStructure& form,
                            const AutofillField& field,
                            base::TimeTicks form_parsed_timestamp,
                            bool off_the_record,
                            base::span<const Suggestion> suggestions) override;

  void OnDidFillFormFillingSuggestion(
      const AutofillProfile& profile,
      const FormStructure& form,
      const AutofillField& field,
      const AutofillTriggerSource trigger_source);

  void OnDidUndoAutofill();

  void OnDestroyed() override;

 protected:
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

  size_t record_type_count_ = 0;
  bool home_profile_suggestion_present_ = false;
  bool work_profile_suggestion_present_ = false;
};
}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_ADDRESS_FORM_EVENT_LOGGER_H_
