// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/animations/tab_strip_animations.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/compositor/layer.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// Returns true if `upper` is stacked above `lower` within their shared
// `ui::Layer` hierarchy.
bool IsLayerStackedAbove(const ui::Layer* upper, const ui::Layer* lower) {
  if (!upper || !lower || upper == lower) {
    return false;
  }

  std::vector<const ui::Layer*> upper_ancestors;
  for (const ui::Layer* cur = upper; cur; cur = cur->parent()) {
    upper_ancestors.push_back(cur);
  }

  std::vector<const ui::Layer*> lower_ancestors;
  for (const ui::Layer* cur = lower; cur; cur = cur->parent()) {
    lower_ancestors.push_back(cur);
  }

  // Must belong to the same root layer tree to compare stacking order.
  if (upper_ancestors.back() != lower_ancestors.back()) {
    return false;
  }

  const ui::Layer* common_parent = nullptr;
  const ui::Layer* upper_branch = nullptr;
  const ui::Layer* lower_branch = nullptr;

  auto u_it = upper_ancestors.rbegin();
  auto l_it = lower_ancestors.rbegin();
  while (u_it != upper_ancestors.rend() && l_it != lower_ancestors.rend() &&
         *u_it == *l_it) {
    common_parent = *u_it;
    ++u_it;
    ++l_it;
  }

  // If one layer is an ancestor of the other, children render above parents.
  if (u_it == upper_ancestors.rend()) {
    return false;
  }
  if (l_it == lower_ancestors.rend()) {
    return true;
  }

  upper_branch = *u_it;
  lower_branch = *l_it;

  const auto& siblings = common_parent->children();
  auto upper_pos = std::ranges::find(siblings, upper_branch);
  auto lower_pos = std::ranges::find(siblings, lower_branch);
  if (upper_pos == siblings.end() || lower_pos == siblings.end()) {
    return false;
  }
  return upper_pos > lower_pos;
}

// Collects all `ui::Layer`s owned by `root_view` and its descendant views,
// including layers attached to hosted native views.
void CollectSubtreeLayers(views::View* root_view,
                          std::vector<const ui::Layer*>& out_layers) {
  if (!root_view || !root_view->GetVisible()) {
    return;
  }
  for (const ui::Layer* layer : root_view->GetLayersInOrder()) {
    if (layer && layer->visible()) {
      out_layers.push_back(layer);
    }
  }
  for (views::View* child : root_view->children()) {
    CollectSubtreeLayers(child, out_layers);
  }
}

}  // namespace

class VerticalTabStripRegionViewInteractiveUiTest
    : public VerticalTabsInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  VerticalTabStripRegionViewInteractiveUiTest() = default;

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
};

// Deactivate() function is flaky on Mac. Test is flaky on Linux Wayland because
// activation events are asynchronous.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_VerifyVerticalTabStripBackground \
  DISABLED_VerifyVerticalTabStripBackground
#else
#define MAYBE_VerifyVerticalTabStripBackground VerifyVerticalTabStripBackground
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewInteractiveUiTest,
                       MAYBE_VerifyVerticalTabStripBackground) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* region_view =
      browser_view->vertical_tab_strip_region_view_for_testing();

  browser_view->Activate();
  EXPECT_TRUE(browser_view->GetWidget()->ShouldPaintAsActive());

  const views::Background* background_while_active =
      region_view->GetBackground();

  browser_view->Deactivate();
  EXPECT_FALSE(browser_view->GetWidget()->ShouldPaintAsActive());

  const views::Background* background_while_inactive =
      region_view->GetBackground();

  // Background should not be recreated, pointers should be the same.
  EXPECT_EQ(background_while_active, background_while_inactive);
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewInteractiveUiTest,
                       LogTimeToSwitchMetric) {
  RunTestSequence(
      PressButton(kToolbarAppMenuButtonElementId),
      WithView(kTabStripElementId,
               [](TabStripView* view) {
                 // Simulate the OnMouseEntered event which doesn't
                 // happen consistently in Kombucha.
                 ui::MouseEvent event(ui::EventType::kMouseEntered,
                                      gfx::Point(), gfx::Point(),
                                      base::TimeTicks(), 0, 0);
                 view->OnMouseEntered(event);
               }),
      SelectTab(kTabStripElementId, 0),
      CheckResult(
          [this]() {
            return histogram_tester()
                .GetTotalCountsForPrefix("TabStrip.Vertical.TimeToSwitch")
                .size();
          },
          1));
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewInteractiveUiTest,
                       OmniboxPopupSuppressesExpandOnHover) {
  auto* const controller =
      tabs::VerticalTabStripStateController::From(browser());

  controller->RequestCollapse(true);
  controller->SetExpandOnHoverEnabled(true);

  RunScheduledLayouts();

  ui::Accelerator focus_location_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_FOCUS_LOCATION, &focus_location_accelerator));

  RunTestSequence(
      WaitForShow(kVerticalTabStripTopContainerElementId),
      EnsurePresent(kVerticalTabStripTopContainerElementId),

      MoveMouseTo(kVerticalTabStripTopContainerElementId),

      SendAccelerator(kBrowserViewElementId, focus_location_accelerator),

      // Verify that the tab strip remains unexpanded despite the mouse hover.
      CheckViewProperty(kTabStripRegionElementId,
                        &VerticalTabStripRegionView::is_expanded_on_hover,
                        false));
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewInteractiveUiTest,
                       LogAnimationPerMetrics) {
  bool has_fps = false;
  RunTestSequence(
      PressButton(kVerticalTabStripCollapseButtonElementId),
      WaitForEvent(kTabStripRegionElementId,
                   VerticalTabStripRegionView::kAnimationCompletedEvent),
      Do([this, &has_fps]() {
        histogram_tester().ExpectTotalCount(
            "TabStrip.Vertical.Collapse.TimeOfLongestAnimationStep", 1);
        has_fps = histogram_tester().GetTotalCountForPrefix(
                      "TabStrip.Vertical.Collapse.AnimationFPS") > 0;
      }),
      CheckResult(
          [this]() {
            return histogram_tester().GetTotalSum(
                "TabStrip.Vertical.Collapse.TimeOfLongestAnimationStep");
          },
          testing::Gt(0), "Check longest step is nonzero."),
      If([&] { return has_fps; },
         Then(CheckResult(
             [this]() {
               return histogram_tester().GetTotalSum(
                   "TabStrip.Vertical.Collapse.AnimationFPS");
             },
             testing::Gt(0), "Check fps is nonzero.")),
         Else(Log("Compositor failed to render during test."))));
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewInteractiveUiTest,
                       CollapseButtonSuppressesExpandOnHover) {
  auto* const controller =
      tabs::VerticalTabStripStateController::From(browser());
  controller->SetExpandOnHoverEnabled(true);

  RunTestSequence(
      WaitForShow(kVerticalTabStripCollapseButtonElementId),
      EnsurePresent(kVerticalTabStripCollapseButtonElementId),

      // Move mouse to the collapse button and click it to collapse.
      MoveMouseTo(kVerticalTabStripCollapseButtonElementId), ClickMouse(),

      // Wait for collapse animation to complete.
      WaitForEvent(kTabStripRegionElementId,
                   VerticalTabStripRegionView::kAnimationCompletedEvent),

      // Verify that the tab strip remains unexpanded despite the mouse hovering
      // over the top container.
      CheckViewProperty(kTabStripRegionElementId,
                        &VerticalTabStripRegionView::is_expanded_on_hover,
                        false));
}

class VerticalTabStripExpandOnHoverInteractiveUiTest
    : public VerticalTabStripRegionViewInteractiveUiTest {
 public:
  VerticalTabStripExpandOnHoverInteractiveUiTest() = default;

  const std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    // TODO(crbug.com/580059964, crbug.com/579015393): Remove this manual
    // feature override once expand-on-hover z-order, popup locking, and
    // activation handoff bugs are fixed with WebUI omnibox enabled.
    return {omnibox::internal::kWebUIOmniboxPopup};
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  VerticalTabStripRegionView* region_view() {
    return browser_view()->vertical_tab_strip_region_view_for_testing();
  }

  auto CollapseVerticalTabStripWithExpandOnHover() {
    return Steps(
        MoveMouseTo(kToolbarAppMenuButtonElementId), Do([this]() {
          vertical_tab_strip_state_controller()->SetExpandOnHoverEnabled(true);
          vertical_tab_strip_state_controller()->RequestCollapse(true);
        }),
        PollUntil(
            [this]() {
              return vertical_tab_strip_state_controller()->IsCollapsed();
            },
            "WaitForCollapsedVerticalTabStrip"),
        Do([this]() { RunScheduledLayouts(); }));
  }

  auto WaitForExpandOnHover(bool expected_expanded) {
    return PollUntil(
        [this, expected_expanded]() {
          return region_view()->is_expanded_on_hover() == expected_expanded &&
                 !BrowserAnimationController::From(browser())->IsAnimating(
                     TabStripAnimations::kVerticalTabStrip);
        },
        "WaitForExpandOnHover");
  }

  auto SetOmniboxText(const std::u16string& text) {
    return Do([this, text]() {
      browser_view()->GetLocationBar()->GetOmniboxView()->SetUserText(text);
    });
  }

  auto WaitForOmniboxFocus(bool expected_focused) {
    return PollUntil(
        [this, expected_focused]() {
          return ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX) ==
                 expected_focused;
        },
        "WaitForOmniboxFocus");
  }

  auto WaitForOmniboxPopupOpen(bool expected_open) {
    return PollUntil(
        [this, expected_open]() {
          const auto* omnibox_controller =
              browser_view()->GetLocationBar()->GetOmniboxController();
          const bool is_dropdown_open =
              omnibox_controller->IsPopupOpen() &&
              !omnibox_controller->autocomplete_controller()->result().empty();
          return is_dropdown_open == expected_open;
        },
        "WaitForOmniboxPopupOpen");
  }

  void VerifyRegionViewHasHighestZOrder() {
    ASSERT_TRUE(region_view()->is_expanded_on_hover());
    ASSERT_NE(nullptr, region_view()->layer());

    // 1. In BrowserView's paint z-order, VerticalTabStripRegionView must be the
    // last (topmost) visible non-empty child view.
    const views::View::Views z_order = browser_view()->GetChildrenInZOrder();
    const auto vts_it = std::ranges::find(z_order, region_view());
    ASSERT_NE(vts_it, z_order.end());
    for (auto it = std::next(vts_it); it != z_order.end(); ++it) {
      views::View* const child = *it;
      EXPECT_TRUE(!child->GetVisible() || child->bounds().IsEmpty())
          << "Visible BrowserView child (" << child->GetClassName()
          << ") is ordered above VerticalTabStripRegionView in z-order.";
    }

    // 2. Every visible ui::Layer in BrowserView's subtree outside region_view()
    // (e.g. top chrome, web contents native view layers, side panel, overlays)
    // must stack below region_view()->layer().
    std::vector<const ui::Layer*> other_browser_layers;
    for (views::View* child : browser_view()->children()) {
      if (child != region_view()) {
        CollectSubtreeLayers(child, other_browser_layers);
      }
    }
    EXPECT_FALSE(other_browser_layers.empty());
    for (const ui::Layer* layer : other_browser_layers) {
      EXPECT_TRUE(IsLayerStackedAbove(region_view()->layer(), layer))
          << "Layer (" << layer->name()
          << ") is stacked at or above VerticalTabStripRegionView.";
    }

    // 3. No visible child widget of BrowserWidget overlapping
    // VerticalTabStripRegionView may render above it while expand-on-hover is
    // active.
    const gfx::Rect vts_screen_bounds = region_view()->GetBoundsInScreen();
    views::Widget* const browser_widget = browser_view()->GetWidget();
    const views::Widget::Widgets child_widgets =
        views::Widget::GetAllChildWidgets(browser_widget->GetNativeView());
    for (views::Widget* child_widget : child_widgets) {
      if (child_widget == browser_widget || !child_widget->IsVisible() ||
          !child_widget->GetWindowBoundsInScreen().Intersects(
              vts_screen_bounds)) {
        continue;
      }
      if (child_widget->GetLayer() &&
          IsLayerStackedAbove(region_view()->layer(),
                              child_widget->GetLayer())) {
        continue;
      }
      ADD_FAILURE() << "Visible child widget (" << child_widget->GetName()
                    << ") renders above VerticalTabStripRegionView while"
                    << " expand-on-hover is active.";
    }
  }
};

// Verifies that when expand-on-hover is active while the omnibox has focus,
// VerticalTabStripRegionView has the highest z-order in the browser window.
IN_PROC_BROWSER_TEST_F(VerticalTabStripExpandOnHoverInteractiveUiTest,
                       ExpandOnHoverHasHighestZOrder) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  RunTestSequence(CollapseVerticalTabStripWithExpandOnHover(),
                  Do([this]() { chrome::FocusLocationBar(browser()); }),
                  WaitForOmniboxFocus(true),
                  MoveMouseTo(kVerticalTabStripCollapseButtonElementId),
                  WaitForExpandOnHover(true), WaitForOmniboxFocus(true),
                  Do([this]() { VerifyRegionViewHasHighestZOrder(); }));
}

// Verifies that opening the omnibox suggestions popup locks expand-on-hover
// into the collapsed state (even while the mouse remains hovered over VTS), and
// dismissing the popup releases the lock so VTS can expand on hover again.
IN_PROC_BROWSER_TEST_F(VerticalTabStripExpandOnHoverInteractiveUiTest,
                       OmniboxPopupLocksAndUnlocksExpandOnHover) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  ui::Accelerator focus_location_accelerator;
  ASSERT_TRUE(browser_view()->GetAccelerator(IDC_FOCUS_LOCATION,
                                             &focus_location_accelerator));

  RunTestSequence(
      CollapseVerticalTabStripWithExpandOnHover(),
      MoveMouseTo(kVerticalTabStripCollapseButtonElementId),
      WaitForExpandOnHover(true), MoveMouseTo(kTabStripRegionElementId),

      // Focus the omnibox and enter user text to open the suggestions popup.
      SendAccelerator(kBrowserViewElementId, focus_location_accelerator),
      WaitForOmniboxFocus(true), SetOmniboxText(u"a"),
      WaitForOmniboxPopupOpen(true),

      // Expand-on-hover must collapse despite the mouse remaining over VTS.
      WaitForExpandOnHover(false),

      // Moving the mouse inside VTS while the popup is open must not re-expand.
      MoveMouseTo(kVerticalTabStripCollapseButtonElementId),
      WaitForOmniboxPopupOpen(true), WaitForExpandOnHover(false),

      // Clear the omnibox input to close the suggestions popup; the lock should
      // be released, allowing VTS to expand on hover again while the mouse
      // remains over VTS and the omnibox retains focus.
      SetOmniboxText(u""), WaitForOmniboxPopupOpen(false),
      WaitForExpandOnHover(true), WaitForOmniboxFocus(true));
}

// Verifies that adding new tabs (NTP), switching between web tabs and NTP tabs,
// and removing tabs while the mouse is hovered over the expanded vertical tab
// strip does not collapse expand-on-hover.
IN_PROC_BROWSER_TEST_F(VerticalTabStripExpandOnHoverInteractiveUiTest,
                       AddingSwitchingAndRemovingTabsPreservesExpandOnHover) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kThirdTab);

  ui::Accelerator close_tab_accelerator;
  ASSERT_TRUE(
      browser_view()->GetAccelerator(IDC_CLOSE_TAB, &close_tab_accelerator));

  base::UserActionTester user_action_tester;

  RunTestSequence(
      InstrumentTab(kFirstTab), CollapseVerticalTabStripWithExpandOnHover(),
      MoveMouseTo(kVerticalTabStripCollapseButtonElementId),
      WaitForExpandOnHover(true),

      // Position cursor inside the expanded region (past the collapsed width)
      // so any transient collapse shrinks out from under the cursor.
      MoveMouseTo(kTabStripRegionElementId),
      Do([&user_action_tester]() { user_action_tester.ResetCounts(); }),

      // 1. Add a new tab (opens NTP, which focuses the omnibox by default).
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUINewTabURL)),
      WaitForExpandOnHover(true),
      CheckResult(
          [&user_action_tester]() {
            return user_action_tester.GetActionCount(
                "VerticalTabs_ExpandOnHover_Hide");
          },
          0),

      // 2. Switch to non-NTP tab 0 and then back to NTP tab 1 while hovered in
      // VTS.
      SelectTab(kTabStripElementId, 0), WaitForExpandOnHover(true),
      CheckResult(
          [&user_action_tester]() {
            return user_action_tester.GetActionCount(
                "VerticalTabs_ExpandOnHover_Hide");
          },
          0),
      SelectTab(kTabStripElementId, 1), WaitForExpandOnHover(true),
      CheckResult(
          [&user_action_tester]() {
            return user_action_tester.GetActionCount(
                "VerticalTabs_ExpandOnHover_Hide");
          },
          0),

      // 3. Add a third tab (NTP), keep the mouse over the expanded VTS region,
      // then close both the active NTP tab and a background tab.
      MoveMouseTo(kTabStripRegionElementId),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUINewTabURL)),
      WaitForExpandOnHover(true),
      CheckResult(
          [&user_action_tester]() {
            return user_action_tester.GetActionCount(
                "VerticalTabs_ExpandOnHover_Hide");
          },
          0),

      // Close the active NTP tab (selection moves to the remaining NTP tab).
      SendAccelerator(kBrowserViewElementId, close_tab_accelerator),
      WaitForHide(kThirdTab), WaitForExpandOnHover(true),
      CheckResult(
          [&user_action_tester]() {
            return user_action_tester.GetActionCount(
                "VerticalTabs_ExpandOnHover_Hide");
          },
          0),

      // Close the background tab while VTS remains hovered.
      Do([this]() {
        tab_strip_model()->CloseWebContentsAt(
            0, TabCloseTypes::CLOSE_USER_GESTURE);
      }),
      WaitForHide(kFirstTab), WaitForExpandOnHover(true),
      CheckResult(
          [&user_action_tester]() {
            return user_action_tester.GetActionCount(
                "VerticalTabs_ExpandOnHover_Hide");
          },
          0));
}
