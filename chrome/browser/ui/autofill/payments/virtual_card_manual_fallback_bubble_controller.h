// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CONTROLLER_H_

#include <string>

#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

class AutofillBubbleBase;
class CreditCard;

// Interface that exposes controller functionality to
// VirtualCardManualFallbackBubbleViews. The bubble is shown when the virtual
// card option in the Autofill credit card suggestion list is clicked. It
// contains the card number, expiry and CVC information of the virtual card that
// users select to use, and serves as a fallback if not all the information is
// filled in the form by Autofill correctly.
class VirtualCardManualFallbackBubbleController {
 public:
  VirtualCardManualFallbackBubbleController() = default;
  virtual ~VirtualCardManualFallbackBubbleController() = default;
  VirtualCardManualFallbackBubbleController(
      const VirtualCardManualFallbackBubbleController&) = delete;
  VirtualCardManualFallbackBubbleController& operator=(
      const VirtualCardManualFallbackBubbleController&) = delete;

  // Returns a reference to VirtualCardManualFallbackBubbleController given the
  // |web_contents|. If the controller does not exist, creates one and returns
  // it.
  static VirtualCardManualFallbackBubbleController* GetOrCreate(
      content::WebContents* web_contents);

  // Returns a reference to VirtualCardManualFallbackBubbleController given the
  // |web_contents|. If the controller does not exist, returns nullptr.
  static VirtualCardManualFallbackBubbleController* Get(
      content::WebContents* web_contents);

  // Returns a reference to the bubble view.
  virtual AutofillBubbleBase* GetBubble() const = 0;

  // Returns the title text of the bubble.
  virtual std::u16string GetBubbleTitle() const = 0;

  // Returns the descriptive label of the virtual card number field.
  virtual std::u16string GetVirtualCardNumberFieldLabel() const = 0;

  // Returns the descriptive label of the expiration date field.
  virtual std::u16string GetExpirationDateFieldLabel() const = 0;

  // Returns the descriptive label of the CVC field.
  virtual std::u16string GetCvcFieldLabel() const = 0;

  // Returns the CVC value of the virtual card.
  virtual std::u16string GetCvc() const = 0;

  // Returns the related virtual card.
  virtual const CreditCard* GetVirtualCard() const = 0;

  // Returns whether the omnibox icon for the bubble should be visible.
  virtual bool ShouldIconBeVisible() const = 0;

  // Handles the event of bubble closure. |closed_reason| is the detailed reason
  // why the bubble was closed.
  virtual void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CONTROLLER_H_
