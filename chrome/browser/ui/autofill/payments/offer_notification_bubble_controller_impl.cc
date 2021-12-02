// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"

#include <string>

#include "chrome/browser/commerce/commerce_feature_list.h"
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
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
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
  if (coupon_service_)
    coupon_service_observation_.Observe(coupon_service_);
}

std::u16string OfferNotificationBubbleControllerImpl::GetWindowTitle() const {
  switch (offer_->GetOfferType()) {
    case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_LINKED_OFFER_REMINDER_TITLE);
    case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PROMO_CODE_OFFERS_REMINDER_TITLE);
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED();
      return std::u16string();
  }
}

std::u16string OfferNotificationBubbleControllerImpl::GetOkButtonLabel() const {
  DCHECK_EQ(offer_->GetOfferType(),
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
  return offer_;
}

bool OfferNotificationBubbleControllerImpl::IsIconVisible() const {
  return !origins_to_display_bubble_.empty();
}

void OfferNotificationBubbleControllerImpl::OnBubbleClosed(
    PaymentsBubbleClosedReason closed_reason) {
  set_bubble_view(nullptr);
  promo_code_button_clicked_ = false;
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
  AutofillMetrics::LogOfferNotificationBubbleResultMetric(
      offer_->GetOfferType(), metric, is_user_gesture_);
}

void OfferNotificationBubbleControllerImpl::OnPromoCodeButtonClicked() {
  promo_code_button_clicked_ = true;

  AutofillMetrics::LogOfferNotificationBubblePromoCodeButtonClicked(
      offer_->GetOfferType());
}

void OfferNotificationBubbleControllerImpl::ShowOfferNotificationIfApplicable(
    const AutofillOfferData* offer,
    const CreditCard* card) {
  DCHECK(offer);
  offer_ = offer;
  // If icon/bubble is already visible, that means we have already shown a
  // notification for this page.
  if (IsIconVisible() || bubble_view())
    return;

  origins_to_display_bubble_.clear();
  for (auto merchant_origin : offer_->merchant_origins)
    origins_to_display_bubble_.emplace_back(merchant_origin);

  if (card)
    card_ = *card;

  if (offer->GetOfferType() ==
      AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER) {
    base::Time last_display_time =
        coupon_service_->GetCouponDisplayTimestamp(*offer);
    if (!last_display_time.is_null() &&
        (base::Time::Now() - last_display_time) <
            commerce::kCouponDisplayInterval.Get()) {
      UpdatePageActionIcon();
      AutofillMetrics::LogOfferNotificationBubbleSuppressed(
          AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER);
      return;
    }
    // This will update the offer's last shown time both in cache layer and
    // storage.
    coupon_service_->RecordCouponDisplayTimestamp(*offer);
  }
  is_user_gesture_ = false;
  bubble_shown_timestamp_ = AutofillClock::Now();

  Show();
}

void OfferNotificationBubbleControllerImpl::ReshowBubble() {
  DCHECK(IsIconVisible());
  if (bubble_view())
    return;

  is_user_gesture_ = true;
  bubble_shown_timestamp_ = AutofillClock::Now();
  Show();
}

void OfferNotificationBubbleControllerImpl::OnCouponInvalidated(
    const autofill::AutofillOfferData& offer_data) {
  if (!offer_ || *offer_ != offer_data)
    return;
  ClearCurrentOffer();
}

void OfferNotificationBubbleControllerImpl::PrimaryPageChanged(
    content::Page& page) {
  // If user is still on an eligible domain for the offer, remove bubble but
  // keep omniicon.
  if (base::ranges::count(origins_to_display_bubble_,
                          page.GetMainDocument()
                              .GetLastCommittedURL()
                              .DeprecatedGetOriginAsURL())) {
    // Only remove bubble if the user has had enough time to view it.
    const base::TimeDelta elapsed_time =
        AutofillClock::Now() - bubble_shown_timestamp_;
    if (elapsed_time < kAutofillBubbleSurviveNavigationTime)
      return;
    // Hide the bubble as we only show on the first page with the eligible
    // offer.
    HideBubble();
    return;
  }
  // Reset variables.
  ClearCurrentOffer();
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

  AutofillMetrics::LogOfferNotificationBubbleOfferMetric(offer_->GetOfferType(),
                                                         is_user_gesture_);
}

bool OfferNotificationBubbleControllerImpl::IsWebContentsActive() {
  Browser* active_browser = chrome::FindBrowserWithActiveWindow();
  if (!active_browser)
    return false;

  return active_browser->tab_strip_model()->GetActiveWebContents() ==
         web_contents();
}

void OfferNotificationBubbleControllerImpl::ClearCurrentOffer() {
  origins_to_display_bubble_.clear();
  UpdatePageActionIcon();
  HideBubble();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OfferNotificationBubbleControllerImpl);

}  // namespace autofill
