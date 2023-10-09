// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "chrome/browser/mandatory_reauth/android/internal/jni/MandatoryReauthOptInBottomSheetControllerBridge_jni.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

MandatoryReauthBubbleControllerImpl::MandatoryReauthBubbleControllerImpl(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<MandatoryReauthBubbleControllerImpl>(
          *web_contents) {}

MandatoryReauthBubbleControllerImpl::~MandatoryReauthBubbleControllerImpl() {
#if BUILDFLAG(IS_ANDROID)
  // The view is closed by the AutofillBubbleControllerBase base class.
  if (java_controller_bridge_) {
    Java_MandatoryReauthOptInBottomSheetControllerBridge_destroy(
        base::android::AttachCurrentThread(), java_controller_bridge_);
  }
#endif
}

void MandatoryReauthBubbleControllerImpl::ShowBubble(
    base::OnceClosure accept_mandatory_reauth_callback,
    base::OnceClosure cancel_mandatory_reauth_callback,
    base::RepeatingClosure close_mandatory_reauth_callback) {
  if (bubble_view()) {
    return;
  }

  is_reshow_ = false;
  accept_mandatory_reauth_callback_ =
      std::move(accept_mandatory_reauth_callback);
  cancel_mandatory_reauth_callback_ =
      std::move(cancel_mandatory_reauth_callback);
  close_mandatory_reauth_callback_ = std::move(close_mandatory_reauth_callback);
  current_bubble_type_ = MandatoryReauthBubbleType::kOptIn;
  autofill_metrics::LogMandatoryReauthOptInBubbleOffer(
      autofill_metrics::MandatoryReauthOptInBubbleOffer::kShown,
      /*is_reshow=*/false);

  Show();
}

void MandatoryReauthBubbleControllerImpl::ReshowBubble() {
  // Don't show the bubble if it's already visible.
  if (bubble_view()) {
    return;
  }

  is_reshow_ = true;
  if (current_bubble_type_ == MandatoryReauthBubbleType::kOptIn) {
    // Callbacks are verified here, but not the following else if block, because
    // they can only be invoked from the opt-in bubble.
    CHECK(accept_mandatory_reauth_callback_ &&
          cancel_mandatory_reauth_callback_ &&
          close_mandatory_reauth_callback_);
    autofill_metrics::LogMandatoryReauthOptInBubbleOffer(
        autofill_metrics::MandatoryReauthOptInBubbleOffer::kShown,
        /*is_reshow=*/true);
  } else if (current_bubble_type_ == MandatoryReauthBubbleType::kConfirmation) {
    // The confirmation bubble cannot be minimized, so it's safe to increment
    // the shown metric when it is reshown, which is the way it transitions
    // from opt-in to confirmation.
    autofill_metrics::LogMandatoryReauthOptInConfirmationBubbleMetric(
        autofill_metrics::MandatoryReauthOptInConfirmationBubbleMetric::kShown);
  }

  Show();
}

std::u16string MandatoryReauthBubbleControllerImpl::GetWindowTitle() const {
  switch (current_bubble_type_) {
    case MandatoryReauthBubbleType::kOptIn:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_MANDATORY_REAUTH_OPT_IN_TITLE);
    case MandatoryReauthBubbleType::kConfirmation:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_MANDATORY_REAUTH_CONFIRMATION_TITLE);
    case MandatoryReauthBubbleType::kInactive:
      return std::u16string();
  }
}

std::u16string MandatoryReauthBubbleControllerImpl::GetAcceptButtonText()
    const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_MANDATORY_REAUTH_OPT_IN_ACCEPT);
}

std::u16string MandatoryReauthBubbleControllerImpl::GetCancelButtonText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_MANDATORY_REAUTH_OPT_IN_NO_THANKS);
}

std::u16string MandatoryReauthBubbleControllerImpl::GetExplanationText() const {
  switch (current_bubble_type_) {
    case MandatoryReauthBubbleType::kOptIn:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_MANDATORY_REAUTH_OPT_IN_EXPLANATION);
    case MandatoryReauthBubbleType::kConfirmation:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_MANDATORY_REAUTH_CONFIRMATION_EXPLANATION);
    case MandatoryReauthBubbleType::kInactive:
      return std::u16string();
  }
}

void MandatoryReauthBubbleControllerImpl::OnBubbleClosed(
    PaymentsBubbleClosedReason closed_reason) {
  set_bubble_view(nullptr);

// After resetting the raw pointer to the view in the base class, the Android
// view has to be deleted.
#if BUILDFLAG(IS_ANDROID)
  view_android_.reset();
#endif

  if (current_bubble_type_ == MandatoryReauthBubbleType::kOptIn) {
    autofill_metrics::MandatoryReauthOptInBubbleResult metric =
        autofill_metrics::MandatoryReauthOptInBubbleResult::kUnknown;
    switch (closed_reason) {
      case PaymentsBubbleClosedReason::kAccepted:
        metric = autofill_metrics::MandatoryReauthOptInBubbleResult::kAccepted;
        // We must set the `current_bubble_type_` before running the callback,
        // as the callback is not always asynchronous (for example, in the case
        // where the user is automatically authenticated due to being within a
        // certain grace period of time from the previous authentication). In
        // these cases, we might re-show the opt-in bubble again if we don't set
        // the `current_bubble_type_` first.
        current_bubble_type_ = MandatoryReauthBubbleType::kConfirmation;
        std::move(accept_mandatory_reauth_callback_).Run();
        break;
      case PaymentsBubbleClosedReason::kCancelled:
        metric = autofill_metrics::MandatoryReauthOptInBubbleResult::kCancelled;
        // We must set the `current_bubble_type_` before running the callback,
        // as the callback is not always asynchronous (for example, in the case
        // where the user is automatically authenticated due to being within a
        // certain grace period of time from the previous authentication). In
        // these cases, we might re-show the opt-in bubble again if we don't set
        // the `current_bubble_type_` first.
        current_bubble_type_ = MandatoryReauthBubbleType::kInactive;
        std::move(cancel_mandatory_reauth_callback_).Run();
        break;
      case PaymentsBubbleClosedReason::kClosed:
        metric = autofill_metrics::MandatoryReauthOptInBubbleResult::kClosed;
        close_mandatory_reauth_callback_.Run();
        break;
      case PaymentsBubbleClosedReason::kNotInteracted:
        metric =
            autofill_metrics::MandatoryReauthOptInBubbleResult::kNotInteracted;
        break;
      case PaymentsBubbleClosedReason::kLostFocus:
        metric = autofill_metrics::MandatoryReauthOptInBubbleResult::kLostFocus;
        break;
      case PaymentsBubbleClosedReason::kUnknown:
        metric = autofill_metrics::MandatoryReauthOptInBubbleResult::kUnknown;
        break;
    }
    DCHECK(metric !=
           autofill_metrics::MandatoryReauthOptInBubbleResult::kUnknown);
    autofill_metrics::LogMandatoryReauthOptInBubbleResult(metric, is_reshow_);
  } else {
    current_bubble_type_ = MandatoryReauthBubbleType::kInactive;
  }

  UpdatePageActionIcon();
}

#if BUILDFLAG(IS_ANDROID)
void MandatoryReauthBubbleControllerImpl::OnClosed(JNIEnv* env,
                                                   jint closed_reason) {
  OnBubbleClosed(
      static_cast<autofill::PaymentsBubbleClosedReason>(closed_reason));
}
#endif

AutofillBubbleBase* MandatoryReauthBubbleControllerImpl::GetBubbleView() {
  return bubble_view();
}

bool MandatoryReauthBubbleControllerImpl::IsIconVisible() {
  return current_bubble_type_ != MandatoryReauthBubbleType::kInactive;
}

MandatoryReauthBubbleType MandatoryReauthBubbleControllerImpl::GetBubbleType()
    const {
  return current_bubble_type_;
}

PageActionIconType
MandatoryReauthBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kMandatoryReauth;
}

void MandatoryReauthBubbleControllerImpl::DoShowBubble() {
#if BUILDFLAG(IS_ANDROID)
  // The Android view's lifecycle is managed by this controller. We also
  // register it as a raw pointer in the base class to use its closing logic
  // when this controller wants to close it.
  view_android_ =
      MandatoryReauthOptInViewAndroid::CreateAndShow(web_contents(), this);
  if (!view_android_) {
    java_controller_bridge_.Reset();
    return;
  }
  set_bubble_view(view_android_.get());
#else
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  AutofillBubbleHandler* autofill_bubble_handler =
      browser->window()->GetAutofillBubbleHandler();
  set_bubble_view(autofill_bubble_handler->ShowMandatoryReauthBubble(
      web_contents(), this, /*is_user_gesture=*/false, current_bubble_type_));
#endif  // BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
MandatoryReauthBubbleControllerImpl::GetJavaControllerBridge() {
  if (!java_controller_bridge_) {
    java_controller_bridge_ =
        Java_MandatoryReauthOptInBottomSheetControllerBridge_create(
            base::android::AttachCurrentThread(),
            reinterpret_cast<intptr_t>(this));
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_controller_bridge_);
}
#endif

WEB_CONTENTS_USER_DATA_KEY_IMPL(MandatoryReauthBubbleControllerImpl);

}  // namespace autofill
