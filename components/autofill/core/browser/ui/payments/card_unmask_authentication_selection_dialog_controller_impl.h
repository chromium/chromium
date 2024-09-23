// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_CONTROLLER_IMPL_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog_controller.h"

namespace autofill {

class CardUnmaskAuthenticationSelectionDialog;

class CardUnmaskAuthenticationSelectionDialogControllerImpl
    : public CardUnmaskAuthenticationSelectionDialogController {
 public:
  // Initialize this controller's parameters.
  // `challenge_options` must not be empty.
  CardUnmaskAuthenticationSelectionDialogControllerImpl(
      const std::vector<CardUnmaskChallengeOption>& challenge_options,
      base::OnceCallback<void(const std::string&)>
          confirm_unmasking_method_callback,
      base::OnceClosure cancel_unmasking_closure);
  CardUnmaskAuthenticationSelectionDialogControllerImpl(
      const CardUnmaskAuthenticationSelectionDialogControllerImpl&) = delete;
  CardUnmaskAuthenticationSelectionDialogControllerImpl& operator=(
      const CardUnmaskAuthenticationSelectionDialogControllerImpl&) = delete;
  ~CardUnmaskAuthenticationSelectionDialogControllerImpl() override;

  // Callback for creating and showing an authentication selection dialog.
  using CreateAndShowCallback =
      base::OnceCallback<CardUnmaskAuthenticationSelectionDialog*(
          CardUnmaskAuthenticationSelectionDialogController*)>;

  // Show the dialog (and log metrics.)
  void ShowDialog(CreateAndShowCallback create_and_show_callback);

  // Called when we receive a server response after the user accepts (clicks the
  // ok button) on a challenge option. |server_success| represents a successful
  // server response, where true means success and false means an error was
  // returned by the server.
  void DismissDialogUponServerProcessedAuthenticationMethodRequest(
      bool server_success);

  // CardUnmaskAuthenticationSelectionDialogController:
  void OnDialogClosed(bool user_closed_dialog, bool server_success) override;
  void OnOkButtonClicked() override;
  std::u16string GetWindowTitle() const override;
  std::u16string GetContentHeaderText() const override;
  const std::vector<CardUnmaskChallengeOption>& GetChallengeOptions()
      const override;
  std::u16string GetAuthenticationModeLabel(
      const CardUnmaskChallengeOption& challenge_option) const override;
  std::u16string GetContentFooterText() const override;
  std::u16string GetOkButtonLabel() const override;
  std::u16string GetProgressLabel() const override;
  void SetSelectedChallengeOptionId(
      const CardUnmaskChallengeOption::ChallengeOptionId&
          selected_challenge_option_id) override;

  CardUnmaskAuthenticationSelectionDialog* GetDialogViewForTesting() {
    return dialog_view_;
  }

  const CardUnmaskChallengeOption::ChallengeOptionId&
  GetSelectedChallengeOptionIdForTesting() const {
    return selected_challenge_option_id_;
  }

  CardUnmaskChallengeOptionType GetSelectedChallengeOptionTypeForTesting()
      const {
    return selected_challenge_option_type_;
  }

  void SetSelectedChallengeOptionsForTesting(
      std::vector<CardUnmaskChallengeOption> challenge_options) {
    challenge_options_ = challenge_options;
  }

  base::WeakPtr<CardUnmaskAuthenticationSelectionDialogControllerImpl>
  GetWeakPtr();

 private:
  // Contains all of the challenge options an issuer has for the user.
  std::vector<CardUnmaskChallengeOption> challenge_options_;

  raw_ptr<CardUnmaskAuthenticationSelectionDialog> dialog_view_ = nullptr;

  // Callback invoked when the user confirmed an authentication method to use.
  base::OnceCallback<void(const std::string&)>
      confirm_unmasking_method_callback_;

  // Callback invoked when the user cancelled the card unmasking.
  base::OnceClosure cancel_unmasking_closure_;

  // Tracks whether a challenge option was selected in the current
  // `dialog_view_`.
  // TODO(crbug.com/40247983): Rename this to `challenge_option_accepted_`, as
  // it only gets set when the user clicks the accept button after selecting a
  // challenge option.
  bool challenge_option_selected_ = false;

  // The currently unique identifier of the challenge option selected in the
  // Card Authentication Selection Dialog View.
  // TODO(crbug.com/40247983): Remove this and just add a
  // `selected_challenge_option_` object once we refactor
  // `SetSelectedChallengeOptionId()` to `SetSelectedChallengeOptionForId()`.
  CardUnmaskChallengeOption::ChallengeOptionId selected_challenge_option_id_ =
      CardUnmaskChallengeOption::ChallengeOptionId();

  // Contains the challenge option type selected by the user. Currently only
  // kCvc and kSmsOtp are supported.
  // TODO(crbug.com/40247983): Remove this and just add a
  // `selected_challenge_option_` object once we refactor
  // `SetSelectedChallengeOptionId()` to `SetSelectedChallengeOptionForId()`.
  CardUnmaskChallengeOptionType selected_challenge_option_type_ =
      CardUnmaskChallengeOptionType::kUnknownType;

  base::WeakPtrFactory<CardUnmaskAuthenticationSelectionDialogControllerImpl>
      weak_ptr_factory_{this};
};

}  // namespace autofill


#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_CONTROLLER_IMPL_H_
