// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_manager_test_api.h"
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/single_field_form_fill_router.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Exposes some testing operations for BrowserAutofillManager.
class BrowserAutofillManagerTestApi : public AutofillManagerTestApi {
 public:
  explicit BrowserAutofillManagerTestApi(BrowserAutofillManager* manager)
      : AutofillManagerTestApi(manager), manager_(*manager) {}

  // Blocks until all pending votes have been emitted. This fails if either a
  // timeout is hit or if the BrowserAutofillManager::vote_upload_task_runner_
  // has not been initialized yet.
  [[nodiscard]] testing::AssertionResult FlushPendingVotes(
      base::TimeDelta timeout = base::Seconds(10));

  void SetExternalDelegate(
      std::unique_ptr<AutofillExternalDelegate> external_delegate) {
    manager_->external_delegate_ = std::move(external_delegate);
  }

  AutofillExternalDelegate* external_delegate() {
    return manager_->external_delegate_.get();
  }

  void set_limit_before_refill(base::TimeDelta limit) {
    manager_->form_filler_->limit_before_refill_ = limit;
  }

  // TODO(crbug.com/1517894): Remove.
  bool ShouldTriggerRefill(const FormStructure& form_structure,
                           RefillTriggerReason refill_trigger_reason) {
    return manager_->form_filler_->ShouldTriggerRefill(form_structure,
                                                       refill_trigger_reason);
  }

  // TODO(crbug.com/1517894): Remove.
  void TriggerRefill(const FormData& form,
                     const AutofillTriggerDetails& trigger_details) {
    manager_->form_filler_->TriggerRefill(form, trigger_details);
  }

  void PreProcessStateMatchingTypes(
      const std::vector<AutofillProfile>& profiles,
      FormStructure* form_structure) {
    manager_->PreProcessStateMatchingTypes(profiles, form_structure);
  }

  AutofillSuggestionGenerator* suggestion_generator() {
    return manager_->suggestion_generator_.get();
  }

  FormInteractionsFlowId address_form_interactions_flow_id() const {
    return manager_->address_form_event_logger_
        ->form_interactions_flow_id_for_test();
  }

  SingleFieldFormFillRouter& single_field_form_fill_router() {
    return *manager_->single_field_form_fill_router_;
  }

  autofill_metrics::CreditCardFormEventLogger* credit_card_form_event_logger() {
    return manager_->credit_card_form_event_logger_.get();
  }

  void set_single_field_form_fill_router(
      std::unique_ptr<SingleFieldFormFillRouter> router) {
    manager_->single_field_form_fill_router_ = std::move(router);
  }

  void set_credit_card_access_manager(
      std::unique_ptr<CreditCardAccessManager> manager) {
    manager_->credit_card_access_manager_ = std::move(manager);
  }

  void OnCreditCardFetched(CreditCardFetchResult result,
                           const CreditCard* credit_card = nullptr) {
    manager_->OnCreditCardFetched(result, credit_card);
  }

  // TODO(crbug.com/1517894): Remove.
  void FillOrPreviewDataModelForm(
      mojom::ActionPersistence action_persistence,
      const FormData& form,
      const FormFieldData& field,
      absl::variant<const AutofillProfile*, const CreditCard*>
          profile_or_credit_card,
      base::optional_ref<const std::u16string> cvc,
      FormStructure* form_structure,
      AutofillField* autofill_field) {
    return manager_->form_filler_->FillOrPreviewForm(
        action_persistence, form, field, profile_or_credit_card, cvc,
        form_structure, autofill_field,
        {.trigger_source = AutofillTriggerSource::kPopup});
  }

  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
  GetVirtualCreditCardsForStandaloneCvcField(const url::Origin& origin) {
    return manager_->GetVirtualCreditCardsForStandaloneCvcField(origin);
  }

  FormData* pending_form_data() { return manager_->pending_form_data_.get(); }

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

  // TODO(crbug.com/1517894): Remove.
  void AddFormFillEntry(
      base::span<const FormFieldData* const> filled_fields,
      base::span<const AutofillField* const> filled_autofill_fields,
      FillingProduct filling_product,
      bool is_refill) {
    manager_->form_filler_->form_autofill_history_.AddFormFillEntry(
        filled_fields, filled_autofill_fields, filling_product, is_refill);
  }

  void set_form_filler(std::unique_ptr<FormFiller> form_filler) {
    manager_->form_filler_ = std::move(form_filler);
  }

  std::vector<Suggestion> GetProfileSuggestions(
      const FormData& form,
      const FormFieldData& field,
      AutofillSuggestionTriggerSource trigger_source =
          AutofillSuggestionTriggerSource::kFormControlElementClicked) {
    FormStructure* form_structure;
    AutofillField* autofill_field;
    CHECK(manager_->GetCachedFormAndField(form, field, &form_structure,
                                          &autofill_field));
    return manager_->GetProfileSuggestions(form, form_structure, field,
                                           autofill_field, trigger_source);
  }

 private:
  raw_ref<BrowserAutofillManager> manager_;
};

inline BrowserAutofillManagerTestApi test_api(BrowserAutofillManager& manager) {
  return BrowserAutofillManagerTestApi(&manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_TEST_API_H_
