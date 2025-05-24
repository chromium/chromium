// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/376283383): This file should be moved closer to the
// `LensOverlayEntryPointController` once the page actions migration is
// complete.

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/test/run_until.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/lens_overlay_homework_page_action_icon_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/events/test/test_event.h"
#include "ui/views/interaction/element_tracker_views.h"
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
                               views::View* starting_view) override {
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

  LensOverlayHomeworkPageActionIconView* lens_overlay_homework_icon_view() {
    views::View* const icon_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kLensOverlayHomeworkPageActionIconElementId,
            browser()->window()->GetElementContext());
    return icon_view
               ? views::AsViewClass<LensOverlayHomeworkPageActionIconView>(
                     icon_view)
               : nullptr;
  }

  LocationBarView* location_bar_view() {
    views::View* const location_bar_view =
        views::ElementTrackerViews::GetInstance()->GetUniqueView(
            kLocationBarElementId, browser()->window()->GetElementContext());
    return location_bar_view
               ? views::AsViewClass<LocationBarView>(location_bar_view)
               : nullptr;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class LensOverlayHomeworkPageActionIconViewTest
    : public LensOverlayHomeworkPageActionIconViewTestBase {
 public:
  LensOverlayHomeworkPageActionIconViewTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {base::test::FeatureRefAndParams(lens::features::kLensOverlay, {}),
         base::test::FeatureRefAndParams(
             lens::features::kLensOverlayEduActionChip,
             {{"url-allow-filters", "[\"*\"]"},
              {"url-path-match-allow-filters", "[\"select\"]"}})},
        {});
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayHomeworkPageActionIconViewTest,
                       ShowsOnMatchingPage) {
  // Navigate to a matching page.
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));

  LensOverlayHomeworkPageActionIconView* icon_view =
      lens_overlay_homework_icon_view();
  views::FocusManager* focus_manager = icon_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_TRUE(icon_view->GetVisible());

  // Focus in the location bar should hide the icon.
  location_bar_view()->FocusLocation(false);
  ViewVisibilityWaiter(icon_view, false).Wait();

  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_FALSE(icon_view->GetVisible());
}

IN_PROC_BROWSER_TEST_F(LensOverlayHomeworkPageActionIconViewTest,
                       HidesOnNonMatchingPage) {
  // Navigate to a non-matching page.
  const GURL url = embedded_test_server()->GetURL(kDocument2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));

  LensOverlayHomeworkPageActionIconView* icon_view =
      lens_overlay_homework_icon_view();
  views::FocusManager* focus_manager = icon_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_FALSE(icon_view->GetVisible());

  // Focus in the location bar should not show the icon.
  location_bar_view()->FocusLocation(false);
  ViewVisibilityWaiter(icon_view, false).Wait();

  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_FALSE(icon_view->GetVisible());
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_OpensNewTabWhenEnteredThroughKeyboard \
  DISABLED_OpensNewTabWhenEnteredThroughKeyboard
#else
#define MAYBE_OpensNewTabWhenEnteredThroughKeyboard \
  OpensNewTabWhenEnteredThroughKeyboard
#endif
// Flaky failures on Windows; see https://crbug.com/419308044.
IN_PROC_BROWSER_TEST_F(LensOverlayHomeworkPageActionIconViewTest,
                       MAYBE_OpensNewTabWhenEnteredThroughKeyboard) {
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

  LensOverlayHomeworkPageActionIconView* icon_view =
      lens_overlay_homework_icon_view();
  views::FocusManager* focus_manager = icon_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_TRUE(icon_view->GetVisible());

  // Executing the lens overlay icon view with keyboard source should open a new
  // tab.
  ui_test_utils::TabAddedWaiter tab_add(browser());
  lens_overlay_homework_icon_view()->ExecuteWithKeyboardSourceForTesting();
  auto* new_tab_contents = tab_add.Wait();

  EXPECT_TRUE(new_tab_contents);
  content::WaitForLoadStop(new_tab_contents);
  EXPECT_THAT(new_tab_contents->GetLastCommittedURL().query(),
              MatchesRegex("ep=crmntob&re=df&s=4&st=\\d+&lm=.+"));
}

IN_PROC_BROWSER_TEST_F(LensOverlayHomeworkPageActionIconViewTest,
                       DoesNotShowWhenSettingDisabled) {
  // Disable the setting.
  browser()->profile()->GetPrefs()->SetBoolean(omnibox::kShowGoogleLensShortcut,
                                               false);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  // Navigate to a matching page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));

  LensOverlayHomeworkPageActionIconView* icon_view =
      lens_overlay_homework_icon_view();
  views::FocusManager* focus_manager = icon_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_FALSE(icon_view->GetVisible());

  // Focus in the location bar should not show the icon.
  location_bar_view()->FocusLocation(false);
  ViewVisibilityWaiter(icon_view, false).Wait();

  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_FALSE(icon_view->GetVisible());
}

IN_PROC_BROWSER_TEST_F(LensOverlayHomeworkPageActionIconViewTest,
                       RespectsShowShortcutPreference) {
  // Ensure the shortcut pref starts enabled.
  browser()->profile()->GetPrefs()->SetBoolean(omnibox::kShowGoogleLensShortcut,
                                               true);

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  // Navigate to a matching page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));

  views::View* icon_view = lens_overlay_homework_icon_view();
  views::FocusManager* focus_manager = icon_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_TRUE(icon_view->GetVisible());

  // Disable the preference, the entrypoint should immediately disappear.
  browser()->profile()->GetPrefs()->SetBoolean(omnibox::kShowGoogleLensShortcut,
                                               false);
  EXPECT_FALSE(icon_view->GetVisible());

  // Re-enable the preference, the entrypoint should immediately become
  // visible.
  browser()->profile()->GetPrefs()->SetBoolean(omnibox::kShowGoogleLensShortcut,
                                               true);
  EXPECT_TRUE(icon_view->GetVisible());
}

}  // namespace
