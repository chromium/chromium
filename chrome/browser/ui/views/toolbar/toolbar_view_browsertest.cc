// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

#include <tuple>
#include <vector>

#include "base/i18n/base_i18n_switches.h"
#include "base/i18n/rtl.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toolbar_controller_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/home_button.h"
#include "chrome/browser/ui/views/toolbar/overflow_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_divider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_glic_actor_task_icon.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/features.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

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

  if (features::IsWebUILocationBarEnabled()) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return browser_view->toolbar()->location_bar()->GetAnchorOrNull() !=
             nullptr;
    }));
    views::BubbleAnchor anchor =
        browser_view->toolbar_button_provider()->GetBubbleAnchor(std::nullopt);
    EXPECT_EQ(anchor.GetIfElement(),
              browser_view->toolbar()->location_bar()->GetAnchorOrNull());
  } else {
    views::BubbleAnchor anchor =
        browser_view->toolbar_button_provider()->GetBubbleAnchor(std::nullopt);
    views::View* anchor_view = anchor.GetIfView();
    EXPECT_EQ(anchor_view, browser_view->toolbar()->location_bar_view());
  }
}

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest,
                       BubbleAnchorUsesTopContainerWhenLocationBarNotDrawn) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(browser_view);

  ToolbarView* toolbar = browser_view->toolbar();
  ASSERT_TRUE(toolbar);
  // Simulate app windows (no visible location bar) by hiding the location bar.
  if (toolbar->location_bar_view()) {
    toolbar->location_bar_view()->SetVisible(false);
  } else {
    ASSERT_TRUE(toolbar->GetWebUIToolbarViewForTesting());
    toolbar->GetWebUIToolbarViewForTesting()->SetVisible(false);
  }

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
  // should be treated as caption. When the WebUI app menu button is enabled,
  // `ToolbarView` zeroes its trailing layout margin (applying only visual
  // spacing in CSS), so `WebUIToolbarWebView` spans to the trailing edge and
  // `IsPositionInWindowCaption` treats `empty_area` as inside a child view.
  gfx::Point empty_area(toolbar->width() - 1, toolbar->height() / 2);
  if (!features::IsWebUIAppMenuButtonEnabled()) {
    EXPECT_TRUE(toolbar->IsPositionInWindowCaption(empty_area));
  } else {
    EXPECT_FALSE(toolbar->IsPositionInWindowCaption(empty_area));
  }

  // A point in the upper portion of empty area should also be caption.
  gfx::Point upper_empty(toolbar->width() - 1, 1);
  EXPECT_TRUE(toolbar->IsPositionInWindowCaption(upper_empty));

  // A point in the lower portion of empty area should also be caption
  // (no centerline restriction).
  gfx::Point lower_empty(toolbar->width() - 1, toolbar->height() - 1);
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
  if (features::IsWebUIToolbarFullyEnabled()) {
    GTEST_SKIP() << "C++ ToolbarController and FlexLayout overflow are "
                    "disabled when WebUIToolbar is fully enabled.";
  }
  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kShowForwardButton,
                                                  true);
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  ASSERT_TRUE(toolbar);
  LocationBar* location_bar = toolbar->location_bar();
  views::View* forward = toolbar->forward_button();
  views::View* home = toolbar->home_button();
  ASSERT_TRUE(location_bar);
  if (!features::IsWebUIBackForwardButtonEnabled()) {
    ASSERT_TRUE(forward);
  }
  if (!features::IsWebUIHomeButtonEnabled()) {
    ASSERT_TRUE(home);
  }
  auto is_forward_visible = [&]() {
    if (forward) {
      return forward->GetVisible() && !forward->bounds().IsEmpty();
    }
    return !toolbar->toolbar_controller()->IsElementOverflowedForTesting(
        kToolbarForwardButtonElementId);
  };
  auto is_home_visible = [&]() {
    if (home) {
      return home->GetVisible() && !home->bounds().IsEmpty();
    }
    return !toolbar->toolbar_controller()->IsElementOverflowedForTesting(
        kToolbarHomeButtonElementId);
  };
  auto get_location_bar_width = [&]() {
    if (toolbar->location_bar_view()) {
      return toolbar->location_bar_view()->bounds().width();
    }
    return toolbar->GetWebUIToolbarViewForTesting()
        ->GetLocationBarWidthForTesting();
  };
  const int preferred_width = location_bar->PreferredSize().width();
  const int min_width = location_bar->MinimumSize().width();
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
  EXPECT_TRUE(is_forward_visible());
  EXPECT_TRUE(is_home_visible());
  EXPECT_GT(get_location_bar_width(), preferred_width);

  // When resized down to a moderate width where all elements fit, the omnibox
  // shrinks down towards its preferred width while buttons remain visible.
  toolbar->SetSize(gfx::Size(all_fit_width, toolbar_height));
  EXPECT_TRUE(is_forward_visible());
  EXPECT_TRUE(is_home_visible());
  EXPECT_GE(get_location_bar_width(), preferred_width);

  // Under narrow widths where responsive buttons have dropped out, buttons are
  // hidden and the omnibox shrinks down from its preferred width towards its
  // minimum width.
  toolbar->SetSize(gfx::Size(omnibox_resizing_width, toolbar_height));
  EXPECT_FALSE(is_forward_visible());
  EXPECT_FALSE(is_home_visible());
  EXPECT_LT(get_location_bar_width(), preferred_width);
  EXPECT_GT(get_location_bar_width(), min_width);

  // At absolute minimum width, all responsive buttons remain hidden and the
  // omnibox reaches its minimum width.
  toolbar->SetSize(gfx::Size(min_width_bound, toolbar_height));
  EXPECT_FALSE(is_forward_visible());
  EXPECT_FALSE(is_home_visible());
  EXPECT_EQ(get_location_bar_width(), min_width);

  // When expanding to moderate width, responsive buttons reappear with
  // non-empty bounds and the omnibox recovers its preferred width.
  toolbar->SetSize(gfx::Size(all_fit_width, toolbar_height));
  EXPECT_TRUE(is_forward_visible());
  EXPECT_TRUE(is_home_visible());
  EXPECT_GE(get_location_bar_width(), preferred_width);

  // Expanding back to a super wide width allows the omnibox to expand into
  // excess space again.
  toolbar->SetSize(gfx::Size(super_wide_width, toolbar_height));
  EXPECT_TRUE(is_forward_visible());
  EXPECT_TRUE(is_home_visible());
  EXPECT_GT(get_location_bar_width(), preferred_width);
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
  views::View* webui_toolbar = toolbar->GetWebUIToolbarViewForTesting();
  if (!features::IsWebUIBackForwardButtonEnabled()) {
    ASSERT_TRUE(forward);
  }
  if (!features::IsWebUIHomeButtonEnabled()) {
    ASSERT_TRUE(home);
  }
  if (features::IsWebUIToolbarEnabled()) {
    ASSERT_TRUE(webui_toolbar);
  }

  const int all_fit_width = toolbar->GetPreferredSize().width();
  // Constrain toolbar height so that the available cross-axis height is smaller
  // than the buttons' preferred height (simulating custom frame mode with
  // vertical tabs).
  const int constrained_height = toolbar->GetPreferredSize().height() - 4;

  toolbar->SetSize(gfx::Size(all_fit_width, constrained_height));

  if (forward) {
    EXPECT_TRUE(forward->GetVisible());
    EXPECT_GT(forward->bounds().height(), 0);
  }

  if (home) {
    EXPECT_TRUE(home->GetVisible());
    EXPECT_GT(home->bounds().height(), 0);
  }

  if (webui_toolbar) {
    EXPECT_TRUE(webui_toolbar->GetVisible());
    EXPECT_GT(webui_toolbar->bounds().height(), 0);
  }

  if (avatar && avatar->GetVisible()) {
    EXPECT_GT(avatar->bounds().height(), 0);
  }
}

class ToolbarViewInteriorMarginBrowserTestBase
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    if (GetParam()) {
      command_line->AppendSwitchASCII(switches::kForceUIDirection,
                                      switches::kForceDirectionRTL);
      command_line->AppendSwitchASCII(switches::kForceTextDirection,
                                      switches::kForceDirectionRTL);
    }
  }

  ToolbarView* toolbar() {
    return BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  }

  ToolbarButtonProvider* toolbar_button_provider() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider();
  }

  views::FlexLayout* flex_layout() {
    return static_cast<views::FlexLayout*>(toolbar()->GetLayoutManager());
  }

  gfx::Insets default_margin() const {
    return GetLayoutInsets(LayoutInset::TOOLBAR_INTERIOR_MARGIN);
  }

  void ExpectDefaultMargins() {
    EXPECT_EQ(flex_layout()->interior_margin(), default_margin());
  }

  void ExpectLeadingMarginZeroed() {
    gfx::Insets expected = default_margin();
    expected.set_left(0);
    EXPECT_EQ(flex_layout()->interior_margin(), expected);
  }

  void ExpectTrailingMarginZeroed() {
    gfx::Insets expected = default_margin();
    expected.set_right(0);
    EXPECT_EQ(flex_layout()->interior_margin(), expected);
  }

  // Aligns the contextual tasks side panel to the right or left. The contextual
  // tasks button is placed on the toolbar edge next to the side panel.
  void SetContextualTasksRightAligned(bool right_aligned) {
    ScopedDictPrefUpdate(browser()->GetProfile()->GetPrefs(),
                         prefs::kSidePanelAlignmentOverrides)
        ->Set(SidePanelEntryIdToString(SidePanelEntryId::kContextualTasks),
              right_aligned);
  }
};

// Tests the baseline scenario without the ContextualTasks feature enabled in
// both LTR and RTL modes.
class ToolbarViewDefaultInteriorMarginBrowserTest
    : public ToolbarViewInteriorMarginBrowserTestBase {
 public:
  ToolbarViewDefaultInteriorMarginBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            contextual_tasks::kContextualTasks,
            contextual_tasks::kContextualTasksSidePanel,
            contextual_tasks::kContextualTasksRearchitecture,
            contextual_tasks::kContextualTasksSidePanelRearchitecture,
            contextual_tasks::kContextualTasksEphemeralBrandedEntryPoint,
            features::kWebUIToolbar, features::kWebUIBackForwardButton,
            features::kWebUIAppMenuButton});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ToolbarViewDefaultInteriorMarginBrowserTest,
                       DefaultInteriorMarginWithoutFeature) {
  const bool is_rtl = GetParam();
  EXPECT_EQ(base::i18n::IsRTL(), is_rtl);

  ExpectDefaultMargins();
}

INSTANTIATE_TEST_SUITE_P(All,
                         ToolbarViewDefaultInteriorMarginBrowserTest,
                         testing::Bool());

class ToolbarViewGlicAvatarCrashTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  ToolbarViewGlicAvatarCrashTest() {
    const char* location =
        IsLeftOfProfile()
            ? features::kGlicToolbarButtonLocationLeftOfProfileChip
            : features::kGlicToolbarButtonLocationRightOfOmnibox;
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kInitialWebUI, {}},
        {features::kGlic, {}},
        {features::kGlicHorizontalTabToolbarButton, {}},
        {features::kGlicActorUi,
         {{features::kGlicActorUiTaskIconName, "true"}}},
        {features::kGlicToolbarButtonLocation,
         {{features::kGlicToolbarButtonLocationParam.name, location}}},
#if BUILDFLAG(IS_CHROMEOS)
        {chromeos::features::kFeatureManagementGlic, {}},
#endif
    };
    std::vector<base::test::FeatureRef> disabled_features = {
        features::kGlicCountryFiltering,
        features::kGlicLocaleFiltering,
        features::kWebUIGlicButton,
        features::kWebUIToolbar,
    };
    if (IsWebUIAvatarEnabled()) {
      enabled_features.push_back({features::kWebUIAvatarButton, {}});
    } else {
      disabled_features.push_back(features::kWebUIAvatarButton);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  bool IsLeftOfProfile() const { return std::get<0>(GetParam()); }
  bool IsWebUIAvatarEnabled() const { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ToolbarViewGlicAvatarCrashTest,
                       ShowAndHideGlicSecondaryButton) {
  ToolbarView* toolbar_view =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();

  ASSERT_EQ(toolbar_view->avatar_toolbar_button() == nullptr,
            IsWebUIAvatarEnabled());

  views::LabelButton* glic_button = toolbar_view->GetGlicButton();
  ASSERT_TRUE(glic_button);
  ASSERT_EQ(glic_button->parent(), toolbar_view);

  // Show the Glic button first so sibling buttons anchored relative to a
  // visible Glic button (such as `contextual_tasks_button_`) settle into their
  // pre-task positions before recording `initial_index`.
  toolbar_view->SetGlicShowState(true);
  ASSERT_TRUE(glic_button->GetVisible());
  const size_t initial_index = toolbar_view->GetIndexOf(glic_button).value();

  auto expect_expected_slot = [&]() {
    EXPECT_EQ(toolbar_view->GetIndexOf(glic_button), initial_index);
    if (IsLeftOfProfile()) {
      views::View* expected_next =
          IsWebUIAvatarEnabled()
              ? static_cast<views::View*>(toolbar_view->overflow_button())
              : static_cast<views::View*>(
                    toolbar_view->avatar_toolbar_button());
      ASSERT_TRUE(expected_next);
      EXPECT_EQ(initial_index + 1, toolbar_view->GetIndexOf(expected_next));
    } else {
      ASSERT_LT(initial_index + 1, toolbar_view->children().size());
      EXPECT_TRUE(views::IsViewClass<ToolbarDivider>(
          toolbar_view->children()[initial_index + 1]));
      ASSERT_TRUE(toolbar_view->overflow_button());
      EXPECT_LT(initial_index + 1,
                toolbar_view->GetIndexOf(toolbar_view->overflow_button()));
    }
  };
  expect_expected_slot();

  auto* secondary_button = toolbar_view->glic_secondary_button();
  ASSERT_TRUE(secondary_button);
  EXPECT_FALSE(secondary_button->GetVisible());

  // Showing the Glic secondary button reparents `glic_button` into the actor
  // buttons container.
  toolbar_view->ShowGlicSecondaryButton();
  EXPECT_TRUE(secondary_button->GetVisible());
  EXPECT_TRUE(secondary_button->parent()->GetVisible());
  EXPECT_EQ(toolbar_view->GetGlicButton(), glic_button);
  EXPECT_EQ(glic_button->parent(), secondary_button->parent());
  EXPECT_NE(glic_button->parent(), toolbar_view);
  EXPECT_FALSE(toolbar_view->GetIndexOf(glic_button).has_value());

  // Hiding the Glic secondary button must not crash when `avatar_` is null and
  // must restore `glic_button` to its original toolbar index.
  toolbar_view->HideGlicSecondaryButton();
  EXPECT_FALSE(secondary_button->GetVisible());
  EXPECT_FALSE(secondary_button->parent()->GetVisible());
  EXPECT_TRUE(glic_button->GetVisible());
  EXPECT_EQ(toolbar_view->GetGlicButton(), glic_button);
  EXPECT_EQ(glic_button->parent(), toolbar_view);
  expect_expected_slot();

  // Re-running `FinalizeHideGlicSecondaryButton()` when `glic_button` is
  // already parented to `toolbar_view` must remain a no-op on child ordering.
  toolbar_view->FinalizeHideGlicSecondaryButton();
  expect_expected_slot();
}

INSTANTIATE_TEST_SUITE_P(All,
                         ToolbarViewGlicAvatarCrashTest,
                         testing::Combine(testing::Bool(), testing::Bool()));
