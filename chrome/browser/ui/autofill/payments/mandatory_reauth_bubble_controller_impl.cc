// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller_impl.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace autofill {

MandatoryReauthBubbleControllerImpl::MandatoryReauthBubbleControllerImpl(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<MandatoryReauthBubbleControllerImpl>(
          *web_contents) {}

MandatoryReauthBubbleControllerImpl::~MandatoryReauthBubbleControllerImpl() =
    default;

void MandatoryReauthBubbleControllerImpl::ShowBubble(
    base::OnceClosure accept_mandatory_reauth_callback,
    base::OnceClosure cancel_mandatory_reauth_callback,
    base::RepeatingClosure close_mandatory_reauth_callback) {
  if (bubble_view()) {
    return;
  }

  accept_mandatory_reauth_callback_ =
      std::move(accept_mandatory_reauth_callback);
  cancel_mandatory_reauth_callback_ =
      std::move(cancel_mandatory_reauth_callback);
  close_mandatory_reauth_callback_ = std::move(close_mandatory_reauth_callback);
  current_bubble_type_ = MandatoryReauthBubbleType::kOptIn;

  Show();
}

void MandatoryReauthBubbleControllerImpl::ReshowBubble() {
  // Don't show the bubble if it's already visible.
  if (bubble_view()) {
    return;
  }

  // We don't run any callbacks in the confirmation view, so there's no need to
  // ensure they exist.
  if (current_bubble_type_ == MandatoryReauthBubbleType::kOptIn) {
    CHECK(accept_mandatory_reauth_callback_ &&
          cancel_mandatory_reauth_callback_ &&
          close_mandatory_reauth_callback_);
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

  if (current_bubble_type_ == MandatoryReauthBubbleType::kOptIn) {
    if (closed_reason == PaymentsBubbleClosedReason::kAccepted) {
      std::move(accept_mandatory_reauth_callback_).Run();
      current_bubble_type_ = MandatoryReauthBubbleType::kConfirmation;
    } else if (closed_reason == PaymentsBubbleClosedReason::kCancelled) {
      std::move(cancel_mandatory_reauth_callback_).Run();
      current_bubble_type_ = MandatoryReauthBubbleType::kInactive;
    } else if (closed_reason == PaymentsBubbleClosedReason::kClosed) {
      close_mandatory_reauth_callback_.Run();
    }
  } else {
    current_bubble_type_ = MandatoryReauthBubbleType::kInactive;
  }

  UpdatePageActionIcon();
}

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
#if !BUILDFLAG(IS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  AutofillBubbleHandler* autofill_bubble_handler =
      browser->window()->GetAutofillBubbleHandler();
  set_bubble_view(autofill_bubble_handler->ShowMandatoryReauthBubble(
      web_contents(), this, /*is_user_gesture=*/false, current_bubble_type_));
#endif  // !BUILDFLAG(IS_ANDROID)
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MandatoryReauthBubbleControllerImpl);

}  // namespace autofill
