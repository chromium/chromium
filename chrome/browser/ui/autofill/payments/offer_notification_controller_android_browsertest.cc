// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_controller_android.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/metrics/payments/offers_metrics.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/autofill_offer_notification_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/messages_feature.h"
#include "components/messages/android/test/messages_test_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace autofill {

const char kHostName[] = "example.com";

class OfferNotificationControllerAndroidBrowserTest
    : public AndroidBrowserTest {
 public:
  OfferNotificationControllerAndroidBrowserTest() = default;
  ~OfferNotificationControllerAndroidBrowserTest() override = default;

  void SetUp() override {
    AndroidBrowserTest::SetUp();
    card_ = test::GetCreditCard();
  }

  content::WebContents* GetWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  AutofillOfferData CreateTestCardLinkedOffer(
      const std::vector<GURL>& merchant_origins,
      const std::vector<int64_t>& eligible_instrument_ids,
      const std::string& offer_reward_amount) {
    int64_t offer_id = 4444;
    base::Time expiry = autofill::AutofillClock::Now() + base::Days(2);
    GURL offer_details_url("https://www.google.com/");
    return autofill::AutofillOfferData::GPayCardLinkedOffer(
        offer_id, expiry, merchant_origins, offer_details_url,
        autofill::DisplayStrings(), eligible_instrument_ids,
        offer_reward_amount);
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
    offer_notification_controller_android_ =
        std::make_unique<OfferNotificationControllerAndroid>(GetWebContents());
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

  AutofillOfferData* SetUpOfferDataWithDomains(const GURL& url) {
    personal_data_->ClearAllServerData();
    std::vector<GURL> merchant_origins;
    merchant_origins.emplace_back(url.DeprecatedGetOriginAsURL());
    std::vector<int64_t> eligible_instrument_ids = {0x4444};
    std::string offer_reward_amount = "5%";
    auto offer = std::make_unique<AutofillOfferData>(CreateTestCardLinkedOffer(
        merchant_origins, eligible_instrument_ids, offer_reward_amount));
    auto* offer_ptr = offer.get();
    personal_data_->AddOfferDataForTest(std::move(offer));
    auto card = std::make_unique<CreditCard>();
    card->set_instrument_id(0x4444);
    personal_data_->AddServerCreditCardForTest(std::move(card));
    personal_data_->NotifyPersonalDataObserver();
    return offer_ptr;
  }

  AutofillOfferManager* GetOfferManager() {
    return ContentAutofillDriver::GetForRenderFrameHost(
               GetWebContents()->GetPrimaryMainFrame())
        ->autofill_manager()
        ->GetOfferManager();
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

 protected:
  std::unique_ptr<OfferNotificationControllerAndroid>
      offer_notification_controller_android_;
  // CreditCard that is linked to the offer displayed in the offer notification.
  CreditCard card_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  raw_ptr<PersonalDataManager> personal_data_;
};

class OfferNotificationControllerAndroidBrowserTestForInfobar
    : public OfferNotificationControllerAndroidBrowserTest {
 public:
  OfferNotificationControllerAndroidBrowserTestForInfobar() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{messages::kMessagesForAndroidInfrastructure,
                               messages::kMessagesForAndroidOfferNotification});
    OfferNotificationControllerAndroidBrowserTest::SetUp();
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
    offer_notification_controller_android_->ShowIfNecessary(offer, &card_);
  }

  void VerifyInfoBarShownCount(int count) {
    histogram_tester_.ExpectTotalCount(
        "Autofill.OfferNotificationInfoBarOffer.CardLinkedOffer", count);
  }

  void VerifyInfoBarResultMetric(
      autofill_metrics::OfferNotificationInfoBarResultMetric metric,
      int count) {
    histogram_tester_.ExpectBucketCount(
        "Autofill.OfferNotificationInfoBarResult.CardLinkedOffer", metric,
        count);
  }
};

IN_PROC_BROWSER_TEST_F(OfferNotificationControllerAndroidBrowserTestForInfobar,
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
      autofill_metrics::OfferNotificationInfoBarResultMetric::
          OFFER_NOTIFICATION_INFOBAR_ACKNOWLEDGED,
      1);
}

IN_PROC_BROWSER_TEST_F(OfferNotificationControllerAndroidBrowserTestForInfobar,
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
      autofill_metrics::OfferNotificationInfoBarResultMetric::
          OFFER_NOTIFICATION_INFOBAR_CLOSED,
      1);
}

IN_PROC_BROWSER_TEST_F(OfferNotificationControllerAndroidBrowserTestForInfobar,
                       CrossTabStatusTracking) {
  GURL offer_url = GetInitialUrl().DeprecatedGetOriginAsURL();
  int64_t id = SetUpOfferDataWithDomains(offer_url)->GetOfferId();

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

class OfferNotificationControllerAndroidBrowserTestForMessagesUi
    : public OfferNotificationControllerAndroidBrowserTest {
 public:
  OfferNotificationControllerAndroidBrowserTestForMessagesUi() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{messages::kMessagesForAndroidInfrastructure,
                              messages::kMessagesForAndroidOfferNotification},
        /*disabled_features=*/{});
    OfferNotificationControllerAndroidBrowserTest::SetUp();
  }

  void VerifyMessageShownCountMetric(int count) {
    histogram_tester_.ExpectBucketCount(
        messages::IsStackingAnimationEnabled()
            ? "Android.Messages.Stacking.InsertAtFront"
            : "Android.Messages.Enqueued.Visible",
        static_cast<int>(messages::MessageIdentifier::OFFER_NOTIFICATION),
        count);
  }

  messages::MessagesTestHelper messages_test_helper_;
};

IN_PROC_BROWSER_TEST_F(
    OfferNotificationControllerAndroidBrowserTestForMessagesUi,
    MessageShown) {
  GURL offer_url = GetInitialUrl().DeprecatedGetOriginAsURL();
  SetUpOfferDataWithDomains(offer_url);
  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), GetInitialUrl()));
  // Verify that the message was shown and logged.
  EXPECT_EQ(messages_test_helper_.GetMessageCount(
                GetWebContents()->GetTopLevelNativeWindow()),
            1);
  EXPECT_EQ(
      messages_test_helper_.GetMessageIdentifier(
          GetWebContents()->GetTopLevelNativeWindow(), /* enqueue index */ 0),
      static_cast<int>(messages::MessageIdentifier::OFFER_NOTIFICATION));
  VerifyMessageShownCountMetric(1);
}

IN_PROC_BROWSER_TEST_F(
    OfferNotificationControllerAndroidBrowserTestForMessagesUi,
    CrossTabStatusTracking) {
  GURL offer_url = GetInitialUrl().DeprecatedGetOriginAsURL();
  int64_t id = SetUpOfferDataWithDomains(offer_url)->GetOfferId();

  SetShownOffer(id);
  // Navigate to a different URL within the same domain and try to show the
  // message.
  offer_url = embedded_test_server()->GetURL(kHostName, "/simple_page.html");
  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), offer_url));

  // Verify that the message was not shown again because it has already been
  // marked as shown for this domain.
  EXPECT_EQ(messages_test_helper_.GetMessageCount(
                GetWebContents()->GetTopLevelNativeWindow()),
            0);
  VerifyMessageShownCountMetric(0);
}

}  // namespace autofill
