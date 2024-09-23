// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_CONTROLLER_IMPL_H_

#include <memory>

#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/ui/payments/virtual_card_enroll_bubble_controller.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

#if BUILDFLAG(IS_ANDROID)
class AutofillVCNEnrollBottomSheetBridge;
#endif

class VirtualCardEnrollBubbleControllerImpl
    : public AutofillBubbleControllerBase,
      public VirtualCardEnrollBubbleController,
      public content::WebContentsUserData<
          VirtualCardEnrollBubbleControllerImpl> {
 public:
  // Virtual card enrollment status
  enum class EnrollmentStatus {
    kNone,
    kPaymentsServerRequestInFlight,
    kCompleted,
  };
  VirtualCardEnrollBubbleControllerImpl(
      const VirtualCardEnrollBubbleControllerImpl&) = delete;
  VirtualCardEnrollBubbleControllerImpl& operator=(
      const VirtualCardEnrollBubbleControllerImpl&) = delete;
  ~VirtualCardEnrollBubbleControllerImpl() override;

  // Displays both the virtual card enroll bubble and its associated omnibox
  // icon. Sets virtual card enrollment fields as well as the closure for the
  // accept and decline bubble events.
  void ShowBubble(
      const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
      base::OnceClosure accept_virtual_card_callback,
      base::OnceClosure decline_virtual_card_callback);

  // Shows the bubble again if the users clicks the omnibox icon.
  void ReshowBubble();

  // Shows the confirmation bubble view after the virtual card enrollment
  // process has completed.
  virtual void ShowConfirmationBubbleView(
      payments::PaymentsAutofillClient::PaymentsRpcResult result);

  // VirtualCardEnrollBubbleController:
  const VirtualCardEnrollUiModel& GetUiModel() const override;
  VirtualCardEnrollmentBubbleSource GetVirtualCardEnrollmentBubbleSource()
      const override;
  AutofillBubbleBase* GetVirtualCardBubbleView() const override;

#if !BUILDFLAG(IS_ANDROID)
  void HideIconAndBubble() override;
  bool IsEnrollmentInProgress() const override;
  bool IsEnrollmentComplete() const override;
#endif

  void OnAcceptButton(bool did_switch_to_loading_state = false) override;
  void OnDeclineButton() override;
  void OnLinkClicked(VirtualCardEnrollmentLinkType link_type,
                     const GURL& url) override;
  void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) override;
  base::OnceCallback<void(PaymentsBubbleClosedReason)>
  GetOnBubbleClosedCallback() override;
  const SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&
  GetConfirmationUiParams() const override;
  bool IsIconVisible() const override;

 protected:
  explicit VirtualCardEnrollBubbleControllerImpl(
      content::WebContents* web_contents);

  // AutofillBubbleControllerBase::
  void OnVisibilityChanged(content::Visibility visibility) override;
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  friend class VirtualCardEnrollBubbleControllerImplTestApi;

  friend class content::WebContentsUserData<
      VirtualCardEnrollBubbleControllerImpl>;

  // Contains the UI assets shown in the virtual card enrollment view.
  std::unique_ptr<VirtualCardEnrollUiModel> ui_model_;

  // Whether we should re-show the dialog when users return to the tab.
  bool reprompt_required_ = false;

#if BUILDFLAG(IS_ANDROID)
  // A Java bridge for the bottom sheet version of the virtual card enrollment
  // UI.
  std::unique_ptr<AutofillVCNEnrollBottomSheetBridge>
      autofill_vcn_enroll_bottom_sheet_bridge_;
#else
  // Returns whether the web content associated with this controller is active.
  virtual bool IsWebContentsActive();

  // Resets bubble to its initial state.
  void ResetBubble();

  // Represents the current status of virtual card enrollment.
  EnrollmentStatus enrollment_status_ = EnrollmentStatus::kNone;

  // Represents the current state of icon and bubble.
  BubbleState bubble_state_ = BubbleState::kHidden;
#endif

  // Denotes whether the bubble is shown due to user gesture. If this is true,
  // it means the bubble is a reshown bubble.
  bool is_user_gesture_ = false;

  // Closure invoked when the user agrees to enroll in a virtual card.
  base::OnceClosure accept_virtual_card_callback_;

  // Closure invoked when the user rejects enrolling in a virtual card.
  base::OnceClosure decline_virtual_card_callback_;

  // Closure used for testing purposes that notifies that the enrollment bubble
  // has been shown.
  base::RepeatingClosure bubble_shown_closure_for_testing_;

  // UI parameters needed to display the virtual card enrollment confirmation
  // view.
  std::optional<SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams>
      confirmation_ui_params_;

  base::WeakPtrFactory<VirtualCardEnrollBubbleControllerImpl> weak_ptr_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_CONTROLLER_IMPL_H_
