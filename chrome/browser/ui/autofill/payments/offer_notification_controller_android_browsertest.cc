// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_controller_android.h"

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/metrics/payments/offers_metrics.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/payments_data_manager_test_api.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/messages_feature.h"
#include "components/messages/android/test/messages_test_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace autofill {
namespace {

constexpr char kHostName[] = "example.com";

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

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
    personal_data_ =
        PersonalDataManagerFactory::GetForBrowserContext(GetProfile());
    // Mimic the user is signed in so payments integration is considered
    // enabled.
    personal_data_->payments_data_manager().SetSyncingForTest(true);
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
    PaymentsDataManager& paydm = personal_data_->payments_data_manager();
    paydm.ClearAllServerDataForTesting();
    std::vector<GURL> merchant_origins;
    merchant_origins.emplace_back(url.DeprecatedGetOriginAsURL());
    std::vector<int64_t> eligible_instrument_ids = {0x4444};
    std::string offer_reward_amount = "5%";
    auto offer = std::make_unique<AutofillOfferData>(CreateTestCardLinkedOffer(
        merchant_origins, eligible_instrument_ids, offer_reward_amount));
    auto* offer_ptr = offer.get();
    test_api(paydm).AddOfferData(std::move(offer));
    auto card = std::make_unique<CreditCard>();
    card->set_instrument_id(0x4444);
    paydm.AddServerCreditCardForTest(std::move(card));
    test_api(paydm).NotifyObservers();
    return offer_ptr;
  }

  AutofillOfferManager* GetOfferManager() {
    return ContentAutofillClient::FromWebContents(GetWebContents())
        ->GetPaymentsAutofillClient()
        ->GetAutofillOfferManager();
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

 private:
  test::AutofillBrowserTestEnvironment autofill_environment_;
  raw_ptr<PersonalDataManager> personal_data_;
};

class OfferNotificationControllerAndroidBrowserTestForMessagesUi
    : public OfferNotificationControllerAndroidBrowserTest {
 public:
  OfferNotificationControllerAndroidBrowserTestForMessagesUi() = default;

  void VerifyMessageShownCountMetric(int count) {
    histogram_tester_.ExpectBucketCount(
        "Android.Messages.Stacking.InsertAtFront",
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
