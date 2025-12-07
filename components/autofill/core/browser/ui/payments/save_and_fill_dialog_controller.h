// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill {

enum class SaveAndFillDialogState {
  // Local version of the Save and Fill dialog.
  kLocalDialog,
  // Pending state where a throbber is shown while waiting for the preflight
  // call response.
  kPendingDialog,
  // Upload version of the Save and Fill dialog.
  kUploadDialog,
};

// Interface that exposes controller functionality to
// SaveAndFillDialogView.
class SaveAndFillDialogController {
 public:
  virtual ~SaveAndFillDialogController() = default;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  virtual std::u16string GetWindowTitle() const = 0;
  virtual std::u16string GetExplanatoryMessage() const = 0;
  virtual std::u16string GetCardNumberLabel() const = 0;
  virtual std::u16string GetCvcLabel() const = 0;
  virtual std::u16string GetExpirationDateLabel() const = 0;
  virtual std::u16string GetNameOnCardLabel() const = 0;
  virtual std::u16string GetAcceptButtonText() const = 0;
  virtual std::u16string GetInvalidCardNumberErrorMessage() const = 0;
  virtual std::u16string GetInvalidCvcErrorMessage() const = 0;
  virtual std::u16string GetInvalidExpirationDateErrorMessage() const = 0;
  virtual std::u16string GetInvalidNameOnCardErrorMessage() const = 0;
  // Format the expiration date input into `MM/YY`. A slash is added
  // automatically after the user inputs the month digits and removed if the
  // user deletes the year digits.
  virtual std::u16string FormatExpirationDateInput(
      std::u16string_view input,
      size_t old_cursor_position,
      size_t& new_cursor_position) const = 0;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // Returns the current state of the Save and Fill dialog. This state
  // can be a local card save, an upload card save, or a pending state while
  // waiting for the preflight response.
  virtual SaveAndFillDialogState GetDialogState() const = 0;
  virtual bool IsValidCreditCardNumber(
      std::u16string_view input_text) const = 0;
  virtual bool IsValidCvc(std::u16string_view input_text) const = 0;
  virtual bool IsValidExpirationDate(
      std::u16string_view expiration_date) const = 0;
  virtual bool IsValidNameOnCard(std::u16string_view input_text) const = 0;

  // Returns empty vector if no legal message should be shown.
  virtual const LegalMessageLines& GetLegalMessageLines() const = 0;

  // Dismisses the dialog by destroying its view and associated widget.
  virtual void Dismiss() = 0;
  // Callbacks for when the user accepts the Save and Fill dialog.
  virtual void OnUserAcceptedDialog(
      const payments::PaymentsAutofillClient::
          UserProvidedCardSaveAndFillDetails&
              user_provided_card_save_and_fill_details) = 0;
  // Callbacks for when the user cancels the Save and Fill dialog.
  virtual void OnUserCanceledDialog() = 0;

  virtual base::WeakPtr<SaveAndFillDialogController> GetWeakPtr() = 0;
};

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_CONTROLLER_H_
