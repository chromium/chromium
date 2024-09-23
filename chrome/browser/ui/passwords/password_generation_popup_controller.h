// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_CONTROLLER_H_

#include <string>

#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"

class PasswordGenerationPopupController
    : public autofill::AutofillPopupViewDelegate {
 public:
  enum GenerationUIState {
    // Generated password is offered in the popup but not filled yet.
    kOfferGeneration,
    // The generated password was accepted.
    kEditGeneratedPassword,
  };

  // Called by the view when the password was accepted.
  virtual void PasswordAccepted() = 0;

  // Called by the view when the password was selected.
  virtual void SetSelected() = 0;

  // Called by the view when the password was cleared.
  virtual void SelectionCleared() = 0;

// Only on Desktop, the password generation promo contains an indicator of which
// Google account passwords are saved to. Therefore, the following method isn't
// relevant for Android.
#if !BUILDFLAG(IS_ANDROID)
  // Returns the email of current primary account. Returns empty string if no
  // account is signed in.
  virtual std::u16string GetPrimaryAccountEmail() = 0;

  // Returns whether `kNudgePassword` Desktop experiment is enabled and whether
  // it was not rejected enough times already.
  virtual bool ShouldShowNudgePassword() const = 0;
#endif  // !BUILDFLAG(IS_ANDROID)

  // Accessors
  virtual GenerationUIState state() const = 0;
  virtual bool password_selected() const = 0;
  virtual bool accept_button_selected() const = 0;
  virtual bool cancel_button_selected() const = 0;
  virtual const std::u16string& password() const = 0;

  // Translated strings
  virtual std::u16string SuggestedText() const = 0;
  virtual const std::u16string& HelpText() const = 0;

 protected:
  ~PasswordGenerationPopupController() override = default;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_CONTROLLER_H_
