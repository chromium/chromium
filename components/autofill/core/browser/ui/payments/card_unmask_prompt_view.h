// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_VIEW_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_VIEW_H_

#include "base/macros.h"
#include "base/strings/string16.h"

namespace autofill {

// The cross-platform UI interface which prompts the user to unlock a masked
// Wallet instrument (credit card). This object is responsible for its own
// lifetime.
class CardUnmaskPromptView {
 public:
  virtual void Show() = 0;
  virtual void ControllerGone() = 0;
  virtual void DisableAndWaitForVerification() = 0;
  virtual void GotVerificationResult(const base::string16& error_message,
                                     bool allow_retry) = 0;

 protected:
  CardUnmaskPromptView() {}
  virtual ~CardUnmaskPromptView() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CardUnmaskPromptView);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_VIEW_H_
