// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/price_tracking_handler.h"

#include "base/test/bind.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/account_checker.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/mojom/shared.mojom.h"
#include "components/commerce/core/test_utils.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace commerce {
namespace {

const char kTestUrl[] = "https://example.com/";

class MockPage : public price_tracking::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<price_tracking::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<price_tracking::mojom::Page> receiver_{this};

  MOCK_METHOD(void,
              PriceTrackedForBookmark,
              (shared::mojom::BookmarkProductInfoPtr bookmark_product),
              (override));
  MOCK_METHOD(void,
              PriceUntrackedForBookmark,
              (shared::mojom::BookmarkProductInfoPtr bookmark_product),
              (override));
  MOCK_METHOD(void,
              OperationFailedForBookmark,
              (shared::mojom::BookmarkProductInfoPtr bookmark_product,
               bool attempted_track),
              (override));
  MOCK_METHOD(void,
              OnProductBookmarkMoved,
              (shared::mojom::BookmarkProductInfoPtr bookmark_product),
              (override));
};

}  // namespace

class PriceTrackingHandlerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents());
    account_checker_ = std::make_unique<MockAccountChecker>();
    account_checker_->SetLocale("en-us");
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    MockAccountChecker::RegisterCommercePrefs(pref_service_->registry());
    SetTabCompareEnterprisePolicyPref(pref_service_.get(), 0);
    SetShoppingListEnterprisePolicyPref(pref_service_.get(), true);
    account_checker_->SetPrefs(pref_service_.get());

    shopping_service_ = std::make_unique<MockShoppingService>();
    shopping_service_->SetAccountChecker(account_checker_.get());

    auto client = std::make_unique<bookmarks::TestBookmarkClient>();
    client->SetIsSyncFeatureEnabledIncludingBookmarks(true);
    bookmark_model_ =
        bookmarks::TestBookmarkClient::CreateModelWithClient(std::move(client));

    handler_ = std::make_unique<commerce::PriceTrackingHandler>(
        page_.BindAndGetRemote(),
        mojo::PendingReceiver<price_tracking::mojom::PriceTrackingHandler>(),
        web_ui_.get(), shopping_service_.get(), &tracker_,
        bookmark_model_.get());

    NavigateToURL(GURL(kTestUrl));
  }

  void TearDownOnMainThread() override {
    handler_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void NavigateToURL(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    base::PlatformThread::Sleep(base::Seconds(2));
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<MockAccountChecker> account_checker_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  feature_engagement::test::MockTracker tracker_;

  MockPage page_;
  std::unique_ptr<commerce::PriceTrackingHandler> handler_;

  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(PriceTrackingHandlerBrowserTest,
                       TestTrackPriceForCurrentUrl) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  AddProductBookmark(bookmark_model_.get(), u"product 1", GURL(kTestUrl), 123L,
                     false, 1230000, "usd");
  EXPECT_CALL(*shopping_service_,
              Subscribe(VectorHasSubscriptionWithId("123"), testing::_))
      .Times(1);

  handler_->SetPriceTrackingStatusForCurrentUrl(true);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Shopping_ShoppingAction::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Shopping_ShoppingAction::kPriceTrackedName, 1);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingHandlerBrowserTest,
                       TestTrackPriceForCurrentUrl_NoBookmark) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const bookmarks::BookmarkNode* other_node = bookmark_model_->other_node();
  size_t bookmark_count = other_node->children().size();

  handler_->SetPriceTrackingStatusForCurrentUrl(true);

  ASSERT_EQ(bookmark_count + 1, other_node->children().size());
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Shopping_ShoppingAction::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Shopping_ShoppingAction::kPriceTrackedName, 1);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingHandlerBrowserTest,
                       TestUntrackPriceForCurrentUrl) {
  ProductInfo info;
  info.product_cluster_id = 123u;
  info.title = "product";
  AddProductBookmark(bookmark_model_.get(), u"product", GURL(kTestUrl),
                     info.product_cluster_id.value(), false, 1230000, "usd");
  shopping_service_->SetIsSubscribedCallbackValue(true);
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  EXPECT_CALL(*shopping_service_,
              Unsubscribe(VectorHasSubscriptionWithId("123"), testing::_))
      .Times(1);

  handler_->SetPriceTrackingStatusForCurrentUrl(false);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(
    PriceTrackingHandlerBrowserTest,
    TestGetParentBookmarkFolderNameForCurrentUrl_NoBookmark) {
  NavigateToURL(GURL(kTestUrl));

  base::RunLoop run_loop;
  handler_->GetParentBookmarkFolderNameForCurrentUrl(
      base::BindOnce([](const std::u16string& name) {
        ASSERT_EQ(u"", name);
      }).Then(run_loop.QuitClosure()));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(PriceTrackingHandlerBrowserTest,
                       TestShowBookmarkEditorForCurrentUrl_WithBookmark) {
  NavigateToURL(GURL(kTestUrl));

  const bookmarks::BookmarkNode* other_node = bookmark_model_->other_node();
  bookmark_model_->AddNewURL(other_node, other_node->children().size(), u"test",
                             GURL(kTestUrl));

  auto bookmark_editor_waiter = views::NamedWidgetShownWaiter(
      views::test::AnyWidgetTestPasskey{}, BookmarkEditorView::kViewClassName);

  handler_->ShowBookmarkEditorForCurrentUrl();

  ASSERT_TRUE(bookmark_editor_waiter.WaitIfNeededAndGet());
}

IN_PROC_BROWSER_TEST_F(PriceTrackingHandlerBrowserTest,
                       TestShowBookmarkEditorForCurrentUrl_WithoutBookmark) {
  NavigateToURL(GURL(kTestUrl));

  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
  observer.set_shown_callback(
      base::BindLambdaForTesting([&](views::Widget* widget) {
        ASSERT_FALSE(widget->GetName() == BookmarkEditorView::kViewClassName);
      }));

  handler_->ShowBookmarkEditorForCurrentUrl();
}

}  // namespace commerce
