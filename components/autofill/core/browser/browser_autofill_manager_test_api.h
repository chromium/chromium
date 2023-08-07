// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_manager_test_api.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Exposes some testing operations for BrowserAutofillManager.
class BrowserAutofillManagerTestApi : public AutofillManagerTestApi {
 public:
  static void DeterminePossibleFieldTypesForUpload(
      const std::vector<AutofillProfile>& profiles,
      const std::vector<CreditCard>& credit_cards,
      const std::u16string& last_unlocked_credit_card_cvc,
      const std::string& app_locale,
      FormStructure* form) {
    // For tests, the observed_submission is hardcoded to true.
    BrowserAutofillManager::DeterminePossibleFieldTypesForUpload(
        profiles, credit_cards, last_unlocked_credit_card_cvc, app_locale,
        /*observed_submission=*/true, form);
  }

  explicit BrowserAutofillManagerTestApi(BrowserAutofillManager* manager)
      : AutofillManagerTestApi(manager), manager_(*manager) {}

  // Blocks until all pending votes have been emitted. This fails if either a
  // timeout is hit or if the BrowserAutofillManager::vote_upload_task_runner_
  // has not been initialized yet.
  [[nodiscard]] testing::AssertionResult FlushPendingVotes(
      base::TimeDelta timeout = base::Seconds(10));

  const std::vector<autofill::AutofillProfile>& test_addresses() {
    return manager_->test_addresses_;
  }

  void SetExternalDelegate(
      std::unique_ptr<AutofillExternalDelegate> external_delegate) {
    manager_->external_delegate_ = std::move(external_delegate);
  }

  AutofillExternalDelegate* external_delegate() {
    return manager_->external_delegate_.get();
  }

  bool ShouldTriggerRefill(const FormStructure& form_structure) {
    return manager_->ShouldTriggerRefill(form_structure);
  }

  void TriggerRefill(const FormData& form,
                     const AutofillTriggerSource trigger_source) {
    manager_->TriggerRefill(form, trigger_source);
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

  void set_single_field_form_fill_router(
      std::unique_ptr<SingleFieldFormFillRouter> router) {
    manager_->single_field_form_fill_router_ = std::move(router);
  }

  void set_credit_card_access_manager(
      std::unique_ptr<CreditCardAccessManager> manager) {
    manager_->credit_card_access_manager_ = std::move(manager);
  }

  void OnCreditCardFetched(CreditCardFetchResult result,
                           const CreditCard* credit_card = nullptr,
                           const std::u16string& cvc = std::u16string()) {
    manager_->OnCreditCardFetched(result, credit_card, cvc);
  }

  bool WillFillCreditCardNumber(const FormData& form,
                                const FormFieldData& field) {
    return manager_->WillFillCreditCardNumber(form, field);
  }

  void FillOrPreviewDataModelForm(
      mojom::AutofillActionPersistence action_persistence,
      const FormData& form,
      const FormFieldData& field,
      absl::variant<const AutofillProfile*, const CreditCard*>
          profile_or_credit_card,
      const std::u16string* optional_cvc,
      FormStructure* form_structure,
      AutofillField* autofill_field) {
    return manager_->FillOrPreviewDataModelForm(
        action_persistence, form, field, profile_or_credit_card, optional_cvc,
        form_structure, autofill_field, AutofillTriggerSource::kPopup);
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

 private:
  raw_ref<BrowserAutofillManager> manager_;
};

inline BrowserAutofillManagerTestApi test_api(BrowserAutofillManager& manager) {
  return BrowserAutofillManagerTestApi(&manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_TEST_API_H_
