// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_OBSERVER_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_OBSERVER_H_

#include "chrome/browser/ui/passwords/password_generation_popup_controller.h"

// Observer for PasswordGenerationPopup events. Currently only used for testing.
class PasswordGenerationPopupObserver {
 public:
  virtual void OnPopupShown(
      PasswordGenerationPopupController::GenerationUIState state) = 0;
  virtual void OnPopupHidden() = 0;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_OBSERVER_H_
