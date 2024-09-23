// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_MANAGER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/single_field_form_filler.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace autofill {

class AutofillClient;
class PersonalDataManager;

// Per-profile IBAN Manager. This class handles IBAN-related functionality
// such as retrieving IBAN data from PersonalDataManager, managing IBAN
// suggestions, filling IBAN fields, and handling form submission data when
// there is an IBAN field present.
class IbanManager : public SingleFieldFormFiller, public KeyedService {
 public:
  // Initializes the instance with the given parameters. `personal_data_manager`
  // is a profile-scope data manager used to retrieve IBAN data from the
  // local autofill table.
  explicit IbanManager(PersonalDataManager* personal_data_manager);

  IbanManager(const IbanManager&) = delete;
  IbanManager& operator=(const IbanManager&) = delete;

  ~IbanManager() override;

  // SingleFieldFormFiller overrides:
  [[nodiscard]] bool OnGetSingleFieldSuggestions(
      const FormStructure* form_structure,
      const FormFieldData& field,
      const AutofillField* autofill_field,
      const AutofillClient& client,
      OnSuggestionsReturnedCallback on_suggestions_returned) override;
  void OnWillSubmitFormWithFields(const std::vector<FormFieldData>& fields,
                                  bool is_autocomplete_enabled) override {}
  void CancelPendingQueries() override {}
  void OnRemoveCurrentSingleFieldSuggestion(const std::u16string& field_name,
                                            const std::u16string& value,
                                            SuggestionType type) override {}
  void OnSingleFieldSuggestionSelected(const Suggestion& suggestion) override;

 private:
  // Records metrics related to the IBAN suggestions popup.
  class UmaRecorder {
   public:
    void OnIbanSuggestionsShown(FieldGlobalId field_global_id);
    void OnIbanSuggestionSelected(const Suggestion& suggestion);

   private:
    // The global id of the field that most recently had IBAN suggestions shown.
    FieldGlobalId most_recent_suggestions_shown_field_global_id_;

    // The global id of the field that most recently had an IBAN suggestion
    // selected.
    FieldGlobalId most_recent_suggestion_selected_field_global_id_;
  };

  // Filters the `ibans` based on the `field`'s value and returns the resulting
  // suggestions.
  std::vector<Suggestion> GetIbanSuggestions(std::vector<Iban> ibans,
                                             const FormFieldData& field);

  // Filter out IBAN-based suggestions based on the following criteria:
  // For local IBANs: Filter out the IBAN value which does not starts with the
  // provided `field_value`.
  // For server IBANs: Filter out IBAN suggestion if any of the following
  // conditions are satisfied:
  // 1. If the IBAN's `prefix` is absent and the length of the `field_value` is
  // less than `kFieldLengthLimitOnServerIbanSuggestion` characters.
  // 2. If the IBAN's prefix is present and prefix matches the `field_value`.
  void FilterIbansToSuggest(const std::u16string& field_value,
                            std::vector<Iban>& ibans);

  const raw_ptr<PersonalDataManager> personal_data_manager_;

  UmaRecorder uma_recorder_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_MANAGER_H_
