// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/image_view.h"

namespace autofill {

class CardUnmaskAuthenticationSelectionDialogController;

class CardUnmaskAuthenticationSelectionDialogView
    : public CardUnmaskAuthenticationSelectionDialog,
      public views::BubbleDialogDelegateView {
 public:
  explicit CardUnmaskAuthenticationSelectionDialogView(
      CardUnmaskAuthenticationSelectionDialogController* controller);
  CardUnmaskAuthenticationSelectionDialogView(
      const CardUnmaskAuthenticationSelectionDialogView&) = delete;
  CardUnmaskAuthenticationSelectionDialogView& operator=(
      const CardUnmaskAuthenticationSelectionDialogView&) = delete;
  ~CardUnmaskAuthenticationSelectionDialogView() override;

  // CardUnmaskAuthenticationSelectionDialog:
  void Dismiss(bool user_closed_dialog, bool server_success) override;
  void UpdateContent() override;

  // views::DialogDelegateView:
  bool Accept() override;
  std::u16string GetWindowTitle() const override;
  void AddedToWidget() override;

 private:
  // Initializes the contents of the view and all of its child views.
  void InitViews();

  void AddHeaderText();

  // Adds all of the challenge options to the dialog. Each challenge option
  // is an option that can be selected to authenticate a user, such
  // as a text message or a phone number.
  void AddChallengeOptionsViews();

  void AddChallengeOptionDetails(
      const CardUnmaskChallengeOption& challenge_option,
      views::View* challenge_options_section);

  void AddFooterText();

  // Replace the contents with a progress throbber. Invoked whenever the OK
  // button is clicked.
  void ReplaceContentWithProgressThrobber();

  // Sets the selected challenge option id in the
  // CardUnmaskAuthenticationSelectionDialogController, as well as updates the
  // accept button text based on the challenge option.
  void OnChallengeOptionSelected(
      const CardUnmaskChallengeOption::ChallengeOptionId&
          selected_challenge_option_id);

  // Creates a radio button with a callback to SetSelectedChallengeOptionId.
  std::unique_ptr<views::RadioButton> CreateChallengeOptionRadioButton(
      CardUnmaskChallengeOption challenge_option);

  // Controller that owns functionality of the
  // CardUnmaskAuthenticationSelectionDialog.
  raw_ptr<CardUnmaskAuthenticationSelectionDialogController> controller_ =
      nullptr;

  // Vector of radio button checked changed subscriptions. Stored due to the
  // requirement that the subscription must be in memory when the callback is
  // used.
  std::vector<base::CallbackListSubscription>
      radio_button_checked_changed_subscriptions_;

  base::WeakPtrFactory<CardUnmaskAuthenticationSelectionDialogView>
      weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_VIEW_H_
