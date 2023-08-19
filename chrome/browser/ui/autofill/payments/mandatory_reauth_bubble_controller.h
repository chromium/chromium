// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_MANDATORY_REAUTH_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_MANDATORY_REAUTH_BUBBLE_CONTROLLER_H_

#include <string>

#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/payments/mandatory_reauth_ui.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

class AutofillBubbleBase;

// Interface that exposes controller functionality to autofill mandatory
// reauthentication bubbles.
class MandatoryReauthBubbleController {
 public:
  MandatoryReauthBubbleController() = default;
  MandatoryReauthBubbleController(const MandatoryReauthBubbleController&) =
      delete;
  MandatoryReauthBubbleController& operator=(
      const MandatoryReauthBubbleController&) = delete;
  virtual ~MandatoryReauthBubbleController() = default;

  virtual std::u16string GetWindowTitle() const = 0;
  virtual std::u16string GetAcceptButtonText() const = 0;
  virtual std::u16string GetCancelButtonText() const = 0;
  virtual std::u16string GetExplanationText() const = 0;

  virtual void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) = 0;

  // Returns the current bubble view. Can return nullptr if bubble is not
  // visible.
  virtual AutofillBubbleBase* GetBubbleView() = 0;

  // Determines if the page action icon should be shown.
  virtual bool IsIconVisible() = 0;

  // The type of bubble currently displayed to the user.
  virtual MandatoryReauthBubbleType GetBubbleType() const = 0;

#if BUILDFLAG(IS_ANDROID)
  virtual base::android::ScopedJavaLocalRef<jobject>
  GetJavaControllerBridge() = 0;
#endif
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_MANDATORY_REAUTH_BUBBLE_CONTROLLER_H_
