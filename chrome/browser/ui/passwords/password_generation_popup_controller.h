// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_CONTROLLER_H_

#include "base/strings/string16.h"
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

  // Accessors
  virtual GenerationUIState state() const = 0;
  virtual bool password_selected() const = 0;
  virtual const base::string16& password() const = 0;

  // Translated strings
  virtual base::string16 SuggestedText() = 0;
  virtual const base::string16& HelpText() = 0;

 protected:
  ~PasswordGenerationPopupController() override = default;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_CONTROLLER_H_
