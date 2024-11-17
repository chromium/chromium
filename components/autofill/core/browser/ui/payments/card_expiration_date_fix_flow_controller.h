// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_EXPIRATION_DATE_FIX_FLOW_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_EXPIRATION_DATE_FIX_FLOW_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill {

// Enables the user to accept or deny expiration date fix flow prompt.
// Only used on mobile.
class CardExpirationDateFixFlowController {
 public:
  virtual ~CardExpirationDateFixFlowController() = default;

  // Interaction.
  virtual void OnAccepted(const std::u16string& month,
                          const std::u16string& year) = 0;
  virtual void OnDismissed() = 0;
  virtual void OnDialogClosed() = 0;

  // State.
  virtual int GetIconId() const = 0;
  virtual std::u16string GetTitleText() const = 0;
  virtual std::u16string GetSaveButtonLabel() const = 0;
  virtual std::u16string GetCardLabel() const = 0;
  virtual std::u16string GetCancelButtonLabel() const = 0;
  virtual std::u16string GetInputLabel() const = 0;
  virtual std::u16string GetDateSeparator() const = 0;
  virtual std::u16string GetInvalidDateError() const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_EXPIRATION_DATE_FIX_FLOW_CONTROLLER_H_
