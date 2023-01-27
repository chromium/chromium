// Copyright 2021 The Chromium Authors
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
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/commerce/core/commerce_feature_list.h"

namespace autofill {

const char kDefaultTestPromoCode[] = "5PCTOFFSHOES";
const char kDefaultTestValuePropText[] = "5% off on shoes. Up to $50.";
const char kDefaultTestSeeDetailsText[] = "See details";
const char kDefaultTestUsageInstructionsText[] =
    "Click the promo code field at checkout to autofill it.";
const char kDefaultTestDetailsUrlString[] = "https://pay.google.com";

OfferNotificationBubbleViewsTestBase::OfferNotificationBubbleViewsTestBase(
    bool promo_code_flag_enabled) {
  if (promo_code_flag_enabled) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{commerce::kRetailCoupons,
          {{commerce::kRetailCouponsWithCodeParam, "true"}}},
         {features::kAutofillEnableOfferNotificationForPromoCodes, {}},
         {features::kAutofillFillMerchantPromoCodeFields, {}}},
        /*disabled_features=*/{});
  } else {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            commerce::kRetailCoupons,
            features::kAutofillEnableOfferNotificationForPromoCodes,
            features::kAutofillFillMerchantPromoCodeFields});
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
  int64_t offer_id = 4444;
  base::Time expiry = AutofillClock::Now() + base::Days(2);
  std::vector<GURL> merchant_origins;
  for (auto url : domains)
    merchant_origins.emplace_back(url.DeprecatedGetOriginAsURL());
  GURL offer_details_url;
  DisplayStrings display_strings;
  std::vector<int64_t> eligible_instrument_ids = {kCreditCardInstrumentId};
  std::string offer_reward_amount = "5%";
  return std::make_unique<AutofillOfferData>(
      AutofillOfferData::GPayCardLinkedOffer(
          offer_id, expiry, merchant_origins, offer_details_url,
          display_strings, eligible_instrument_ids, offer_reward_amount));
}

std::unique_ptr<AutofillOfferData>
OfferNotificationBubbleViewsTestBase::CreateFreeListingCouponDataWithDomains(
    const std::vector<GURL>& domains) {
  int64_t offer_id = 5555;
  base::Time expiry = AutofillClock::Now() + base::Days(2);
  std::vector<GURL> merchant_origins;
  for (auto url : domains)
    merchant_origins.emplace_back(url.DeprecatedGetOriginAsURL());
  DisplayStrings display_strings;
  display_strings.value_prop_text = GetDefaultTestValuePropText();
  display_strings.see_details_text = GetDefaultTestSeeDetailsText();
  display_strings.usage_instructions_text =
      GetDefaultTestUsageInstructionsText();
  auto promo_code = GetDefaultTestPromoCode();
  return std::make_unique<AutofillOfferData>(
      AutofillOfferData::FreeListingCouponOffer(
          offer_id, expiry, merchant_origins,
          /*offer_details_url=*/GURL(), display_strings, promo_code));
}

std::unique_ptr<AutofillOfferData>
OfferNotificationBubbleViewsTestBase::CreateGPayPromoCodeOfferDataWithDomains(
    const std::vector<GURL>& domains) {
  int64_t offer_id = 5555;
  base::Time expiry = AutofillClock::Now() + base::Days(2);
  std::vector<GURL> merchant_origins;
  for (auto url : domains)
    merchant_origins.emplace_back(url.DeprecatedGetOriginAsURL());
  DisplayStrings display_strings;
  display_strings.value_prop_text = GetDefaultTestValuePropText();
  display_strings.see_details_text = GetDefaultTestSeeDetailsText();
  display_strings.usage_instructions_text =
      GetDefaultTestUsageInstructionsText();
  auto promo_code = GetDefaultTestPromoCode();
  GURL offer_details_url = GURL(GetDefaultTestDetailsUrlString());
  return std::make_unique<AutofillOfferData>(
      AutofillOfferData::GPayPromoCodeOffer(offer_id, expiry, merchant_origins,
                                            offer_details_url, display_strings,
                                            promo_code));
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
    case AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER:
      SetUpGPayPromoCodeOfferDataWithDomains(domains);
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
  personal_data_->AddOfferDataForTest(
      CreateFreeListingCouponDataWithDomains(domains));
  personal_data_->NotifyPersonalDataObserver();
}

void OfferNotificationBubbleViewsTestBase::
    SetUpGPayPromoCodeOfferDataWithDomains(const std::vector<GURL>& domains) {
  personal_data_->ClearAllServerData();
  personal_data_->AddOfferDataForTest(
      CreateGPayPromoCodeOfferDataWithDomains(domains));
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
  for (auto origin : offer->GetMerchantOrigins()) {
    coupon_map[origin].emplace_back(std::move(offer));
  }
  coupon_service_->UpdateFreeListingCoupons(coupon_map);
}

OfferNotificationBubbleViewsTestBase::TestAutofillManager*
OfferNotificationBubbleViewsTestBase::GetAutofillManager() {
  return autofill_manager_injector_[GetActiveWebContents()];
}

void OfferNotificationBubbleViewsTestBase::NavigateTo(
    const std::string& file_path) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(file_path)));
}

void OfferNotificationBubbleViewsTestBase::NavigateToAndWaitForForm(
    const std::string& file_path) {
  NavigateTo(file_path);
  ASSERT_TRUE(GetAutofillManager()->WaitForFormsSeen(1));
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

std::string OfferNotificationBubbleViewsTestBase::GetDefaultTestValuePropText()
    const {
  return kDefaultTestValuePropText;
}

std::string OfferNotificationBubbleViewsTestBase::GetDefaultTestSeeDetailsText()
    const {
  return kDefaultTestSeeDetailsText;
}

std::string
OfferNotificationBubbleViewsTestBase::GetDefaultTestUsageInstructionsText()
    const {
  return kDefaultTestUsageInstructionsText;
}

std::string
OfferNotificationBubbleViewsTestBase::GetDefaultTestDetailsUrlString() const {
  return kDefaultTestDetailsUrlString;
}

AutofillOfferManager* OfferNotificationBubbleViewsTestBase::GetOfferManager() {
  return ContentAutofillDriver::GetForRenderFrameHost(
             GetActiveWebContents()->GetPrimaryMainFrame())
      ->autofill_manager()
      ->GetOfferManager();
}

}  // namespace autofill
