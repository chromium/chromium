// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_H_

namespace autofill::payments {

// Interface for managing the Save and Fill dialog flow.
class SaveAndFillManager {
 public:
  SaveAndFillManager() = default;
  SaveAndFillManager(const SaveAndFillManager& other) = delete;
  SaveAndFillManager& operator=(const SaveAndFillManager& other) = delete;
  virtual ~SaveAndFillManager() = default;

  // Initiates the Save and Fill flow after the user accepts the Save and Fill
  // suggestion.
  virtual void OnDidAcceptCreditCardSaveAndFillSuggestion() = 0;
  // Begins the process to show the local Save and Fill dialog.
  virtual void OfferLocalSaveAndFill() = 0;
  // Called when the user makes a decision on the local Save and Fill dialog.
  // The `user_provided_card_save_and_fill_details` holds the  data entered by
  // the user in the Save and Fill dialog when the `user_decision` is
  // `kAccepted`.
  virtual void OnUserDidDecideOnLocalSave(
      PaymentsAutofillClient::CardSaveAndFillDialogUserDecision user_decision,
      const payments::PaymentsAutofillClient::
          UserProvidedCardSaveAndFillDetails&
              user_provided_card_save_and_fill_details) = 0;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_H_
