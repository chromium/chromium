// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views_test_base.h"

#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/commerce/coupons/coupon_service.h"
#include "chrome/browser/commerce/coupons/coupon_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/commerce/core/commerce_feature_list.h"

namespace autofill {

const char kDefaultTestPromoCode[] = "5PCTOFFSHOES";

OfferNotificationBubbleViewsTestBase::OfferNotificationBubbleViewsTestBase(
    bool promo_code_flag_enabled) {
  if (promo_code_flag_enabled) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kAutofillEnableOfferNotificationForPromoCodes, {}},
         {commerce::kRetailCoupons,
          {{commerce::kRetailCouponsWithCodeParam, "true"}}}},
        /*disabled_features=*/{});
  } else {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            features::kAutofillEnableOfferNotificationForPromoCodes,
            commerce::kRetailCoupons});
  }
}

OfferNotificationBubbleViewsTestBase::~OfferNotificationBubbleViewsTestBase() =
    default;

void OfferNotificationBubbleViewsTestBase::SetUpOnMainThread() {
  // Set up this class as the ObserverForTest implementation.
  OfferNotificationBubbleControllerImpl* controller =
      static_cast<OfferNotificationBubbleControllerImpl*>(
          OfferNotificationBubbleController::GetOrCreate(
              GetActiveWebContents()));
  AddEventObserverToController(controller);

  personal_data_ =
      PersonalDataManagerFactory::GetForProfile(browser()->profile());
  coupon_service_ = CouponServiceFactory::GetForProfile(browser()->profile());

  // Wait for Personal Data Manager to be fully loaded to prevent that
  // spurious notifications deceive the tests.
  WaitForPersonalDataManagerToBeLoaded(browser()->profile());
}

void OfferNotificationBubbleViewsTestBase::OnBubbleShown() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::BUBBLE_SHOWN);
}

std::unique_ptr<AutofillOfferData>
OfferNotificationBubbleViewsTestBase::CreateCardLinkedOfferDataWithDomains(
    const std::vector<GURL>& domains) {
  auto card = std::make_unique<CreditCard>();
  card->set_instrument_id(kCreditCardInstrumentId);
  personal_data_->AddServerCreditCardForTest(std::move(card));
  personal_data_->NotifyPersonalDataObserver();

  std::unique_ptr<AutofillOfferData> offer_data_entry =
      std::make_unique<AutofillOfferData>();
  offer_data_entry->offer_id = 4444;
  offer_data_entry->offer_reward_amount = "5%";
  offer_data_entry->expiry = AutofillClock::Now() + base::Days(2);
  offer_data_entry->merchant_origins = {};
  for (auto url : domains)
    offer_data_entry->merchant_origins.emplace_back(
        url.DeprecatedGetOriginAsURL());
  offer_data_entry->eligible_instrument_id = {kCreditCardInstrumentId};

  return offer_data_entry;
}

std::unique_ptr<AutofillOfferData>
OfferNotificationBubbleViewsTestBase::CreatePromoCodeOfferDataWithDomains(
    const std::vector<GURL>& domains) {
  std::unique_ptr<AutofillOfferData> offer_data_entry =
      std::make_unique<AutofillOfferData>();
  offer_data_entry->offer_id = 5555;
  offer_data_entry->expiry = AutofillClock::Now() + base::Days(2);
  offer_data_entry->merchant_origins = {};
  for (auto url : domains)
    offer_data_entry->merchant_origins.emplace_back(
        url.DeprecatedGetOriginAsURL());
  offer_data_entry->offer_details_url = GURL("https://www.google.com/");
  offer_data_entry->promo_code = GetDefaultTestPromoCode();
  offer_data_entry->display_strings.value_prop_text =
      "5% off on shoes. Up to $50.";
  offer_data_entry->display_strings.see_details_text = "See details";
  offer_data_entry->display_strings.usage_instructions_text =
      "Click the promo code field at checkout to autofill it.";

  return offer_data_entry;
}

void OfferNotificationBubbleViewsTestBase::DeleteFreeListingCouponForUrl(
    const GURL& url) {
  coupon_service_->DeleteFreeListingCouponsForUrl(url);
}

void OfferNotificationBubbleViewsTestBase::SetUpOfferDataWithDomains(
    AutofillOfferData::OfferType offer_type,
    const std::vector<GURL>& domains) {
  switch (offer_type) {
    case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
      SetUpCardLinkedOfferDataWithDomains(domains);
      break;
    case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
      SetUpFreeListingCouponOfferDataWithDomains(domains);
      break;
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED();
      break;
  }
}

void OfferNotificationBubbleViewsTestBase::SetUpCardLinkedOfferDataWithDomains(
    const std::vector<GURL>& domains) {
  personal_data_->ClearAllServerData();
  // CreateCardLinkedOfferDataWithDomains(~) will add the necessary card.
  personal_data_->AddOfferDataForTest(
      CreateCardLinkedOfferDataWithDomains(domains));
  personal_data_->NotifyPersonalDataObserver();
}

void OfferNotificationBubbleViewsTestBase::
    SetUpFreeListingCouponOfferDataWithDomains(
        const std::vector<GURL>& domains) {
  personal_data_->ClearAllServerData();
  // TODO(crbug.com/1203811): Should distinguish between activated GPay promo
  //     code offers and free-listing coupon offers separately. When that
  //     separation is created in a followup CL, this class should probably
  //     create a `SetUpGPayPromoCodeOfferDataWithDomains(~)` helper.
  personal_data_->AddOfferDataForTest(
      CreatePromoCodeOfferDataWithDomains(domains));
  personal_data_->NotifyPersonalDataObserver();
}

void OfferNotificationBubbleViewsTestBase::
    SetUpFreeListingCouponOfferDataForCouponService(
        std::unique_ptr<AutofillOfferData> offer) {
  coupon_service_->DeleteAllFreeListingCoupons();
  // Simulate that user has given the consent to opt in the feature.
  coupon_service_->MaybeFeatureStatusChanged(true);
  base::flat_map<GURL, std::vector<std::unique_ptr<AutofillOfferData>>>
      coupon_map;
  for (auto origin : offer->merchant_origins) {
    coupon_map[origin].emplace_back(std::move(offer));
  }
  coupon_service_->UpdateFreeListingCoupons(coupon_map);
}

void OfferNotificationBubbleViewsTestBase::NavigateTo(
    const std::string& file_path) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(file_path)));
}

OfferNotificationBubbleViews*
OfferNotificationBubbleViewsTestBase::GetOfferNotificationBubbleViews() {
  OfferNotificationBubbleControllerImpl* controller =
      static_cast<OfferNotificationBubbleControllerImpl*>(
          OfferNotificationBubbleController::Get(GetActiveWebContents()));
  if (!controller) {
    return nullptr;
  }
  return static_cast<OfferNotificationBubbleViews*>(
      controller->GetOfferNotificationBubbleView());
}

OfferNotificationIconView*
OfferNotificationBubbleViewsTestBase::GetOfferNotificationIconView() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  PageActionIconView* icon =
      browser_view->toolbar_button_provider()->GetPageActionIconView(
          PageActionIconType::kPaymentsOfferNotification);
  DCHECK(browser_view->GetLocationBarView()->Contains(icon));
  return static_cast<OfferNotificationIconView*>(icon);
}

bool OfferNotificationBubbleViewsTestBase::IsIconVisible() {
  return GetOfferNotificationIconView() &&
         GetOfferNotificationIconView()->GetVisible();
}

content::WebContents*
OfferNotificationBubbleViewsTestBase::GetActiveWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

void OfferNotificationBubbleViewsTestBase::AddEventObserverToController(
    OfferNotificationBubbleControllerImpl* controller) {
  DCHECK(controller);
  controller->SetEventObserverForTesting(this);
}

void OfferNotificationBubbleViewsTestBase::ResetEventWaiterForSequence(
    std::list<DialogEvent> event_sequence) {
  event_waiter_ =
      std::make_unique<EventWaiter<DialogEvent>>(std::move(event_sequence));
}

void OfferNotificationBubbleViewsTestBase::UpdateFreeListingCouponDisplayTime(
    std::unique_ptr<AutofillOfferData> offer) {
  coupon_service_->RecordCouponDisplayTimestamp(*offer);
}

std::string OfferNotificationBubbleViewsTestBase::GetDefaultTestPromoCode()
    const {
  return kDefaultTestPromoCode;
}

AutofillOfferManager* OfferNotificationBubbleViewsTestBase::GetOfferManager() {
  return ContentAutofillDriver::GetForRenderFrameHost(
             GetActiveWebContents()->GetMainFrame())
      ->autofill_manager()
      ->GetOfferManager();
}

}  // namespace autofill
