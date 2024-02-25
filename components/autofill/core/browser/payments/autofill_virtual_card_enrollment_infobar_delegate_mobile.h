// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_INFOBAR_DELEGATE_MOBILE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_INFOBAR_DELEGATE_MOBILE_H_

#include <memory>
#include <string>

#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/payments/virtual_card_enroll_bubble_controller.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ui/gfx/image/image.h"

namespace autofill {

// An InfoBarDelegate that enables the user to enroll their payment method into
// virtual card. Only used on mobile.
class AutofillVirtualCardEnrollmentInfoBarDelegateMobile
    : public ConfirmInfoBarDelegate {
 public:
  AutofillVirtualCardEnrollmentInfoBarDelegateMobile(
      VirtualCardEnrollBubbleController* virtual_card_enroll_bubble_controller);

  AutofillVirtualCardEnrollmentInfoBarDelegateMobile(
      const AutofillVirtualCardEnrollmentInfoBarDelegateMobile&) = delete;
  AutofillVirtualCardEnrollmentInfoBarDelegateMobile& operator=(
      const AutofillVirtualCardEnrollmentInfoBarDelegateMobile&) = delete;

  ~AutofillVirtualCardEnrollmentInfoBarDelegateMobile() override;

  // Returns |delegate| as an
  // AutofillVirtualCardEnrollmentInfoBarDelegateMobile, or nullptr if it is of
  // another type.
  static AutofillVirtualCardEnrollmentInfoBarDelegateMobile*
  FromInfobarDelegate(infobars::InfoBarDelegate* delegate);

  // Description text to be shown above the card information in the infobar.
  std::u16string GetDescriptionText() const;

  // Text of the learn more link in the description.
  std::u16string GetLearnMoreLinkText() const;

  // Issuer icon for the card.
  const gfx::ImageSkia* GetIssuerIcon() const;

  // The label for the card to show in the content of the infobar.
  std::u16string GetCardLabel() const;

  // The Google-specific legal messages that the user must accept before
  // opting-in to virtual card enrollment.
  LegalMessageLines GetGoogleLegalMessage() const;

  // The Issuer-specific legal messages that the user must accept before
  // opting-in to virtual card enrollment. Empty for some issuers.
  LegalMessageLines GetIssuerLegalMessage() const;

  // Called when a link in the legal message text was clicked.
  virtual void OnInfobarLinkClicked(GURL url,
                                    VirtualCardEnrollmentLinkType link_type);

  // Returns the "source" of the virtual card number enrollment flow, e.g.,
  // "upstream", "downstream", "settings".
  VirtualCardEnrollmentBubbleSource GetVirtualCardEnrollmentBubbleSource();

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  void InfoBarDismissed() override;
  bool Cancel() override;
  bool Accept() override;

 private:
  // Logs metrics via the native controller.
  void OnInfobarClosed(PaymentsBubbleClosedReason closed_reason);

  // Pointer to the native controller.
  raw_ptr<VirtualCardEnrollBubbleController>
      virtual_card_enroll_bubble_controller_;

  // Did the user ever explicitly accept or dismiss this infobar?
  bool had_user_interaction_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_INFOBAR_DELEGATE_MOBILE_H_
