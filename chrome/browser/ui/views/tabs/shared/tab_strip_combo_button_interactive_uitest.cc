// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"

#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/actions_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
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

  const std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    return {organizer_panel::kOrganizerPanel};
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

class TabStripComboButtonOrganizerPanelInteractiveUiTest
    : public VerticalTabsInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  TabStripComboButtonOrganizerPanelInteractiveUiTest() = default;
  ~TabStripComboButtonOrganizerPanelInteractiveUiTest() override = default;

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    return {{organizer_panel::kOrganizerPanel, {}}};
  }

  auto SetPinned(const char* pref, bool pinned) {
    return Do([this, pref, pinned]() {
      browser()->GetProfile()->GetPrefs()->SetBoolean(pref, pinned);
    });
  }

  auto CheckTabSearchIcon(const gfx::VectorIcon& expected_icon) {
    return CheckView(
        kTabSearchButtonElementId,
        [&expected_icon](TabStripFlatEdgeButton* button) {
          const auto& image_model =
              button->GetImageModel(views::Button::STATE_NORMAL);
          return image_model.has_value() && image_model->IsVectorIcon() &&
                 image_model->GetVectorIcon().vector_icon() == &expected_icon;
        });
  }

  auto CheckTabSearchTooltip(const std::u16string& expected_tooltip) {
    return CheckView(kTabSearchButtonElementId,
                     [&expected_tooltip](TabStripFlatEdgeButton* button) {
                       return button->GetTooltipText() == expected_tooltip;
                     });
  }

  auto ResizeVerticalTabsRegionToWidth(int width) {
    return Do([this, width]() {
      auto* vt_region_view = BrowserView::GetBrowserViewForBrowser(browser())
                                 ->vertical_tab_strip_region_view_for_testing();
      vt_region_view->OnResize(width - vt_region_view->width(), true);
      RunScheduledLayouts();
    });
  }

  auto ExecuteCommand(int command_id) {
    return WithView(
        kTabStripComboButtonElementId, [command_id](views::View* combo) {
          views::AsViewClass<TabStripComboButton>(combo)->ExecuteCommand(
              command_id, 0);
        });
  }
};

IN_PROC_BROWSER_TEST_F(TabStripComboButtonOrganizerPanelInteractiveUiTest,
                       TabGroupsButtonNotPresentWhenOrganizerPanelEnabled) {
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

IN_PROC_BROWSER_TEST_F(TabStripComboButtonOrganizerPanelInteractiveUiTest,
                       TabSearchIconAndTooltipWhenOrganizerPanelVisible) {
  const gfx::VectorIcon& search_icon = features::IsRoundedIconsEnabled()
                                           ? kManageSearchIcon
                                           : kTabSearchTabStripOldIcon;
  const gfx::VectorIcon& close_icon = features::IsRoundedIconsEnabled()
                                          ? kCloseSmallIcon
                                          : kCloseChromeRefreshOldIcon;
  const std::u16string view_tooltip =
      chrome::GetCleanText(l10n_util::GetStringUTF16(IDS_TAB_SEARCH_MENU));
  const std::u16string close_tooltip = chrome::GetCleanText(
      l10n_util::GetStringUTF16(IDS_ORGANIZER_PANEL_CLOSE_TAB_SEARCH));

  RunTestSequence(
      SetPinned(prefs::kTabSearchPinnedToTabstrip, true),
      WaitForShow(kTabSearchButtonElementId), CheckTabSearchIcon(search_icon),
      CheckTabSearchTooltip(view_tooltip),
      PressButton(kTabSearchButtonElementId),
      WaitForShow(kOrganizerPanelElementId), CheckTabSearchIcon(close_icon),
      CheckTabSearchTooltip(close_tooltip),
      PressButton(kTabSearchButtonElementId),
      WaitForHide(kOrganizerPanelElementId), CheckTabSearchIcon(search_icon),
      CheckTabSearchTooltip(view_tooltip));
}

IN_PROC_BROWSER_TEST_F(TabStripComboButtonOrganizerPanelInteractiveUiTest,
                       HideTabSearchAfterEphemeralShow) {
  const gfx::VectorIcon& search_icon = features::IsRoundedIconsEnabled()
                                           ? kManageSearchIcon
                                           : kTabSearchTabStripOldIcon;
  const gfx::VectorIcon& close_icon = features::IsRoundedIconsEnabled()
                                          ? kCloseSmallIcon
                                          : kCloseChromeRefreshOldIcon;
  const std::u16string search_tooltip =
      chrome::GetCleanText(l10n_util::GetStringUTF16(IDS_TAB_SEARCH_MENU));
  const std::u16string close_tooltip = chrome::GetCleanText(
      l10n_util::GetStringUTF16(IDS_ORGANIZER_PANEL_CLOSE_TAB_SEARCH));

  RunTestSequence(
      // Ensure tab search is not pinned.
      SetPinned(prefs::kTabSearchPinnedToTabstrip, false),
      WaitForHide(kTabSearchButtonElementId),
      // Open organizer panel.
      Do([this]() { chrome::ExecuteCommand(browser(), IDC_TAB_SEARCH); }),
      WaitForShow(kOrganizerPanelElementId),
      // Button should be visible ephemerally with close icon/tooltip.
      WaitForShow(kTabSearchButtonElementId), CheckTabSearchIcon(close_icon),
      CheckTabSearchTooltip(close_tooltip),
      // Close organizer panel by pressing the button.
      PressButton(kTabSearchButtonElementId),
      WaitForHide(kOrganizerPanelElementId),
      // After closing, the button should ephemerally display the search icon.
      CheckTabSearchIcon(search_icon), CheckTabSearchTooltip(search_tooltip),
      // Button should disappear after a couple seconds.
      WaitForHide(kTabSearchButtonElementId));
}

IN_PROC_BROWSER_TEST_F(TabStripComboButtonOrganizerPanelInteractiveUiTest,
                       PinTabSearchWhileEphemeral) {
  RunTestSequence(
      // Ensure tab search is not pinned.
      SetPinned(prefs::kTabSearchPinnedToTabstrip, false),
      WaitForHide(kTabSearchButtonElementId),
      // Open organizer panel.
      Do([this]() { chrome::ExecuteCommand(browser(), IDC_TAB_SEARCH); }),
      WaitForShow(kOrganizerPanelElementId),
      // Button should be visible ephemerally.
      WaitForShow(kTabSearchButtonElementId),
      // Pin tab search via context menu command.
      ExecuteCommand(IDC_TAB_SEARCH_TOGGLE_PIN),
      // Close organizer panel.
      PressButton(kTabSearchButtonElementId),
      WaitForHide(kOrganizerPanelElementId),
      // Button should still be visible because it is pinned.
      CheckView(kTabSearchButtonElementId,
                [](views::View* view) { return view->GetVisible(); }),
      CheckResult(
          [this]() {
            return browser()->GetProfile()->GetPrefs()->GetBoolean(
                prefs::kTabSearchPinnedToTabstrip);
          },
          true));
}

IN_PROC_BROWSER_TEST_F(TabStripComboButtonOrganizerPanelInteractiveUiTest,
                       PinTabSearchAfterClosingPanelWhileEphemeral) {
  RunTestSequence(
      // Ensure tab search is not pinned.
      SetPinned(prefs::kTabSearchPinnedToTabstrip, false),
      WaitForHide(kTabSearchButtonElementId),
      // Open organizer panel.
      Do([this]() { chrome::ExecuteCommand(browser(), IDC_TAB_SEARCH); }),
      WaitForShow(kOrganizerPanelElementId),
      WaitForShow(kTabSearchButtonElementId),
      // Close organizer panel.
      PressButton(kTabSearchButtonElementId),
      WaitForHide(kOrganizerPanelElementId),
      // Button should still be visible ephemerally. Pin via context menu.
      ExecuteCommand(IDC_TAB_SEARCH_TOGGLE_PIN),
      // Button should remain visible because it is pinned.
      CheckView(kTabSearchButtonElementId,
                [](views::View* view) { return view->GetVisible(); }),
      CheckResult(
          [this]() {
            return browser()->GetProfile()->GetPrefs()->GetBoolean(
                prefs::kTabSearchPinnedToTabstrip);
          },
          true));
}

IN_PROC_BROWSER_TEST_F(TabStripComboButtonOrganizerPanelInteractiveUiTest,
                       ContextMenuClosedWhilePanelIsOpenDoesNotHideButton) {
  RunTestSequence(
      // Ensure tab search is not pinned.
      SetPinned(prefs::kTabSearchPinnedToTabstrip, false),
      WaitForHide(kTabSearchButtonElementId),
      // Open organizer panel.
      Do([this]() { chrome::ExecuteCommand(browser(), IDC_TAB_SEARCH); }),
      WaitForShow(kOrganizerPanelElementId),
      WaitForShow(kTabSearchButtonElementId),
      // Simulate context menu closing without pinning while panel is open.
      WithView(kTabStripComboButtonElementId,
               [](TabStripComboButton* combo) {
                 combo->OnMenuClosedForTesting();
                 EXPECT_FALSE(
                     combo->hide_tab_search_timer_for_testing().IsRunning());
               }),
      // Button should remain visible while panel is open.
      CheckView(kTabSearchButtonElementId,
                [](views::View* view) { return view->GetVisible(); }));
}

IN_PROC_BROWSER_TEST_F(TabStripComboButtonOrganizerPanelInteractiveUiTest,
                       NotEphemeralWhenPanelInTray) {
  constexpr int kVerticalTabsRegionWidth =
      organizer_panel::kOrganizerPanelMinWidth -
      organizer_panel::kOrganizerPanelMinOverlap;
  RunTestSequence(
      SetPinned(prefs::kTabSearchPinnedToTabstrip, false),
      WaitForHide(kTabSearchButtonElementId),
      ResizeVerticalTabsRegionToWidth(kVerticalTabsRegionWidth),
      Do([this]() { chrome::ExecuteCommand(browser(), IDC_TAB_SEARCH); }),
      WaitForShow(kOrganizerPanelElementId),
      // Button in vertical tab strip should NOT be shown because panel is in
      // tray.
      CheckView(kTabStripComboButtonElementId, [](TabStripComboButton* combo) {
        return !combo->end_button() || !combo->end_button()->GetVisible();
      }));
}

class TabStripComboButtonHorizontalInteractiveUiTest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  TabStripComboButtonHorizontalInteractiveUiTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          {organizer_panel::kOrganizerPanel, tabs::kTabStripUnification}, {});
    } else {
      scoped_feature_list_.InitWithFeatures({organizer_panel::kOrganizerPanel},
                                            {tabs::kTabStripUnification});
    }
  }
  ~TabStripComboButtonHorizontalInteractiveUiTest() override = default;

  auto SetPinned(const char* pref, bool pinned) {
    return Do([this, pref, pinned]() {
      browser()->GetProfile()->GetPrefs()->SetBoolean(pref, pinned);
    });
  }

  auto CheckTabSearchIcon(const gfx::VectorIcon& expected_icon) {
    return CheckView(
        kTabSearchButtonElementId,
        [&expected_icon](TabStripFlatEdgeButton* button) {
          const auto& image_model =
              button->GetImageModel(views::Button::STATE_NORMAL);
          return image_model.has_value() && image_model->IsVectorIcon() &&
                 image_model->GetVectorIcon().vector_icon() == &expected_icon;
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(TabStripComboButtonHorizontalInteractiveUiTest,
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

IN_PROC_BROWSER_TEST_P(TabStripComboButtonHorizontalInteractiveUiTest,
                       TabSearchUsesVerticalTabStripIcon) {
  const gfx::VectorIcon& expected_icon = features::IsRoundedIconsEnabled()
                                             ? kManageSearchIcon
                                             : kTabSearchTabStripOldIcon;
  RunTestSequence(SetPinned(prefs::kTabSearchPinnedToTabstrip, true),
                  WaitForShow(kTabSearchButtonElementId),
                  CheckTabSearchIcon(expected_icon));
}

INSTANTIATE_TEST_SUITE_P(All,
                         TabStripComboButtonHorizontalInteractiveUiTest,
                         testing::Bool());

}  // namespace
