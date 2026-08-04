// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"

#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_utils.h"

namespace {

class TabStripComboButtonInteractiveUiTest
    : public VerticalTabsInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  TabStripComboButtonInteractiveUiTest() {
    animation_mode_reset_ = gfx::AnimationTestApi::SetRichAnimationRenderMode(
        gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
  }
  ~TabStripComboButtonInteractiveUiTest() override = default;

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    return {{tabs::kVerticalTabs, {}}, {organizer_panel::kOrganizerPanel, {}}};
  }

  auto SetPinned(const char* pref, bool pinned) {
    return Do([this, pref, pinned]() {
      browser()->GetProfile()->GetPrefs()->SetBoolean(pref, pinned);
    });
  }

  auto CheckFlatEdge(ui::ElementIdentifier id,
                     TabStripFlatEdgeButton::FlatEdge expected) {
    using Observer =
        views::test::PollingViewObserver<TabStripFlatEdgeButton::FlatEdge,
                                         TabStripFlatEdgeButton>;
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(Observer, kFlatEdgeState);
    return Steps(PollView(kFlatEdgeState, id,
                          [](const TabStripFlatEdgeButton* button) {
                            return button->flat_edge_for_testing();
                          }),
                 WaitForState(kFlatEdgeState, expected),
                 StopObservingState(kFlatEdgeState));
  }

  auto TriggerEphemeralState() {
    return WithView(kTabStripComboButtonElementId, [](views::View* view) {
      views::AsViewClass<TabStripComboButton>(view)->OnBubbleInitializing();
    });
  }

  auto TriggerBubbleDestroying() {
    return WithView(kTabStripComboButtonElementId, [](views::View* view) {
      views::AsViewClass<TabStripComboButton>(view)->OnBubbleDestroying();
    });
  }

  auto EnsureBothButtonsVisible() {
    return Steps(SetPinned(prefs::kTabSearchPinnedToTabstrip, true),
                 SetPinned(prefs::kEverythingMenuPinnedToTabstrip, true),
                 WaitForShow(kTabSearchButtonElementId),
                 WaitForShow(kSavedTabGroupButtonElementId));
  }

  auto ExecuteCommand(int command_id) {
    return WithView(
        kTabStripComboButtonElementId, [command_id](views::View* combo) {
          views::AsViewClass<TabStripComboButton>(combo)->ExecuteCommand(
              command_id, 0);
        });
  }

  auto CheckUserAction(const std::string& action, int expected_count) {
    return CheckResult(
        [this, action]() { return user_action_tester_.GetActionCount(action); },
        expected_count);
  }

 private:
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;
  base::UserActionTester user_action_tester_;
};

IN_PROC_BROWSER_TEST_F(TabStripComboButtonInteractiveUiTest,
                       RecordUserActionsOnPinUnpin) {
  RunTestSequence(
      EnsureBothButtonsVisible(),
      // Unpin Tab Search.
      ExecuteCommand(IDC_TAB_SEARCH_TOGGLE_PIN),
      CheckUserAction("TabStripComboButton.TabSearch.Unpinned", 1),
      // Pin Tab Search.
      ExecuteCommand(IDC_TAB_SEARCH_TOGGLE_PIN),
      CheckUserAction("TabStripComboButton.TabSearch.Pinned", 1),
      // Unpin Everything Menu.
      ExecuteCommand(IDC_EVERYTHING_MENU_TOGGLE_PIN),
      CheckUserAction("TabStripComboButton.EverythingMenu.Unpinned", 1),
      // Pin Everything Menu.
      ExecuteCommand(IDC_EVERYTHING_MENU_TOGGLE_PIN),
      CheckUserAction("TabStripComboButton.EverythingMenu.Pinned", 1));
}

IN_PROC_BROWSER_TEST_F(TabStripComboButtonInteractiveUiTest,
                       UpdateStylesOnOrientationChange) {
  using FlatEdge = TabStripFlatEdgeButton::FlatEdge;
  RunTestSequence(
      EnsureBothButtonsVisible(),
      CheckFlatEdge(kSavedTabGroupButtonElementId, FlatEdge::kRight),
      CheckFlatEdge(kTabSearchButtonElementId, FlatEdge::kLeft),
      // Collapse vertical tabs.
      PressButton(kVerticalTabStripCollapseButtonElementId),
      Do([this]() { RunScheduledLayouts(); }),
      CheckFlatEdge(kSavedTabGroupButtonElementId, FlatEdge::kBottom),
      CheckFlatEdge(kTabSearchButtonElementId, FlatEdge::kTop),
      // Expand vertical tabs.
      PressButton(kVerticalTabStripCollapseButtonElementId),
      Do([this]() { RunScheduledLayouts(); }),
      CheckFlatEdge(kSavedTabGroupButtonElementId, FlatEdge::kRight),
      CheckFlatEdge(kTabSearchButtonElementId, FlatEdge::kLeft));
}

IN_PROC_BROWSER_TEST_F(TabStripComboButtonInteractiveUiTest,
                       UpdateStylesOnVisibilityChange) {
  using FlatEdge = TabStripFlatEdgeButton::FlatEdge;
  RunTestSequence(
      EnsureBothButtonsVisible(),
      CheckFlatEdge(kSavedTabGroupButtonElementId, FlatEdge::kRight),
      CheckFlatEdge(kTabSearchButtonElementId, FlatEdge::kLeft),
      // Hide end button via pref.
      SetPinned(prefs::kTabSearchPinnedToTabstrip, false),
      WaitForHide(kTabSearchButtonElementId),
      CheckFlatEdge(kSavedTabGroupButtonElementId, FlatEdge::kNone),
      // Show end button again.
      SetPinned(prefs::kTabSearchPinnedToTabstrip, true),
      WaitForShow(kTabSearchButtonElementId),
      CheckFlatEdge(kSavedTabGroupButtonElementId, FlatEdge::kRight),
      CheckFlatEdge(kTabSearchButtonElementId, FlatEdge::kLeft),
      // Hide start button via pref.
      SetPinned(prefs::kEverythingMenuPinnedToTabstrip, false),
      WaitForHide(kSavedTabGroupButtonElementId),
      CheckFlatEdge(kTabSearchButtonElementId, FlatEdge::kNone),
      // Show start button again.
      SetPinned(prefs::kEverythingMenuPinnedToTabstrip, true));
}

IN_PROC_BROWSER_TEST_F(TabStripComboButtonInteractiveUiTest, UnpinTabSearch) {
  RunTestSequence(EnsureBothButtonsVisible(),
                  ExecuteCommand(IDC_TAB_SEARCH_TOGGLE_PIN),
                  // Verify button is hidden and pref is updated.
                  WaitForHide(kTabSearchButtonElementId),
                  CheckResult(
                      [this]() {
                        return browser()->GetProfile()->GetPrefs()->GetBoolean(
                            prefs::kTabSearchPinnedToTabstrip);
                      },
                      false));
}

IN_PROC_BROWSER_TEST_F(TabStripComboButtonInteractiveUiTest,
                       UnpinEverythingMenu) {
  RunTestSequence(EnsureBothButtonsVisible(),
                  ExecuteCommand(IDC_EVERYTHING_MENU_TOGGLE_PIN),
                  // Verify button is hidden and pref is updated.
                  WaitForHide(kSavedTabGroupButtonElementId),
                  CheckResult(
                      [this]() {
                        return browser()->GetProfile()->GetPrefs()->GetBoolean(
                            prefs::kEverythingMenuPinnedToTabstrip);
                      },
                      false));
}

IN_PROC_BROWSER_TEST_F(TabStripComboButtonInteractiveUiTest,
                       HideTabSearchAfterEphemeralShow) {
  RunTestSequence(
      // Ensure tab search is not pinned.
      SetPinned(prefs::kTabSearchPinnedToTabstrip, false),
      WaitForHide(kTabSearchButtonElementId),
      // Trigger ephemeral state.
      TriggerEphemeralState(), WaitForShow(kTabSearchButtonElementId),
      TriggerBubbleDestroying(),
      // Button should disappear after a couple seconds.
      WaitForHide(kTabSearchButtonElementId));
}

IN_PROC_BROWSER_TEST_F(TabStripComboButtonInteractiveUiTest,
                       PinTabSearchWhileEphemeral) {
  RunTestSequence(
      // Ensure tab search is not pinned.
      SetPinned(prefs::kTabSearchPinnedToTabstrip, false),
      WaitForHide(kTabSearchButtonElementId),
      // Trigger ephemeral state.
      TriggerEphemeralState(), WaitForShow(kTabSearchButtonElementId),
      ExecuteCommand(IDC_TAB_SEARCH_TOGGLE_PIN),
      // Button should still be visible.
      CheckView(kTabSearchButtonElementId,
                [](views::View* view) { return view->GetVisible(); }));
}

class TabStripComboButtonEverythingMenuInteractiveUiTest
    : public VerticalTabsInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  TabStripComboButtonEverythingMenuInteractiveUiTest() = default;
  ~TabStripComboButtonEverythingMenuInteractiveUiTest() override = default;

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    return {{tabs::kVerticalTabs, {}}};
  }

  const std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    return {organizer_panel::kOrganizerPanel};
  }

  auto SetPinned(const char* pref, bool pinned) {
    return Do([this, pref, pinned]() {
      browser()->GetProfile()->GetPrefs()->SetBoolean(pref, pinned);
    });
  }

  auto EnsureBothButtonsVisible() {
    return Steps(SetPinned(prefs::kTabSearchPinnedToTabstrip, true),
                 SetPinned(prefs::kEverythingMenuPinnedToTabstrip, true),
                 WaitForShow(kTabSearchButtonElementId),
                 WaitForShow(kSavedTabGroupButtonElementId));
  }

  auto ExecuteCommand(int command_id) {
    return WithView(
        kTabStripComboButtonElementId, [command_id](views::View* combo) {
          views::AsViewClass<TabStripComboButton>(combo)->ExecuteCommand(
              command_id, 0);
        });
  }

  auto CheckUserAction(const std::string& action, int expected_count) {
    return CheckResult(
        [this, action]() { return user_action_tester_.GetActionCount(action); },
        expected_count);
  }

 private:
  base::UserActionTester user_action_tester_;
};

IN_PROC_BROWSER_TEST_F(TabStripComboButtonEverythingMenuInteractiveUiTest,
                       RecordUserActionsOnPinUnpin) {
  RunTestSequence(
      EnsureBothButtonsVisible(),
      // Unpin Everything Menu.
      ExecuteCommand(IDC_EVERYTHING_MENU_TOGGLE_PIN),
      CheckUserAction("TabStripComboButton.EverythingMenu.Unpinned", 1),
      // Pin Everything Menu.
      ExecuteCommand(IDC_EVERYTHING_MENU_TOGGLE_PIN),
      CheckUserAction("TabStripComboButton.EverythingMenu.Pinned", 1));
}

IN_PROC_BROWSER_TEST_F(TabStripComboButtonEverythingMenuInteractiveUiTest,
                       UnpinEverythingMenu) {
  RunTestSequence(EnsureBothButtonsVisible(),
                  ExecuteCommand(IDC_EVERYTHING_MENU_TOGGLE_PIN),
                  // Verify button is hidden and pref is updated.
                  WaitForHide(kSavedTabGroupButtonElementId),
                  CheckResult(
                      [this]() {
                        return browser()->GetProfile()->GetPrefs()->GetBoolean(
                            prefs::kEverythingMenuPinnedToTabstrip);
                      },
                      false));
}

class TabStripComboButtonHorizontalInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  TabStripComboButtonHorizontalInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures({organizer_panel::kOrganizerPanel},
                                          {});
  }
  ~TabStripComboButtonHorizontalInteractiveUiTest() override = default;

  auto SetPinned(const char* pref, bool pinned) {
    return Do([this, pref, pinned]() {
      browser()->GetProfile()->GetPrefs()->SetBoolean(pref, pinned);
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabStripComboButtonHorizontalInteractiveUiTest,
                       OnlyTabSearchIsPresent) {
  RunTestSequence(
      // Pin both tab search and everything menu.
      SetPinned(prefs::kTabSearchPinnedToTabstrip, true),
      SetPinned(prefs::kEverythingMenuPinnedToTabstrip, true),
      // Tab search should be visible.
      WaitForShow(kTabSearchButtonElementId),
      // Saved tab group button should NOT be present in the view hierarchy of
      // the combo button.
      EnsureNotPresent(kSavedTabGroupButtonElementId));
}

}  // namespace
