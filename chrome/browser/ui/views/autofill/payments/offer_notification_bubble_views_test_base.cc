// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views_test_base.h"

#include <string_view>

#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/payments_data_manager_test_api.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"

namespace autofill {

const char kDefaultTestPromoCode[] = "5PCTOFFSHOES";
const char kDefaultTestValuePropText[] = "5% off on shoes. Up to $50.";
const char kDefaultTestSeeDetailsText[] = "See details";
const char kDefaultTestUsageInstructionsText[] =
    "Click the promo code field at checkout to autofill it.";
const char kDefaultTestDetailsUrlString[] = "https://pay.google.com";

OfferNotificationBubbleViewsTestBase::OfferNotificationBubbleViewsTestBase()
    : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

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
      PersonalDataManagerFactory::GetForBrowserContext(browser()->profile());

  // Mimic the user is signed in so payments integration is considered enabled.
  personal_data_->payments_data_manager().SetSyncingForTest(true);

  // Wait for Personal Data Manager to be fully loaded to prevent that
  // spurious notifications deceive the tests.
  WaitForPersonalDataManagerToBeLoaded(browser()->profile());

  host_resolver()->AddRule("*", "127.0.0.1");
  cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
  https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  https_server_.RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_OK);
        response->set_content_type("text/html;charset=utf-8");
        response->set_content(R"(
          <html> <body> <form> <input> </form>
        )");
        return response;
      }));
  ASSERT_TRUE(https_server_.InitializeAndListen());
  https_server_.StartAcceptingConnections();
}

void OfferNotificationBubbleViewsTestBase::TearDownOnMainThread() {
  // Null explicitly to avoid dangling pointers.
  personal_data_ = nullptr;

  InProcessBrowserTest::TearDownOnMainThread();
}

void OfferNotificationBubbleViewsTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  cert_verifier_.SetUpCommandLine(command_line);
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
  personal_data_->payments_data_manager().AddServerCreditCardForTest(
      std::move(card));
  test_api(personal_data_->payments_data_manager()).NotifyObservers();
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

void OfferNotificationBubbleViewsTestBase::SetUpOfferDataWithDomains(
    AutofillOfferData::OfferType offer_type,
    const std::vector<GURL>& domains) {
  switch (offer_type) {
    case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
      SetUpCardLinkedOfferDataWithDomains(domains);
      break;
    case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
      break;
    case AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER:
      SetUpGPayPromoCodeOfferDataWithDomains(domains);
      break;
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED();
  }
}

void OfferNotificationBubbleViewsTestBase::SetUpCardLinkedOfferDataWithDomains(
    const std::vector<GURL>& domains) {
  personal_data_->payments_data_manager().ClearAllServerDataForTesting();
  // CreateCardLinkedOfferDataWithDomains(~) will add the necessary card.
  test_api(personal_data_->payments_data_manager())
      .AddOfferData(CreateCardLinkedOfferDataWithDomains(domains));
  test_api(personal_data_->payments_data_manager()).NotifyObservers();
}

void OfferNotificationBubbleViewsTestBase::
    SetUpGPayPromoCodeOfferDataWithDomains(const std::vector<GURL>& domains) {
  personal_data_->payments_data_manager().ClearAllServerDataForTesting();
  test_api(personal_data_->payments_data_manager())
      .AddOfferData(CreateGPayPromoCodeOfferDataWithDomains(domains));
  test_api(personal_data_->payments_data_manager()).NotifyObservers();
}

OfferNotificationBubbleViewsTestBase::TestAutofillManager*
OfferNotificationBubbleViewsTestBase::GetAutofillManager() {
  return autofill_manager_injector_[GetActiveWebContents()];
}

GURL OfferNotificationBubbleViewsTestBase::GetUrl(std::string_view host,
                                                  std::string_view path) const {
  return https_server_.GetURL(host, path);
}

void OfferNotificationBubbleViewsTestBase::NavigateTo(const GURL& url) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
}

void OfferNotificationBubbleViewsTestBase::NavigateToAndWaitForForm(
    const GURL& url) {
  NavigateTo(url);
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
  return ContentAutofillClient::FromWebContents(GetActiveWebContents())
      ->GetPaymentsAutofillClient()
      ->GetAutofillOfferManager();
}

}  // namespace autofill
