// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/commerce/shopping_collection_iph_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/test_utils.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/interactive_test.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/cross_device_signin_promo_manager.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/signin/cross_device_signin_qr_bubble.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "ui/actions/actions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/widget/widget.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

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

  bookmarks::BookmarkModel* CreateBookmarkModel() {
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(browser()->GetProfile()),
        "test@email.com", signin::ConsentLevel::kSignin);

    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
    // Create account nodes in order to use the shopping collection.
    bookmark_model->CreateAccountPermanentFolders();

    return bookmark_model;
  }

 protected:
  feature_engagement::test::MockTracker* GetMockTracker(Profile* profile) {
    return static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetInstance()->GetForBrowserContext(
            browser()->GetProfile()));
  }

 private:
  base::CallbackListSubscription create_services_subscription_;

  base::WeakPtrFactory<BookmarkBubbleViewIPHInteractiveTest> weak_ptr_factory_{
      this};
};

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewIPHInteractiveTest,
                       ShoppingCollectionIPH_Shown) {
  bookmarks::BookmarkModel* model = CreateBookmarkModel();

  ON_CALL(*GetMockTracker(browser()->GetProfile()),
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
      browser()->GetTabStripModel()->GetActiveWebContents()->GetURL());

  commerce::AddProductInfoToExistingBookmark(model, node, u"Product", 12345L);

  EXPECT_CALL(*GetMockTracker(browser()->GetProfile()),
              Dismissed(testing::Ref(
                  feature_engagement::kIPHShoppingCollectionFeature)));

  RunTestSequence(PressButton(kBookmarkStarViewElementId),
                  WaitForShow(commerce::kShoppingCollectionIPHViewId),
                  PressButton(kBookmarkBubbleOkButtonId),
                  WaitForHide(commerce::kShoppingCollectionIPHViewId));
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewIPHInteractiveTest,
                       ShoppingCollectionIPH_NotShown) {
  bookmarks::BookmarkModel* model = CreateBookmarkModel();

  ON_CALL(*GetMockTracker(browser()->GetProfile()),
          ShouldTriggerHelpUI(testing::_))
      .WillByDefault(testing::Return(false));

  RunTestSequence(InstrumentTab(kTestTab),
                  NavigateWebContents(
                      kTestTab, embedded_test_server()->GetURL(kBookmarkURL)));

  // Make sure the bookmark is added to the shopping collection.
  const bookmarks::BookmarkNode* node = model->AddURL(
      commerce::GetShoppingCollectionBookmarkFolder(model, true), 0,
      u"bookmark",
      browser()->GetTabStripModel()->GetActiveWebContents()->GetURL());

  commerce::AddProductInfoToExistingBookmark(model, node, u"Product", 12345L);

  RunTestSequence(PressButton(kBookmarkStarViewElementId),
                  EnsureNotPresent(commerce::kShoppingCollectionIPHViewId));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class BookmarkBubbleViewCrossDeviceSigninInteractiveTest
    : public BookmarkBubbleViewInteractiveTest {
 public:
  BookmarkBubbleViewCrossDeviceSigninInteractiveTest() {
    feature_list_.InitWithFeatures(
        {switches::kCrossDeviceSigninFromDesktop,
         switches::kCrossDeviceSigninFromDesktopFollowup,
         switches::kSyncEnableBookmarksInTransportMode},
        {});
  }

  void SetUpOnMainThread() override {
    BookmarkBubbleViewInteractiveTest::SetUpOnMainThread();
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  }

 protected:
  // Signs in with bookmarks syncing to the account storage.
  AccountInfo SetUpEligibleAccount() {
    AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(browser()->GetProfile()),
        "elisa.g.beckett@gmail.com", signin::ConsentLevel::kSignin);
    SyncServiceFactory::GetForProfile(browser()->GetProfile())
        ->GetUserSettings()
        ->SetSelectedType(syncer::UserSelectableType::kBookmarks, true);
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
    bookmark_model->CreateAccountPermanentFolders();
    return account_info;
  }

  int GetBookmarkSaveDismissedCount(const AccountInfo& account_info) {
    return SigninPrefs(*browser()->GetProfile()->GetPrefs())
        .GetCrossDevicePromoState(
            account_info.GetGaiaId(),
            SigninPrefs::CrossDevicePromoSurface::kBookmarkSave)
        .dismissed_count;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewCrossDeviceSigninInteractiveTest,
                       DoneOpensOmniboxAnchoredQrBubbleAndHandlesDismissal) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info = SetUpEligibleAccount();

  // 1. Saving a bookmark opens the QR bubble anchored to the omnibox. Clicking
  // outside on the web page (near the top-left of the tab, well outside the
  // right-anchored bubble's window and Win32 shadow bounds) closes the QR
  // bubble on focus loss without counting an explicit dismissal or starting the
  // 7-day cooldown.
  RunTestSequence(
      InstrumentTab(kTestTab),
      NavigateWebContents(kTestTab,
                          embedded_test_server()->GetURL(kBookmarkURL)),
      PressButton(kBookmarkStarViewElementId),
      WaitForShow(kBookmarkBubbleOkButtonId),
      PressButton(kBookmarkBubbleOkButtonId),
      WaitForHide(kBookmarkBubbleOkButtonId),
      InAnyContext(WaitForShow(kCrossDeviceSigninQrBubbleWebViewElementId)),
      InAnyContext(WithView(kCrossDeviceSigninQrBubbleWebViewElementId,
                            [](views::View* web_view) {
                              views::Widget* bubble_widget =
                                  web_view->GetWidget();
                              ASSERT_TRUE(bubble_widget);
                              EXPECT_TRUE(bubble_widget->widget_delegate()
                                              ->AsBubbleDialogDelegate()
                                              ->ShouldCloseOnDeactivate());
                              bubble_widget->Activate();
                              views::test::WaitForWidgetActive(bubble_widget,
                                                               true);
                            })),
      MoveMouseTo(kTestTab, base::BindOnce([](ui::TrackedElement* el) {
                    return el->GetScreenBounds().origin() +
                           gfx::Vector2d(50, 50);
                  })),
      ClickMouse(),
      InAnyContext(WaitForHide(kCrossDeviceSigninQrBubbleWebViewElementId)));

  ASSERT_TRUE(base::test::RunUntil([this]() {
    return !IsCrossDeviceSigninPromoBubbleOpen(browser()->GetProfile());
  }));

  histogram_tester.ExpectTotalCount(
      "Signin.CrossDeviceSigninPromo.DismissedAtShownCount.BookmarkSave", 0);
  EXPECT_EQ(GetBookmarkSaveDismissedCount(account_info), 0);
  EXPECT_TRUE(ShouldStartShowingCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kBookmarkSave,
      browser()->GetProfile()));

  // 2. Saving a second bookmark shows the QR bubble again; clicking its [X]
  // close button records an explicit dismissal and starts the 7-day cooldown.
  RunTestSequence(
      NavigateWebContents(kTestTab,
                          embedded_test_server()->GetURL("/bookmark2.html")),
      PressButton(kBookmarkStarViewElementId),
      WaitForShow(kBookmarkBubbleOkButtonId),
      PressButton(kBookmarkBubbleOkButtonId),
      WaitForHide(kBookmarkBubbleOkButtonId),
      InAnyContext(WaitForShow(kCrossDeviceSigninQrBubbleWebViewElementId)),
      InAnyContext(PressButton(views::BubbleFrameView::kCloseButtonElementId)),
      InAnyContext(WaitForHide(kCrossDeviceSigninQrBubbleWebViewElementId)));

  ASSERT_TRUE(base::test::RunUntil([this]() {
    return !IsCrossDeviceSigninPromoBubbleOpen(browser()->GetProfile());
  }));

  histogram_tester.ExpectBucketCount(
      "Signin.CrossDeviceSigninPromo.OpenedQrCodeBubble",
      CrossDeviceSigninPromoEntryPoint::kBookmarkSave, 2);
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.DismissedAtShownCount.BookmarkSave", 2, 1);
  EXPECT_EQ(GetBookmarkSaveDismissedCount(account_info), 1);
  EXPECT_FALSE(ShouldStartShowingCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kBookmarkSave,
      browser()->GetProfile()));
}

// Clicking the bookmark star while the QR bubble is shown closes the QR bubble
// on focus loss (which is not a dismissal), and the click does not reopen the
// bookmark bubble, like for other bubbles anchored to the star.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewCrossDeviceSigninInteractiveTest,
                       StarClickClosesQrBubblePassivelyWithoutReopening) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info = SetUpEligibleAccount();
  actions::ActionItem* const bookmark_action_item =
      actions::ActionManager::Get().FindAction(
          kActionBookmarkThisTab,
          BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(bookmark_action_item);

  RunTestSequence(
      InstrumentTab(kTestTab),
      NavigateWebContents(kTestTab,
                          embedded_test_server()->GetURL(kBookmarkURL)),
      PressButton(kBookmarkStarViewElementId),
      WaitForShow(kBookmarkBubbleOkButtonId),
      PressButton(kBookmarkBubbleOkButtonId),
      WaitForHide(kBookmarkBubbleOkButtonId),
      InAnyContext(WaitForShow(kCrossDeviceSigninQrBubbleWebViewElementId)),
      InAnyContext(WithView(kCrossDeviceSigninQrBubbleWebViewElementId,
                            [](views::View* web_view) {
                              views::Widget* bubble_widget =
                                  web_view->GetWidget();
                              ASSERT_TRUE(bubble_widget);
                              bubble_widget->Activate();
                              views::test::WaitForWidgetActive(bubble_widget,
                                                               true);
                            })),
      // The star is marked as showing a bubble while the QR bubble is shown.
      CheckResult(
          [bookmark_action_item]() {
            return bookmark_action_item->GetIsShowingBubble();
          },
          true),
      MoveMouseTo(kBookmarkStarViewElementId), ClickMouse(),
      InAnyContext(WaitForHide(kCrossDeviceSigninQrBubbleWebViewElementId)),
      EnsureNotPresent(kBookmarkBubbleFrameViewId));

  ASSERT_TRUE(base::test::RunUntil([this]() {
    return !IsCrossDeviceSigninPromoBubbleOpen(browser()->GetProfile());
  }));
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
  EXPECT_FALSE(bookmark_action_item->GetIsShowingBubble());
  histogram_tester.ExpectTotalCount(
      "Signin.CrossDeviceSigninPromo.DismissedAtShownCount.BookmarkSave", 0);
  EXPECT_EQ(GetBookmarkSaveDismissedCount(account_info), 0);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
