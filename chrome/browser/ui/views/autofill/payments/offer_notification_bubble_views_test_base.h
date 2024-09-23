// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_TEST_BASE_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_icon_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace autofill {

namespace {
constexpr int64_t kCreditCardInstrumentId = 0x4444;
}  // namespace

// Test base class for the OfferNotificationBubbleViews related tests. Provides
// helper function and common setups.
class OfferNotificationBubbleViewsTestBase
    : public InProcessBrowserTest,
      public OfferNotificationBubbleControllerImpl::ObserverForTest {
 public:
  class TestAutofillManager : public BrowserAutofillManager {
   public:
    explicit TestAutofillManager(ContentAutofillDriver* driver)
        : BrowserAutofillManager(driver, "en-US") {}

    testing::AssertionResult WaitForFormsSeen(int min_num_awaited_calls) {
      return forms_seen_waiter_.Wait(min_num_awaited_calls);
    }

   private:
    TestAutofillManagerWaiter forms_seen_waiter_{
        *this,
        {AutofillManagerEvent::kFormsSeen}};
  };

  // Various events that can be waited on by the DialogEventWaiter.
  enum class DialogEvent : int {
    BUBBLE_SHOWN,
  };

  OfferNotificationBubbleViewsTestBase();
  ~OfferNotificationBubbleViewsTestBase() override;
  OfferNotificationBubbleViewsTestBase(
      const OfferNotificationBubbleViewsTestBase&) = delete;
  OfferNotificationBubbleViewsTestBase& operator=(
      const OfferNotificationBubbleViewsTestBase&) = delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // OfferNotificationBubbleControllerImpl::ObserverForTest:
  void OnBubbleShown() override;

  // Also creates a credit card for the offer.
  std::unique_ptr<AutofillOfferData> CreateCardLinkedOfferDataWithDomains(
      const std::vector<GURL>& domains);

  std::unique_ptr<AutofillOfferData> CreateGPayPromoCodeOfferDataWithDomains(
      const std::vector<GURL>& domains);

  void SetUpOfferDataWithDomains(AutofillOfferData::OfferType offer_type,
                                 const std::vector<GURL>& domains);

  // Also creates a credit card for the offer.
  void SetUpCardLinkedOfferDataWithDomains(const std::vector<GURL>& domains);

  void SetUpGPayPromoCodeOfferDataWithDomains(const std::vector<GURL>& domains);

  TestAutofillManager* GetAutofillManager();

  // The test fixture's HTTPS server listens at a random port.
  // `GetUrl("foo.com", "/index.html")` returns a URL
  // `GURL("https://foo.com:1234/index.html")` for the right port.
  GURL GetUrl(std::string_view host, std::string_view path) const;
  void NavigateTo(const GURL& url);
  void NavigateToAndWaitForForm(const GURL& url);

  OfferNotificationBubbleViews* GetOfferNotificationBubbleViews();

  OfferNotificationIconView* GetOfferNotificationIconView();

  bool IsIconVisible();

  content::WebContents* GetActiveWebContents();

  void AddEventObserverToController(
      OfferNotificationBubbleControllerImpl* controller);

  void ResetEventWaiterForSequence(std::list<DialogEvent> event_sequence);

  AutofillOfferManager* GetOfferManager();

  [[nodiscard]] testing::AssertionResult WaitForObservedEvent() {
    return event_waiter_->Wait();
  }

  PersonalDataManager* personal_data() { return personal_data_; }

 protected:
  // Returns the string used for the default test promo code data, so that it
  // can be expected on UI elements if desired.
  std::string GetDefaultTestPromoCode() const;

  // Returns the value prop string used for the default test GPay promo code,
  // so that it can be expected on UI elements if desired.
  std::string GetDefaultTestValuePropText() const;

  // Returns the see details string used for the default test GPay promo code.
  std::string GetDefaultTestSeeDetailsText() const;

  // Returns the user instructions string used for the default GPay promo code
  // data.
  std::string GetDefaultTestUsageInstructionsText() const;

  // Returns the offer details url string used for the default GPay promo code.
  std::string GetDefaultTestDetailsUrlString() const;

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  TestAutofillManagerInjector<TestAutofillManager> autofill_manager_injector_;
  raw_ptr<PersonalDataManager> personal_data_ = nullptr;
  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;
  net::EmbeddedTestServer https_server_;
  content::ContentMockCertVerifier cert_verifier_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_TEST_BASE_H_
