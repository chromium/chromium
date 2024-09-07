// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/commerce/shopping_collection_iph_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/test_utils.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/interactive_test.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTab);
const char kBookmarkURL[] = "/bookmark.html";

std::unique_ptr<net::test_server::HttpResponse> BasicResponse(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content("bookmark page");
  response->set_content_type("text/html");
  return response;
}
}  // namespace

class BookmarkBubbleViewInteractiveTest : public InteractiveBrowserTest {
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
};

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewInteractiveTest, NewBookmark) {
  RunTestSequence(
      InstrumentTab(kTestTab),
      NavigateWebContents(kTestTab,
                          embedded_test_server()->GetURL(kBookmarkURL)),
      PressButton(kBookmarkStarViewElementId),
      WaitForShow(kBookmarkNameFieldId),
      CheckViewProperty(kBookmarkNameFieldId, &views::View::HasFocus, true),
      EnsurePresent(kBookmarkFolderFieldId));
}

class BookmarkBubbleViewIPHInteractiveTest
    : public BookmarkBubbleViewInteractiveTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&BookmarkBubbleViewIPHInteractiveTest::
                                        OnWillCreateBrowserContextServices,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(CreateMockTracker));
  }

  static std::unique_ptr<KeyedService> CreateMockTracker(
      content::BrowserContext* context) {
    auto mock_tracker = std::make_unique<
        testing::NiceMock<feature_engagement::test::MockTracker>>();
    return mock_tracker;
  }

 protected:
  feature_engagement::test::MockTracker* GetMockTracker(Profile* profile) {
    return static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetInstance()->GetForBrowserContext(
            browser()->profile()));
  }

 private:
  base::CallbackListSubscription create_services_subscription_;

  base::WeakPtrFactory<BookmarkBubbleViewIPHInteractiveTest> weak_ptr_factory_{
      this};
};

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewIPHInteractiveTest,
                       ShoppingCollectionIPH_Shown) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());

  ON_CALL(*GetMockTracker(browser()->profile()),
          ShouldTriggerHelpUI(
              testing::Ref(feature_engagement::kIPHShoppingCollectionFeature)))
      .WillByDefault(testing::Return(true));

  RunTestSequence(InstrumentTab(kTestTab),
                  NavigateWebContents(
                      kTestTab, embedded_test_server()->GetURL(kBookmarkURL)));

  // Make sure the bookmark is added to the shopping collection.
  const bookmarks::BookmarkNode* node = model->AddURL(
      commerce::GetShoppingCollectionBookmarkFolder(model, true), 0,
      u"bookmark",
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  commerce::AddProductInfoToExistingBookmark(model, node, u"Product", 12345L);

  EXPECT_CALL(*GetMockTracker(browser()->profile()),
              Dismissed(testing::Ref(
                  feature_engagement::kIPHShoppingCollectionFeature)));

  RunTestSequence(PressButton(kBookmarkStarViewElementId),
                  WaitForShow(commerce::kShoppingCollectionIPHViewId),
                  PressButton(kBookmarkBubbleOkButtonId),
                  WaitForHide(commerce::kShoppingCollectionIPHViewId));
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewIPHInteractiveTest,
                       ShoppingCollectionIPH_NotShown) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());

  ON_CALL(*GetMockTracker(browser()->profile()),
          ShouldTriggerHelpUI(testing::_))
      .WillByDefault(testing::Return(false));

  RunTestSequence(InstrumentTab(kTestTab),
                  NavigateWebContents(
                      kTestTab, embedded_test_server()->GetURL(kBookmarkURL)));

  // Make sure the bookmark is added to the shopping collection.
  const bookmarks::BookmarkNode* node = model->AddURL(
      commerce::GetShoppingCollectionBookmarkFolder(model, true), 0,
      u"bookmark",
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  commerce::AddProductInfoToExistingBookmark(model, node, u"Product", 12345L);

  RunTestSequence(PressButton(kBookmarkStarViewElementId),
                  EnsureNotPresent(commerce::kShoppingCollectionIPHViewId));
}
