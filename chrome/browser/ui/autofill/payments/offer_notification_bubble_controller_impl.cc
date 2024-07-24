// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/commerce/coupons/coupon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/offers_metrics.h"
#include "components/autofill/core/browser/payments/offer_notification_options.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/commerce/core/commerce_feature_list.h"
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
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<OfferNotificationBubbleControllerImpl>(
          *web_contents),
      coupon_service_(CouponServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
  // TODO(crbug.com/40172797): Explore if there is a way to move CouponService
  // out of this file.
  if (coupon_service_)
    coupon_service_observation_.Observe(coupon_service_);
}

std::u16string OfferNotificationBubbleControllerImpl::GetWindowTitle() const {
  switch (offer_.GetOfferType()) {
    case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_LINKED_OFFER_REMINDER_TITLE);
    case AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_GPAY_PROMO_CODE_OFFERS_REMINDER_TITLE);
    case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
  }
}

std::u16string OfferNotificationBubbleControllerImpl::GetOkButtonLabel() const {
  DCHECK_EQ(offer_.GetOfferType(),
            AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER);
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_OFFERS_REMINDER_POSITIVE_BUTTON_LABEL);
}

std::u16string
OfferNotificationBubbleControllerImpl::GetPromoCodeButtonTooltip() const {
  return l10n_util::GetStringUTF16(
      promo_code_button_clicked_
          ? IDS_AUTOFILL_PROMO_CODE_OFFER_BUTTON_TOOLTIP_CLICKED
          : IDS_AUTOFILL_PROMO_CODE_OFFER_BUTTON_TOOLTIP_NORMAL);
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

const AutofillOfferData* OfferNotificationBubbleControllerImpl::GetOffer()
    const {
  return &offer_;
}

bool OfferNotificationBubbleControllerImpl::IsIconVisible() const {
  return bubble_state_ != BubbleState::kHidden;
}

bool OfferNotificationBubbleControllerImpl::ShouldIconExpand() const {
  return icon_should_expand_;
}

void OfferNotificationBubbleControllerImpl::OnIconExpanded() {
  icon_should_expand_ = false;
}

void OfferNotificationBubbleControllerImpl::OnBubbleClosed(
    PaymentsBubbleClosedReason closed_reason) {
  set_bubble_view(nullptr);
  promo_code_button_clicked_ = false;
  UpdatePageActionIcon();

  // Log bubble result according to the closed reason.
  autofill_metrics::OfferNotificationBubbleResultMetric metric;
  switch (closed_reason) {
    case PaymentsBubbleClosedReason::kAccepted:
      metric = autofill_metrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_ACKNOWLEDGED;
      break;
    case PaymentsBubbleClosedReason::kClosed:
      metric = autofill_metrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_CLOSED;
      break;
    case PaymentsBubbleClosedReason::kNotInteracted:
      metric = autofill_metrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_NOT_INTERACTED;
      break;
    case PaymentsBubbleClosedReason::kLostFocus:
      metric = autofill_metrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_LOST_FOCUS;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  autofill_metrics::LogOfferNotificationBubbleResultMetric(
      offer_.GetOfferType(), metric, is_user_gesture_);
}

void OfferNotificationBubbleControllerImpl::ShowOfferNotificationIfApplicable(
    const AutofillOfferData& offer,
    const CreditCard* card,
    const OfferNotificationOptions& options) {
  icon_should_expand_ = options.expand_notification_icon;

  // If this is not the bubble's first show, and offer to be shown has not
  // changed, and it has not been shown for more than
  // kAutofillBubbleSurviveNavigationTime, do not dismiss the bubble.
  if (offer_.GetOfferType() != AutofillOfferData::OfferType::UNKNOWN &&
      offer_ == offer && bubble_shown_timestamp_.has_value() &&
      AutofillClock::Now() - *bubble_shown_timestamp_ <
          kAutofillBubbleSurviveNavigationTime) {
    return;
  }

  offer_ = offer;

  // Hides the old bubble. Sets bubble_state_ to show icon here since we are
  // going to show another bubble anyway.
  HideBubbleAndClearTimestamp(/*should_show_icon=*/true);

  DCHECK(IsIconVisible());

  if (card)
    card_ = *card;

  is_user_gesture_ = false;

  if (options.show_notification_automatically) {
    Show();
  } else {
    HideBubbleAndClearTimestamp(/*should_show_icon=*/true);
  }
}

void OfferNotificationBubbleControllerImpl::ReshowBubble() {
  DCHECK(IsIconVisible());
  if (bubble_view())
    return;

  is_user_gesture_ = true;

  Show();
}

void OfferNotificationBubbleControllerImpl::DismissNotification() {
  HideBubbleAndClearTimestamp(/*should_show_icon=*/false);
}

void OfferNotificationBubbleControllerImpl::OnCouponInvalidated(
    const autofill::AutofillOfferData& offer_data) {
  if (offer_.GetOfferType() == AutofillOfferData::OfferType::UNKNOWN ||
      offer_ != offer_data)
    return;
  DismissNotification();
}

void OfferNotificationBubbleControllerImpl::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE && !bubble_view() &&
      bubble_state_ == BubbleState::kShowingIconAndBubble) {
    Show();
  } else if (visibility == content::Visibility::HIDDEN) {
    HideBubbleAndClearTimestamp(bubble_state_ == BubbleState::kShowingIcon);
  }
}

PageActionIconType
OfferNotificationBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kPaymentsOfferNotification;
}

void OfferNotificationBubbleControllerImpl::DoShowBubble() {
  bubble_state_ = BubbleState::kShowingIconAndBubble;
  // Don't show bubble yet if web content is not active (bubble will instead be
  // shown when web content become visible and active).
  if (!IsWebContentsActive())
    return;

  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  set_bubble_view(browser->window()
                      ->GetAutofillBubbleHandler()
                      ->ShowOfferNotificationBubble(web_contents(), this,
                                                    is_user_gesture_));
  DCHECK(bubble_view());

  // Update |bubble_state_| after bubble is shown once. In OnVisibilityChanged()
  // we display the bubble if the the state is kShowingIconAndBubble. Once we
  // open the bubble here once, we set |bubble_state_| to kShowingIcon to make
  // sure further OnVisibilityChanged() don't trigger opening the bubble because
  // we don't want to re-show it every time the web contents become visible.
  bubble_state_ = BubbleState::kShowingIcon;
  bubble_shown_timestamp_ = AutofillClock::Now();

  if (observer_for_testing_)
    observer_for_testing_->OnBubbleShown();

  autofill_metrics::LogOfferNotificationBubbleOfferMetric(offer_.GetOfferType(),
                                                          is_user_gesture_);
}

bool OfferNotificationBubbleControllerImpl::IsWebContentsActive() {
  Browser* active_browser = chrome::FindBrowserWithActiveWindow();
  if (!active_browser)
    return false;

  return active_browser->tab_strip_model()->GetActiveWebContents() ==
         web_contents();
}

void OfferNotificationBubbleControllerImpl::HideBubbleAndClearTimestamp(
    bool should_show_icon) {
  bubble_state_ =
      should_show_icon ? BubbleState::kShowingIcon : BubbleState::kHidden;
  UpdatePageActionIcon();
  HideBubble();
  bubble_shown_timestamp_ = std::nullopt;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OfferNotificationBubbleControllerImpl);

}  // namespace autofill
