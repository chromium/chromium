// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/shared/new_tab_button_menu_model.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/view.h"

class SplitTabViewInteractiveUiTest
    : public VerticalTabsInteractiveTestMixin<InteractiveBrowserTest>,
      public testing::WithParamInterface<TabStripOrientation> {
 public:
  SplitTabViewInteractiveUiTest() = default;
  ~SplitTabViewInteractiveUiTest() override = default;

  TabStripOrientation orientation() const { return GetParam(); }
  bool is_horizontal() const {
    return orientation() == TabStripOrientation::kHorizontal;
  }

  void SetUpOnMainThread() override {
    VerticalTabsInteractiveTestMixin<
        InteractiveBrowserTest>::SetUpOnMainThread();
    if (is_horizontal()) {
      VerticalTabsBrowserTestMixin<
          InteractiveBrowserTest>::ExitVerticalTabsMode();
    }
  }

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    auto enabled = VerticalTabsInteractiveTestMixin<
        InteractiveBrowserTest>::GetEnabledFeatures();
    enabled.push_back({tabs::kTabStripUnification, {}});
    return enabled;
  }

 private:
  const gfx::AnimationTestApi::RenderModeResetter disable_rich_animations_ =
      gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
};

// TODO(crbug.com/447617263): Rewrite these tests so that they work on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_VerifyNewTabButtonContextMenu \
  DISABLED_VerifyNewTabButtonContextMenu
#else
#define MAYBE_VerifyNewTabButtonContextMenu VerifyNewTabButtonContextMenu
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(SplitTabViewInteractiveUiTest,
                       MAYBE_VerifyNewTabButtonContextMenu) {
  RunTestSequence(
      WaitForShow(kNewTabButtonElementId), MoveMouseTo(kNewTabButtonElementId),
      ClickMouse(ui_controls::RIGHT),
      WaitForShow(NewTabButtonMenuModel::kNewTab),
      EnsurePresent(NewTabButtonMenuModel::kNewTabInGroup),
      EnsurePresent(NewTabButtonMenuModel::kNewSplitView),
      EnsurePresent(NewTabButtonMenuModel::kCreateNewTabGroup),
      SendAccelerator(NewTabButtonMenuModel::kNewTab,
                      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)));
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_NewTabButtonContextMenuSplitView \
  DISABLED_NewTabButtonContextMenuSplitView
#else
#define MAYBE_NewTabButtonContextMenuSplitView NewTabButtonContextMenuSplitView
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(SplitTabViewInteractiveUiTest,
                       MAYBE_NewTabButtonContextMenuSplitView) {
  RunTestSequence(WaitForShow(kNewTabButtonElementId),
                  MoveMouseTo(kNewTabButtonElementId),
                  ClickMouse(ui_controls::RIGHT),
                  WaitForShow(NewTabButtonMenuModel::kNewTab),
                  EnsurePresent(NewTabButtonMenuModel::kNewSplitView),
                  SelectMenuItem(NewTabButtonMenuModel::kNewSplitView));

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  EXPECT_TRUE(browser_view->IsInSplitView());
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_NewTabButtonContextMenuSplitViewDisabled \
  DISABLED_NewTabButtonContextMenuSplitViewDisabled
#else
#define MAYBE_NewTabButtonContextMenuSplitViewDisabled \
  NewTabButtonContextMenuSplitViewDisabled
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(SplitTabViewInteractiveUiTest,
                       MAYBE_NewTabButtonContextMenuSplitViewDisabled) {
  AppendSplitTab();
  RunScheduledLayouts();

  RunTestSequence(
      WaitForShow(kNewTabButtonElementId), MoveMouseTo(kNewTabButtonElementId),
      ClickMouse(ui_controls::RIGHT),
      WaitForShow(NewTabButtonMenuModel::kNewTab),
      EnsurePresent(NewTabButtonMenuModel::kNewSplitView),
      WaitForViewProperty(NewTabButtonMenuModel::kNewSplitView, views::View,
                          Enabled, false),
      SendAccelerator(NewTabButtonMenuModel::kNewTab,
                      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)));

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  EXPECT_TRUE(browser_view->IsInSplitView());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SplitTabViewInteractiveUiTest,
    testing::Values(TabStripOrientation::kVertical,
                    TabStripOrientation::kHorizontal),
    [](const testing::TestParamInfo<TabStripOrientation>& info) {
      switch (info.param) {
        case TabStripOrientation::kVertical:
          return "Vertical";
        case TabStripOrientation::kHorizontal:
          return "Horizontal";
      }
    });
