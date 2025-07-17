// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_IMPL_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/save_and_fill_manager.h"

namespace autofill::payments {

class PaymentsAutofillClient;

// Owned by PaymentsAutofillClient. There is one instance of this class per Web
// Contents. This class manages the flow for the Save and Fill dialog.
class SaveAndFillManagerImpl : public SaveAndFillManager {
 public:
  explicit SaveAndFillManagerImpl(AutofillClient* autofill_client);
  SaveAndFillManagerImpl(const SaveAndFillManagerImpl& other) = delete;
  SaveAndFillManagerImpl& operator=(const SaveAndFillManagerImpl& other) =
      delete;
  ~SaveAndFillManagerImpl() override;

  // SaveAndFillManager:
  void OnDidAcceptCreditCardSaveAndFillSuggestion(
      FillCardCallback fill_card_callback) override;
  void OfferLocalSaveAndFill() override;
  void OnUserDidDecideOnLocalSave(
      PaymentsAutofillClient::CardSaveAndFillDialogUserDecision user_decision,
      const PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails&
          user_provided_card_save_and_fill_details) override;
  void PopulateCreditCardInfo(
      CreditCard& card,
      const PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails&
          user_provided_card_save_and_fill_details) override;

  PaymentsAutofillClient* payments_autofill_client() const {
    return autofill_client_->GetPaymentsAutofillClient();
  }

  void SetCreditCardUploadEnabledOverrideForTesting(
      bool credit_card_upload_enabled_override);

 private:
  // Whether all prerequisites for credit card uploading are met.
  bool IsCreditCardUploadEnabled() const;

  // Callback invoked when the response to fetch upload details is returned.
  void OnDidGetDetailsForCreateCard(
      PaymentsAutofillClient::PaymentsRpcResult result,
      const std::u16string& context_token,
      std::unique_ptr<base::Value::Dict> legal_message,
      std::vector<std::pair<int, int>> supported_card_bin_ranges);

  // If server upload is enabled, populate info to the `upload_details_` for
  // server communication.
  void PopulateInitialUploadDetails();

  // Reference to the AutofillClient. `autofill_client_` outlives `this`.
  const raw_ref<AutofillClient> autofill_client_;

  // Struct that contains necessary information for uploading the card to
  // server.
  payments::UploadCardRequestDetails upload_details_;

  FillCardCallback fill_card_callback_;

  base::WeakPtrFactory<SaveAndFillManagerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_IMPL_H_
