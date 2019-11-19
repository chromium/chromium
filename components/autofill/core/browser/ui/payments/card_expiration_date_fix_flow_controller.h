// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_EXPIRATION_DATE_FIX_FLOW_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_EXPIRATION_DATE_FIX_FLOW_CONTROLLER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

namespace autofill {

// Enables the user to accept or deny expiration date fix flow prompt.
// Only used on mobile.
class CardExpirationDateFixFlowController {
 public:
  virtual ~CardExpirationDateFixFlowController() {}

  // Interaction.
  virtual void OnAccepted(const base::string16& month,
                          const base::string16& year) = 0;
  virtual void OnDismissed() = 0;
  virtual void OnDialogClosed() = 0;

  // State.
  virtual int GetIconId() const = 0;
  virtual base::string16 GetTitleText() const = 0;
  virtual base::string16 GetSaveButtonLabel() const = 0;
  virtual base::string16 GetCardLabel() const = 0;
  virtual base::string16 GetCancelButtonLabel() const = 0;
  virtual base::string16 GetInputLabel() const = 0;
  virtual base::string16 GetDateSeparator() const = 0;
  virtual base::string16 GetInvalidDateError() const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_EXPIRATION_DATE_FIX_FLOW_CONTROLLER_H_
