// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views_test_base.h"

#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

OfferNotificationBubbleViewsTestBase::OfferNotificationBubbleViewsTestBase() {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillEnableOfferNotification);
}

OfferNotificationBubbleViewsTestBase::~OfferNotificationBubbleViewsTestBase() =
    default;

void OfferNotificationBubbleViewsTestBase::SetUpOnMainThread() {
  // Set up this class as the ObserverForTest implementation.
  AddEventObserverToController();

  personal_data_ =
      PersonalDataManagerFactory::GetForProfile(browser()->profile());

  // Wait for Personal Data Manager to be fully loaded to prevent that
  // spurious notifications deceive the tests.
  WaitForPersonalDataManagerToBeLoaded(browser()->profile());
}

void OfferNotificationBubbleViewsTestBase::OnBubbleShown() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::BUBBLE_SHOWN);
}

std::unique_ptr<AutofillOfferData>
OfferNotificationBubbleViewsTestBase::CreateOfferDataWithDomains(
    const std::vector<GURL>& domains) {
  std::unique_ptr<AutofillOfferData> offer_data_entry =
      std::make_unique<AutofillOfferData>();
  offer_data_entry->offer_id = 4444;
  offer_data_entry->offer_reward_amount = "5%";
  offer_data_entry->expiry =
      AutofillClock::Now() + base::TimeDelta::FromDays(2);
  offer_data_entry->merchant_domain = {};
  for (auto url : domains)
    offer_data_entry->merchant_domain.emplace_back(url.GetOrigin());
  offer_data_entry->eligible_instrument_id = {kCreditCardInstrumentId};

  return offer_data_entry;
}

void OfferNotificationBubbleViewsTestBase::SetUpOfferDataWithDomains(
    const std::vector<GURL>& domains) {
  personal_data_->ClearAllServerData();
  personal_data_->AddOfferDataForTest(CreateOfferDataWithDomains(domains));
  auto card = std::make_unique<CreditCard>();
  card->set_instrument_id(kCreditCardInstrumentId);
  personal_data_->AddServerCreditCardForTest(std::move(card));
  personal_data_->NotifyPersonalDataObserver();
}

void OfferNotificationBubbleViewsTestBase::NavigateTo(
    const std::string& file_path) {
  ui_test_utils::NavigateToURL(browser(), GURL(file_path));
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

void OfferNotificationBubbleViewsTestBase::AddEventObserverToController() {
  OfferNotificationBubbleControllerImpl* controller =
      static_cast<OfferNotificationBubbleControllerImpl*>(
          OfferNotificationBubbleController::GetOrCreate(
              GetActiveWebContents()));
  DCHECK(controller);
  controller->SetEventObserverForTesting(this);
}

void OfferNotificationBubbleViewsTestBase::ResetEventWaiterForSequence(
    std::list<DialogEvent> event_sequence) {
  event_waiter_ =
      std::make_unique<EventWaiter<DialogEvent>>(std::move(event_sequence));
}

}  // namespace autofill
