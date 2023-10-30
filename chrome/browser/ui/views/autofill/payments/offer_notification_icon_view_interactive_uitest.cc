// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/offer_notification_icon_view.h"

#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace autofill {
namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kShoppingTab);

const char kShoppingURL[] = "/shopping.html";

std::unique_ptr<net::test_server::HttpResponse> BasicResponse(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content("page content");
  response->set_content_type("text/html");
  return response;
}
}  // namespace

class OfferNotificationIconViewInteractiveTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&BasicResponse));
    embedded_test_server()->StartAcceptingConnections();

    InteractiveBrowserTest::SetUpOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&OfferNotificationIconViewInteractiveTest::
                                        OnWillCreateBrowserContextServices,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    commerce::ShoppingServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context) {
          return commerce::MockShoppingService::Build();
        }));
  }

  void SetIconAnimationTimer(base::MockRetainingOneShotTimer* timer) {
    auto* icon_view = static_cast<OfferNotificationIconView*>(
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kOfferNotificationChipElementId,
            browser()->window()->GetElementContext()));
    icon_view->SetAnimateOutTimerForTesting(timer);
  }

  void SetUpShoppingServiceToReturnDiscounts() {
    const double expiry_time_sec =
        (AutofillClock::Now() + base::Days(2)).InSecondsFSinceUnixEpoch();

    auto* mock_shopping_service = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForBrowserContext(
            browser()->profile()));
    mock_shopping_service->SetIsDiscountEligibleToShowOnNavigation(true);
    mock_shopping_service->SetResponseForGetDiscountInfoForUrls(
        {{embedded_test_server()->GetURL(kShoppingURL),
          {commerce::CreateValidDiscountInfo(
              /*detail=*/"Discount description detail",
              /*terms_and_conditions=*/"",
              /*value_in_text=*/"$10 off", /*discount_code=*/"code",
              /*id=*/123,
              /*is_merchant_wide=*/true, expiry_time_sec)}}});

    EXPECT_CALL(*mock_shopping_service, IsDiscountEligibleToShowOnNavigation)
        .Times(testing::AtLeast(1));
    EXPECT_CALL(*mock_shopping_service, GetDiscountInfoForUrls)
        .Times(testing::AtLeast(1));
  }
  bool WaitForIconFinishAnimating() {
    auto* icon_view = static_cast<OfferNotificationIconView*>(
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kOfferNotificationChipElementId,
            browser()->window()->GetElementContext()));
    while (icon_view->is_animating_label()) {
      base::RunLoop().RunUntilIdle();
    }
    return false;
  }

 private:
  base::test::ScopedFeatureList test_features_{
      commerce::kShowDiscountOnNavigation};
  base::CallbackListSubscription create_services_subscription_;
  base::WeakPtrFactory<OfferNotificationIconViewInteractiveTest>
      weak_ptr_factory_{this};
};

// TODO(crbug.com/1497347): Flaky on Linux MSAN.
#if BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER)
#define MAYBE_IconAutoCollapse DISABLED_IconAutoCollapse
#else
#define MAYBE_IconAutoCollapse IconAutoCollapse
#endif
IN_PROC_BROWSER_TEST_F(OfferNotificationIconViewInteractiveTest,
                       MAYBE_IconAutoCollapse) {
  base::MockRetainingOneShotTimer timer;
  SetIconAnimationTimer(&timer);

  SetUpShoppingServiceToReturnDiscounts();

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      FlushEvents(), EnsurePresent(kOfferNotificationChipElementId),
      CheckViewProperty(kOfferNotificationChipElementId,
                        &OfferNotificationIconView::ShouldShowLabel, true),
      WaitForEvent(kOfferNotificationChipElementId, kLabelExpansionFinished),
      Check([&]() { return timer.IsRunning(); }, "Mock timer is running"),
      // Simulate ready to collapse the icon.
      Do([&]() { timer.Fire(); }),
      WaitForEvent(kOfferNotificationChipElementId, kLabelAnimationFinished),
      CheckViewProperty(kOfferNotificationChipElementId,
                        &OfferNotificationIconView::ShouldShowLabel, false));
}

IN_PROC_BROWSER_TEST_F(OfferNotificationIconViewInteractiveTest,
                       StopCollapseTimerAfterClickingIcon) {
  base::MockRetainingOneShotTimer timer;
  SetIconAnimationTimer(&timer);

  SetUpShoppingServiceToReturnDiscounts();

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      FlushEvents(), EnsurePresent(kOfferNotificationChipElementId),
      CheckViewProperty(kOfferNotificationChipElementId,
                        &OfferNotificationIconView::ShouldShowLabel, true),
      WaitForEvent(kOfferNotificationChipElementId, kLabelExpansionFinished),
      Check([&]() { return timer.IsRunning(); }, "Mock timer is running"),
      PressButton(kOfferNotificationChipElementId),
      Check([&]() { return !timer.IsRunning(); }, "Mock timer is stopped"),
      CheckViewProperty(kOfferNotificationChipElementId,
                        &OfferNotificationIconView::ShouldShowLabel, true));
}

IN_PROC_BROWSER_TEST_F(OfferNotificationIconViewInteractiveTest,
                       IconCollapseAfterBubbleWidgetIsClosed) {
  SetUpShoppingServiceToReturnDiscounts();

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      FlushEvents(), EnsurePresent(kOfferNotificationChipElementId),
      CheckViewProperty(kOfferNotificationChipElementId,
                        &OfferNotificationIconView::ShouldShowLabel, true),
      WaitForEvent(kOfferNotificationChipElementId, kLabelExpansionFinished),
      PressButton(kOfferNotificationChipElementId),
      EnsurePresent(kOfferNotificationBubbleElementId),
      PressButton(views::BubbleFrameView::kCloseButtonElementId),
      WaitForEvent(kOfferNotificationChipElementId, kLabelAnimationFinished),
      CheckViewProperty(kOfferNotificationChipElementId,
                        &OfferNotificationIconView::ShouldShowLabel, false));
}

}  // namespace autofill
