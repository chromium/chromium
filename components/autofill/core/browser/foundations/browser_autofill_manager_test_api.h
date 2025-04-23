// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_BROWSER_AUTOFILL_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_BROWSER_AUTOFILL_MANAGER_TEST_API_H_

#include <optional>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/filling/form_filler_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/payments/amount_extraction_manager.h"
#include "components/autofill/core/browser/payments/bnpl_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/single_field_fillers/single_field_fill_router.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Exposes some testing operations for BrowserAutofillManager.
class BrowserAutofillManagerTestApi : public AutofillManagerTestApi {
 public:
  explicit BrowserAutofillManagerTestApi(BrowserAutofillManager* manager)
      : AutofillManagerTestApi(manager), manager_(*manager) {}

  void SetExternalDelegate(
      std::unique_ptr<AutofillExternalDelegate> external_delegate) {
    manager_->external_delegate_ = std::move(external_delegate);
  }

  AutofillExternalDelegate* external_delegate() {
    return manager_->external_delegate_.get();
  }

  FormInteractionsFlowId address_form_interactions_flow_id() const {
    return manager_->metrics_->address_form_event_logger
        .form_interactions_flow_id_for_test();
  }

  autofill_metrics::CreditCardFormEventLogger* credit_card_form_event_logger() {
    return &manager_->metrics_->credit_card_form_event_logger;
  }

  void set_credit_card_access_manager(
      std::unique_ptr<CreditCardAccessManager> manager) {
    manager_->credit_card_access_manager_ = std::move(manager);
  }

  void set_amount_extraction_manager(
      std::unique_ptr<payments::AmountExtractionManager> manager) {
    manager_->amount_extraction_manager_ = std::move(manager);
  }

  void set_bnpl_manager(std::unique_ptr<payments::BnplManager> bnpl_manager) {
    manager_->bnpl_manager_ = std::move(bnpl_manager);
  }

  payments::AmountExtractionManager&
  get_amount_extraction_manager_for_testing() {
    return *manager_->amount_extraction_manager_;
  }

  void OnFormProcessed(const FormData& form,
                       const FormStructure& form_structure) {
    manager_->OnFormProcessed(form, form_structure);
  }

  void SetFourDigitCombinationsInDOM(
      const std::vector<std::string>& combinations) {
    manager_->four_digit_combinations_in_dom_ = combinations;
  }

  void SetConsiderFormAsSecureForTesting(
      std::optional<bool> consider_form_as_secure_for_testing) {
    manager_->consider_form_as_secure_for_testing_ =
        consider_form_as_secure_for_testing;
  }

  FormFiller& form_filler() { return *manager_->form_filler_; }

  void set_form_filler(std::unique_ptr<FormFiller> form_filler) {
    manager_->form_filler_ = std::move(form_filler);
  }

  std::vector<Suggestion> GetProfileSuggestions(
      const FormData& form,
      const FormFieldData& field,
      AutofillSuggestionTriggerSource trigger_source =
          AutofillSuggestionTriggerSource::kFormControlElementClicked,
      std::optional<std::string> plus_address_override = std::nullopt) {
    FormStructure* form_structure;
    AutofillField* autofill_field;
    CHECK(manager_->GetCachedFormAndField(form.global_id(), field.global_id(),
                                          &form_structure, &autofill_field));
    return manager_->GetProfileSuggestions(
        form, CHECK_DEREF(form_structure), field, CHECK_DEREF(autofill_field),
        trigger_source, std::move(plus_address_override));
  }

 private:
  raw_ref<BrowserAutofillManager> manager_;
};

inline BrowserAutofillManagerTestApi test_api(BrowserAutofillManager& manager) {
  return BrowserAutofillManagerTestApi(&manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_BROWSER_AUTOFILL_MANAGER_TEST_API_H_
