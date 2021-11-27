// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_VIEWS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_authentication_selection_dialog_view.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace autofill {

class CardUnmaskAuthenticationSelectionDialogController;

class CardUnmaskAuthenticationSelectionDialogViews
    : public CardUnmaskAuthenticationSelectionDialogView,
      public views::DialogDelegateView {
 public:
  explicit CardUnmaskAuthenticationSelectionDialogViews(
      CardUnmaskAuthenticationSelectionDialogController* controller);
  CardUnmaskAuthenticationSelectionDialogViews(
      const CardUnmaskAuthenticationSelectionDialogViews&) = delete;
  CardUnmaskAuthenticationSelectionDialogViews& operator=(
      const CardUnmaskAuthenticationSelectionDialogViews&) = delete;
  ~CardUnmaskAuthenticationSelectionDialogViews() override;

  // CardUnmaskAuthenticationSelectionDialogView:
  void Dismiss(bool user_closed_dialog, bool server_success) override;

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

  void AddFooterText();

  // Replace the contents with a progress throbber. Invoked whenever the OK
  // button is clicked.
  void ReplaceContentWithProgressThrobber();

  raw_ptr<CardUnmaskAuthenticationSelectionDialogController> controller_ =
      nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_VIEWS_H_
