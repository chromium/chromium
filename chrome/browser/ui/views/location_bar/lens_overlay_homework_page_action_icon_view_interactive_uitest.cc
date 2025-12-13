// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/376283383): This file should be moved closer to the
// `LensOverlayEntryPointController` once the page actions migration is
// complete.

#include <memory>
#include <optional>
#include <vector>

#include "base/test/run_until.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/lens/lens_keyed_service.h"
#include "chrome/browser/ui/lens/lens_keyed_service_factory.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/location_bar/lens_overlay_homework_page_action_controller.h"
#include "chrome/browser/ui/views/location_bar/lens_overlay_homework_page_action_icon_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lens/lens_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/test_event.h"
#include "ui/views/test/widget_test.h"
#include "url/url_constants.h"

using ::testing::MatchesRegex;

namespace {

constexpr char kDocumentWithNamedElement[] = "/select.html";
constexpr char kDocument2[] = "/title1.html";

class ViewVisibilityWaiter : public views::ViewObserver {
 public:
  explicit ViewVisibilityWaiter(views::View* observed_view,
                                bool expected_visible)
      : view_(observed_view), expected_visible_(expected_visible) {
    observation_.Observe(view_.get());
  }
  ViewVisibilityWaiter(const ViewVisibilityWaiter&) = delete;
  ViewVisibilityWaiter& operator=(const ViewVisibilityWaiter&) = delete;

  ~ViewVisibilityWaiter() override = default;

  // Wait for changes to occur, or return immediately if view already has
  // expected visibility.
  void Wait() {
    if (expected_visible_ != view_->GetVisible()) {
      run_loop_.Run();
    }
  }

 private:
  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view,
                               bool visible) override {
    if (expected_visible_ == observed_view->GetVisible()) {
      run_loop_.Quit();
    }
  }

  raw_ptr<views::View> view_;
  const bool expected_visible_;
  base::RunLoop run_loop_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

class LensOverlayHomeworkPageActionIconViewTestBase
    : public InProcessBrowserTest {
 public:
  LensOverlayHomeworkPageActionIconViewTestBase() = default;
  LensOverlayHomeworkPageActionIconViewTestBase(
      const LensOverlayHomeworkPageActionIconViewTestBase&) = delete;
  LensOverlayHomeworkPageActionIconViewTestBase& operator=(
      const LensOverlayHomeworkPageActionIconViewTestBase&) = delete;
  ~LensOverlayHomeworkPageActionIconViewTestBase() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  IconLabelBubbleView* lens_overlay_homework_icon_view() {
    return BrowserElementsViews::From(browser())
        ->GetViewAs<IconLabelBubbleView>(
            kLensOverlayHomeworkPageActionIconElementId);
  }

  void PressOnView(bool is_migrated) {
    if (IsPageActionMigrated(PageActionIconType::kLensOverlayHomework)) {
      LensOverlayHomeworkPageActionController::From(
          *browser()->tab_strip_model()->GetActiveTab())
          ->HandlePageActionEvent(/*is_from_keyboard=*/true);
    } else {
      BrowserElementsViews::From(browser())
          ->GetViewAs<LensOverlayHomeworkPageActionIconView>(
              kLensOverlayHomeworkPageActionIconElementId)
          ->ExecuteWithKeyboardSourceForTesting();
    }
  }

  LocationBarView* location_bar_view() {
    return BrowserElementsViews::From(browser())->GetViewAs<LocationBarView>(
        kLocationBarElementId);
  }

  // Sets the number of times the edu action chip has been shown.
  void SetLensOverlayEduActionChipShownCount(Profile* profile, int count) {
    LensKeyedService* service = LensKeyedServiceFactory::GetForProfile(
        profile, /*create_if_necessary=*/true);
    service->SetActionChipShownCount(count);
  }

  // Returns the number of times the edu action chip has been shown.
  int GetLensOverlayEduActionChipShownCount(Profile* profile) {
    LensKeyedService* service = LensKeyedServiceFactory::GetForProfile(
        profile, /*create_if_necessary=*/true);
    return service->GetActionChipShownCount();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class LensOverlayHomeworkPageActionIconViewTest
    : public LensOverlayHomeworkPageActionIconViewTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  LensOverlayHomeworkPageActionIconViewTest() {
    bool is_migrated = GetParam();
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        base::test::FeatureRefAndParams(lens::features::kLensOverlay, {}),
        base::test::FeatureRefAndParams(
            lens::features::kLensOverlayOmniboxEntryPoint, {}),
        base::test::FeatureRefAndParams(
            lens::features::kLensOverlayEduActionChip,
            {{"url-allow-filters", "[\"*\"]"},
             {"url-path-match-allow-filters", "[\"select\"]"},
             {"max-shown-count", "3"}})};
    enabled_features.push_back(base::test::FeatureRefAndParams(
        features::kPageActionsMigration,
        {{features::kPageActionsMigrationLensOverlayHomework.name,
          is_migrated ? "true" : "false"}}));

    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features, {lens::features::kLensOverlayKeyboardSelection,
                           lens::features::kLensOverlayOptimizationFilter});
  }
};

IN_PROC_BROWSER_TEST_P(LensOverlayHomeworkPageActionIconViewTest,
                       ShowsOnMatchingPage) {
  SetLensOverlayEduActionChipShownCount(browser()->profile(), 0);
  // Navigate to a matching page.
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));

  IconLabelBubbleView* icon_view = lens_overlay_homework_icon_view();
  views::FocusManager* focus_manager = icon_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_TRUE(icon_view->GetVisible());

  // Focus in the location bar should hide the icon.
  location_bar_view()->FocusLocation(false);
  ViewVisibilityWaiter(icon_view, false).Wait();

  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_FALSE(icon_view->GetVisible());
  EXPECT_EQ(GetLensOverlayEduActionChipShownCount(browser()->profile()), 1);
}

IN_PROC_BROWSER_TEST_P(LensOverlayHomeworkPageActionIconViewTest,
                       HidesOnNonMatchingPage) {
  SetLensOverlayEduActionChipShownCount(browser()->profile(), 0);
  // Navigate to a non-matching page.
  const GURL url = embedded_test_server()->GetURL(kDocument2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));

  IconLabelBubbleView* icon_view = lens_overlay_homework_icon_view();
  views::FocusManager* focus_manager = icon_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_FALSE(icon_view->GetVisible());

  // Focus in the location bar should not show the icon.
  location_bar_view()->FocusLocation(false);
  ViewVisibilityWaiter(icon_view, false).Wait();

  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_FALSE(icon_view->GetVisible());
  EXPECT_EQ(GetLensOverlayEduActionChipShownCount(browser()->profile()), 0);
}

IN_PROC_BROWSER_TEST_P(LensOverlayHomeworkPageActionIconViewTest,
                       HidesAfterMaxShownCountReached) {
  SetLensOverlayEduActionChipShownCount(browser()->profile(), 4);
  // Navigate to a matching page.
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));

  IconLabelBubbleView* icon_view = lens_overlay_homework_icon_view();
  views::FocusManager* focus_manager = icon_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_FALSE(icon_view->GetVisible());

  // Focus in the location bar should not show the icon.
  location_bar_view()->FocusLocation(false);
  ViewVisibilityWaiter(icon_view, false).Wait();

  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_FALSE(icon_view->GetVisible());
  EXPECT_EQ(GetLensOverlayEduActionChipShownCount(browser()->profile()), 4);
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_OpensNewTabWhenEnteredThroughKeyboard \
  DISABLED_OpensNewTabWhenEnteredThroughKeyboard
#else
#define MAYBE_OpensNewTabWhenEnteredThroughKeyboard \
  OpensNewTabWhenEnteredThroughKeyboard
#endif
// Flaky failures on Windows; see https://crbug.com/419308044.
IN_PROC_BROWSER_TEST_P(LensOverlayHomeworkPageActionIconViewTest,
                       MAYBE_OpensNewTabWhenEnteredThroughKeyboard) {
  SetLensOverlayEduActionChipShownCount(browser()->profile(), 0);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  // Navigate to a matching page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
  // We need to wait for paint in order to take a screenshot of the page.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()
        ->tab_strip_model()
        ->GetActiveTab()
        ->GetContents()
        ->CompletedFirstVisuallyNonEmptyPaint();
  }));

  IconLabelBubbleView* icon_view = lens_overlay_homework_icon_view();
  views::FocusManager* focus_manager = icon_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_TRUE(icon_view->GetVisible());

  // Executing the lens overlay icon view with keyboard source should open a new
  // tab.
  ui_test_utils::TabAddedWaiter tab_add(browser());
  PressOnView(lens_overlay_homework_icon_view());
  auto* new_tab_contents = tab_add.Wait();

  EXPECT_TRUE(new_tab_contents);
  content::WaitForLoadStop(new_tab_contents);
  EXPECT_THAT(new_tab_contents->GetLastCommittedURL().GetQuery(),
              MatchesRegex("ep=crmntob&re=df&s=4&st=\\d+&lm=.+"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         LensOverlayHomeworkPageActionIconViewTest,
                         ::testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Migrated" : "Original";
                         });

class LensOverlayHomeworkPageActionIconViewTest_OptimizationFilter
    : public LensOverlayHomeworkPageActionIconViewTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  LensOverlayHomeworkPageActionIconViewTest_OptimizationFilter() {
    bool is_migrated = GetParam();
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        base::test::FeatureRefAndParams(lens::features::kLensOverlay, {}),
        base::test::FeatureRefAndParams(
            lens::features::kLensOverlayOmniboxEntryPoint, {}),
        base::test::FeatureRefAndParams(
            lens::features::kLensOverlayOptimizationFilter, {}),
        base::test::FeatureRefAndParams(
            lens::features::kLensOverlayEduActionChip,
            {{"max-shown-count", "3"}})};
    if (is_migrated) {
      enabled_features.push_back(base::test::FeatureRefAndParams(
          features::kPageActionsMigration,
          {{features::kPageActionsMigrationLensOverlayHomework.name, "true"}}));
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features, {lens::features::kLensOverlayKeyboardSelection});
  }

  void SetupOptimizationFilter() {
    auto* optimization_guide_decider =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    // Simulate the URL being allowed by both the allowlist and the blocklist.
    optimization_guide_decider->AddHintWithMultipleOptimizationsForTesting(
        GURL(embedded_test_server()->GetURL(kDocumentWithNamedElement)),
        {optimization_guide::proto::LENS_OVERLAY_EDU_ACTION_CHIP_ALLOWLIST,
         optimization_guide::proto::LENS_OVERLAY_EDU_ACTION_CHIP_BLOCKLIST});
  }
};

IN_PROC_BROWSER_TEST_P(
    LensOverlayHomeworkPageActionIconViewTest_OptimizationFilter,
    ShowsOnMatchingPage) {
  SetupOptimizationFilter();
  SetLensOverlayEduActionChipShownCount(browser()->profile(), 0);
  // Navigate to a matching page.
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));

  IconLabelBubbleView* icon_view = lens_overlay_homework_icon_view();
  views::FocusManager* focus_manager = icon_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_TRUE(icon_view->GetVisible());

  // Focus in the location bar should hide the icon.
  location_bar_view()->FocusLocation(false);
  ViewVisibilityWaiter(icon_view, false).Wait();

  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_FALSE(icon_view->GetVisible());
  EXPECT_EQ(GetLensOverlayEduActionChipShownCount(browser()->profile()), 1);
}

IN_PROC_BROWSER_TEST_P(
    LensOverlayHomeworkPageActionIconViewTest_OptimizationFilter,
    HidesOnNonMatchingPage) {
  SetupOptimizationFilter();
  SetLensOverlayEduActionChipShownCount(browser()->profile(), 0);
  // Navigate to a non-matching page.
  const GURL url = embedded_test_server()->GetURL(kDocument2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));

  IconLabelBubbleView* icon_view = lens_overlay_homework_icon_view();
  views::FocusManager* focus_manager = icon_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_FALSE(icon_view->GetVisible());

  // Focus in the location bar should not show the icon.
  location_bar_view()->FocusLocation(false);
  ViewVisibilityWaiter(icon_view, false).Wait();

  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_FALSE(icon_view->GetVisible());
  EXPECT_EQ(GetLensOverlayEduActionChipShownCount(browser()->profile()), 0);
}

IN_PROC_BROWSER_TEST_P(
    LensOverlayHomeworkPageActionIconViewTest_OptimizationFilter,
    HidesAfterMaxShownCountReached) {
  SetupOptimizationFilter();
  SetLensOverlayEduActionChipShownCount(browser()->profile(), 4);
  // Navigate to a matching page.
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));

  IconLabelBubbleView* icon_view = lens_overlay_homework_icon_view();
  views::FocusManager* focus_manager = icon_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_FALSE(icon_view->GetVisible());

  // Focus in the location bar should not show the icon.
  location_bar_view()->FocusLocation(false);
  ViewVisibilityWaiter(icon_view, false).Wait();

  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_FALSE(icon_view->GetVisible());
  EXPECT_EQ(GetLensOverlayEduActionChipShownCount(browser()->profile()), 4);
}

// fix
#if BUILDFLAG(IS_WIN)
#define MAYBE_OpensNewTabWhenEnteredThroughKeyboard \
  DISABLED_OpensNewTabWhenEnteredThroughKeyboard
#else
#define MAYBE_OpensNewTabWhenEnteredThroughKeyboard \
  OpensNewTabWhenEnteredThroughKeyboard
#endif
// Flaky failures on Windows; see https://crbug.com/419308044.
IN_PROC_BROWSER_TEST_P(
    LensOverlayHomeworkPageActionIconViewTest_OptimizationFilter,
    MAYBE_OpensNewTabWhenEnteredThroughKeyboard) {
  SetupOptimizationFilter();
  SetLensOverlayEduActionChipShownCount(browser()->profile(), 0);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  // Navigate to a matching page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
  // We need to wait for paint in order to take a screenshot of the page.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()
        ->tab_strip_model()
        ->GetActiveTab()
        ->GetContents()
        ->CompletedFirstVisuallyNonEmptyPaint();
  }));

  IconLabelBubbleView* icon_view = lens_overlay_homework_icon_view();
  views::FocusManager* focus_manager = icon_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_TRUE(icon_view->GetVisible());

  // Executing the lens overlay icon view with keyboard source should open a new
  // tab.
  ui_test_utils::TabAddedWaiter tab_add(browser());
  PressOnView(lens_overlay_homework_icon_view());
  auto* new_tab_contents = tab_add.Wait();

  EXPECT_TRUE(new_tab_contents);
  content::WaitForLoadStop(new_tab_contents);
  EXPECT_THAT(new_tab_contents->GetLastCommittedURL().GetQuery(),
              MatchesRegex("ep=crmntob&re=df&s=4&st=\\d+&lm=.+"));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    LensOverlayHomeworkPageActionIconViewTest_OptimizationFilter,
    ::testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "Migrated" : "Original";
    });

}  // namespace
