// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_utils.h"

namespace {

class TabStripComboButtonInteractiveUiTest
    : public TabStripInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  TabStripComboButtonInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        {tabs::kHorizontalTabStripComboButton, tab_groups::kProjectsPanel}, {});
  }
  ~TabStripComboButtonInteractiveUiTest() override = default;

  auto SetPinned(const char* pref, bool pinned) {
    return Do([this, pref, pinned]() {
      browser()->profile()->GetPrefs()->SetBoolean(pref, pinned);
    });
  }

  auto CheckFlatEdge(ui::ElementIdentifier id,
                     TabStripFlatEdgeButton::FlatEdge expected) {
    return CheckView(id, [expected](TabStripFlatEdgeButton* button) {
      return button->flat_edge_for_testing() == expected;
    });
  }

  auto SetOrientation(views::LayoutOrientation orientation) {
    return WithView(kTabSearchButtonElementId,
                    [orientation](views::View* view) {
                      views::AsViewClass<TabStripComboButton>(view->parent())
                          ->SetOrientation(orientation);
                    });
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
                 SetPinned(prefs::kProjectsPanelPinnedToTabstrip, true),
                 WaitForShow(kTabSearchButtonElementId),
                 WaitForShow(kVerticalTabStripProjectsButtonElementId));
  }

  auto ExecuteCommand(ui::ElementIdentifier id, int command_id) {
    return WithView(id, [command_id](views::View* button) {
      views::AsViewClass<TabStripComboButton>(button->parent())
          ->ExecuteCommand(command_id, 0);
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabStripComboButtonInteractiveUiTest,
                       UpdateStylesOnOrientationChange) {
  using FlatEdge = TabStripFlatEdgeButton::FlatEdge;
  RunTestSequence(
      EnsureBothButtonsVisible(),
      CheckFlatEdge(kVerticalTabStripProjectsButtonElementId, FlatEdge::kRight),
      CheckFlatEdge(kTabSearchButtonElementId, FlatEdge::kLeft),
      // Set to vertical.
      SetOrientation(views::LayoutOrientation::kVertical),
      CheckFlatEdge(kVerticalTabStripProjectsButtonElementId,
                    FlatEdge::kBottom),
      CheckFlatEdge(kTabSearchButtonElementId, FlatEdge::kTop),
      // Set back to horizontal.
      SetOrientation(views::LayoutOrientation::kHorizontal),
      CheckFlatEdge(kVerticalTabStripProjectsButtonElementId, FlatEdge::kRight),
      CheckFlatEdge(kTabSearchButtonElementId, FlatEdge::kLeft));
}

IN_PROC_BROWSER_TEST_F(TabStripComboButtonInteractiveUiTest,
                       UpdateStylesOnVisibilityChange) {
  using FlatEdge = TabStripFlatEdgeButton::FlatEdge;
  RunTestSequence(
      EnsureBothButtonsVisible(),
      CheckFlatEdge(kVerticalTabStripProjectsButtonElementId, FlatEdge::kRight),
      CheckFlatEdge(kTabSearchButtonElementId, FlatEdge::kLeft),
      // Hide end button via pref.
      SetPinned(prefs::kTabSearchPinnedToTabstrip, false),
      WaitForHide(kTabSearchButtonElementId),
      CheckFlatEdge(kVerticalTabStripProjectsButtonElementId, FlatEdge::kNone),
      // Show end button again.
      SetPinned(prefs::kTabSearchPinnedToTabstrip, true),
      WaitForShow(kTabSearchButtonElementId),
      CheckFlatEdge(kVerticalTabStripProjectsButtonElementId, FlatEdge::kRight),
      CheckFlatEdge(kTabSearchButtonElementId, FlatEdge::kLeft),
      // Hide start button via pref.
      SetPinned(prefs::kProjectsPanelPinnedToTabstrip, false),
      WaitForHide(kVerticalTabStripProjectsButtonElementId),
      CheckFlatEdge(kTabSearchButtonElementId, FlatEdge::kNone),
      // Show start button again.
      SetPinned(prefs::kProjectsPanelPinnedToTabstrip, true));
}

IN_PROC_BROWSER_TEST_F(TabStripComboButtonInteractiveUiTest, UnpinTabSearch) {
  RunTestSequence(
      EnsureBothButtonsVisible(),
      ExecuteCommand(kTabSearchButtonElementId, IDC_TAB_SEARCH_TOGGLE_PIN),
      // Verify button is hidden and pref is updated.
      WaitForHide(kTabSearchButtonElementId),
      CheckResult(
          [this]() {
            return browser()->profile()->GetPrefs()->GetBoolean(
                prefs::kTabSearchPinnedToTabstrip);
          },
          false));
}

IN_PROC_BROWSER_TEST_F(TabStripComboButtonInteractiveUiTest,
                       UnpinProjectsPanel) {
  RunTestSequence(
      EnsureBothButtonsVisible(),
      ExecuteCommand(kVerticalTabStripProjectsButtonElementId,
                     IDC_PROJECTS_PANEL_TOGGLE_PIN),
      // Verify button is hidden and pref is updated.
      WaitForHide(kVerticalTabStripProjectsButtonElementId),
      CheckResult(
          [this]() {
            return browser()->profile()->GetPrefs()->GetBoolean(
                prefs::kProjectsPanelPinnedToTabstrip);
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
      TriggerEphemeralState(), FinishTabstripAnimations(),
      WaitForShow(kTabSearchButtonElementId), TriggerBubbleDestroying(),
      FinishTabstripAnimations(),
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
      TriggerEphemeralState(), FinishTabstripAnimations(),
      WaitForShow(kTabSearchButtonElementId),
      ExecuteCommand(kTabSearchButtonElementId, IDC_TAB_SEARCH_TOGGLE_PIN),
      FinishTabstripAnimations(),
      // Button should still be visible.
      CheckView(kTabSearchButtonElementId,
                [](views::View* view) { return view->GetVisible(); }));
}

class TabStripComboButtonEverythingMenuInteractiveUiTest
    : public TabStripInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  TabStripComboButtonEverythingMenuInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        {tabs::kHorizontalTabStripComboButton}, {tab_groups::kProjectsPanel});
  }
  ~TabStripComboButtonEverythingMenuInteractiveUiTest() override = default;

  auto SetPinned(const char* pref, bool pinned) {
    return Do([this, pref, pinned]() {
      browser()->profile()->GetPrefs()->SetBoolean(pref, pinned);
    });
  }

  auto EnsureBothButtonsVisible() {
    return Steps(SetPinned(prefs::kTabSearchPinnedToTabstrip, true),
                 SetPinned(prefs::kEverythingMenuPinnedToTabstrip, true),
                 WaitForShow(kTabSearchButtonElementId),
                 WaitForShow(kSavedTabGroupButtonElementId));
  }

  auto ExecuteCommand(ui::ElementIdentifier id, int command_id) {
    return WithView(id, [command_id](views::View* button) {
      views::AsViewClass<TabStripComboButton>(button->parent())
          ->ExecuteCommand(command_id, 0);
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabStripComboButtonEverythingMenuInteractiveUiTest,
                       UnpinEverythingMenu) {
  RunTestSequence(
      EnsureBothButtonsVisible(),
      ExecuteCommand(kSavedTabGroupButtonElementId,
                     IDC_EVERYTHING_MENU_TOGGLE_PIN),
      // Verify button is hidden and pref is updated.
      WaitForHide(kSavedTabGroupButtonElementId),
      CheckResult(
          [this]() {
            return browser()->profile()->GetPrefs()->GetBoolean(
                prefs::kEverythingMenuPinnedToTabstrip);
          },
          false));
}

}  // namespace
