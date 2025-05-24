// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_VIEW_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_VIEW_H_

#include <string>

namespace autofill {

// The cross-platform UI interface which prompts the user to unlock a masked
// Wallet instrument (credit card). This object is responsible for its own
// lifetime.
class CardUnmaskPromptView {
 public:
  CardUnmaskPromptView(const CardUnmaskPromptView&) = delete;
  CardUnmaskPromptView& operator=(const CardUnmaskPromptView&) = delete;

  virtual void Show() = 0;
  virtual void Dismiss() {}
  virtual void ControllerGone() = 0;
  virtual void DisableAndWaitForVerification() = 0;
  virtual void GotVerificationResult(const std::u16string& error_message,
                                     bool allow_retry) = 0;

 protected:
  CardUnmaskPromptView() = default;
  virtual ~CardUnmaskPromptView() = default;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_VIEW_H_
