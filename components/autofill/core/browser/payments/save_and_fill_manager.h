// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_H_

#include "components/autofill/core/browser/metrics/payments/save_and_fill_metrics.h"

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
  // Called when the Save and Fill suggestion is shown to the user.
  virtual void OnSuggestionOffered() = 0;
  // If the strike database exists, add a strike if the suggestion was shown but
  // not selected.
  virtual void MaybeAddStrikeForSaveAndFill() = 0;
  // Returns true if the feature offer should be blocked.
  virtual bool ShouldBlockFeature() = 0;
  // Logs the reason why the Save and Fill suggestion was not shown if this
  // metric has not yet been recorded, as this is logged once per page load.
  virtual void MaybeLogSaveAndFillSuggestionNotShownReason(
      autofill_metrics::SaveAndFillSuggestionNotShownReason reason) = 0;
  // Logs when the credit card form was filled / submitted with the
  // Save-and-Fill candidate card.
  virtual void LogCreditCardFormFilled() = 0;
  virtual void LogCreditCardFormSubmitted() = 0;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_H_
