// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/single_field_fillers/single_field_fill_router.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace autofill {

class AutofillClient;
class PaymentsDataManager;

// Per-tab IBAN Manager. This class handles IBAN-related functionality
// such as retrieving IBAN data from PersonalDataManager, managing IBAN
// suggestions, filling IBAN fields, and handling form submission data when
// there is an IBAN field present.
class IbanManager {
 public:
  // Initializes the instance with the given parameters. `payments_data_manager`
  // is a profile-scope data manager used to retrieve IBAN data from the
  // local autofill table.
  explicit IbanManager(PaymentsDataManager* payments_data_manager);

  IbanManager(const IbanManager&) = delete;
  IbanManager& operator=(const IbanManager&) = delete;

  virtual ~IbanManager() = default;

  // Log IBAN form filled metrics.
  void LogIbanFormFilled();

  // Log the form has been submitted metrics if the IBAN has been filled.
  void OnWillSubmitFormWithFields();

  // May generate IBAN suggestions for the given `autofill_field` in `form`.
  // If `OnGetSingleFieldSuggestions` decides to claim the opportunity to fill
  // `field`, it returns true and calls `on_suggestions_returned`.
  // Claiming the opportunity is not a promise that suggestions will be available.
  // The callback may be called with no suggestions.
  // TODO(crbug.com/409962888): Remove once the migration to
  // `SuggestionGenerator`s is complete.
  [[nodiscard]] virtual bool OnGetSingleFieldSuggestions(
      const FormStructure& form,
      const FormFieldData& field,
      const AutofillField& autofill_field,
      const AutofillClient& client,
      SingleFieldFillRouter::OnSuggestionsReturnedCallback&
          on_suggestions_returned);
  virtual void OnSingleFieldSuggestionSelected(const Suggestion& suggestion);

  // Called when IBAN suggestions are shown; used to record metrics.
  // `field_global_id` is the global id of the field that had suggestions shown.
  void OnIbanSuggestionsShown(FieldGlobalId field_global_id);

 private:
  friend class IbanManagerTestApi;

  // Records metrics related to the IBAN suggestions popup.
  class UmaRecorder {
   public:
    void OnIbanSuggestionsShown(FieldGlobalId field_global_id);
    void OnIbanSuggestionSelected(const Suggestion& suggestion);

   private:
    friend class IbanManagerTestApi;

    // The global id of the field that most recently had IBAN suggestions shown.
    FieldGlobalId most_recent_suggestions_shown_field_global_id_;

    // The global id of the field that most recently had an IBAN suggestion
    // selected.
    FieldGlobalId most_recent_suggestion_selected_field_global_id_;
  };

  // The current status of the IBAN autofill flow. This is used for logging
  // purpose to ensure the same logging won't happen twice.
  enum class SuggestionStatus {
    kNotShown = 0,
    kShown = 1,
    kSelected = 2,
    kFilled = 3,
    kFormSubmitted = 4,
    kMaxValue = kFormSubmitted,
  };

  const raw_ptr<PaymentsDataManager> payments_data_manager_;

  UmaRecorder uma_recorder_;

  SuggestionStatus suggestion_status_ = SuggestionStatus::kNotShown;

  // If true, the selected IBAN suggestion is a local IBAN. This value
  // is set upon the first selection; it will not be updated if the user
  // re-triggers the suggestions and chooses a different IBAN type.
  bool is_local_iban_suggestion_selected_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_MANAGER_H_
