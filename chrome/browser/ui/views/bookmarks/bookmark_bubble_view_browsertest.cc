// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/commerce/mock_commerce_ui_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/commerce/price_tracking_view.h"
#include "chrome/browser/ui/views/commerce/shopping_collection_iph_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "base/auto_reset.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/signin/cross_device_signin_promo_manager.h"
#include "chrome/browser/signin/mock_signin_ui_delegate.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/actions/actions.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

class BaseBookmarkBubbleViewBrowserTest : public DialogBrowserTest {
 public:
  BaseBookmarkBubbleViewBrowserTest() = default;
  ~BaseBookmarkBubbleViewBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&BaseBookmarkBubbleViewBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    commerce::ShoppingServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context) {
          return commerce::MockShoppingService::Build();
        }));
  }

  void ShowUi(const std::string& name) override {
#if !BUILDFLAG(IS_CHROMEOS)
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->GetProfile());
    signin::MakePrimaryAccountAvailable(identity_manager, "testuser@gtest.com",
                                        signin::ConsentLevel::kSignin);
#endif

    if (name == "bookmark_details_on_trackable_product") {
      commerce::ProductInfo info;
      info.product_cluster_id.emplace(12345L);
      commerce::MockShoppingService* mock_shopping_service =
          static_cast<commerce::MockShoppingService*>(
              commerce::ShoppingServiceFactory::GetForBrowserContext(
                  browser()->GetProfile()));
      mock_shopping_service->SetIsShoppingListEligible(true);
      mock_shopping_service->SetResponseForGetProductInfoForUrl(info);
      mock_shopping_service->SetIsSubscribedCallbackValue(false);
    }

    const GURL url = GURL("https://www.google.com");
    const std::u16string title = u"Title";
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
    bookmarks::AddIfNotBookmarked(bookmark_model, url, title);
    BrowserWindow::FromBrowser(browser())->ShowBookmarkBubble(url, true);

    if (name == "ios_promotion") {
      BookmarkBubbleView::bookmark_bubble()->AcceptDialog();
    }
  }

 protected:
  base::test::ScopedFeatureList test_features_;

 private:
  base::CallbackListSubscription create_services_subscription_;
  base::WeakPtrFactory<BaseBookmarkBubbleViewBrowserTest> weak_ptr_factory_{
      this};
};

class PowerBookmarkBubbleViewBrowserTest
    : public BaseBookmarkBubbleViewBrowserTest {
 public:
  PowerBookmarkBubbleViewBrowserTest() {
    commerce_ui_override_ = MockCommerceUiTabHelper::ReplaceFactory();
    test_features_.InitWithFeatures({commerce::kShoppingList}, {});
  }
  ~PowerBookmarkBubbleViewBrowserTest() override = default;

 private:
  ui::UserDataFactory::ScopedOverride commerce_ui_override_;
};

IN_PROC_BROWSER_TEST_F(PowerBookmarkBubbleViewBrowserTest,
                       InvokeUi_bookmark_details_on_trackable_product) {
  ShowAndVerifyUi();
}

namespace {
const char kTestBookmarkURL[] = "http://www.google.com";
}

class BookmarkBubbleViewMigrationBrowserTest : public InProcessBrowserTest {
 public:
  enum class InitializationMode { kEnableShoppingList, kDefer };

  explicit BookmarkBubbleViewMigrationBrowserTest(
      InitializationMode mode = InitializationMode::kEnableShoppingList) {
    if (mode == InitializationMode::kEnableShoppingList) {
      test_features_.InitAndEnableFeature(commerce::kShoppingList);
    }
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&BookmarkBubbleViewMigrationBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  virtual void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {
    commerce::ShoppingServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context) {
          return commerce::MockShoppingService::Build();
        }));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    bookmark_model_ =
        BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
    bookmark_node_ = bookmarks::AddIfNotBookmarked(
        bookmark_model_, GURL(kTestBookmarkURL), std::u16string());

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(kTestBookmarkURL)));
  }

  void TearDownOnMainThread() override {
    if (BookmarkBubbleView::bookmark_bubble()) {
      views::test::WidgetDestroyedWaiter destroyed_waiter(
          BookmarkBubbleView::bookmark_bubble()->GetWidget());
      BookmarkBubbleView::bookmark_bubble()->GetWidget()->CloseWithReason(
          views::Widget::ClosedReason::kUnspecified);
      destroyed_waiter.Wait();
    }
    bookmark_model_ = nullptr;
    bookmark_node_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void CreateBubbleView(bool already_bookmarked = true) {
    BrowserWindow::FromBrowser(browser())->ShowBookmarkBubble(
        GURL(kTestBookmarkURL), already_bookmarked);
  }

  bookmarks::BookmarkModel* GetBookmarkModel() { return bookmark_model_; }
  const bookmarks::BookmarkNode* GetBookmark() { return bookmark_node_; }

  views::View* GetViewInBookmarkBubble(ui::ElementIdentifier id) {
    views::BubbleDialogDelegate* const bubble =
        BookmarkBubbleView::bookmark_bubble();
    if (!bubble || !bubble->GetWidget()) {
      return nullptr;
    }
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForWidget(bubble->GetWidget());
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
        id, context);
  }

  PriceTrackingView* GetPriceTrackingView() {
    views::View* const matched_view =
        GetViewInBookmarkBubble(kPriceTrackingBookmarkViewElementId);
    return matched_view ? views::AsViewClass<PriceTrackingView>(matched_view)
                        : nullptr;
  }

 protected:
  raw_ptr<const bookmarks::BookmarkNode> bookmark_node_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  base::test::ScopedFeatureList test_features_;

 private:
  base::CallbackListSubscription create_services_subscription_;
  base::WeakPtrFactory<BookmarkBubbleViewMigrationBrowserTest>
      weak_ptr_factory_{this};
};

// The bubble edits the bookmark of one specific tab. Closing that tab used to
// leave the bubble on screen, still holding on to the destroyed tab's state;
// pressing "Done" on the orphaned bubble then crashed the browser. See
// https://crbug.com/526451600.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       BubbleClosesWhenTabIsClosed) {
  // A second tab keeps the browser window around once the bookmarked tab goes
  // away.
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  TabStripModel* const tab_strip = browser()->GetTabStripModel();
  ASSERT_EQ(2, tab_strip->count());

  tab_strip->ActivateTabAt(0);
  ASSERT_EQ(0, tab_strip->active_index());

  CreateBubbleView();
  ASSERT_TRUE(BookmarkBubbleView::bookmark_bubble());

  tab_strip->CloseWebContentsAt(0, TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       BubbleClosesWhenTabIsBackgrounded) {
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  TabStripModel* const tab_strip = browser()->GetTabStripModel();
  ASSERT_EQ(2, tab_strip->count());

  tab_strip->ActivateTabAt(0);
  ASSERT_EQ(0, tab_strip->active_index());

  CreateBubbleView();
  ASSERT_TRUE(BookmarkBubbleView::bookmark_bubble());

  tab_strip->ActivateTabAt(1);
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
}

// Verifies that the sync promo is not displayed for a signed in user.
// TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is
// deleted from the codebase. See ConsentLevel::kSync documentation for
// details.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       SyncPromoSignedIn) {
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(browser()->GetProfile()),
      "fake_username@gmail.com", signin::ConsentLevel::kSync);
  CreateBubbleView();
  EXPECT_FALSE(
      BookmarkBubbleView::bookmark_bubble()->GetFootnoteViewForTesting());
}

// Verifies that the sync promo is displayed for a user that is not signed in.
// TODO(crbug.com/491520553): This test is highly brittle in the
// InProcessBrowserTest environment. It relies on injecting a TestSyncService
// before the real IdentityManager and SyncService fully initialize, which
// causes timing and race conditions when attempting to trick the native UI
// layer into showing the "Not Signed In" promo state. See the migration KI for
// details on mocking SyncService.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       DISABLED_SyncPromoNotSignedIn) {
  CreateBubbleView();
  views::View* footnote =
      BookmarkBubbleView::bookmark_bubble()->GetFootnoteViewForTesting();
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(footnote);
#else
  EXPECT_TRUE(footnote);
#endif
}

// Verifies that the price tracking view is displayed for trackable product.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       PriceTrackingViewIsVisible) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(
              browser()->GetProfile()));
  mock_shopping_service->SetIsShoppingListEligible(true);

  commerce::ProductInfo info;
  info.product_cluster_id.emplace(12345L);
  mock_shopping_service->SetIsSubscribedCallbackValue(false);
  mock_shopping_service->SetResponseForGetProductInfoForUrl(info);
  CreateBubbleView();
  // Verify the view is displayed with toggle off.
  auto* price_tracking_view = GetPriceTrackingView();
  EXPECT_TRUE(price_tracking_view);
  EXPECT_FALSE(price_tracking_view->IsToggleOn());

  // No Price Tracking UKM tracked for bookmark that is not new.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Shopping_ShoppingAction::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       RecordPriceTrackingUkmForNewBookmark) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(
              browser()->GetProfile()));
  mock_shopping_service->SetIsShoppingListEligible(true);

  commerce::ProductInfo info;
  info.product_cluster_id.emplace(12345L);
  mock_shopping_service->SetIsSubscribedCallbackValue(false);
  mock_shopping_service->SetResponseForGetProductInfoForUrl(info);

  CreateBubbleView(false);

  // Price Tracking UKM tracked for new bookmark.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Shopping_ShoppingAction::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Shopping_ShoppingAction::kPriceTrackedName, 1);
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       PriceTrackingViewIsHidden) {
  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(
              browser()->GetProfile()));
  mock_shopping_service->SetResponseForGetProductInfoForUrl(std::nullopt);

  CreateBubbleView();
  auto* price_tracking_view = GetPriceTrackingView();
  EXPECT_FALSE(price_tracking_view);
}

// Verifies that the price tracking view is displayed with the correct toggle
// state
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       PriceTrackingViewWithToggleOn) {
  commerce::AddProductBookmark(GetBookmarkModel(), u"title",
                               GURL(kTestBookmarkURL), 0, true);

  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(
              browser()->GetProfile()));
  mock_shopping_service->SetIsShoppingListEligible(true);

  commerce::ProductInfo info;
  info.product_cluster_id.emplace(12345L);
  mock_shopping_service->SetResponseForGetProductInfoForUrl(info);
  mock_shopping_service->SetIsSubscribedCallbackValue(true);

  CreateBubbleView();
  auto* price_tracking_view = GetPriceTrackingView();
  EXPECT_TRUE(price_tracking_view);
  EXPECT_TRUE(price_tracking_view->IsToggleOn());
}

class PriceTrackingViewFeatureFlagBrowserTest
    : public BookmarkBubbleViewMigrationBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  PriceTrackingViewFeatureFlagBrowserTest()
      : BookmarkBubbleViewMigrationBrowserTest(InitializationMode::kDefer) {
    commerce_ui_override_ = MockCommerceUiTabHelper::ReplaceFactory();
    const bool is_feature_enabled = GetParam();
    if (is_feature_enabled) {
      test_features_.InitAndEnableFeature(commerce::kShoppingList);
    } else {
      test_features_.InitAndDisableFeature(commerce::kShoppingList);
    }
  }

  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    return info.param ? "ShoppingListEnabled" : "ShoppingListDisabled";
  }

 private:
  ui::UserDataFactory::ScopedOverride commerce_ui_override_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PriceTrackingViewFeatureFlagBrowserTest,
    testing::Bool(),
    &PriceTrackingViewFeatureFlagBrowserTest::DescribeParams);

IN_PROC_BROWSER_TEST_P(PriceTrackingViewFeatureFlagBrowserTest,
                       PriceTrackingViewCreation) {
  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(
              browser()->GetProfile()));
  commerce::ProductInfo info;
  info.product_cluster_id.emplace(12345L);
  mock_shopping_service->SetResponseForGetProductInfoForUrl(info);

  const bool is_feature_enabled = GetParam();
  mock_shopping_service->SetIsShoppingListEligible(is_feature_enabled);

  auto* mock_tab_helper = static_cast<MockCommerceUiTabHelper*>(
      commerce::CommerceUiTabHelper::From(browser()->GetActiveTabInterface()));
  ON_CALL(*mock_tab_helper, GetProductImage)
      .WillByDefault(
          testing::ReturnRef(mock_tab_helper->GetValidProductImage()));

  CreateBubbleView();

  auto* price_tracking_view = GetPriceTrackingView();

  if (is_feature_enabled) {
    EXPECT_TRUE(price_tracking_view);
  } else {
    EXPECT_FALSE(price_tracking_view);
  }
}

class BookmarkBubbleViewShoppingCollectionBrowserTest
    : public BookmarkBubbleViewMigrationBrowserTest {
 public:
  void SetUpOnMainThread() override {
    BookmarkBubbleViewMigrationBrowserTest::SetUpOnMainThread();

    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(browser()->GetProfile()),
        "test@example.com", signin::ConsentLevel::kSignin);
    GetBookmarkModel()->CreateAccountPermanentFolders();
  }

  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    BookmarkBubbleViewMigrationBrowserTest::OnWillCreateBrowserContextServices(
        context);
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&BookmarkBubbleViewShoppingCollectionBrowserTest::
                                BuildMockTracker));
  }

  static std::unique_ptr<KeyedService> BuildMockTracker(
      content::BrowserContext* context) {
    auto tracker = std::make_unique<feature_engagement::test::MockTracker>();
    ON_CALL(*tracker, ShouldTriggerHelpUI(testing::Ref(
                          feature_engagement::kIPHShoppingCollectionFeature)))
        .WillByDefault(testing::Return(true));
    return tracker;
  }

  void MoveBookmarkToShoppingCollection() {
    const bookmarks::BookmarkNode* collection =
        commerce::GetShoppingCollectionBookmarkFolder(GetBookmarkModel(), true);

    GetBookmarkModel()->Move(GetBookmark(), collection,
                             collection->children().size());
  }

  void AddProductInfoToBookmark() {
    commerce::AddProductInfoToExistingBookmark(
        GetBookmarkModel(), GetBookmark(), u"product", 12345L);
  }
};

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewShoppingCollectionBrowserTest,
                       IPHShown) {
  AddProductInfoToBookmark();
  MoveBookmarkToShoppingCollection();

  CreateBubbleView();

  views::View* iph_root =
      GetViewInBookmarkBubble(commerce::kShoppingCollectionIPHViewId);

  // The IPH should be shown in this case.
  EXPECT_TRUE(iph_root);
  EXPECT_TRUE(
      BookmarkBubbleView::bookmark_bubble()->GetFootnoteViewForTesting());
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewShoppingCollectionBrowserTest,
                       IPHNotShown_NotInCollection) {
  AddProductInfoToBookmark();

  CreateBubbleView();

  views::View* iph_root =
      GetViewInBookmarkBubble(commerce::kShoppingCollectionIPHViewId);

  // The IPH should not be shown.
  EXPECT_FALSE(iph_root);
  EXPECT_FALSE(
      BookmarkBubbleView::bookmark_bubble()->GetFootnoteViewForTesting());
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewShoppingCollectionBrowserTest,
                       IPHNotShown_NotAProduct) {
  MoveBookmarkToShoppingCollection();

  CreateBubbleView();

  views::View* const iph_root =
      GetViewInBookmarkBubble(commerce::kShoppingCollectionIPHViewId);

  // The IPH should not be shown.
  EXPECT_FALSE(iph_root);
  EXPECT_FALSE(
      BookmarkBubbleView::bookmark_bubble()->GetFootnoteViewForTesting());
}

class BookmarkBubbleViewWithAccountBookmarksBrowserTest
    : public BookmarkBubbleViewMigrationBrowserTest {
 public:
  BookmarkBubbleViewWithAccountBookmarksBrowserTest()
      : BookmarkBubbleViewMigrationBrowserTest(InitializationMode::kDefer) {
    test_features_.InitWithFeatures(
        {commerce::kShoppingList,
         switches::kSyncEnableBookmarksInTransportMode},
        {});
  }
};

// Verifies that the bookmark bubble correctly instantiates a combobox that
// separates account bookmarks and local bookmarks with headers. It also
// verifies that RecentlyUsedFoldersComboModel correctly reports those as
// "title" items and that ComboboxMenuModel correctly translates that to a
// TYPE_TITLE item that can be rendered differently when the popup menu is
// displayed within the browser UI environment.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewWithAccountBookmarksBrowserTest,
                       ComboboxUsesTitlesForHeaders) {
  GetBookmarkModel()->CreateAccountPermanentFolders();
  CreateBubbleView();

  views::View* folder_field = GetViewInBookmarkBubble(kBookmarkFolderFieldId);
  ASSERT_TRUE(folder_field);
  views::Combobox* combobox = views::AsViewClass<views::Combobox>(folder_field);
  ASSERT_TRUE(combobox);

  EXPECT_EQ(ui::MenuModel::TYPE_TITLE,
            combobox->menu_model_for_testing()->GetTypeAt(0));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

class BookmarkBubbleViewCrossDeviceSigninPromoBrowserTest
    : public BookmarkBubbleViewMigrationBrowserTest {
 public:
  explicit BookmarkBubbleViewCrossDeviceSigninPromoBrowserTest(
      bool enable_followup_feature = true)
      : BookmarkBubbleViewMigrationBrowserTest(InitializationMode::kDefer),
        delegate_auto_reset_(signin_ui_util::SetSigninUiDelegateForTesting(
            &mock_signin_ui_delegate_)) {
    std::vector<base::test::FeatureRef> enabled = {
        switches::kCrossDeviceSigninFromDesktop,
        switches::kSyncEnableBookmarksInTransportMode};
    std::vector<base::test::FeatureRef> disabled;
    if (enable_followup_feature) {
      enabled.push_back(switches::kCrossDeviceSigninFromDesktopFollowup);
    } else {
      disabled.push_back(switches::kCrossDeviceSigninFromDesktopFollowup);
    }
    test_features_.InitWithFeatures(enabled, disabled);
  }

  void SetUpOnMainThread() override {
    BookmarkBubbleViewMigrationBrowserTest::SetUpOnMainThread();

    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(browser()->GetProfile()),
        "fake_username@gmail.com", signin::ConsentLevel::kSignin);
    SetBookmarksSyncing(true);
    GetBookmarkModel()->CreateAccountPermanentFolders();
    GetBookmarkModel()->Move(GetBookmark(),
                             GetBookmarkModel()->account_other_node(), 0);
  }

  void SetBookmarksSyncing(bool is_syncing) {
    syncer::SyncService* const sync_service =
        SyncServiceFactory::GetForProfile(browser()->GetProfile());
    ASSERT_TRUE(sync_service);
    sync_service->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kBookmarks, is_syncing);
    ASSERT_EQ(is_syncing,
              sync_service->GetUserSettings()->GetSelectedTypes().Has(
                  syncer::UserSelectableType::kBookmarks));
  }

  void AcceptBubble() {
    ASSERT_TRUE(BookmarkBubbleView::bookmark_bubble());
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        BookmarkBubbleView::bookmark_bubble()->GetWidget());
    BookmarkBubbleView::bookmark_bubble()->AcceptDialog();
    destroyed_waiter.Wait();
  }

 protected:
  void UseRealSigninUiDelegate() { delegate_auto_reset_.reset(); }

  testing::StrictMock<signin_ui_util::MockSigninUiDelegate>
      mock_signin_ui_delegate_;

 private:
  std::optional<base::AutoReset<signin_ui_util::SigninUiDelegate*>>
      delegate_auto_reset_;
};

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewCrossDeviceSigninPromoBrowserTest,
                       QrBubbleShownAndExplicitlyDismissedAfterSavingBookmark) {
  base::HistogramTester histogram_tester;

  GURL qr_code_url;
  base::OnceClosure close_callback;
  base::OnceClosure explicit_dismissal_callback;
  EXPECT_CALL(mock_signin_ui_delegate_,
              ShowCrossDeviceSigninQrBubble(
                  testing::NotNull(), testing::_, testing::_,
                  CrossDeviceSigninPromoEntryPoint::kBookmarkSave, testing::_))
      .WillOnce([&](BrowserWindowInterface*, GURL url,
                    base::OnceClosure on_closed,
                    CrossDeviceSigninPromoEntryPoint,
                    base::OnceClosure on_explicit_dismissal) {
        qr_code_url = std::move(url);
        close_callback = std::move(on_closed);
        explicit_dismissal_callback = std::move(on_explicit_dismissal);
      });

  CreateBubbleView(/*already_bookmarked=*/false);
  ASSERT_NO_FATAL_FAILURE(AcceptBubble());

  std::string ios_campaign;
  EXPECT_TRUE(
      net::GetValueForKeyInQuery(qr_code_url, "ios-campaign", &ios_campaign));
  EXPECT_EQ("XDeviceBookmarkSave", ios_campaign);

  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.BookmarkSave",
      CrossDeviceSigninPromoShouldShowResult::kCanShow, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.OpenedQrCodeBubble",
      CrossDeviceSigninPromoEntryPoint::kBookmarkSave, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.ShownCount.BookmarkSave", 1, 1);

  // Explicit dismissal of the QR bubble activates the 7-day cooldown.
  ASSERT_TRUE(explicit_dismissal_callback);
  std::move(explicit_dismissal_callback).Run();
  ASSERT_TRUE(close_callback);
  std::move(close_callback).Run();
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.DismissedAtShownCount.BookmarkSave", 1, 1);

  CreateBubbleView(/*already_bookmarked=*/false);
  ASSERT_NO_FATAL_FAILURE(AcceptBubble());
  histogram_tester.ExpectBucketCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.BookmarkSave",
      CrossDeviceSigninPromoShouldShowResult::kCooldownActive, 1);
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewCrossDeviceSigninPromoBrowserTest,
                       QrBubbleNotShownForAlreadyBookmarkedPage) {
  base::HistogramTester histogram_tester;

  CreateBubbleView(/*already_bookmarked=*/true);
  ASSERT_NO_FATAL_FAILURE(AcceptBubble());

  histogram_tester.ExpectTotalCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.BookmarkSave", 0);
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewCrossDeviceSigninPromoBrowserTest,
                       QrBubbleNotShownForLocalOnlyBookmark) {
  base::HistogramTester histogram_tester;
  GetBookmarkModel()->Move(GetBookmark(), GetBookmarkModel()->other_node(), 0);
  ASSERT_TRUE(GetBookmarkModel()->IsLocalOnlyNode(*GetBookmark()));

  CreateBubbleView(/*already_bookmarked=*/false);
  ASSERT_NO_FATAL_FAILURE(AcceptBubble());

  histogram_tester.ExpectTotalCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.BookmarkSave", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.CrossDeviceSigninPromo.OpenedQrCodeBubble", 0);
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewCrossDeviceSigninPromoBrowserTest,
                       QrBubbleNotShownWhenBookmarksAreNotSyncing) {
  base::HistogramTester histogram_tester;
  GetBookmarkModel()->Move(GetBookmark(), GetBookmarkModel()->other_node(), 0);
  GetBookmarkModel()->RemoveAccountPermanentFolders();
  ASSERT_NO_FATAL_FAILURE(SetBookmarksSyncing(false));

  CreateBubbleView(/*already_bookmarked=*/false);
  ASSERT_NO_FATAL_FAILURE(AcceptBubble());

  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.BookmarkSave",
      CrossDeviceSigninPromoShouldShowResult::kDataTypeNotEnabled, 1);
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewCrossDeviceSigninPromoBrowserTest,
                       QrBubbleNotShownWhenSignedOut) {
  base::HistogramTester histogram_tester;
  GetBookmarkModel()->Move(GetBookmark(), GetBookmarkModel()->other_node(), 0);
  GetBookmarkModel()->RemoveAccountPermanentFolders();
  signin::ClearPrimaryAccount(
      IdentityManagerFactory::GetForProfile(browser()->GetProfile()));

  CreateBubbleView(/*already_bookmarked=*/false);
  ASSERT_NO_FATAL_FAILURE(AcceptBubble());

  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.BookmarkSave",
      CrossDeviceSigninPromoShouldShowResult::kNotSignedIn, 1);
}

// Saves a product bookmark and lets the price tracking email promo request
// permission to show, so that closing the bookmark bubble runs its post-save
// callback, which checks whether the product is price tracked.
class BookmarkBubbleViewCrossDeviceSigninPromoPriceTrackingBrowserTest
    : public BookmarkBubbleViewCrossDeviceSigninPromoBrowserTest {
 public:
  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    BookmarkBubbleViewCrossDeviceSigninPromoBrowserTest::
        OnWillCreateBrowserContextServices(context);
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          auto tracker = std::make_unique<
              testing::NiceMock<feature_engagement::test::MockTracker>>();
          ON_CALL(
              *tracker,
              ShouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHPriceTrackingEmailConsentFeature)))
              .WillByDefault(testing::Return(true));
          return tracker;
        }));
  }

  void SetUpOnMainThread() override {
    BookmarkBubbleViewCrossDeviceSigninPromoBrowserTest::SetUpOnMainThread();
    commerce::AddProductInfoToExistingBookmark(
        GetBookmarkModel(), GetBookmark(), u"product", kProductClusterId);
    // Not tracked, so that the post-save callback does not show a dialog.
    GetMockShoppingService()->SetIsSubscribedCallbackValue(false);
  }

  commerce::MockShoppingService* GetMockShoppingService() {
    return static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForBrowserContext(
            browser()->GetProfile()));
  }

  // Expects the post-save callback of the bookmark bubble to run `times`.
  void ExpectPostSaveCallbackRuns(int times) {
    EXPECT_CALL(
        *GetMockShoppingService(),
        IsSubscribed(testing::Field(&commerce::CommerceSubscription::id,
                                    base::NumberToString(kProductClusterId)),
                     testing::_))
        .Times(times);
  }

 private:
  static constexpr uint64_t kProductClusterId = 12345;
};

// A QR bubble opened from an unrelated entry point must not swallow the post-
// save price tracking promo of the bookmark bubble.
IN_PROC_BROWSER_TEST_F(
    BookmarkBubbleViewCrossDeviceSigninPromoPriceTrackingBrowserTest,
    PostSaveCallbackRunsWhileUnrelatedQrBubbleIsOpen) {
  base::OnceClosure close_callback;
  EXPECT_CALL(mock_signin_ui_delegate_,
              ShowCrossDeviceSigninQrBubble(
                  testing::NotNull(), testing::_, testing::_,
                  CrossDeviceSigninPromoEntryPoint::kProfileMenu, testing::_))
      .WillOnce(MoveArg<2>(&close_callback));
  OpenSigninToPhoneQrCodeBubble(browser(),
                                CrossDeviceSigninPromoEntryPoint::kProfileMenu,
                                base::DoNothing());
  ASSERT_TRUE(IsCrossDeviceSigninPromoBubbleOpen(browser()->GetProfile()));

  ExpectPostSaveCallbackRuns(1);
  CreateBubbleView(/*already_bookmarked=*/false);
  ASSERT_NO_FATAL_FAILURE(AcceptBubble());
  testing::Mock::VerifyAndClearExpectations(GetMockShoppingService());

  ASSERT_TRUE(close_callback);
  std::move(close_callback).Run();
}

// The QR promo opened by the bookmark save replaces its post-save price
// tracking promo.
IN_PROC_BROWSER_TEST_F(
    BookmarkBubbleViewCrossDeviceSigninPromoPriceTrackingBrowserTest,
    PostSaveCallbackDroppedWhenSaveOpensQrBubble) {
  base::OnceClosure close_callback;
  EXPECT_CALL(mock_signin_ui_delegate_,
              ShowCrossDeviceSigninQrBubble(
                  testing::NotNull(), testing::_, testing::_,
                  CrossDeviceSigninPromoEntryPoint::kBookmarkSave, testing::_))
      .WillOnce(MoveArg<2>(&close_callback));

  ExpectPostSaveCallbackRuns(0);
  CreateBubbleView(/*already_bookmarked=*/false);
  ASSERT_NO_FATAL_FAILURE(AcceptBubble());
  testing::Mock::VerifyAndClearExpectations(GetMockShoppingService());

  ASSERT_TRUE(close_callback);
  std::move(close_callback).Run();
}

// Reopening `BookmarkBubbleView` while the post-save QR bubble is either still
// loading or already visible closes the QR bubble passively (without recording
// a dismissal or starting the cooldown) and keeps `kActionBookmarkThisTab`'s
// showing-bubble state set after `DeleteSoon()` drains.
IN_PROC_BROWSER_TEST_F(
    BookmarkBubbleViewCrossDeviceSigninPromoBrowserTest,
    ReopeningBookmarkBubbleClosesLoadingOrVisibleQrBubbleCleanly) {
  static constexpr char kQrBubbleWidgetName[] =
      "CrossDeviceSigninQrBubbleViews";
  UseRealSigninUiDelegate();
  base::HistogramTester histogram_tester;
  Profile* const profile = browser()->GetProfile();
  actions::ActionItem* const bookmark_action_item =
      actions::ActionManager::Get().FindAction(
          kActionBookmarkThisTab,
          BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(bookmark_action_item);

  // 1. Reopening `BookmarkBubbleView` while the post-save QR bubble is still
  // loading (before WebUI load completes).
  {
    views::Widget* loading_qr_widget = nullptr;
    bool qr_widget_shown = false;
    views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
    observer.set_initialized_callback(
        base::BindLambdaForTesting([&](views::Widget* widget) {
          if (widget->GetName() == kQrBubbleWidgetName) {
            loading_qr_widget = widget;
          }
        }));
    observer.set_shown_callback(
        base::BindLambdaForTesting([&](views::Widget* widget) {
          if (widget->GetName() == kQrBubbleWidgetName) {
            qr_widget_shown = true;
          }
        }));

    CreateBubbleView(/*already_bookmarked=*/false);
    ASSERT_NO_FATAL_FAILURE(AcceptBubble());
    ASSERT_TRUE(loading_qr_widget);
    EXPECT_FALSE(loading_qr_widget->IsVisible());
    EXPECT_TRUE(IsCrossDeviceSigninPromoBubbleOpen(profile));

    views::test::WidgetDestroyedWaiter qr_destroyed_waiter(loading_qr_widget);
    CreateBubbleView(/*already_bookmarked=*/true);
    ASSERT_NE(BookmarkBubbleView::bookmark_bubble(), nullptr);
    auto bookmark_pin =
        BookmarkBubbleView::bookmark_bubble()->PreventCloseOnDeactivate();
    qr_destroyed_waiter.Wait();

    EXPECT_FALSE(qr_widget_shown);
    EXPECT_FALSE(IsCrossDeviceSigninPromoBubbleOpen(profile));
    ASSERT_NE(BookmarkBubbleView::bookmark_bubble(), nullptr);
    EXPECT_TRUE(
        BookmarkBubbleView::bookmark_bubble()->GetWidget()->IsVisible());
    EXPECT_TRUE(bookmark_action_item->GetIsShowingBubble());
    histogram_tester.ExpectTotalCount(
        "Signin.CrossDeviceSigninPromo.DismissedAtShownCount.BookmarkSave", 0);

    views::test::WidgetDestroyedWaiter bookmark_closed_waiter(
        BookmarkBubbleView::bookmark_bubble()->GetWidget());
    BookmarkBubbleView::Hide();
    bookmark_closed_waiter.Wait();
    EXPECT_FALSE(bookmark_action_item->GetIsShowingBubble());
  }

  // 2. Reopening `BookmarkBubbleView` while the post-save QR bubble is visible
  // on screen.
  {
    std::unique_ptr<views::BubbleDialogDelegate::CloseOnDeactivatePin> pin;
    views::AnyWidgetObserver init_observer(views::test::AnyWidgetTestPasskey{});
    init_observer.set_initialized_callback(
        base::BindLambdaForTesting([&](views::Widget* widget) {
          if (widget->GetName() == kQrBubbleWidgetName) {
            pin = widget->widget_delegate()
                      ->AsBubbleDialogDelegate()
                      ->PreventCloseOnDeactivate();
          }
        }));
    views::NamedWidgetShownWaiter qr_shown_waiter(
        views::test::AnyWidgetTestPasskey{}, kQrBubbleWidgetName);

    CreateBubbleView(/*already_bookmarked=*/false);
    ASSERT_NO_FATAL_FAILURE(AcceptBubble());

    views::Widget* visible_qr_widget = qr_shown_waiter.WaitIfNeededAndGet();
    ASSERT_TRUE(visible_qr_widget);
    ASSERT_TRUE(pin);
    EXPECT_TRUE(visible_qr_widget->IsVisible());
    EXPECT_TRUE(IsCrossDeviceSigninPromoBubbleOpen(profile));
    EXPECT_TRUE(bookmark_action_item->GetIsShowingBubble());

    views::test::WidgetDestroyedWaiter qr_destroyed_waiter(visible_qr_widget);
    CreateBubbleView(/*already_bookmarked=*/true);
    ASSERT_NE(BookmarkBubbleView::bookmark_bubble(), nullptr);
    auto bookmark_pin =
        BookmarkBubbleView::bookmark_bubble()->PreventCloseOnDeactivate();
    qr_destroyed_waiter.Wait();

    EXPECT_FALSE(IsCrossDeviceSigninPromoBubbleOpen(profile));
    ASSERT_NE(BookmarkBubbleView::bookmark_bubble(), nullptr);
    EXPECT_TRUE(
        BookmarkBubbleView::bookmark_bubble()->GetWidget()->IsVisible());
    EXPECT_TRUE(bookmark_action_item->GetIsShowingBubble());
    histogram_tester.ExpectTotalCount(
        "Signin.CrossDeviceSigninPromo.DismissedAtShownCount.BookmarkSave", 0);
    EXPECT_TRUE(ShouldStartShowingCrossDeviceSigninPromo(
        CrossDeviceSigninPromoEntryPoint::kBookmarkSave, profile));
  }
}

class BookmarkBubbleViewCrossDeviceSigninPromoDisabledBrowserTest
    : public BookmarkBubbleViewCrossDeviceSigninPromoBrowserTest {
 public:
  BookmarkBubbleViewCrossDeviceSigninPromoDisabledBrowserTest()
      : BookmarkBubbleViewCrossDeviceSigninPromoBrowserTest(
            /*enable_followup_feature=*/false) {}
};

// In the control arm (`kCrossDeviceSigninFromDesktopFollowup` disabled), no QR
// bubble is shown, while `ShouldShowResult.BookmarkSave` is still logged at the
// Done trigger.
IN_PROC_BROWSER_TEST_F(
    BookmarkBubbleViewCrossDeviceSigninPromoDisabledBrowserTest,
    ControlArmLogsEligibilityWithoutShowingQrBubble) {
  base::HistogramTester histogram_tester;

  CreateBubbleView(/*already_bookmarked=*/false);
  ASSERT_NO_FATAL_FAILURE(AcceptBubble());

  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.BookmarkSave",
      CrossDeviceSigninPromoShouldShowResult::kCanShow, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.CrossDeviceSigninPromo.OpenedQrCodeBubble", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.CrossDeviceSigninPromo.ShownCount.BookmarkSave", 0);
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
