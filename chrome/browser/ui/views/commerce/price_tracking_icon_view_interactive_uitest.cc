// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/sync/local_or_syncable_bookmark_sync_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/commerce/mock_commerce_ui_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/commerce/price_tracking_bubble_dialog_view.h"
#include "chrome/browser/ui/views/commerce/price_tracking_icon_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

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

class PriceTrackingIconViewInteractiveTest : public InteractiveBrowserTest {
 public:
  PriceTrackingIconViewInteractiveTest() {
    test_features_.InitWithFeatures(
        {commerce::kCommerceAllowChipExpansion, commerce::kShoppingList,
         feature_engagement::kIPHPriceTrackingInSidePanelFeature},
        {});
  }

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

    SetUpTabHelperAndShoppingService();

    // Pretend sync is on for bookmarks, required for price tracking.
    LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(
        browser()->profile())
        ->SetIsTrackingMetadataForTesting();
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&PriceTrackingIconViewInteractiveTest::
                                        OnWillCreateBrowserContextServices,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  void TearDownInProcessBrowserTestFixture() override {
    is_browser_context_services_created = false;
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    is_browser_context_services_created = true;
    commerce::ShoppingServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context) {
          return commerce::MockShoppingService::Build();
        }));
  }

  void SimulateServerPriceTrackStateUpdated(bool is_price_tracked,
                                            const GURL& url) {
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());

    mock_shopping_service_->SetIsSubscribedCallbackValue(true);

    commerce::AddProductBookmark(bookmark_model, u"title", url, 0,
                                 is_price_tracked);
  }

 protected:
  raw_ptr<commerce::MockShoppingService, AcrossTasksDanglingUntriaged>
      mock_shopping_service_;
  raw_ptr<commerce::CommerceUiTabHelper, AcrossTasksDanglingUntriaged>
      tab_helper_;
  std::unique_ptr<image_fetcher::MockImageFetcher> image_fetcher_;
  std::optional<commerce::ProductInfo> product_info_;
  base::CallbackListSubscription create_services_subscription_;
  bool is_browser_context_services_created{false};

 private:
  base::test::ScopedFeatureList test_features_;

  void SetUpTabHelperAndShoppingService() {
    EXPECT_TRUE(is_browser_context_services_created);
    mock_shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForBrowserContext(
            browser()->profile()));

    image_fetcher_ = std::make_unique<image_fetcher::MockImageFetcher>();
    ON_CALL(*image_fetcher_, FetchImageAndData_)
        .WillByDefault(
            [](const GURL& image_url,
               image_fetcher::ImageDataFetcherCallback* image_data_callback,
               image_fetcher::ImageFetcherCallback* image_callback,
               image_fetcher::ImageFetcherParams params) {
              SkBitmap bitmap;
              // The image size must not be too small (e.g., 1x1), as it becomes
              // empty when cropped for rounded corners in BubbleFrameView,
              // triggering a DCHECK in ImageSkiaRep.
              bitmap.allocN32Pixels(10, 10);
              gfx::Image image =
                  gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));

              std::move(*image_callback)
                  .Run(std::move(image), image_fetcher::RequestMetadata());
            });

    tab_helper_ = browser()
                      ->GetActiveTabInterface()
                      ->GetTabFeatures()
                      ->commerce_ui_tab_helper();
    tab_helper_->GetPriceTrackingControllerForTesting()
        ->SetImageFetcherForTesting(image_fetcher_.get());

    product_info_ = commerce::ProductInfo();
    product_info_->title = "Product";
    product_info_->product_cluster_title = "Product";
    product_info_->product_cluster_id = 12345L;
    product_info_->image_url = GURL("http://example.com/image.png");
    mock_shopping_service_->SetResponseForGetProductInfoForUrl(product_info_);

    EXPECT_CALL(*mock_shopping_service_, IsPriceInsightsEligible)
        .Times(testing::AnyNumber());

    mock_shopping_service_->SetIsShoppingListEligible(true);
    mock_shopping_service_->SetIsPriceInsightsEligible(false);
    mock_shopping_service_->SetIsDiscountEligibleToShowOnNavigation(false);
  }

  base::WeakPtrFactory<PriceTrackingIconViewInteractiveTest> weak_ptr_factory_{
      this};
};

// TODO(crbug.com/41494779): Test is failing on Mac under ChromeRefresh2023
// flags. Evaluate, fix, and remove the skip.
#if BUILDFLAG(IS_MAC)
#define MAYBE_FUEBubbleShownOnPress DISABLED_FUEBubbleShownOnPress
#else
#define MAYBE_FUEBubbleShownOnPress FUEBubbleShownOnPress
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       MAYBE_FUEBubbleShownOnPress) {
  mock_shopping_service_->SetIsSubscribedCallbackValue(false);
  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kPriceTrackingChipElementId),
      PressButton(kPriceTrackingChipElementId),
      WaitForShow(kPriceTrackingBubbleDialogId));

  auto* bubble = static_cast<PriceTrackingBubbleDialogView*>(
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kPriceTrackingBubbleDialogId,
          browser()->window()->GetElementContext()));
  EXPECT_EQ(bubble->GetTypeForTesting(),
            PriceTrackingBubbleDialogView::Type::TYPE_FIRST_USE_EXPERIENCE);
}

// TODO(crbug.com/41494779): Test is failing on Mac under ChromeRefresh2023
// flags. Evaluate, fix, and remove the skip.
#if BUILDFLAG(IS_MAC)
#define MAYBE_PriceTrackingBubbleShownOnPress_BeforeFUEOnTrackedProduct \
  DISABLED_PriceTrackingBubbleShownOnPress_BeforeFUEOnTrackedProduct
#else
#define MAYBE_PriceTrackingBubbleShownOnPress_BeforeFUEOnTrackedProduct \
  PriceTrackingBubbleShownOnPress_BeforeFUEOnTrackedProduct
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(
    PriceTrackingIconViewInteractiveTest,
    MAYBE_PriceTrackingBubbleShownOnPress_BeforeFUEOnTrackedProduct) {
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble));
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  commerce::AddProductBookmark(bookmark_model, u"title",
                               embedded_test_server()->GetURL(kShoppingURL), 0,
                               true);
  mock_shopping_service_->SetIsSubscribedCallbackValue(true);

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kPriceTrackingChipElementId),
      CheckView(kPriceTrackingChipElementId,
                base::BindOnce([](PriceTrackingIconView* view) {
                  return view->GetVectorIcon().name ==
                         omnibox::kPriceTrackingEnabledRefreshIcon.name;
                })),
      PressButton(kPriceTrackingChipElementId),
      WaitForShow(kPriceTrackingBubbleDialogId));

  auto* bubble = static_cast<PriceTrackingBubbleDialogView*>(
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kPriceTrackingBubbleDialogId,
          browser()->window()->GetElementContext()));
  EXPECT_EQ(bubble->GetTypeForTesting(),
            PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);
}

// TODO(crbug.com/41494779): Test is failing on Mac under ChromeRefresh2023
// flags. Evaluate, fix, and remove the skip.
#if BUILDFLAG(IS_MAC)
#define MAYBE_PriceTrackingBubbleShownOnPress_AfterFUE \
  DISABLED_PriceTrackingBubbleShownOnPress_AfterFUE
#else
#define MAYBE_PriceTrackingBubbleShownOnPress_AfterFUE \
  PriceTrackingBubbleShownOnPress_AfterFUE
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       MAYBE_PriceTrackingBubbleShownOnPress_AfterFUE) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);
  mock_shopping_service_->SetIsSubscribedCallbackValue(false);

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kPriceTrackingChipElementId),
      PressButton(kPriceTrackingChipElementId),
      WaitForShow(kPriceTrackingBubbleDialogId));

  auto* bubble = static_cast<PriceTrackingBubbleDialogView*>(
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kPriceTrackingBubbleDialogId,
          browser()->window()->GetElementContext()));
  EXPECT_EQ(bubble->GetTypeForTesting(),
            PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);
}

// TODO(crbug.com/41494779): Test is failing on Mac under ChromeRefresh2023
// flags. Evaluate, fix, and remove the skip.
#if BUILDFLAG(IS_MAC)
#define MAYBE_BubbleCanBeReshowOnPress DISABLED_BubbleCanBeReshowOnPress
#else
#define MAYBE_BubbleCanBeReshowOnPress BubbleCanBeReshowOnPress
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       MAYBE_BubbleCanBeReshowOnPress) {
  mock_shopping_service_->SetIsSubscribedCallbackValue(false);

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kPriceTrackingChipElementId),
      PressButton(kPriceTrackingChipElementId),
      WaitForShow(kPriceTrackingBubbleDialogId));

  auto* widget =
      static_cast<PriceTrackingBubbleDialogView*>(
          views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
              kPriceTrackingBubbleDialogId,
              browser()->window()->GetElementContext()))
          ->GetWidget();
  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  widget->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
  destroyed_waiter.Wait();

  RunTestSequence(WaitForHide(kPriceTrackingBubbleDialogId),
                  // Click the icon again to reshow the bubble.
                  PressButton(kPriceTrackingChipElementId),
                  WaitForShow(kPriceTrackingBubbleDialogId));
}

// TODO(crbug.com/41483562): fix and re-enable for CR2023.
IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       DISABLED_EnablePriceTrackOnPress) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);
  mock_shopping_service_->SetIsSubscribedCallbackValue(false);

  const GURL shopping_url = embedded_test_server()->GetURL(kShoppingURL);

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab, shopping_url),
      WaitForShow(kPriceTrackingChipElementId),
      CheckView(kPriceTrackingChipElementId,
                base::BindOnce([](PriceTrackingIconView* view) {
                  return view->GetIconLabelForTesting() ==
                         l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE);
                })),
      CheckView(kPriceTrackingChipElementId,
                base::BindOnce([](PriceTrackingIconView* view) {
                  return view->GetVectorIcon().name ==
                          omnibox::kPriceTrackingDisabledRefreshIcon.name;
                })),
      PressButton(kPriceTrackingChipElementId),
      WaitForShow(kPriceTrackingBubbleDialogId));

  SimulateServerPriceTrackStateUpdated(true, shopping_url);
  mock_shopping_service_->SetIsSubscribedCallbackValue(true);

  RunTestSequence(
      CheckView(kPriceTrackingChipElementId,
                base::BindOnce([](PriceTrackingIconView* view) {
                  return view->GetIconLabelForTesting() ==
                         l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACKING_PRICE);
                })),
      CheckViewProperty(
          kPriceTrackingChipElementId,
          &PriceTrackingIconView::GetTextForTooltipAndAccessibleName,
          l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACKING_PRICE)),
      CheckView(kPriceTrackingChipElementId,
                base::BindOnce([](PriceTrackingIconView* view) {
                  return view->GetVectorIcon().name ==
                          omnibox::kPriceTrackingEnabledRefreshIcon.name;
                })));
}

// TODO(crbug.com/41494779): Test is failing on Mac under ChromeRefresh2023
// flags. Evaluate, fix, and remove the skip.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CreateBookmarkOnPressIfNotExist \
  DISABLED_CreateBookmarkOnPressIfNotExist
#else
#define MAYBE_CreateBookmarkOnPressIfNotExist CreateBookmarkOnPressIfNotExist
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       MAYBE_CreateBookmarkOnPressIfNotExist) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);
  mock_shopping_service_->SetIsSubscribedCallbackValue(false);

  const GURL shopping_url = embedded_test_server()->GetURL(kShoppingURL);
  RunTestSequence(InstrumentTab(kShoppingTab),
                  NavigateWebContents(kShoppingTab, shopping_url),
                  WaitForShow(kPriceTrackingChipElementId));

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  EXPECT_FALSE(bookmarks::IsBookmarkedByUser(bookmark_model, shopping_url));
  RunTestSequence(PressButton(kPriceTrackingChipElementId));

  EXPECT_TRUE(bookmarks::IsBookmarkedByUser(bookmark_model, shopping_url));
}

// TODO(crbug.com/41494779): Test is failing on Mac under ChromeRefresh2023
// flags. Evaluate, fix, and remove the skip.
#if BUILDFLAG(IS_MAC)
#define MAYBE_RecordOmniboxChipClicked DISABLED_RecordOmniboxChipClicked
#else
#define MAYBE_RecordOmniboxChipClicked RecordOmniboxChipClicked
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       MAYBE_RecordOmniboxChipClicked) {
  base::UserActionTester user_action_tester;
  mock_shopping_service_->SetIsSubscribedCallbackValue(false);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Commerce.PriceTracking.OmniboxChipClicked"),
            0);
  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kPriceTrackingChipElementId),
      PressButton(kPriceTrackingChipElementId));
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Commerce.PriceTracking.OmniboxChipClicked"),
            1);
}

// TODO(crbug.com/41494779): Test is failing on Mac under ChromeRefresh2023
// flags. Evaluate, fix, and remove the skip.
#if BUILDFLAG(IS_MAC)
#define MAYBE_RecordOmniboxChipTracked DISABLED_RecordOmniboxChipTracked
#else
#define MAYBE_RecordOmniboxChipTracked RecordOmniboxChipTracked
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       MAYBE_RecordOmniboxChipTracked) {
  base::UserActionTester user_action_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  mock_shopping_service_->SetIsSubscribedCallbackValue(false);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);

  EXPECT_EQ(user_action_tester.GetActionCount(
                "Commerce.PriceTracking.OmniboxChip.Tracked"),
            0);
  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kPriceTrackingChipElementId),
      PressButton(kPriceTrackingChipElementId));
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Commerce.PriceTracking.OmniboxChip.Tracked"),
            1);
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Shopping_ShoppingAction::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Shopping_ShoppingAction::kPriceTrackedName, 1);
  ukm_recorder.ExpectEntrySourceHasUrl(
      entries[0], embedded_test_server()->GetURL(kShoppingURL));
}

// TODO(crbug.com/41494779): Test is failing on Mac under ChromeRefresh2023
// flags. Evaluate, fix, and remove the skip.
#if BUILDFLAG(IS_MAC)
#define MAYBE_NoRecordOmniboxChipTracked_ForTrackedProduct \
  DISABLED_NoRecordOmniboxChipTracked_ForTrackedProduct
#else
#define MAYBE_NoRecordOmniboxChipTracked_ForTrackedProduct \
  NoRecordOmniboxChipTracked_ForTrackedProduct
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       MAYBE_NoRecordOmniboxChipTracked_ForTrackedProduct) {
  base::UserActionTester user_action_tester;
  mock_shopping_service_->SetIsSubscribedCallbackValue(true);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);

  EXPECT_EQ(user_action_tester.GetActionCount(
                "Commerce.PriceTracking.OmniboxChip.Tracked"),
            0);
  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kPriceTrackingChipElementId),
      PressButton(kPriceTrackingChipElementId));

  EXPECT_EQ(user_action_tester.GetActionCount(
                "Commerce.PriceTracking.OmniboxChip.Tracked"),
            0);
}

// TODO(crbug.com/41494779): Test is failing on Mac under ChromeRefresh2023
// flags. Evaluate, fix, and remove the skip.
#if BUILDFLAG(IS_MAC)
#define MAYBE_NoRecordOmniboxChipTracked_ForFUEFlow \
  DISABLED_NoRecordOmniboxChipTracked_ForFUEFlow
#else
#define MAYBE_NoRecordOmniboxChipTracked_ForFUEFlow \
  NoRecordOmniboxChipTracked_ForFUEFlow
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       MAYBE_NoRecordOmniboxChipTracked_ForFUEFlow) {
  base::UserActionTester user_action_tester;
  mock_shopping_service_->SetIsSubscribedCallbackValue(false);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Commerce.PriceTracking.OmniboxChip.Tracked"),
            0);
  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kPriceTrackingChipElementId),
      PressButton(kPriceTrackingChipElementId));
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Commerce.PriceTracking.OmniboxChip.Tracked"),
            0);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       IconViewAccessibleName) {
  mock_shopping_service_->SetIsSubscribedCallbackValue(true);
  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kPriceTrackingChipElementId),
      CheckViewProperty(kPriceTrackingChipElementId,
                        &PriceTrackingIconView::GetAccessibleName,
                        l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACKING_PRICE)),
      CheckViewProperty(
          kPriceTrackingChipElementId,
          &PriceTrackingIconView::GetTextForTooltipAndAccessibleName,
          l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACKING_PRICE)));
}

class PriceTrackingIconViewErrorHandelingTest
    : public PriceTrackingIconViewInteractiveTest {
 public:
  PriceTrackingIconViewErrorHandelingTest() {
    test_features_.InitWithFeaturesAndParameters(
        {{commerce::kShoppingList,
          {{commerce::kRevertIconOnFailureParam, "true"}}}},
        {commerce::kPriceInsights});
  }

 private:
  base::test::ScopedFeatureList test_features_;
};

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewErrorHandelingTest,
                       IconRevertedOnFailure) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);
  mock_shopping_service_->SetIsSubscribedCallbackValue(false);

  // Simulate subscription failure.
  mock_shopping_service_->SetSubscribeCallbackValue(false);

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kPriceTrackingChipElementId),
      CheckView(kPriceTrackingChipElementId,
                base::BindOnce([](PriceTrackingIconView* view) {
                  return view->GetIconLabelForTesting() ==
                         l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE);
                })),
      CheckView(kPriceTrackingChipElementId,
                base::BindOnce([](PriceTrackingIconView* view) {
                  return view->GetVectorIcon().name ==
                          omnibox::kPriceTrackingDisabledRefreshIcon.name;
                })),
      PressButton(kPriceTrackingChipElementId),

      // After the button press, nothing should have changes since the
      // subscription failed.
      CheckView(kPriceTrackingChipElementId,
                base::BindOnce([](PriceTrackingIconView* view) {
                  return view->GetIconLabelForTesting() ==
                         l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE);
                })),
      CheckView(kPriceTrackingChipElementId,
                base::BindOnce([](PriceTrackingIconView* view) {
                  return view->GetVectorIcon().name ==
                          omnibox::kPriceTrackingDisabledRefreshIcon.name;
                })),
      EnsureNotPresent(kPriceTrackingBubbleDialogId));
}

class PriceTrackingBubbleInteractiveTest
    : public PriceTrackingIconViewInteractiveTest {
 public:
  PriceTrackingBubbleInteractiveTest() = default;

  PriceTrackingBubbleInteractiveTest(
      const PriceTrackingBubbleInteractiveTest&) = delete;
  PriceTrackingBubbleInteractiveTest& operator=(
      const PriceTrackingBubbleInteractiveTest&) = delete;

  ~PriceTrackingBubbleInteractiveTest() override = default;

 protected:
  base::UserActionTester user_action_tester_;
};

// TODO(crbug.com/41494779): Test is failing on Mac under ChromeRefresh2023
// flags. Evaluate, fix, and remove the skip.
#if BUILDFLAG(IS_MAC)
#define MAYBE_RecordFirstRunBubbleShown DISABLED_RecordFirstRunBubbleShown
#else
#define MAYBE_RecordFirstRunBubbleShown RecordFirstRunBubbleShown
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(PriceTrackingBubbleInteractiveTest,
                       MAYBE_RecordFirstRunBubbleShown) {
  mock_shopping_service_->SetIsSubscribedCallbackValue(false);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.FirstRunBubbleShown"),
            0);

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kPriceTrackingChipElementId),
      PressButton(kPriceTrackingChipElementId),
      WaitForShow(kPriceTrackingBubbleDialogId));

  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.FirstRunBubbleShown"),
            1);
}

// TODO(crbug.com/41494779): Test is failing on Mac under ChromeRefresh2023
// flags. Evaluate, fix, and remove the skip.
#if BUILDFLAG(IS_MAC)
#define MAYBE_RecordConfirmationShown DISABLED_RecordConfirmationShown
#else
#define MAYBE_RecordConfirmationShown RecordConfirmationShown
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(PriceTrackingBubbleInteractiveTest,
                       MAYBE_RecordConfirmationShown) {
  mock_shopping_service_->SetIsSubscribedCallbackValue(false);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.ConfirmationShown"),
            0);

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kPriceTrackingChipElementId),
      PressButton(kPriceTrackingChipElementId));

  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.ConfirmationShown"),
            1);
}

// TODO(crbug.com/41494779): Test is failing on Mac under ChromeRefresh2023
// flags. Evaluate, fix, and remove the skip.
#if BUILDFLAG(IS_MAC)
#define MAYBE_RecordConfirmationUntracked DISABLED_RecordConfirmationUntracked
#else
#define MAYBE_RecordConfirmationUntracked RecordConfirmationUntracked
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(PriceTrackingBubbleInteractiveTest,
                       MAYBE_RecordConfirmationUntracked) {
  mock_shopping_service_->SetIsSubscribedCallbackValue(false);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.Confirmation.Untrack"),
            0);

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kPriceTrackingChipElementId),
      PressButton(kPriceTrackingChipElementId),
      WaitForShow(kPriceTrackingBubbleDialogId));

  static_cast<PriceTrackingBubbleDialogView*>(
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kPriceTrackingBubbleDialogId,
          browser()->window()->GetElementContext()))
      ->Cancel();

  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.Confirmation.Untrack"),
            1);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingBubbleInteractiveTest,
                       RecordEditedBookmarkFolderFromOmniboxBubble) {
  mock_shopping_service_->SetIsSubscribedCallbackValue(false);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.EditedBookmarkFolderFromOmniboxBubble"),
            0);

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kPriceTrackingChipElementId),
      PressButton(kPriceTrackingChipElementId),
      WaitForShow(kPriceTrackingBubbleDialogId));

  static_cast<PriceTrackingBubbleDialogView*>(
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kPriceTrackingBubbleDialogId,
          browser()->window()->GetElementContext()))
      ->GetBodyLabelForTesting()
      ->ClickFirstLinkForTesting();

  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.EditedBookmarkFolderFromOmniboxBubble"),
            1);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       TabDiscardDuringNavigationNoCrash) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
  constexpr char kEmptyDocumentURL[] = "/empty.html";
  mock_shopping_service_->SetIsSubscribedCallbackValue(false);
  const GURL shopping_url = embedded_test_server()->GetURL(kShoppingURL);

  RunTestSequence(
      InstrumentTab(kShoppingTab),

      // Make a second tab.
      AddInstrumentedTab(kSecondTab,
                         embedded_test_server()->GetURL(kEmptyDocumentURL)),
      SelectTab(kTabStripElementId, 0),

      // Navigate the first tab to a shopping URL and wait for the chip to show.
      NavigateWebContents(kShoppingTab, shopping_url),
      WaitForShow(kPriceTrackingChipElementId),

      // Start a navigation to an empty URL. Do not wait for the navigation to
      // finish.
      Do(base::BindLambdaForTesting([&]() {
        ui_test_utils::NavigateToURLWithDisposition(
            browser(), embedded_test_server()->GetURL(kEmptyDocumentURL),
            WindowOpenDisposition::CURRENT_TAB,
            ui_test_utils::BROWSER_TEST_NO_WAIT);
      })),

      // Immediately switch tabs and discard the navigating tab.
      SelectTab(kTabStripElementId, 1), Do(base::BindLambdaForTesting([&]() {
        // Note that because the active tab cannot be discarded, this line is
        // guaranteed to discard the first tab.
        g_browser_process->GetTabManager()->DiscardTab(
            mojom::LifecycleUnitDiscardReason::EXTERNAL);
      })));

  // There should not be a crash.
}
