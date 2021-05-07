// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/virtual_card_manual_fallback_bubble_controller_impl.h"

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

// static
VirtualCardManualFallbackBubbleController*
VirtualCardManualFallbackBubbleController::GetOrCreate(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  VirtualCardManualFallbackBubbleControllerImpl::CreateForWebContents(
      web_contents);
  return VirtualCardManualFallbackBubbleControllerImpl::FromWebContents(
      web_contents);
}

// static
VirtualCardManualFallbackBubbleController*
VirtualCardManualFallbackBubbleController::Get(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  return VirtualCardManualFallbackBubbleControllerImpl::FromWebContents(
      web_contents);
}

VirtualCardManualFallbackBubbleControllerImpl::
    ~VirtualCardManualFallbackBubbleControllerImpl() = default;

void VirtualCardManualFallbackBubbleControllerImpl::ShowBubble(
    const CreditCard* virtual_card,
    const std::u16string& virtual_card_cvc) {
  // If another bubble is visible, dismiss it and show a new one since the card
  // information can be different.
  if (bubble_view())
    HideBubble();

  virtual_card_ = *virtual_card;
  virtual_card_cvc_ = virtual_card_cvc;
  is_user_gesture_ = false;
  should_icon_be_visible_ = true;
  Show();
}

void VirtualCardManualFallbackBubbleControllerImpl::ReshowBubble() {
  // If bubble is already visible, return early.
  if (bubble_view())
    return;

  is_user_gesture_ = true;
  should_icon_be_visible_ = true;
  Show();
}

AutofillBubbleBase* VirtualCardManualFallbackBubbleControllerImpl::GetBubble()
    const {
  return bubble_view();
}

std::u16string VirtualCardManualFallbackBubbleControllerImpl::GetBubbleTitle()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_TITLE);
}

std::u16string
VirtualCardManualFallbackBubbleControllerImpl::GetVirtualCardNumberFieldLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CARD_NUMBER_LABEL);
}

std::u16string
VirtualCardManualFallbackBubbleControllerImpl::GetExpirationDateFieldLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_EXP_DATE_LABEL);
}

std::u16string VirtualCardManualFallbackBubbleControllerImpl::GetCvcFieldLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CVC_LABEL);
}

std::u16string VirtualCardManualFallbackBubbleControllerImpl::GetCvc() const {
  return virtual_card_cvc_;
}

const CreditCard*
VirtualCardManualFallbackBubbleControllerImpl::GetVirtualCard() const {
  return &virtual_card_;
}

bool VirtualCardManualFallbackBubbleControllerImpl::ShouldIconBeVisible()
    const {
  return should_icon_be_visible_;
}

void VirtualCardManualFallbackBubbleControllerImpl::OnBubbleClosed(
    PaymentsBubbleClosedReason closed_reason) {
  set_bubble_view(nullptr);
  UpdatePageActionIcon();
}

VirtualCardManualFallbackBubbleControllerImpl::
    VirtualCardManualFallbackBubbleControllerImpl(
        content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents) {}

void VirtualCardManualFallbackBubbleControllerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  // Don't react to same-document (fragment) navigations.
  if (navigation_handle->IsSameDocument())
    return;

  should_icon_be_visible_ = false;
  UpdatePageActionIcon();
  HideBubble();
}

PageActionIconType
VirtualCardManualFallbackBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kVirtualCardManualFallback;
}

void VirtualCardManualFallbackBubbleControllerImpl::DoShowBubble() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  set_bubble_view(browser->window()
                      ->GetAutofillBubbleHandler()
                      ->ShowVirtualCardManualFallbackBubble(
                          web_contents(), this, is_user_gesture_));
  DCHECK(bubble_view());

  if (observer_for_test_)
    observer_for_test_->OnBubbleShown();
}

void VirtualCardManualFallbackBubbleControllerImpl::SetEventObserverForTesting(
    ObserverForTest* observer_for_test) {
  observer_for_test_ = observer_for_test;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(VirtualCardManualFallbackBubbleControllerImpl)

}  // namespace autofill
