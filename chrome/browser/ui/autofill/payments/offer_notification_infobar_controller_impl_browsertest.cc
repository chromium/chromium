// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_infobar_controller_impl.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/autofill_offer_notification_infobar_delegate_mobile.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace autofill {

const char kHostName[] = "example.com";
const char kOfferDetailsURL[] = "http://pay.google.com";

class OfferNotificationInfoBarControllerImplBrowserTest
    : public AndroidBrowserTest {
 public:
  OfferNotificationInfoBarControllerImplBrowserTest() = default;
  ~OfferNotificationInfoBarControllerImplBrowserTest() override = default;

  void SetUp() override {
    AndroidBrowserTest::SetUp();
    card_ = test::GetCreditCard();
  }

  infobars::InfoBar* GetInfoBar() {
    InfoBarService* infobar_service = InfoBarService::FromWebContents(
        chrome_test_utils::GetActiveWebContents(this));
    for (size_t i = 0; i < infobar_service->infobar_count(); ++i) {
      infobars::InfoBar* infobar = infobar_service->infobar_at(i);
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

  void ShowOfferNotificationInfoBar(const std::vector<GURL>& origins) {
    offer_notification_infobar_controller_->ShowIfNecessary(
        origins, GURL(kOfferDetailsURL), &card_);
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

  content::WebContents* GetWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  GURL GetInitialUrl() {
    return embedded_test_server()->GetURL(kHostName, "/empty.html");
  }

  // AndroidBrowserTest
  void SetUpOnMainThread() override {
    offer_notification_infobar_controller_ =
        std::make_unique<OfferNotificationInfoBarControllerImpl>(
            GetWebContents());
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(content::NavigateToURL(GetWebContents(), GetInitialUrl()));
  }

 private:
  std::unique_ptr<OfferNotificationInfoBarControllerImpl>
      offer_notification_infobar_controller_;
  // CreditCard that is linked to the offer displayed in the offer notification
  // infobar.
  CreditCard card_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(OfferNotificationInfoBarControllerImplBrowserTest,
                       ShowInfobarOnlyOncePerDomain) {
  ShowOfferNotificationInfoBar({GetInitialUrl().GetOrigin()});
  // Verify that the infobar was shown and logged.
  infobars::InfoBar* infobar = GetInfoBar();
  ASSERT_TRUE(infobar);
  VerifyInfoBarShownCount(1);
  // Remove the infobar without any action.
  infobar->RemoveSelf();

  // Navigate to a different URL within the same domain and try to show the
  // infobar.
  GURL secondURL =
      embedded_test_server()->GetURL(kHostName, "/simple_page.html");
  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), secondURL));
  ShowOfferNotificationInfoBar({secondURL.GetOrigin()});

  // Verify that the infobar was not shown again because it has already been
  // shown for this domain.
  ASSERT_FALSE(GetInfoBar());
  VerifyInfoBarShownCount(1);

  // Verify that the previous infobar was closed without user interaction.
  VerifyInfoBarResultMetric(
      AutofillMetrics::OfferNotificationInfoBarResultMetric::
          OFFER_NOTIFICATION_INFOBAR_IGNORED,
      1);
}

IN_PROC_BROWSER_TEST_F(OfferNotificationInfoBarControllerImplBrowserTest,
                       ShowInfobarAndAccept) {
  ShowOfferNotificationInfoBar({GetInitialUrl().GetOrigin()});
  // Verify that the infobar was shown.
  infobars::InfoBar* infobar = GetInfoBar();
  ASSERT_TRUE(infobar);

  // Accept and close the infobar.
  GetInfoBarDelegate(infobar)->Accept();
  infobar->RemoveSelf();

  // Verify histogram counts.
  VerifyInfoBarShownCount(1);
  VerifyInfoBarResultMetric(
      AutofillMetrics::OfferNotificationInfoBarResultMetric::
          OFFER_NOTIFICATION_INFOBAR_ACKNOWLEDGED,
      1);
}

IN_PROC_BROWSER_TEST_F(OfferNotificationInfoBarControllerImplBrowserTest,
                       ShowInfobarAndClose) {
  ShowOfferNotificationInfoBar({GetInitialUrl().GetOrigin()});
  // Verify that the infobar was shown.
  infobars::InfoBar* infobar = GetInfoBar();
  ASSERT_TRUE(infobar);

  // Dismiss and close the infobar.
  GetInfoBarDelegate(infobar)->InfoBarDismissed();
  infobar->RemoveSelf();

  // Verify histogram counts.
  VerifyInfoBarShownCount(1);
  VerifyInfoBarResultMetric(
      AutofillMetrics::OfferNotificationInfoBarResultMetric::
          OFFER_NOTIFICATION_INFOBAR_CLOSED,
      1);
}

}  // namespace autofill
