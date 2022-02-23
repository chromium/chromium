// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_infobar_controller_impl.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/autofill_offer_notification_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace autofill {

const char kHostName[] = "example.com";

class OfferNotificationInfoBarControllerImplBrowserTest
    : public AndroidBrowserTest {
 public:
  OfferNotificationInfoBarControllerImplBrowserTest() = default;
  ~OfferNotificationInfoBarControllerImplBrowserTest() override = default;

  void SetUp() override {
    AndroidBrowserTest::SetUp();
    card_ = test::GetCreditCard();
  }

  content::WebContents* GetWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  infobars::InfoBar* GetInfoBar() {
    infobars::ContentInfoBarManager* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(GetWebContents());
    for (size_t i = 0; i < infobar_manager->infobar_count(); ++i) {
      infobars::InfoBar* infobar = infobar_manager->infobar_at(i);
      if (infobar->delegate()->GetIdentifier() ==
          infobars::InfoBarDelegate::
              AUTOFILL_OFFER_NOTIFICATION_INFOBAR_DELEGATE) {
        return infobar;
      }
    }
    return nullptr;
  }

  AutofillOfferNotificationInfoBarDelegateMobile* GetInfoBarDelegate(
      infobars::InfoBar* infobar) {
    return static_cast<AutofillOfferNotificationInfoBarDelegateMobile*>(
        infobar->delegate());
  }

  void ShowOfferNotificationInfoBar(const AutofillOfferData* offer) {
    offer_notification_infobar_controller_->ShowIfNecessary(offer, &card_);
  }

  AutofillOfferData CreateTestOfferWithOrigins(
      const std::vector<GURL>& merchant_origins) {
    // Only adding what the tests need to pass. Feel free to add more populated
    // fields as necessary.
    AutofillOfferData offer;
    offer.merchant_origins = merchant_origins;
    return offer;
  }

  void VerifyInfoBarShownCount(int count) {
    histogram_tester_.ExpectTotalCount(
        "Autofill.OfferNotificationInfoBarOffer.CardLinkedOffer", count);
  }

  void VerifyInfoBarResultMetric(
      AutofillMetrics::OfferNotificationInfoBarResultMetric metric,
      int count) {
    histogram_tester_.ExpectBucketCount(
        "Autofill.OfferNotificationInfoBarResult.CardLinkedOffer", metric,
        count);
  }

  GURL GetInitialUrl() {
    return embedded_test_server()->GetURL(kHostName, "/empty.html");
  }

  // AndroidBrowserTest
  void SetUpOnMainThread() override {
    personal_data_ = PersonalDataManagerFactory::GetForProfile(GetProfile());
    // Wait for Personal Data Manager to be fully loaded to prevent that
    // spurious notifications deceive the tests.
    WaitForPersonalDataManagerToBeLoaded(GetProfile());
    offer_notification_infobar_controller_ =
        std::make_unique<OfferNotificationInfoBarControllerImpl>(
            GetWebContents());
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

  AutofillOfferData* SetUpOfferDataWithDomains(const GURL& url) {
    personal_data_->ClearAllServerData();
    std::unique_ptr<AutofillOfferData> offer_data_entry =
        std::make_unique<AutofillOfferData>();
    offer_data_entry->offer_id = 4444;
    offer_data_entry->offer_reward_amount = "5%";
    offer_data_entry->expiry = AutofillClock::Now() + base::Days(2);
    offer_data_entry->merchant_origins = {};
    offer_data_entry->merchant_origins.emplace_back(
        url.DeprecatedGetOriginAsURL());
    offer_data_entry->eligible_instrument_id = {0x4444};
    auto* offer = offer_data_entry.get();
    personal_data_->AddOfferDataForTest(std::move(offer_data_entry));
    auto card = std::make_unique<CreditCard>();
    card->set_instrument_id(0x4444);
    personal_data_->AddServerCreditCardForTest(std::move(card));
    personal_data_->NotifyPersonalDataObserver();
    return offer;
  }

  AutofillOfferManager* GetOfferManager() {
    return ContentAutofillDriver::GetForRenderFrameHost(
               GetWebContents()->GetMainFrame())
        ->browser_autofill_manager()
        ->offer_manager();
  }

  void SetShownOffer(int64_t id) {
    if (!GetOfferManager())
      return;

    auto* handler = &(GetOfferManager()->notification_handler_);
    if (!handler)
      return;

    handler->ClearShownNotificationIdForTesting();
    handler->AddShownNotificationIdForTesting(id);
  }

 private:
  std::unique_ptr<OfferNotificationInfoBarControllerImpl>
      offer_notification_infobar_controller_;
  // CreditCard that is linked to the offer displayed in the offer notification
  // infobar.
  CreditCard card_;
  base::HistogramTester histogram_tester_;
  PersonalDataManager* personal_data_;
};

IN_PROC_BROWSER_TEST_F(OfferNotificationInfoBarControllerImplBrowserTest,
                       ShowInfobarAndAccept) {
  GURL offer_url = GetInitialUrl().DeprecatedGetOriginAsURL();
  SetUpOfferDataWithDomains(offer_url);
  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), GetInitialUrl()));
  // Verify that the infobar was shown and logged.
  infobars::InfoBar* infobar = GetInfoBar();
  ASSERT_TRUE(infobar);
  VerifyInfoBarShownCount(1);

  // Accept and close the infobar.
  GetInfoBarDelegate(infobar)->Accept();
  infobar->RemoveSelf();

  // Verify histogram counts.
  VerifyInfoBarResultMetric(
      AutofillMetrics::OfferNotificationInfoBarResultMetric::
          OFFER_NOTIFICATION_INFOBAR_ACKNOWLEDGED,
      1);
}

IN_PROC_BROWSER_TEST_F(OfferNotificationInfoBarControllerImplBrowserTest,
                       ShowInfobarAndClose) {
  GURL offer_url = GetInitialUrl().DeprecatedGetOriginAsURL();
  SetUpOfferDataWithDomains(offer_url);
  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), GetInitialUrl()));
  // Verify that the infobar was shown and logged.
  infobars::InfoBar* infobar = GetInfoBar();
  ASSERT_TRUE(infobar);
  VerifyInfoBarShownCount(1);

  // Dismiss and close the infobar.
  GetInfoBarDelegate(infobar)->InfoBarDismissed();
  infobar->RemoveSelf();

  // Verify histogram counts.
  VerifyInfoBarResultMetric(
      AutofillMetrics::OfferNotificationInfoBarResultMetric::
          OFFER_NOTIFICATION_INFOBAR_CLOSED,
      1);
}

IN_PROC_BROWSER_TEST_F(OfferNotificationInfoBarControllerImplBrowserTest,
                       CrossTabStatusTracking) {
  GURL offer_url = GetInitialUrl().DeprecatedGetOriginAsURL();
  int64_t id = SetUpOfferDataWithDomains(offer_url)->offer_id;

  SetShownOffer(id);
  // Navigate to a different URL within the same domain and try to show the
  // infobar.
  offer_url = embedded_test_server()->GetURL(kHostName, "/simple_page.html");
  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), offer_url));

  // Verify that the infobar was not shown again because it has already been
  // marked as shown for this domain.
  ASSERT_FALSE(GetInfoBar());
  VerifyInfoBarShownCount(0);
}

}  // namespace autofill
