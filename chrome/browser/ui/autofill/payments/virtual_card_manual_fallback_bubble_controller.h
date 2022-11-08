// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CONTROLLER_H_

#include <string>

#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/image/image.h"

namespace autofill {

class AutofillBubbleBase;
struct VirtualCardManualFallbackBubbleOptions;

// The fields inside of the virtual card manual fallback bubble.
enum class VirtualCardManualFallbackBubbleField {
  kCardNumber = 0,
  kExpirationMonth = 1,
  kExpirationYear = 2,
  kCardholderName = 3,
  kCvc = 4,
  kMaxValue = kCvc,
};

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
  virtual std::u16string GetBubbleTitleText() const = 0;

  // Returns the bubble popup options.
  virtual const VirtualCardManualFallbackBubbleOptions& GetBubbleOptions()
      const = 0;

  // Returns the text used to show that the card entity in the bubble is a
  // virtual card.
  virtual std::u16string GetVirtualCardIndicatorLabel() const = 0;

  // Returns the text used in the learn more link.
  virtual std::u16string GetLearnMoreLinkText() const = 0;

  // Returns the educational label shown in the body of the bubble.
  virtual std::u16string GetEducationalBodyLabel() const = 0;

  // Returns the descriptive label of the virtual card number field.
  virtual std::u16string GetVirtualCardNumberFieldLabel() const = 0;

  // Returns the descriptive label of the expiration date field.
  virtual std::u16string GetExpirationDateFieldLabel() const = 0;

  // Returns the descriptive label of the cardholder name field.
  virtual std::u16string GetCardholderNameFieldLabel() const = 0;

  // Returns the descriptive label of the CVC field.
  virtual std::u16string GetCvcFieldLabel() const = 0;

  // Returns the text value of the |field| for display.
  virtual std::u16string GetValueForField(
      VirtualCardManualFallbackBubbleField field) const = 0;

  // Returns the tooltip of the button.
  virtual std::u16string GetFieldButtonTooltip(
      VirtualCardManualFallbackBubbleField field) const = 0;

  // Returns whether the omnibox icon for the bubble should be visible.
  virtual bool ShouldIconBeVisible() const = 0;

  // Handles the event of bubble closure. |closed_reason| is the detailed reason
  // why the bubble was closed.
  virtual void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) = 0;

  // Handles the event on the learn more link being clicked.
  virtual void OnLinkClicked(const GURL& url) = 0;

  // Handles the event of clicking the |field|'s button.
  virtual void OnFieldClicked(VirtualCardManualFallbackBubbleField field) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CONTROLLER_H_
