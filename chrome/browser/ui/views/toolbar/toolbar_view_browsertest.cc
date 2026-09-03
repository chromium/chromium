// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

#include "base/test/run_until.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/toolbar_controller_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/home_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view.h"

using ToolbarViewUnitTest = InProcessBrowserTest;

class ToolbarViewResponsiveTest : public InProcessBrowserTest {
 public:
  ToolbarViewResponsiveTest() {
    ToolbarControllerUtil::SetPreventOverflowForTesting(false);
  }
  ~ToolbarViewResponsiveTest() override {
    ToolbarControllerUtil::SetPreventOverflowForTesting(true);
  }
};

namespace {

// This macro attaches the location that the helper function was called to any
// test failures. This helps disambiguate where the test failed when if fails
// in a test helper.
#define WITH_SCOPE(function_call) \
  do {                            \
    SCOPED_TRACE(#function_call); \
    function_call;                \
  } while (false)

void WaitForTrackedElementVisible(BrowserWindowInterface* browser,
                                  ui::ElementIdentifier id) {
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return BrowserElements::From(browser)->GetElement(id) != nullptr;
  }));
}

void WaitForTrackedElementNotVisible(BrowserWindowInterface* browser,
                                     ui::ElementIdentifier id) {
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return BrowserElements::From(browser)->GetElement(id) == nullptr;
  }));
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest, ForwardButtonVisibility) {
  // Forward button should be visible by default.
  WITH_SCOPE(
      WaitForTrackedElementVisible(browser(), kToolbarForwardButtonElementId));

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  browser_view->GetProfile()->GetPrefs()->SetBoolean(prefs::kShowForwardButton,
                                                     false);
  // Now it should disappear
  WITH_SCOPE(WaitForTrackedElementNotVisible(browser(),
                                             kToolbarForwardButtonElementId));
}

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest, AccessibleProperties) {
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  ui::AXNodeData data;

  toolbar->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kToolbar);
}

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest,
                       BubbleAnchorUsesLocationBarWhenDrawn) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(browser_view);

  views::BubbleAnchor anchor =
      browser_view->toolbar_button_provider()->GetBubbleAnchor(std::nullopt);
  views::View* anchor_view = anchor.GetIfView();
  EXPECT_EQ(anchor_view, browser_view->toolbar()->location_bar_view());
}

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest,
                       BubbleAnchorUsesTopContainerWhenLocationBarNotDrawn) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(browser_view);

  ToolbarView* toolbar = browser_view->toolbar();
  ASSERT_TRUE(toolbar);
  ASSERT_TRUE(toolbar->location_bar_view());
  // Simulate app windows (no visible location bar) by hiding the location bar.
  toolbar->location_bar_view()->SetVisible(false);

  views::BubbleAnchor anchor =
      browser_view->toolbar_button_provider()->GetBubbleAnchor(std::nullopt);
  views::View* anchor_view = anchor.GetIfView();
  // The fallback uses top_container when the location bar is not drawn.
  EXPECT_EQ(anchor_view,
            static_cast<views::View*>(browser_view->top_container()));
}

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest,
                       IsPositionInWindowCaption_EmptyArea) {
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  ASSERT_TRUE(toolbar);

  // A point in the toolbar's empty area (beyond all children on the right)
  // should be treated as caption.
  gfx::Point empty_area(toolbar->width() - 1,
                        toolbar->height() / 2);
  EXPECT_TRUE(toolbar->IsPositionInWindowCaption(empty_area));

  // A point in the upper portion of empty area should also be caption.
  gfx::Point upper_empty(toolbar->width() - 1, 1);
  EXPECT_TRUE(toolbar->IsPositionInWindowCaption(upper_empty));

  // A point in the lower portion of empty area should also be caption
  // (no centerline restriction).
  gfx::Point lower_empty(toolbar->width() - 1,
                          toolbar->height() - 1);
  EXPECT_TRUE(toolbar->IsPositionInWindowCaption(lower_empty));
}

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest,
                       IsPositionInWindowCaption_OnButton) {
  WITH_SCOPE(
      WaitForTrackedElementVisible(browser(), kToolbarForwardButtonElementId));

  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  ASSERT_TRUE(toolbar);

  // The forward button center should NOT be caption (it's an interactive
  // control).
  ui::TrackedElement* forward = BrowserElements::From(browser())->GetElement(
      kToolbarForwardButtonElementId);
  ASSERT_TRUE(forward);
  gfx::Point forward_center = forward->GetScreenBounds().CenterPoint();
  views::View::ConvertPointFromScreen(toolbar, &forward_center);
  EXPECT_FALSE(toolbar->IsPositionInWindowCaption(forward_center));
}

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest,
                       IsPositionInWindowCaption_BelowButton) {
  WITH_SCOPE(
      WaitForTrackedElementVisible(browser(), kToolbarForwardButtonElementId));

  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  ASSERT_TRUE(toolbar);

  // A point directly below a button (same x, but y below the button's bottom
  // edge) should be caption since it's not inside any child's bounds.
  ui::TrackedElement* forward = BrowserElements::From(browser())->GetElement(
      kToolbarForwardButtonElementId);
  ASSERT_TRUE(forward);
  gfx::Rect forward_bounds =
      views::View::ConvertRectFromScreen(toolbar, forward->GetScreenBounds());
  gfx::Point forward_center = forward_bounds.CenterPoint();
  gfx::Point below_button(forward_center.x(), toolbar->height() - 1);

  // This point is below the button — should be treated as caption.
  if (!forward_bounds.Contains(below_button)) {
    EXPECT_TRUE(toolbar->IsPositionInWindowCaption(below_button));
  }
}

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest,
                       IsPositionInWindowCaption_OnSeparator) {
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  ASSERT_TRUE(toolbar);

  // Find a visible separator or divider in the toolbar and verify it's treated
  // as caption.
  for (views::View* child : toolbar->children()) {
    if (!child->GetVisible()) {
      continue;
    }
    if (views::IsViewClass<views::Separator>(child) &&
        child->bounds().width() > 0 && child->bounds().height() > 0) {
      gfx::Point separator_center = child->bounds().CenterPoint();
      EXPECT_TRUE(toolbar->IsPositionInWindowCaption(separator_center));
      break;
    }
  }
}

// Regression test for crbug.com/505902447: Verify that popup windows still
// have pinned toolbar actions, as they're used for pinning the download button.
IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest, PinnedToolbarActionsInPopup) {
  BrowserWindowInterface* popup =
      CreateBrowserForPopup(browser()->GetProfile());
  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(popup)
                  ->toolbar_button_provider()
                  ->GetPinnedToolbarActions());
}

IN_PROC_BROWSER_TEST_F(ToolbarViewResponsiveTest,
                       OmniboxTwoStageFlexResizingAndButtonLifecycle) {
  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kShowForwardButton,
                                                  true);
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  ASSERT_TRUE(toolbar);
  views::View* location_bar = toolbar->location_bar_view();
  views::View* forward = toolbar->forward_button();
  views::View* home = toolbar->home_button();
  ASSERT_TRUE(location_bar);
  ASSERT_TRUE(forward);
  ASSERT_TRUE(home);
  const int preferred_width = location_bar->GetPreferredSize().width();
  const int min_width = location_bar->GetMinimumSize().width();
  const int toolbar_height = toolbar->GetPreferredSize().height();
  EXPECT_GT(preferred_width, min_width);
  EXPECT_GT(toolbar_height, 0);

  const int super_wide_width = toolbar->GetPreferredSize().width() + 400;
  const int all_fit_width = toolbar->GetPreferredSize().width();
  const int omnibox_resizing_width = toolbar->GetMinimumSize().width() + 30;
  const int min_width_bound = toolbar->GetMinimumSize().width();

  // In a super wide toolbar, the omnibox absorbs all excess space beyond its
  // preferred width, and all responsive buttons remain visible.
  toolbar->SetSize(gfx::Size(super_wide_width, toolbar_height));
  toolbar->DeprecatedLayoutImmediately();
  EXPECT_TRUE(forward->GetVisible());
  EXPECT_FALSE(forward->bounds().IsEmpty());
  EXPECT_TRUE(home->GetVisible());
  EXPECT_FALSE(home->bounds().IsEmpty());
  EXPECT_GT(location_bar->bounds().width(), preferred_width);

  // When resized down to a moderate width where all elements fit, the omnibox
  // shrinks down towards its preferred width while buttons remain visible.
  toolbar->SetSize(gfx::Size(all_fit_width, toolbar_height));
  toolbar->DeprecatedLayoutImmediately();
  EXPECT_TRUE(forward->GetVisible());
  EXPECT_FALSE(forward->bounds().IsEmpty());
  EXPECT_TRUE(home->GetVisible());
  EXPECT_FALSE(home->bounds().IsEmpty());
  EXPECT_GE(location_bar->bounds().width(), preferred_width);

  // Under narrow widths where responsive buttons have dropped out, buttons are
  // hidden and the omnibox shrinks down from its preferred width towards its
  // minimum width.
  toolbar->SetSize(gfx::Size(omnibox_resizing_width, toolbar_height));
  toolbar->DeprecatedLayoutImmediately();
  EXPECT_FALSE(forward->GetVisible());
  EXPECT_FALSE(home->GetVisible());
  EXPECT_LT(location_bar->bounds().width(), preferred_width);
  EXPECT_GT(location_bar->bounds().width(), min_width);

  // At absolute minimum width, all responsive buttons remain hidden and the
  // omnibox reaches its minimum width.
  toolbar->SetSize(gfx::Size(min_width_bound, toolbar_height));
  toolbar->DeprecatedLayoutImmediately();
  EXPECT_FALSE(forward->GetVisible());
  EXPECT_FALSE(home->GetVisible());
  EXPECT_EQ(location_bar->bounds().width(), min_width);

  // When expanding to moderate width, responsive buttons reappear with
  // non-empty bounds and the omnibox recovers its preferred width.
  toolbar->SetSize(gfx::Size(all_fit_width, toolbar_height));
  toolbar->DeprecatedLayoutImmediately();
  EXPECT_TRUE(forward->GetVisible());
  EXPECT_FALSE(forward->bounds().IsEmpty());
  EXPECT_TRUE(home->GetVisible());
  EXPECT_FALSE(home->bounds().IsEmpty());
  EXPECT_GE(location_bar->bounds().width(), preferred_width);

  // Expanding back to a super wide width allows the omnibox to expand into
  // excess space again.
  toolbar->SetSize(gfx::Size(super_wide_width, toolbar_height));
  toolbar->DeprecatedLayoutImmediately();
  EXPECT_TRUE(forward->GetVisible());
  EXPECT_FALSE(forward->bounds().IsEmpty());
  EXPECT_TRUE(home->GetVisible());
  EXPECT_FALSE(home->bounds().IsEmpty());
  EXPECT_GT(location_bar->bounds().width(), preferred_width);
}

IN_PROC_BROWSER_TEST_F(ToolbarViewResponsiveTest,
                       ButtonsDoNotCollapseCrossAxisHeight) {
  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kShowForwardButton,
                                                  true);
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  ASSERT_TRUE(toolbar);
  views::View* forward = toolbar->forward_button();
  views::View* home = toolbar->home_button();
  views::View* avatar = toolbar->avatar_toolbar_button();
  ASSERT_TRUE(forward);
  ASSERT_TRUE(home);

  const int all_fit_width = toolbar->GetPreferredSize().width();
  // Constrain toolbar height so that the available cross-axis height is smaller
  // than the buttons' preferred height (simulating custom frame mode with
  // vertical tabs).
  const int constrained_height = toolbar->GetPreferredSize().height() - 4;

  toolbar->SetSize(gfx::Size(all_fit_width, constrained_height));
  toolbar->DeprecatedLayoutImmediately();

  EXPECT_TRUE(forward->GetVisible());
  EXPECT_GT(forward->bounds().height(), 0);

  EXPECT_TRUE(home->GetVisible());
  EXPECT_GT(home->bounds().height(), 0);

  if (avatar && avatar->GetVisible()) {
    EXPECT_GT(avatar->bounds().height(), 0);
  }
}
