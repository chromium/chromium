// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_virtual_card_enrollment_infobar_delegate_mobile.h"

#include <utility>

#include "base/notreached.h"
#include "components/grit/components_scaled_resources.h"
#include "url/gurl.h"

namespace autofill {

AutofillVirtualCardEnrollmentInfoBarDelegateMobile::
    AutofillVirtualCardEnrollmentInfoBarDelegateMobile(
        VirtualCardEnrollBubbleController*
            virtual_card_enroll_bubble_controller)
    : ConfirmInfoBarDelegate(),
      virtual_card_enroll_bubble_controller_(
          virtual_card_enroll_bubble_controller) {}

AutofillVirtualCardEnrollmentInfoBarDelegateMobile::
    ~AutofillVirtualCardEnrollmentInfoBarDelegateMobile() {
  if (!had_user_interaction_) {
    OnInfobarClosed(PaymentsBubbleClosedReason::kNotInteracted);
  }
}

// static
AutofillVirtualCardEnrollmentInfoBarDelegateMobile*
AutofillVirtualCardEnrollmentInfoBarDelegateMobile::FromInfobarDelegate(
    infobars::InfoBarDelegate* delegate) {
  return delegate->GetIdentifier() ==
                 AUTOFILL_VIRTUAL_CARD_ENROLLMENT_INFOBAR_DELEGATE_MOBILE
             ? static_cast<AutofillVirtualCardEnrollmentInfoBarDelegateMobile*>(
                   delegate)
             : nullptr;
}

std::u16string
AutofillVirtualCardEnrollmentInfoBarDelegateMobile::GetDescriptionText() const {
  return virtual_card_enroll_bubble_controller_->GetExplanatoryMessage();
}

std::u16string
AutofillVirtualCardEnrollmentInfoBarDelegateMobile::GetLearnMoreLinkText()
    const {
  return virtual_card_enroll_bubble_controller_->GetLearnMoreLinkText();
}

const gfx::ImageSkia*
AutofillVirtualCardEnrollmentInfoBarDelegateMobile::GetIssuerIcon() const {
  return virtual_card_enroll_bubble_controller_
      ->GetVirtualCardEnrollmentFields()
      .card_art_image;
}

std::u16string
AutofillVirtualCardEnrollmentInfoBarDelegateMobile::GetCardLabel() const {
  return virtual_card_enroll_bubble_controller_
      ->GetVirtualCardEnrollmentFields()
      .credit_card.CardNameAndLastFourDigits();
}

LegalMessageLines
AutofillVirtualCardEnrollmentInfoBarDelegateMobile::GetGoogleLegalMessage()
    const {
  return virtual_card_enroll_bubble_controller_
      ->GetVirtualCardEnrollmentFields()
      .google_legal_message;
}

LegalMessageLines
AutofillVirtualCardEnrollmentInfoBarDelegateMobile::GetIssuerLegalMessage()
    const {
  return virtual_card_enroll_bubble_controller_
      ->GetVirtualCardEnrollmentFields()
      .issuer_legal_message;
}

void AutofillVirtualCardEnrollmentInfoBarDelegateMobile::OnInfobarLinkClicked(
    GURL url,
    VirtualCardEnrollmentLinkType link_type) {
  virtual_card_enroll_bubble_controller_->OnLinkClicked(link_type, url);
}

VirtualCardEnrollmentBubbleSource
AutofillVirtualCardEnrollmentInfoBarDelegateMobile::
    GetVirtualCardEnrollmentBubbleSource() {
  return virtual_card_enroll_bubble_controller_
      ->GetVirtualCardEnrollmentBubbleSource();
}

infobars::InfoBarDelegate::InfoBarIdentifier
AutofillVirtualCardEnrollmentInfoBarDelegateMobile::GetIdentifier() const {
  return AUTOFILL_VIRTUAL_CARD_ENROLLMENT_INFOBAR_DELEGATE_MOBILE;
}

int AutofillVirtualCardEnrollmentInfoBarDelegateMobile::GetIconId() const {
  return IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER;
}

std::u16string
AutofillVirtualCardEnrollmentInfoBarDelegateMobile::GetMessageText() const {
  return virtual_card_enroll_bubble_controller_->GetWindowTitle();
}

int AutofillVirtualCardEnrollmentInfoBarDelegateMobile::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

std::u16string
AutofillVirtualCardEnrollmentInfoBarDelegateMobile::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK) {
    return virtual_card_enroll_bubble_controller_->GetAcceptButtonText();
  }
  if (button == BUTTON_CANCEL) {
    return virtual_card_enroll_bubble_controller_->GetDeclineButtonText();
  }
  NOTREACHED() << "Unsupported button label requested.";
  return std::u16string();
}

void AutofillVirtualCardEnrollmentInfoBarDelegateMobile::InfoBarDismissed() {
  OnInfobarClosed(PaymentsBubbleClosedReason::kCancelled);
  virtual_card_enroll_bubble_controller_->OnDeclineButton();
}

bool AutofillVirtualCardEnrollmentInfoBarDelegateMobile::Cancel() {
  OnInfobarClosed(PaymentsBubbleClosedReason::kCancelled);
  virtual_card_enroll_bubble_controller_->OnDeclineButton();
  return true;
}

bool AutofillVirtualCardEnrollmentInfoBarDelegateMobile::Accept() {
  OnInfobarClosed(PaymentsBubbleClosedReason::kAccepted);
  virtual_card_enroll_bubble_controller_->OnAcceptButton();
  return true;
}

void AutofillVirtualCardEnrollmentInfoBarDelegateMobile::OnInfobarClosed(
    PaymentsBubbleClosedReason closed_reason) {
  DCHECK(!had_user_interaction_);

  virtual_card_enroll_bubble_controller_->OnBubbleClosed(closed_reason);
  had_user_interaction_ = true;
}

}  // namespace autofill
