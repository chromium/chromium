// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_CONTROLLER_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_view.h"

namespace autofill {

class SaveAndFillDialogView;

// Implementation of the SaveAndFillDialogController.
class SaveAndFillDialogControllerImpl : public SaveAndFillDialogController {
 public:
  SaveAndFillDialogControllerImpl();
  SaveAndFillDialogControllerImpl(const SaveAndFillDialogControllerImpl&) =
      delete;
  SaveAndFillDialogControllerImpl& operator=(
      const SaveAndFillDialogControllerImpl&) = delete;
  ~SaveAndFillDialogControllerImpl() override;

  void ShowLocalDialog(
      base::OnceCallback<std::unique_ptr<SaveAndFillDialogView>()>
          create_and_show_view_callback,
      payments::PaymentsAutofillClient::CardSaveAndFillDialogCallback
          card_save_and_fill_dialog_callback);

  void ShowUploadDialog(
      const LegalMessageLines& legal_message_lines,
      base::OnceCallback<std::unique_ptr<SaveAndFillDialogView>()>
          create_and_show_view_callback,
      payments::PaymentsAutofillClient::CardSaveAndFillDialogCallback
          card_save_and_fill_dialog_callback);

  void ShowPendingDialog(
      base::OnceCallback<std::unique_ptr<SaveAndFillDialogView>()>
          create_and_show_view_callback);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  std::u16string GetWindowTitle() const override;
  std::u16string GetExplanatoryMessage() const override;
  std::u16string GetCardNumberLabel() const override;
  std::u16string GetCvcLabel() const override;
  std::u16string GetExpirationDateLabel() const override;
  std::u16string GetNameOnCardLabel() const override;
  std::u16string GetAcceptButtonText() const override;
  std::u16string GetInvalidCardNumberErrorMessage() const override;
  std::u16string GetInvalidCvcErrorMessage() const override;
  std::u16string GetInvalidExpirationDateErrorMessage() const override;
  std::u16string GetInvalidNameOnCardErrorMessage() const override;
  std::u16string FormatExpirationDateInput(
      std::u16string_view input,
      size_t old_cursor_position,
      size_t& new_cursor_position) const override;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  SaveAndFillDialogState GetDialogState() const override;
  bool IsValidCreditCardNumber(std::u16string_view input_text) const override;
  bool IsValidCvc(std::u16string_view input_text) const override;
  bool IsValidExpirationDate(
      std::u16string_view expiration_date) const override;
  bool IsValidNameOnCard(std::u16string_view input_text) const override;

  const LegalMessageLines& GetLegalMessageLines() const override;

  void Dismiss() override;
  void OnUserAcceptedDialog(
      const payments::PaymentsAutofillClient::
          UserProvidedCardSaveAndFillDetails&
              user_provided_card_save_and_fill_details) override;
  void OnUserCanceledDialog() override;

  base::WeakPtr<SaveAndFillDialogController> GetWeakPtr() override;

 private:
  friend class SaveAndFillDialogControllerImplTest;

  std::unique_ptr<SaveAndFillDialogView> dialog_view_;

  // Determines the current state of the Save and Fill dialog. This state
  // can be a local card save, an upload card save, or a pending state while
  // waiting for the preflight response.
  SaveAndFillDialogState dialog_state_;

  LegalMessageLines legal_message_lines_;

  payments::PaymentsAutofillClient::CardSaveAndFillDialogCallback
      card_save_and_fill_dialog_callback_;

  base::WeakPtrFactory<SaveAndFillDialogControllerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_CONTROLLER_IMPL_H_
