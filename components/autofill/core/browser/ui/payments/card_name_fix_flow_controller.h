// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_CONTROLLER_H_

#include "base/strings/string16.h"

namespace autofill {

// Enables the user to accept or deny cardholder name fix flow prompt.
// Only used on mobile.
class CardNameFixFlowController {
 public:
  virtual ~CardNameFixFlowController() {}

  // Interaction.
  virtual void OnConfirmNameDialogClosed() = 0;
  virtual void OnNameAccepted(const base::string16& name) = 0;
  virtual void OnDismissed() = 0;

  // State.
  virtual int GetIconId() const = 0;
  virtual base::string16 GetCancelButtonLabel() const = 0;
  virtual base::string16 GetInferredCardholderName() const = 0;
  virtual base::string16 GetInferredNameTooltipText() const = 0;
  virtual base::string16 GetInputLabel() const = 0;
  virtual base::string16 GetInputPlaceholderText() const = 0;
  virtual base::string16 GetSaveButtonLabel() const = 0;
  virtual base::string16 GetTitleText() const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_CONTROLLER_H_
