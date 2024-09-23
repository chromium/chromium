// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/mock_commerce_ui_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/commerce/price_tracking_email_dialog_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/interactive_test.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kShoppingTab);
const char kShoppingURL[] = "/shopping.html";
const uint64_t kClusterId = 123345L;

std::unique_ptr<net::test_server::HttpResponse> BasicResponse(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content("shopping page");
  response->set_content_type("text/html");
  return response;
}
}  // namespace

class PriceTrackingEmailDialogConsentViewInteractiveTest
    : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    MockCommerceUiTabHelper::ReplaceFactory();
    test_iph_features_.InitForDemo(
        feature_engagement::kIPHPriceTrackingEmailConsentFeature);

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

    SetUpTabHelperAndShoppingService();
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &PriceTrackingEmailDialogConsentViewInteractiveTest::
                    OnWillCreateBrowserContextServices,
                weak_ptr_factory_.GetWeakPtr()));
  }

  void TearDownInProcessBrowserTestFixture() override {
    is_browser_context_services_created_ = false;
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    is_browser_context_services_created_ = true;
    commerce::ShoppingServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context) {
          return commerce::MockShoppingService::Build();
        }));
  }

  void ApplyMetaToBookmark() {
    bookmarks::BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    const bookmarks::BookmarkNode* node =
        model->GetMostRecentlyAddedUserNodeForURL(
            embedded_test_server()->GetURL(kShoppingURL));

    commerce::AddProductInfoToExistingBookmark(model, node, u"title",
                                               kClusterId);
  }

 private:
  base::test::ScopedFeatureList test_features_;
  feature_engagement::test::ScopedIphFeatureList test_iph_features_;
  base::CallbackListSubscription create_services_subscription_;
  bool is_browser_context_services_created_{false};

  void SetUpTabHelperAndShoppingService() {
    EXPECT_TRUE(is_browser_context_services_created_);
    auto* mock_shopping_service = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForBrowserContext(
            browser()->profile()));
    MockCommerceUiTabHelper* mock_tab_helper =
        static_cast<MockCommerceUiTabHelper*>(browser()
                                                  ->GetActiveTabInterface()
                                                  ->GetTabFeatures()
                                                  ->commerce_ui_tab_helper());
    ON_CALL(*mock_tab_helper, GetProductImage)
        .WillByDefault(
            testing::ReturnRef(mock_tab_helper->GetValidProductImage()));

    EXPECT_CALL(*mock_shopping_service, GetProductInfoForUrl)
        .Times(testing::AnyNumber());

    commerce::ProductInfo info;
    info.product_cluster_id.emplace(kClusterId);
    mock_shopping_service->SetResponseForGetProductInfoForUrl(info);
    mock_shopping_service->SetIsSubscribedCallbackValue(true);
  }

  base::WeakPtrFactory<PriceTrackingEmailDialogConsentViewInteractiveTest>
      weak_ptr_factory_{this};
};

IN_PROC_BROWSER_TEST_F(PriceTrackingEmailDialogConsentViewInteractiveTest,
                       EmailConsentDialogShown) {
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(browser()->profile()),
      "test@example.com", signin::ConsentLevel::kSync);

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),

      PressButton(kBookmarkStarViewElementId),

      WaitForShow(kBookmarkBubbleOkButtonId));

  // Manually apply the meta to the bookmark since everything else is mocked.
  ApplyMetaToBookmark();

  RunTestSequence(PressButton(kBookmarkBubbleOkButtonId),

                  WaitForShow(kPriceTrackingEmailConsentDialogId));
}

IN_PROC_BROWSER_TEST_F(PriceTrackingEmailDialogConsentViewInteractiveTest,
                       EmailConsentDialogNotShown) {
  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),

      PressButton(kBookmarkStarViewElementId),

      WaitForShow(kBookmarkBubbleOkButtonId),

      PressButton(kBookmarkBubbleOkButtonId),

      EnsureNotPresent(kPriceTrackingEmailConsentDialogId));
}
