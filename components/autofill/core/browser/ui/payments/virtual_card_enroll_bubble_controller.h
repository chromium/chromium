// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "url/gurl.h"

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_CONTROLLER_H_

namespace autofill {

class AutofillBubbleBase;
enum class VirtualCardEnrollmentBubbleSource;
enum class VirtualCardEnrollmentState;

// Interface that exposes controller functionality to virtual card enrollment
// bubbles.
class VirtualCardEnrollBubbleController {
 public:
  VirtualCardEnrollBubbleController() = default;
  VirtualCardEnrollBubbleController(const VirtualCardEnrollBubbleController&) =
      delete;
  VirtualCardEnrollBubbleController& operator=(
      const VirtualCardEnrollBubbleController&) = delete;
  virtual ~VirtualCardEnrollBubbleController() = default;

  // Returns a reference to the VirtualCardEnrollBubbleController associated
  // with the given |web_contents|. If controller does not exist, this will
  // create the controller from the |web_contents| then return the reference.
  static VirtualCardEnrollBubbleController* GetOrCreate(
      content::WebContents* web_contents);

  // Returns the title displayed in the bubble.
  virtual std::u16string GetWindowTitle() const = 0;

  // Returns the main text displayed in the bubble.
  virtual std::u16string GetExplanatoryMessage() const = 0;

  // Returns the button label text for virtual card enroll bubbles.
  virtual std::u16string GetAcceptButtonText() const = 0;
  virtual std::u16string GetDeclineButtonText() const = 0;

  // Returns the text used in the learn more link.
  virtual std::u16string GetLearnMoreLinkText() const = 0;

  // Returns the enrollment fields for the virtual card.
  virtual const VirtualCardEnrollmentFields GetVirtualCardEnrollmentFields()
      const = 0;

  // Returns the "source" of the virtual card number enrollment flow, e.g.,
  // "upstream", "downstream", "settings".
  virtual VirtualCardEnrollmentBubbleSource
  GetVirtualCardEnrollmentBubbleSource() const = 0;

  // Returns the currently active virtual card enroll bubble view. Can be
  // nullptr if no bubble is visible.
  virtual AutofillBubbleBase* GetVirtualCardEnrollBubbleView() const = 0;

#if !BUILDFLAG(IS_ANDROID)
  // Hides the bubble and icon if it is showing.
  virtual void HideIconAndBubble() = 0;
#endif

  // Virtual card enroll button takes card information to enroll into a VCN.
  virtual void OnAcceptButton() = 0;
  virtual void OnDeclineButton() = 0;
  virtual void OnLinkClicked(VirtualCardEnrollmentLinkType link_type,
                             const GURL& url) = 0;
  virtual void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) = 0;

  // Returns whether the omnibox icon should be visible.
  virtual bool IsIconVisible() const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_CONTROLLER_H_
