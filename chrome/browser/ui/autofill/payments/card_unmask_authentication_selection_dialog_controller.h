// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_CONTROLLER_H_

#include <string>
#include <vector>

namespace ui {
class ImageModel;
}

namespace autofill {

struct CardUnmaskChallengeOption;

// Interface that exposes controller functionality to
// CardUnmaskAuthenticationSelectionDialogView.
class CardUnmaskAuthenticationSelectionDialogController {
 public:
  CardUnmaskAuthenticationSelectionDialogController() = default;
  virtual ~CardUnmaskAuthenticationSelectionDialogController() = default;
  CardUnmaskAuthenticationSelectionDialogController(
      const CardUnmaskAuthenticationSelectionDialogController&) = delete;
  CardUnmaskAuthenticationSelectionDialogController& operator=(
      const CardUnmaskAuthenticationSelectionDialogController&) = delete;

  // Called whenever the dialog is closed, and it sets the |dialog_view_|
  // variable in this class to nullptr.
  virtual void OnDialogClosed() = 0;

  virtual std::u16string GetWindowTitle() const = 0;

  virtual std::u16string GetContentHeaderText() const = 0;

  // Returns the vector of challenge options for authentication
  // (text, email, etc...). Each CardUnmaskChallengeOption in the vector
  // is a struct that has the challenge option type, as well as the relevant
  // data to send the authentication to (such as a masked phone number or masked
  // email).
  virtual const std::vector<CardUnmaskChallengeOption>& GetChallengeOptions()
      const = 0;

  // Returns the corresponding authentication mode icon for the given challenge
  // option.
  virtual ui::ImageModel GetAuthenticationModeIcon(
      const CardUnmaskChallengeOption& challenge_option) const = 0;

  // Returns the text that describes the authentication mode choice, for
  // example: text message, email.
  virtual std::u16string GetAuthenticationModeLabel(
      const CardUnmaskChallengeOption& challenge_options) const = 0;

  virtual std::u16string GetContentFooterText() const = 0;

  virtual std::u16string GetOkButtonLabel() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_CONTROLLER_H_
