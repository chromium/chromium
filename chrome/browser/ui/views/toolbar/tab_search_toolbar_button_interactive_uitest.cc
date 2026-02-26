// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/test/views_test_utils.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

class TabSearchToolbarButtonTest : public InteractiveBrowserTest {
 public:
  auto CheckElementCount(ui::ElementIdentifier id, size_t expected_count) {
    return Check([id, expected_count, this]() {
      return BrowserElements::From(browser())->GetAllElements(id).size() ==
             expected_count;
    });
  }

  auto SendTabSearchKeyPress(ui::ElementIdentifier target) {
#if BUILDFLAG(IS_MAC)
    return SendKeyPress(target, ui::VKEY_A,
                        ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
#else
    return SendKeyPress(target, ui::VKEY_A,
                        ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
#endif
  }
};

class TabSearchToolbarButtonInteractiveUiTest
    : public TabSearchToolbarButtonTest {
 public:
  TabSearchToolbarButtonInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            features::kGlic,
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementGlic,
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        /*disabled_features=*/{
            features::kGlicLocaleFiltering, features::kGlicCountryFiltering,
            tabs::kHorizontalTabStripComboButton, tabs::kVerticalTabs});
  }
  ~TabSearchToolbarButtonInteractiveUiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test verifies the TabSearch functionality when pinned.
IN_PROC_BROWSER_TEST_F(TabSearchToolbarButtonInteractiveUiTest,
                       VerifyTabSearchWhenPinned) {
  RunTestSequence(
      // Clicking Tab Search Button.
      WaitForShow(kTabSearchButtonElementId),
      CheckElementCount(kTabSearchButtonElementId, 1),
      CheckView(kTabSearchButtonElementId,
                [](views::View* view) {
                  return views::IsViewClass<PinnedActionToolbarButton>(view);
                }),
      EnsurePresent(kTabSearchButtonElementId),
      MoveMouseTo(kTabSearchButtonElementId), ClickMouse(),
      WaitForShow(kTabSearchBubbleElementId),
      // Closing Tab Search Bubble.
      SendKeyPress(kTabSearchButtonElementId, ui::VKEY_ESCAPE),
      WaitForHide(kTabSearchBubbleElementId));
}

// This test verifies the TabSearch functionality after unpinning.
IN_PROC_BROWSER_TEST_F(TabSearchToolbarButtonInteractiveUiTest,
                       VerifyTabSearchWhenUnpinned) {
#if BUILDFLAG(IS_LINUX)
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP() << "Skipping on Wayland due to flakiness.";
  }
#endif
  RunTestSequence(
      // Unpinning Tab Search Button
      WaitForShow(kTabSearchButtonElementId),
      CheckElementCount(kTabSearchButtonElementId, 1),
      MoveMouseTo(kTabSearchButtonElementId),
      MayInvolveNativeContextMenu(
          ClickMouse(ui_controls::RIGHT),
          WaitForShow(kPinnedActionToolbarUnpinElementId),
          SelectMenuItem(kPinnedActionToolbarUnpinElementId)),
      // Verifying that it is no longer present.
      WaitForHide(kTabSearchButtonElementId),
      // Clicking the Tab Search Button.
      SendTabSearchKeyPress(kTabStripElementId),
      WaitForShow(kTabSearchBubbleElementId),
      EnsurePresent(kTabSearchBubbleElementId),
      // Closing Tab Search Bubble.
      SendKeyPress(kTabSearchBubbleElementId, ui::VKEY_ESCAPE),
      WaitForHide(kTabSearchBubbleElementId));
}

class TabSearchToolbarButtonGlicDisabledTest
    : public TabSearchToolbarButtonTest {
 public:
  TabSearchToolbarButtonGlicDisabledTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kGlic, tabs::kVerticalTabs});
  }
  ~TabSearchToolbarButtonGlicDisabledTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test verifies that the TabSearch button is NOT in the toolbar when Glic
// is disabled.
IN_PROC_BROWSER_TEST_F(TabSearchToolbarButtonGlicDisabledTest,
                       ButtonNotInToolbar) {
  RunTestSequence(WaitForShow(kTabSearchButtonElementId),
                  CheckElementCount(kTabSearchButtonElementId, 1),
                  CheckView(kTabSearchButtonElementId, [](views::View* view) {
                    // When Glic is disabled, the TabSearch button should be in
                    // the tab strip, not a PinnedActionToolbarButton in the
                    // toolbar.
                    return !views::IsViewClass<PinnedActionToolbarButton>(view);
                  }));
}

class TabSearchToolbarButtonComboEnabledTest
    : public VerticalTabsInteractiveTestMixin<TabSearchToolbarButtonTest>,
      public testing::WithParamInterface<bool> {
 public:
  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    return {{tabs::kVerticalTabs, {}},
            {tabs::kHorizontalTabStripComboButton, {}},
            {features::kGlic, {}}};
  }

  void SetUpOnMainThread() override {
    TabSearchToolbarButtonTest::SetUpOnMainThread();
  }

  auto EnsureTabSearchVisible() {
    return Steps(Do([this]() {
                   browser()->profile()->GetPrefs()->SetBoolean(
                       prefs::kTabSearchPinnedToTabstrip, true);
                 }),
                 If([this]() { return this->GetParam(); },
                    Then(EnterVerticalTabsMode())),
                 WaitForShow(kTabSearchButtonElementId));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test verifies that the TabSearch button is NOT in the toolbar when
// the tab strip combo button is enabled.
IN_PROC_BROWSER_TEST_P(TabSearchToolbarButtonComboEnabledTest,
                       ButtonNotInToolbar) {
  RunTestSequence(EnsureTabSearchVisible(),
                  CheckElementCount(kTabSearchButtonElementId, 1),
                  CheckView(kTabSearchButtonElementId, [](views::View* view) {
                    // When combo button is enabled, the TabSearch button should
                    // be in the tab strip combo button, not a
                    // PinnedActionToolbarButton in the toolbar.
                    return !views::IsViewClass<PinnedActionToolbarButton>(view);
                  }));
}

// This test verifies that the TabSearch button is NOT in the toolbar when
// the tab strip combo button is enabled, even if it is pinned in the model.
IN_PROC_BROWSER_TEST_P(TabSearchToolbarButtonComboEnabledTest,
                       ButtonNotInToolbarEvenIfPinned) {
  // Pin the tab search button in the model.
  PinnedToolbarActionsModel::Get(browser()->profile())
      ->UpdatePinnedState(kActionTabSearch, true);

  RunTestSequence(EnsureTabSearchVisible(),
                  CheckElementCount(kTabSearchButtonElementId, 1),
                  CheckView(kTabSearchButtonElementId, [](views::View* view) {
                    // When combo button is enabled, the TabSearch button should
                    // be in the tab strip combo button, not a
                    // PinnedActionToolbarButton in the toolbar.
                    return !views::IsViewClass<PinnedActionToolbarButton>(view);
                  }));
}

INSTANTIATE_TEST_SUITE_P(All,
                         TabSearchToolbarButtonComboEnabledTest,
                         testing::Bool());

}  // namespace
