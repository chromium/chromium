// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_SAVE_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_SAVE_MANAGER_TEST_API_H_

#include "components/autofill/core/browser/payments/iban_save_manager.h"

namespace autofill {

class IbanSaveManagerTestApi {
 public:
  explicit IbanSaveManagerTestApi(IbanSaveManager& iban_save_manager)
      : iban_save_manager_(iban_save_manager) {}

  void OnUserDidDecideOnLocalSave(
      const Iban& import_candidate,
      payments::PaymentsAutofillClient::SaveIbanOfferUserDecision user_decision,
      std::u16string_view nickname = u"") {
    iban_save_manager_->OnUserDidDecideOnLocalSave(import_candidate,
                                                   user_decision, nickname);
  }

  void OnUserDidDecideOnUploadSave(
      const Iban& import_candidate,
      bool show_save_prompt,
      payments::PaymentsAutofillClient::SaveIbanOfferUserDecision user_decision,
      std::u16string_view nickname = u"") {
    iban_save_manager_->OnUserDidDecideOnUploadSave(
        import_candidate, show_save_prompt, user_decision, nickname);
  }

  IbanSaveStrikeDatabase* GetIbanSaveStrikeDatabase() {
    return iban_save_manager_->GetIbanSaveStrikeDatabase();
  }

  void SetEventObserverForTesting(IbanSaveManager::ObserverForTest* observer) {
    iban_save_manager_->observer_for_testing_ = observer;
  }

  bool AttemptToOfferLocalSave(Iban& iban) {
    return iban_save_manager_->AttemptToOfferLocalSave(iban);
  }

  bool AttemptToOfferUploadSave(Iban& iban) {
    return iban_save_manager_->AttemptToOfferUploadSave(iban);
  }

  IbanSaveManager::TypeOfOfferToSave DetermineHowToSaveIban(
      const Iban& import_candidate) {
    return iban_save_manager_->DetermineHowToSaveIban(import_candidate);
  }

  void OnDidUploadIban(
      const Iban& import_candidate,
      bool show_save_prompt,
      payments::PaymentsAutofillClient::PaymentsRpcResult result) {
    iban_save_manager_->OnDidUploadIban(import_candidate, show_save_prompt,
                                        result);
  }

  bool HasContextToken() const {
    return !iban_save_manager_->context_token_.empty();
  }

 private:
  const raw_ref<IbanSaveManager> iban_save_manager_;
};

inline IbanSaveManagerTestApi test_api(IbanSaveManager& iban_save_manager) {
  return IbanSaveManagerTestApi(iban_save_manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_SAVE_MANAGER_TEST_API_H_
