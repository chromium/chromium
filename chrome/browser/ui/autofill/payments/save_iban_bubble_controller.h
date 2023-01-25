// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_IBAN_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_IBAN_BUBBLE_CONTROLLER_H_

#include <string>

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

class AutofillBubbleBase;
class IBAN;
enum class IBANBubbleType;

// Interface that exposes controller functionality to save IBAN bubbles.
class SaveIbanBubbleController {
 public:
  SaveIbanBubbleController() = default;
  SaveIbanBubbleController(const SaveIbanBubbleController&) = delete;
  SaveIbanBubbleController& operator=(const SaveIbanBubbleController&) = delete;
  virtual ~SaveIbanBubbleController() = default;

  // Returns a reference to the SaveIbanBubbleController associated with the
  // given `web_contents`. If controller does not exist, this will create the
  // controller from the `web_contents` then return the reference.
  static SaveIbanBubbleController* GetOrCreate(
      content::WebContents* web_contents);

  // Returns the title that should be displayed in the bubble.
  virtual std::u16string GetWindowTitle() const = 0;

  // Returns the button label text for IBAN save bubbles.
  virtual std::u16string GetAcceptButtonText() const = 0;
  virtual std::u16string GetDeclineButtonText() const = 0;

  // Returns the IBAN that will be saved if the user accepts.
  virtual const IBAN& GetIBAN() const = 0;

  virtual AutofillBubbleBase* GetSaveBubbleView() const = 0;

  // Interaction.
  virtual void OnSaveButton(const std::u16string& nickname) = 0;
  virtual void OnCancelButton() = 0;
  virtual void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_IBAN_BUBBLE_CONTROLLER_H_
