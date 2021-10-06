// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_CONTROLLER_IMPL_H_

#include <string>

#include "chrome/browser/ui/autofill/payments/card_unmask_authentication_selection_dialog_controller.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class CardUnmaskAuthenticationSelectionDialogView;

class CardUnmaskAuthenticationSelectionDialogControllerImpl
    : public CardUnmaskAuthenticationSelectionDialogController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<
          CardUnmaskAuthenticationSelectionDialogControllerImpl> {
 public:
  CardUnmaskAuthenticationSelectionDialogControllerImpl(
      const CardUnmaskAuthenticationSelectionDialogControllerImpl&) = delete;
  CardUnmaskAuthenticationSelectionDialogControllerImpl& operator=(
      const CardUnmaskAuthenticationSelectionDialogControllerImpl&) = delete;
  ~CardUnmaskAuthenticationSelectionDialogControllerImpl() override;

  void ShowDialog(
      const std::vector<CardUnmaskChallengeOption>& challenge_options);

  // CardUnmaskAuthenticationSelectionDialogController:
  void OnDialogClosed() override;
  std::u16string GetWindowTitle() const override;
  std::u16string GetContentHeaderText() const override;
  const std::vector<CardUnmaskChallengeOption>& GetChallengeOptions()
      const override;
  ui::ImageModel GetAuthenticationModeIcon(
      const CardUnmaskChallengeOption& challenge_option) const override;
  std::u16string GetAuthenticationModeLabel(
      const CardUnmaskChallengeOption& challenge_option) const override;
  std::u16string GetContentFooterText() const override;
  std::u16string GetOkButtonLabel() const override;

#if defined(UNIT_TEST)
  CardUnmaskAuthenticationSelectionDialogView* GetDialogViewForTesting();
#endif

 private:
  explicit CardUnmaskAuthenticationSelectionDialogControllerImpl(
      content::WebContents* web_contents);

  friend class content::WebContentsUserData<
      CardUnmaskAuthenticationSelectionDialogControllerImpl>;

  // Contains all of the challenge options an issuer has for the user.
  std::vector<CardUnmaskChallengeOption> challenge_options_;

  CardUnmaskAuthenticationSelectionDialogView* dialog_view_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_CONTROLLER_IMPL_H_
