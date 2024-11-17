// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_CONTROLLER_H_

#include <string>


namespace autofill {

// Enables the user to accept or deny cardholder name fix flow prompt.
// Only used on mobile.
class CardNameFixFlowController {
 public:
  virtual ~CardNameFixFlowController() = default;

  // Interaction.
  virtual void OnConfirmNameDialogClosed() = 0;
  virtual void OnNameAccepted(const std::u16string& name) = 0;
  virtual void OnDismissed() = 0;

  // State.
  virtual int GetIconId() const = 0;
  virtual std::u16string GetCancelButtonLabel() const = 0;
  virtual std::u16string GetInferredCardholderName() const = 0;
  virtual std::u16string GetInferredNameTooltipText() const = 0;
  virtual std::u16string GetInputLabel() const = 0;
  virtual std::u16string GetInputPlaceholderText() const = 0;
  virtual std::u16string GetSaveButtonLabel() const = 0;
  virtual std::u16string GetTitleText() const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_CONTROLLER_H_
