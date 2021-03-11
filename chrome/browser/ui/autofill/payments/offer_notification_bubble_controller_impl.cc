// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"

#include <string>

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

OfferNotificationBubbleControllerImpl::
    ~OfferNotificationBubbleControllerImpl() = default;

// static
OfferNotificationBubbleController*
OfferNotificationBubbleController::GetOrCreate(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  OfferNotificationBubbleControllerImpl::CreateForWebContents(web_contents);
  return OfferNotificationBubbleControllerImpl::FromWebContents(web_contents);
}

// static
OfferNotificationBubbleController* OfferNotificationBubbleController::Get(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  return OfferNotificationBubbleControllerImpl::FromWebContents(web_contents);
}

OfferNotificationBubbleControllerImpl::OfferNotificationBubbleControllerImpl(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents) {}

std::u16string OfferNotificationBubbleControllerImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_OFFERS_REMINDER_TITLE);
}

std::u16string OfferNotificationBubbleControllerImpl::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_OFFERS_REMINDER_POSITIVE_BUTTON_LABEL);
}

AutofillBubbleBase*
OfferNotificationBubbleControllerImpl::GetOfferNotificationBubbleView() const {
  return bubble_view();
}

const CreditCard* OfferNotificationBubbleControllerImpl::GetLinkedCard() const {
  if (card_.has_value())
    return &(*card_);

  return nullptr;
}

bool OfferNotificationBubbleControllerImpl::IsIconVisible() const {
  return !origins_to_display_bubble_.empty();
}

void OfferNotificationBubbleControllerImpl::OnBubbleClosed(
    PaymentsBubbleClosedReason closed_reason) {
  set_bubble_view(nullptr);
  UpdatePageActionIcon();

  // Log bubble result according to the closed reason.
  AutofillMetrics::OfferNotificationBubbleResultMetric metric;
  switch (closed_reason) {
    case PaymentsBubbleClosedReason::kAccepted:
      metric = AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_ACKNOWLEDGED;
      break;
    case PaymentsBubbleClosedReason::kClosed:
      metric = AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_CLOSED;
      break;
    case PaymentsBubbleClosedReason::kNotInteracted:
      metric = AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_NOT_INTERACTED;
      break;
    case PaymentsBubbleClosedReason::kLostFocus:
      metric = AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_LOST_FOCUS;
      break;
    default:
      NOTREACHED();
      return;
  }
  AutofillMetrics::LogOfferNotificationBubbleResultMetric(metric,
                                                          is_user_gesture_);
}

void OfferNotificationBubbleControllerImpl::ShowOfferNotificationIfApplicable(
    const std::vector<GURL>& origins_to_display_bubble,
    const CreditCard* card) {
  // If icon/bubble is already visible, that means we have already shown a
  // notification for this page.
  if (IsIconVisible() || bubble_view())
    return;

  origins_to_display_bubble_.clear();
  for (auto origin : origins_to_display_bubble)
    origins_to_display_bubble_.emplace_back(origin);

  card_ = *card;

  is_user_gesture_ = false;
  Show();
}

void OfferNotificationBubbleControllerImpl::ReshowBubble() {
  DCHECK(IsIconVisible());
  if (bubble_view())
    return;

  is_user_gesture_ = true;
  Show();
}

void OfferNotificationBubbleControllerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  // Don't react to same-document (fragment) navigations.
  if (navigation_handle->IsSameDocument())
    return;

  // Don't do anything if user is still on an eligible origin for this offer.
  if (base::ranges::count(origins_to_display_bubble_,
                          navigation_handle->GetURL().GetOrigin())) {
    return;
  }

  // Reset variables.
  origins_to_display_bubble_.clear();
  UpdatePageActionIcon();

  // Hide the bubble.
  HideBubble();
}

PageActionIconType
OfferNotificationBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kPaymentsOfferNotification;
}

void OfferNotificationBubbleControllerImpl::DoShowBubble() {
  // TODO(crbug.com/1187190): Add cross-tab status tracking for bubble so we
  // show bubble only once per merchant.
  if (!IsWebContentsActive())
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  set_bubble_view(browser->window()
                      ->GetAutofillBubbleHandler()
                      ->ShowOfferNotificationBubble(web_contents(), this,
                                                    is_user_gesture_));
  DCHECK(bubble_view());

  if (observer_for_testing_)
    observer_for_testing_->OnBubbleShown();

  AutofillMetrics::LogOfferNotificationBubbleOfferMetric(is_user_gesture_);
}

bool OfferNotificationBubbleControllerImpl::IsWebContentsActive() {
  Browser* active_browser = chrome::FindBrowserWithActiveWindow();
  if (!active_browser)
    return false;

  return active_browser->tab_strip_model()->GetActiveWebContents() ==
         web_contents();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OfferNotificationBubbleControllerImpl)

}  // namespace autofill
