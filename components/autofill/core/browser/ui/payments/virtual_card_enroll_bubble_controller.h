// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "components/autofill/core/browser/ui/payments/save_payment_method_and_virtual_card_enroll_confirmation_ui_params.h"
#include "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"
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

  // Returns the UI assets needed to display the virtual card enrollment view.
  virtual const VirtualCardEnrollUiModel& GetUiModel() const = 0;

  // Returns the "source" of the virtual card number enrollment flow, e.g.,
  // "upstream", "downstream", "settings".
  virtual VirtualCardEnrollmentBubbleSource
  GetVirtualCardEnrollmentBubbleSource() const = 0;

  // Returns the currently active virtual card enroll or confirmation bubble
  // view. Can be nullptr if no bubble is visible.
  virtual AutofillBubbleBase* GetVirtualCardBubbleView() const = 0;

#if !BUILDFLAG(IS_ANDROID)
  // Hides the bubble and icon if it is showing.
  virtual void HideIconAndBubble() = 0;

  // Returns true if bubble is already accepted and the virtual card enrollment
  // process is in progress.
  virtual bool IsEnrollmentInProgress() const = 0;

  // Returns true if server request for virtual card enrollment is complete.
  virtual bool IsEnrollmentComplete() const = 0;
#endif

  // Virtual card enroll button takes card information to enroll into a VCN.
  // `did_switch_to_loading_state` denotes if bubble is waiting for enrollment
  // to finish on server before closing.
  virtual void OnAcceptButton(bool did_switch_to_loading_state = false) = 0;
  virtual void OnDeclineButton() = 0;
  virtual void OnLinkClicked(VirtualCardEnrollmentLinkType link_type,
                             const GURL& url) = 0;
  virtual void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) = 0;
  virtual base::OnceCallback<void(PaymentsBubbleClosedReason)>
  GetOnBubbleClosedCallback() = 0;

  // Returns the UI parameters needed to display the virtual card enroll
  // confirmation view.
  virtual const SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&
  GetConfirmationUiParams() const = 0;

  // Returns whether the omnibox icon should be visible.
  virtual bool IsIconVisible() const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_CONTROLLER_H_
