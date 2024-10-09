// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#include "components/autofill/core/browser/payments/virtual_card_enroll_metrics_logger.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/autofill/autofill_vcn_enroll_bottom_sheet_bridge.h"
#include "components/autofill/core/browser/payments/autofill_virtual_card_enrollment_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/payments/autofill_virtual_card_enrollment_infobar_mobile.h"
#include "components/infobars/core/infobar.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {
namespace {
using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;
}

VirtualCardEnrollBubbleControllerImpl::VirtualCardEnrollBubbleControllerImpl(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<VirtualCardEnrollBubbleControllerImpl>(
          *web_contents) {}

VirtualCardEnrollBubbleControllerImpl::
    ~VirtualCardEnrollBubbleControllerImpl() = default;

// static
VirtualCardEnrollBubbleController*
VirtualCardEnrollBubbleController::GetOrCreate(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  VirtualCardEnrollBubbleControllerImpl::CreateForWebContents(web_contents);
  return VirtualCardEnrollBubbleControllerImpl::FromWebContents(web_contents);
}

void VirtualCardEnrollBubbleControllerImpl::ShowBubble(
    const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
    base::OnceClosure accept_virtual_card_callback,
    base::OnceClosure decline_virtual_card_callback) {
  ui_model_ = std::make_unique<VirtualCardEnrollUiModel>(
      virtual_card_enrollment_fields);
  accept_virtual_card_callback_ = std::move(accept_virtual_card_callback);
  decline_virtual_card_callback_ = std::move(decline_virtual_card_callback);

  is_user_gesture_ = false;
  Show();

  VirtualCardEnrollMetricsLogger::OnCardArtAvailable(
      ui_model_->enrollment_fields().card_art_image,
      ui_model_->enrollment_fields().virtual_card_enrollment_source);
}

void VirtualCardEnrollBubbleControllerImpl::ReshowBubble() {
  DCHECK(IsIconVisible());

  if (bubble_view()) {
    return;
  }

  is_user_gesture_ = true;
  Show();
}

void VirtualCardEnrollBubbleControllerImpl::ShowConfirmationBubbleView(
    PaymentsRpcResult result) {
#if BUILDFLAG(IS_ANDROID)
  if (autofill_vcn_enroll_bottom_sheet_bridge_) {
    autofill_vcn_enroll_bottom_sheet_bridge_->Hide();
  }
#else  // !BUILDFLAG(IS_ANDROID)
  HideIconAndBubble();
  if (result == PaymentsRpcResult::kClientSideTimeout) {
    return;
  }
  enrollment_status_ = EnrollmentStatus::kCompleted;
  confirmation_ui_params_ =
      result == PaymentsRpcResult::kSuccess
          ? SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
                CreateForVirtualCardSuccess()
          : SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
                CreateForVirtualCardFailure(
                    /*card_label=*/ui_model_->enrollment_fields()
                        .credit_card.NetworkAndLastFourDigits());
  // Show enrollment confirmation bubble.
  Show();
#endif
}

const VirtualCardEnrollUiModel&
VirtualCardEnrollBubbleControllerImpl::GetUiModel() const {
  return *ui_model_.get();
}

VirtualCardEnrollmentBubbleSource
VirtualCardEnrollBubbleControllerImpl::GetVirtualCardEnrollmentBubbleSource()
    const {
  return ConvertToVirtualCardEnrollmentBubbleSource(
      ui_model_->enrollment_fields().virtual_card_enrollment_source);
}

AutofillBubbleBase*
VirtualCardEnrollBubbleControllerImpl::GetVirtualCardBubbleView() const {
  return bubble_view();
}

#if !BUILDFLAG(IS_ANDROID)
void VirtualCardEnrollBubbleControllerImpl::HideIconAndBubble() {
  HideBubble();
  ResetBubble();
  UpdatePageActionIcon();
}

bool VirtualCardEnrollBubbleControllerImpl::IsEnrollmentInProgress() const {
  return enrollment_status_ == EnrollmentStatus::kPaymentsServerRequestInFlight;
}

bool VirtualCardEnrollBubbleControllerImpl::IsEnrollmentComplete() const {
  return enrollment_status_ == EnrollmentStatus::kCompleted;
}
#endif

void VirtualCardEnrollBubbleControllerImpl::OnAcceptButton(
    bool did_switch_to_loading_state) {
  std::move(accept_virtual_card_callback_).Run();
  decline_virtual_card_callback_.Reset();

#if !BUILDFLAG(IS_ANDROID)
  if (did_switch_to_loading_state) {
    // When user clicks "Accept", the bubble closing is delayed since we wait
    // for the enrollment to finish on the server.
    enrollment_status_ = EnrollmentStatus::kPaymentsServerRequestInFlight;
    LogVirtualCardEnrollmentLoadingViewShown(/*is_shown=*/true);

    // Log metrics here instead of when the bubble is closed. When
    // "did_switch_to_loading_state == true" we don't immediately close the
    // bubble, so this ensures we don't have to wait for a future closure to log
    // the user's acceptance.
    VirtualCardEnrollMetricsLogger::OnDismissed(
        VirtualCardEnrollmentBubbleResult::
            VIRTUAL_CARD_ENROLLMENT_BUBBLE_ACCEPTED,
        ui_model_->enrollment_fields().virtual_card_enrollment_source,
        is_user_gesture_, ui_model_->enrollment_fields().previously_declined);
  } else {
    bubble_state_ = BubbleState::kHidden;
    LogVirtualCardEnrollmentLoadingViewShown(/*is_shown=*/false);
  }
#endif
}

void VirtualCardEnrollBubbleControllerImpl::OnDeclineButton() {
  std::move(decline_virtual_card_callback_).Run();
  accept_virtual_card_callback_.Reset();

#if !BUILDFLAG(IS_ANDROID)
  bubble_state_ = BubbleState::kHidden;
#endif
}

void VirtualCardEnrollBubbleControllerImpl::OnLinkClicked(
    VirtualCardEnrollmentLinkType link_type,
    const GURL& url) {
  reprompt_required_ = true;

  VirtualCardEnrollMetricsLogger::OnLinkClicked(
      link_type, ui_model_->enrollment_fields().virtual_card_enrollment_source);

  web_contents()->OpenURL(
      content::OpenURLParams(url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});

#if !BUILDFLAG(IS_ANDROID)
  bubble_state_ = BubbleState::kShowingIconAndBubble;
#endif
}

void VirtualCardEnrollBubbleControllerImpl::OnBubbleClosed(
    PaymentsBubbleClosedReason closed_reason) {
  set_bubble_view(nullptr);
  UpdatePageActionIcon();

  // If the dialog is to be shown again because user clicked on links, do not
  // log metrics.
  if (reprompt_required_) {
    return;
  }

  auto get_metric = [](PaymentsBubbleClosedReason reason) {
    switch (reason) {
      case PaymentsBubbleClosedReason::kAccepted:
        return VirtualCardEnrollmentBubbleResult::
            VIRTUAL_CARD_ENROLLMENT_BUBBLE_ACCEPTED;
      case PaymentsBubbleClosedReason::kCancelled:
        return VirtualCardEnrollmentBubbleResult::
            VIRTUAL_CARD_ENROLLMENT_BUBBLE_CANCELLED;
      case PaymentsBubbleClosedReason::kClosed:
        return VirtualCardEnrollmentBubbleResult::
            VIRTUAL_CARD_ENROLLMENT_BUBBLE_CLOSED;
      case PaymentsBubbleClosedReason::kNotInteracted:
        return VirtualCardEnrollmentBubbleResult::
            VIRTUAL_CARD_ENROLLMENT_BUBBLE_NOT_INTERACTED;
      case PaymentsBubbleClosedReason::kLostFocus:
        return VirtualCardEnrollmentBubbleResult::
            VIRTUAL_CARD_ENROLLMENT_BUBBLE_LOST_FOCUS;
      case PaymentsBubbleClosedReason::kUnknown:
        return VirtualCardEnrollmentBubbleResult::
            VIRTUAL_CARD_ENROLLMENT_BUBBLE_RESULT_UNKNOWN;
    }
  };

  const bool result_metric_already_recorded = [&] {
#if BUILDFLAG(IS_ANDROID)
    return false;
#else
    switch (enrollment_status_) {
      case EnrollmentStatus::kPaymentsServerRequestInFlight:
        LogVirtualCardEnrollmentLoadingViewResult(get_metric(closed_reason));
        return true;
      case EnrollmentStatus::kCompleted:
        LogVirtualCardEnrollmentConfirmationViewResult(
            get_metric(closed_reason), confirmation_ui_params_->is_success);
        return true;
      case EnrollmentStatus::kNone:
        return false;
    }
    NOTREACHED();
#endif
  }();

  // If the result metric wasn't already recorded, record it here.
  if (!result_metric_already_recorded) {
    VirtualCardEnrollMetricsLogger::OnDismissed(
        get_metric(closed_reason),
        ui_model_->enrollment_fields().virtual_card_enrollment_source,
        is_user_gesture_, ui_model_->enrollment_fields().previously_declined);
  }

#if !BUILDFLAG(IS_ANDROID)
  // If the bubble is closed with the enrollment_status_ as
  // kCompleted, hide the bubble and icon and reset bubble to its initial
  // state.
  if (enrollment_status_ == EnrollmentStatus::kCompleted) {
    ResetBubble();
    UpdatePageActionIcon();
  }
#endif
}

base::OnceCallback<void(PaymentsBubbleClosedReason)>
VirtualCardEnrollBubbleControllerImpl::GetOnBubbleClosedCallback() {
  return base::BindOnce(&VirtualCardEnrollBubbleControllerImpl::OnBubbleClosed,
                        weak_ptr_factory_.GetWeakPtr());
}

const SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&
VirtualCardEnrollBubbleControllerImpl::GetConfirmationUiParams() const {
  CHECK(confirmation_ui_params_.has_value());
  return confirmation_ui_params_.value();
}

bool VirtualCardEnrollBubbleControllerImpl::IsIconVisible() const {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return bubble_state_ != BubbleState::kHidden;
#endif
}

void VirtualCardEnrollBubbleControllerImpl::OnVisibilityChanged(
    content::Visibility visibility) {
#if !BUILDFLAG(IS_ANDROID)
  if (visibility == content::Visibility::VISIBLE && !bubble_view() &&
      bubble_state_ == BubbleState::kShowingIconAndBubble) {
    Show();
  } else if (visibility == content::Visibility::HIDDEN) {
    HideBubble();
    if (bubble_state_ != BubbleState::kShowingIcon) {
      bubble_state_ = BubbleState::kHidden;
    }
  }
#endif
}

PageActionIconType
VirtualCardEnrollBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kVirtualCardEnroll;
}

void VirtualCardEnrollBubbleControllerImpl::DoShowBubble() {
#if BUILDFLAG(IS_ANDROID)
  auto delegate_mobile =
      std::make_unique<AutofillVirtualCardEnrollmentInfoBarDelegateMobile>(
          this);
  autofill_vcn_enroll_bottom_sheet_bridge_ =
      std::make_unique<AutofillVCNEnrollBottomSheetBridge>();
  autofill_vcn_enroll_bottom_sheet_bridge_->RequestShowContent(
      web_contents(), std::move(delegate_mobile));
#else
  // If bubble is already showing for another card, close it.
  if (bubble_view()) {
    HideBubble();
  }

  bubble_state_ = BubbleState::kShowingIconAndBubble;
  if (!IsWebContentsActive()) {
    return;
  }

  Browser* browser = chrome::FindBrowserWithTab(web_contents());

  if (enrollment_status_ == EnrollmentStatus::kCompleted) {
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableVcnEnrollLoadingAndConfirmation)) {
      set_bubble_view(
          browser->window()
              ->GetAutofillBubbleHandler()
              ->ShowVirtualCardEnrollConfirmationBubble(web_contents(), this));
      LogVirtualCardEnrollmentConfirmationViewShown(
          /*is_shown=*/true, confirmation_ui_params_->is_success);
    } else {
      LogVirtualCardEnrollmentConfirmationViewShown(
          /*is_shown=*/false, confirmation_ui_params_->is_success);
    }
  } else {
    // For reprompts after link clicks, `is_user_gesture` is set to false.
    bool user_gesture_reprompt = reprompt_required_ ? false : is_user_gesture_;

    set_bubble_view(browser->window()
                        ->GetAutofillBubbleHandler()
                        ->ShowVirtualCardEnrollBubble(web_contents(), this,
                                                      user_gesture_reprompt));
  }
  DCHECK(bubble_view());
  // Update |bubble_state_| after bubble is shown once. In OnVisibilityChanged()
  // we display the bubble if the the state is kShowingIconAndBubble. Once we
  // open the bubble here once, we set |bubble_state_| to kShowingIcon to make
  // sure further OnVisibilityChanged() don't trigger opening the bubble because
  // we don't want to re-show it every time the web contents become visible.
  bubble_state_ = BubbleState::kShowingIcon;

  // Metrics for showing virtual card enroll bubble are logged once when
  // enrollment is offered, do not log the same metrics again while showing
  // confirmation bubble.
  if (enrollment_status_ == EnrollmentStatus::kCompleted) {
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // If the dialog is to be shown again because user clicked on links, do not
  // log metrics.
  if (!reprompt_required_) {
    VirtualCardEnrollMetricsLogger::OnShown(
        ui_model_->enrollment_fields().virtual_card_enrollment_source,
        is_user_gesture_);
  }

  // Reset value for the next time tab is switched.
  reprompt_required_ = false;

  if (bubble_shown_closure_for_testing_) {
    bubble_shown_closure_for_testing_.Run();
  }
}

#if !BUILDFLAG(IS_ANDROID)
bool VirtualCardEnrollBubbleControllerImpl::IsWebContentsActive() {
  Browser* active_browser = chrome::FindBrowserWithActiveWindow();
  if (!active_browser) {
    return false;
  }

  return active_browser->tab_strip_model()->GetActiveWebContents() ==
         web_contents();
}

void VirtualCardEnrollBubbleControllerImpl::ResetBubble() {
  bubble_state_ = BubbleState::kHidden;
  enrollment_status_ = EnrollmentStatus::kNone;
  confirmation_ui_params_.reset();
}
#endif

WEB_CONTENTS_USER_DATA_KEY_IMPL(VirtualCardEnrollBubbleControllerImpl);

}  // namespace autofill
