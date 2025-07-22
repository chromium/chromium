// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_virtual_card_enrollment_bottom_sheet_delegate_mobile.h"

#include <utility>

#include "base/notreached.h"
#include "components/grit/components_scaled_resources.h"
#include "url/gurl.h"

namespace autofill {

AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::
    AutofillVirtualCardEnrollmentBottomSheetDelegateMobile(
        VirtualCardEnrollBubbleController*
            virtual_card_enroll_bubble_controller)
    : virtual_card_enroll_bubble_controller_(
          virtual_card_enroll_bubble_controller) {}

AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::
    ~AutofillVirtualCardEnrollmentBottomSheetDelegateMobile() {
  if (!had_user_interaction_) {
    OnClosed(PaymentsUiClosedReason::kNotInteracted);
  }
}

std::u16string
AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::GetDescriptionText()
    const {
  return virtual_card_enroll_bubble_controller_->GetUiModel()
      .explanatory_message();
}

std::u16string
AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::GetLearnMoreLinkText()
    const {
  return virtual_card_enroll_bubble_controller_->GetUiModel()
      .learn_more_link_text();
}

const gfx::ImageSkia*
AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::GetIssuerIcon() const {
  return virtual_card_enroll_bubble_controller_->GetUiModel()
      .enrollment_fields()
      .card_art_image;
}

int AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::
    GetNetworkIconResourceId() const {
  return CreditCard::IconResourceId(
      virtual_card_enroll_bubble_controller_->GetUiModel()
          .enrollment_fields()
          .credit_card.network());
}

GURL AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::GetIssuerIconUrl()
    const {
  const CreditCard& credit_card =
      virtual_card_enroll_bubble_controller_->GetUiModel()
          .enrollment_fields()
          .credit_card;
  if (credit_card.HasRichCardArtImageFromMetadata()) {
    return credit_card.card_art_url();
  }
  return GURL();
}

std::u16string
AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::GetCardLabel() const {
  return virtual_card_enroll_bubble_controller_->GetUiModel()
      .enrollment_fields()
      .credit_card.CardNameAndLastFourDigits();
}

LegalMessageLines
AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::GetGoogleLegalMessage()
    const {
  return virtual_card_enroll_bubble_controller_->GetUiModel()
      .enrollment_fields()
      .google_legal_message;
}

LegalMessageLines
AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::GetIssuerLegalMessage()
    const {
  return virtual_card_enroll_bubble_controller_->GetUiModel()
      .enrollment_fields()
      .issuer_legal_message;
}

VirtualCardEnrollmentBubbleSource
AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::
    GetVirtualCardEnrollmentBubbleSource() {
  return virtual_card_enroll_bubble_controller_
      ->GetVirtualCardEnrollmentBubbleSource();
}

std::u16string
AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::GetMessageText() const {
  return virtual_card_enroll_bubble_controller_->GetUiModel().window_title();
}

std::u16string
AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::GetAcceptButtonLabel()
    const {
  return virtual_card_enroll_bubble_controller_->GetUiModel()
      .accept_action_text();
}

std::u16string
AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::GetCancelButtonLabel()
    const {
  return virtual_card_enroll_bubble_controller_->GetUiModel()
      .cancel_action_text();
}
void AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::OnDismiss() {
  OnClosed(PaymentsUiClosedReason::kCancelled);
  virtual_card_enroll_bubble_controller_->OnDeclineButton();
}

bool AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::Cancel() {
  OnClosed(PaymentsUiClosedReason::kCancelled);
  virtual_card_enroll_bubble_controller_->OnDeclineButton();
  return true;
}

bool AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::Accept() {
  OnClosed(PaymentsUiClosedReason::kAccepted);
  virtual_card_enroll_bubble_controller_->OnAcceptButton();
  return true;
}

void AutofillVirtualCardEnrollmentBottomSheetDelegateMobile::OnClosed(
    PaymentsUiClosedReason closed_reason) {
  DCHECK(!had_user_interaction_);

  virtual_card_enroll_bubble_controller_->OnBubbleClosed(closed_reason);
  had_user_interaction_ = true;
}

}  // namespace autofill
