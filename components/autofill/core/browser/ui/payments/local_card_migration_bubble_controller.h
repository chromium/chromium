// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_LOCAL_CARD_MIGRATION_BUBBLE_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_LOCAL_CARD_MIGRATION_BUBBLE_CONTROLLER_H_

#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"

namespace autofill {

class LocalCardMigrationBubble;

// Interface that exposes controller functionality to
// LocalCardMigrationBubble. The bubble is shown to offer user an option
// to upload credit cards stored in browser to Google Payments.
class LocalCardMigrationBubbleController {
 public:
  LocalCardMigrationBubbleController() = default;

  LocalCardMigrationBubbleController(
      const LocalCardMigrationBubbleController&) = delete;
  LocalCardMigrationBubbleController& operator=(
      const LocalCardMigrationBubbleController&) = delete;

  virtual ~LocalCardMigrationBubbleController() = default;

  virtual void OnConfirmButtonClicked() = 0;
  virtual void OnCancelButtonClicked() = 0;
  virtual void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_LOCAL_CARD_MIGRATION_BUBBLE_CONTROLLER_H_
