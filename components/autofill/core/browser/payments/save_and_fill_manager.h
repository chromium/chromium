// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_H_

namespace autofill::payments {

// Interface for managing the Save and Fill dialog flow.
class SaveAndFillManager {
 public:
  using CardSaveAndFillDialogUserDecision =
      PaymentsAutofillClient::CardSaveAndFillDialogUserDecision;
  using UserProvidedCardSaveAndFillDetails =
      PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails;
  using FillCardCallback = base::OnceCallback<void(const CreditCard&)>;

  SaveAndFillManager() = default;
  SaveAndFillManager(const SaveAndFillManager& other) = delete;
  SaveAndFillManager& operator=(const SaveAndFillManager& other) = delete;
  virtual ~SaveAndFillManager() = default;

  // Initiates the Save and Fill flow after the user accepts the Save and Fill
  // suggestion.
  virtual void OnDidAcceptCreditCardSaveAndFillSuggestion(
      FillCardCallback fill_card_callback) = 0;
  // Returns true if the maximum number of strikes has been reached.
  virtual bool IsMaxStrikesLimitReached() = 0;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_H_
