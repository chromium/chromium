// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/split_tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/test/split_view_interactive_test_mixin.h"
#include "chrome/browser/ui/views/toolbar/split_tabs_button.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "url/gurl.h"

namespace {
class ActiveTabObserver : public TabStripModelObserver,
                          public ui::test::StateObserver<bool> {
 public:
  explicit ActiveTabObserver(TabStripModel* tab_strip_model)
      : tab_strip_model_(tab_strip_model) {
    tab_strip_model_->AddObserver(this);
  }

  ~ActiveTabObserver() override { tab_strip_model_->RemoveObserver(this); }

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kMoved) {
      OnStateObserverStateChanged(true);
    }
  }

 private:
  raw_ptr<TabStripModel> tab_strip_model_;
};

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents1Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents2Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents3Id);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ActiveTabObserver, kActiveTabChanged);
}  // namespace

class SplitTabButtonInteractiveTest
    : public SplitViewInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  void SetUpOnMainThread() override {
    SplitViewInteractiveTestMixin::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetTestUrl(std::string relative_url = "/title1.html") const {
    return embedded_test_server()->GetURL(relative_url);
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  auto UpdateSplitTabButtonPinState(bool should_pin) {
    return Do([=, this]() {
      browser()->profile()->GetPrefs()->SetBoolean(prefs::kPinSplitTabButton,
                                                   should_pin);
    });
  }

  auto CheckSplitTabButtonIcon(const gfx::VectorIcon& expected_icon) {
    return CheckView(
        kToolbarSplitTabsToolbarButtonElementId,
        [](SplitTabsToolbarButton* button) {
          auto vector_icons = button->GetIconsForTesting();
          CHECK(vector_icons.has_value());
          return vector_icons->icon.name;
        },
        expected_icon.name);
  }

  auto CheckSplitTabButtonStrings(int string_id) {
    std::u16string string = l10n_util::GetStringUTF16(string_id);
    return Steps(CheckView(
                     kToolbarSplitTabsToolbarButtonElementId,
                     [](SplitTabsToolbarButton* button) {
                       return button->GetTooltipText();
                     },
                     string),
                 CheckView(
                     kToolbarSplitTabsToolbarButtonElementId,
                     [](SplitTabsToolbarButton* button) {
                       return button->GetAccessibleName();
                     },
                     string));
  }

  auto CheckSplitTabButtonRole(ax::mojom::Role role) {
    return CheckView(
        kToolbarSplitTabsToolbarButtonElementId,
        [](SplitTabsToolbarButton* button) {
          return button->GetAccessibleRole();
        },
        role);
  }

  auto CheckTabCount(int expected_count) {
    return CheckResult(
        [this]() { return browser()->tab_strip_model()->count(); },
        expected_count);
  }

  auto CheckTabInSplit(int tab_index, bool expected_split_state) {
    return CheckResult(
        [=, this]() {
          tabs::TabInterface* const tab =
              browser()->tab_strip_model()->GetTabAtIndex(tab_index);
          return tab->IsSplit();
        },
        expected_split_state);
  }

  auto CheckMenuString(ElementSpecifier identifier, int expected_string_id) {
    return CheckView(
        identifier,
        [](views::MenuItemView* menu_item_view) {
          return menu_item_view->title();
        },
        l10n_util::GetStringUTF16(expected_string_id));
  }

  auto CheckMenuIcon(ElementSpecifier identifier, const gfx::VectorIcon& icon) {
    return CheckView(
        identifier,
        [](views::MenuItemView* menu_item_view) {
          return menu_item_view->GetIcon().GetVectorIcon().vector_icon()->name;
        },
        icon.name);
  }

  auto ClickActiveTabInSplit() {
    return Steps(MoveMouseTo(base::BindLambdaForTesting([this]() {
                   return multi_contents_view()
                       ->GetActiveContentsView()
                       ->GetBoundsInScreen()
                       .CenterPoint();
                 })),
                 ClickMouse());
  }

  auto CheckSplitTabButtonPinState(bool should_pin) {
    return CheckResult(
        [=, this]() {
          return browser()->profile()->GetPrefs()->GetBoolean(
              prefs::kPinSplitTabButton);
        },
        should_pin);
  }

  auto RightClickSplitTabsButton() {
    return Steps(MoveMouseTo(kToolbarSplitTabsToolbarButtonElementId),
                 ClickMouse(ui_controls::RIGHT));
  }

  auto NamePinStateMenuItem(std::string_view name, int menu_index) {
    return NameViewRelative(SplitTabsToolbarButton::kUpdatePinStateMenu, name,
                            [=](views::SubmenuView* root_item) {
                              return root_item->GetMenuItemAt(menu_index);
                            });
  }

  auto CheckMenuHistogram(SplitTabMenuModel::CommandId command_id) {
    return Do([this, command_id] {
      histogram_tester().ExpectBucketCount("Tabs.SplitViewMenu.ToolbarButton",
                                           command_id, 1);
    });
  }

 private:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest, PinSplitTabButton) {
  RunTestSequence(EnsureNotPresent(kToolbarSplitTabsToolbarButtonElementId),
                  UpdateSplitTabButtonPinState(true),
                  WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
                  UpdateSplitTabButtonPinState(false),
                  WaitForHide(kToolbarSplitTabsToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest,
                       UnpinSplitTabWhileActive) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(InstrumentTab(kWebContents1Id),
                  NavigateWebContents(kWebContents1Id, url1),
                  AddInstrumentedTab(kWebContents2Id, url1),
                  AddInstrumentedTab(kWebContents3Id, url1),
                  UpdateSplitTabButtonPinState(true),
                  WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
                  SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
                  EnsurePresent(kToolbarSplitTabsToolbarButtonElementId),
                  UpdateSplitTabButtonPinState(false),
                  EnsurePresent(kToolbarSplitTabsToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest, PinButtonWithMenu) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  constexpr char kPinMenuItem[] = "pin_menu_item";
  constexpr char kUnpinMenuItem[] = "unpin_menu_item";
  RunTestSequence(
      AddInstrumentedTab(kWebContents2Id, url1),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      // The split tab button should be unpinned and the menu should reflect
      // that.
      MayInvolveNativeContextMenu(
          RightClickSplitTabsButton(),
          WaitForShow(SplitTabsToolbarButton::kUpdatePinStateMenu),
          CheckSplitTabButtonPinState(false),
          NamePinStateMenuItem(kPinMenuItem, 0),
          CheckMenuString(kPinMenuItem,
                          IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_PIN),
          CheckMenuIcon(kPinMenuItem, kKeepIcon), SelectMenuItem(kPinMenuItem)),
      WaitForHide(kPinMenuItem),
      // Verify that the split tab button is pinned.
      MayInvolveNativeContextMenu(
          RightClickSplitTabsButton(),
          WaitForShow(SplitTabsToolbarButton::kUpdatePinStateMenu),
          CheckSplitTabButtonPinState(true),
          NamePinStateMenuItem(kUnpinMenuItem, 1),
          CheckMenuString(kUnpinMenuItem,
                          IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_UNPIN),
          CheckMenuIcon(kUnpinMenuItem, kKeepOffIcon)));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest, UnpinButtonWithMenu) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  constexpr char kPinMenuItem[] = "pin_menu_item";
  constexpr char kUnpinMenuItem[] = "unpin_menu_item";
  RunTestSequence(AddInstrumentedTab(kWebContents2Id, url1),
                  UpdateSplitTabButtonPinState(true),
                  WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
                  SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
                  // Unpin the split tab button with the menu item
                  MayInvolveNativeContextMenu(
                      RightClickSplitTabsButton(),
                      WaitForShow(SplitTabsToolbarButton::kUpdatePinStateMenu),
                      NamePinStateMenuItem(kUnpinMenuItem, 1),
                      SelectMenuItem(kUnpinMenuItem)),
                  WaitForHide(kUnpinMenuItem),
                  // Verify the button is now unpinned and the menu should have
                  // the pin option
                  MayInvolveNativeContextMenu(
                      RightClickSplitTabsButton(),
                      WaitForShow(SplitTabsToolbarButton::kUpdatePinStateMenu),
                      NamePinStateMenuItem(kPinMenuItem, 0),
                      CheckSplitTabButtonPinState(false),
                      CheckMenuString(kPinMenuItem,
                                      IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_PIN),
                      CheckMenuIcon(kPinMenuItem, kKeepIcon)));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest,
                       OpenCustomizedChromeSidePanel) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  constexpr char kCustomizeChromeMenuItem[] = "customize_chrome_menu_item";
  RunTestSequence(
      AddInstrumentedTab(kWebContents2Id, url1),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      MayInvolveNativeContextMenu(
          RightClickSplitTabsButton(),
          WaitForShow(SplitTabsToolbarButton::kUpdatePinStateMenu),
          NamePinStateMenuItem(kCustomizeChromeMenuItem, 2),
          EnsureNotPresent(kCustomizeChromeSidePanelWebViewElementId),
          SelectMenuItem(kCustomizeChromeMenuItem)),
      WaitForHide(SplitTabsToolbarButton::kUpdatePinStateMenu),
      WaitForShow(kCustomizeChromeSidePanelWebViewElementId));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest, DefaultButtonIcon) {
  RunTestSequence(UpdateSplitTabButtonPinState(true),
                  WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
                  CheckSplitTabButtonIcon(kSplitSceneIcon));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest, ButtonIconUpdates) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(InstrumentTab(kWebContents1Id),
                  NavigateWebContents(kWebContents1Id, url1),
                  AddInstrumentedTab(kWebContents2Id, url1),
                  SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
                  WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
                  CheckSplitTabButtonIcon(kSplitSceneLeftIcon),
                  FocusInactiveTabInSplit(),
                  EnsurePresent(kToolbarSplitTabsToolbarButtonElementId),
                  CheckSplitTabButtonIcon(kSplitSceneRightIcon));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest, EnterSplitView) {
  RunTestSequence(
      UpdateSplitTabButtonPinState(true),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId), CheckTabCount(1),
      PressButton(kToolbarSplitTabsToolbarButtonElementId), CheckTabCount(2),
      CheckTabInSplit(0, true), CheckTabInSplit(1, true));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest, ToggleMenu) {
  RunTestSequence(
      UpdateSplitTabButtonPinState(true),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      CheckTabInSplit(0, false),
      // Since the active tab isn't in a split, the button press
      // should create an empty split tab.
      PressButton(kToolbarSplitTabsToolbarButtonElementId), CheckTabCount(2),
      // Pressing the button while we are in a split should open the
      // menu instead.
      PressButton(kToolbarSplitTabsToolbarButtonElementId),
      WaitForShow(SplitTabMenuModel::kReversePositionMenuItem),
      CheckTabCount(2),
      // Click on the button again while the menu for the split
      // button is open and confirm it hides the menu.
      MoveMouseTo(kToolbarSplitTabsToolbarButtonElementId), ClickMouse(),
      WaitForHide(SplitTabMenuModel::kReversePositionMenuItem),
      CheckTabCount(2));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest,
                       ReversePositionMenuItemUpdates) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(AddInstrumentedTab(kWebContents2Id, url1),
                  SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
                  WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
                  PressButton(kToolbarSplitTabsToolbarButtonElementId),
                  WaitForShow(SplitTabMenuModel::kReversePositionMenuItem),
                  CheckMenuString(SplitTabMenuModel::kReversePositionMenuItem,
                                  IDS_SPLIT_TAB_REVERSE_VIEWS),
                  CheckMenuIcon(SplitTabMenuModel::kReversePositionMenuItem,
                                kSplitSceneRightIcon),
                  ClickActiveTabInSplit(),
                  WaitForHide(SplitTabMenuModel::kReversePositionMenuItem),
                  // Change the focus and reopen the menu
                  FocusInactiveTabInSplit(),
                  PressButton(kToolbarSplitTabsToolbarButtonElementId),
                  WaitForShow(SplitTabMenuModel::kReversePositionMenuItem),
                  CheckMenuString(SplitTabMenuModel::kReversePositionMenuItem,
                                  IDS_SPLIT_TAB_REVERSE_VIEWS),
                  CheckMenuIcon(SplitTabMenuModel::kReversePositionMenuItem,
                                kSplitSceneLeftIcon));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest, ReverseSplitTabPosition) {
  RunTestSequence(
      InstrumentTab(kWebContents1Id),
      AddInstrumentedTab(kWebContents2Id, GetTestUrl("/links.html")),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      // The newly created split tab should be active
      CheckSplitTabButtonIcon(kSplitSceneLeftIcon),
      NavigateWebContents(kWebContents1Id, GetTestUrl()),
      // Reversing the tab positions should move the active tab to the left.
      PressButton(kToolbarSplitTabsToolbarButtonElementId),
      WaitForShow(SplitTabMenuModel::kReversePositionMenuItem),
      ObserveState(kActiveTabChanged, browser()->tab_strip_model()),
      SelectMenuItem(SplitTabMenuModel::kReversePositionMenuItem),
      WaitForState(kActiveTabChanged, true),
      CheckSplitTabButtonIcon(kSplitSceneRightIcon),
      CheckResult(
          [this]() {
            TabStripModel* const tab_strip_model = browser()->tab_strip_model();
            return tab_strip_model
                ->GetWebContentsAt(tab_strip_model->active_index())
                ->GetURL();
          },
          GetTestUrl()),
      CheckMenuHistogram(SplitTabMenuModel::CommandId::kReversePosition));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest, CloseLeftRightTabs) {
  RunTestSequence(
      InstrumentTab(kWebContents1Id),
      AddInstrumentedTab(kWebContents2Id, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      // Open the button's context menu.
      PressButton(kToolbarSplitTabsToolbarButtonElementId),
      WaitForShow(SplitTabMenuModel::kCloseStartTabMenuItem),
      // Selecting close left menu item should close the left tab
      EnsureNotPresent(SplitTabMenuModel::kCloseMenuItem),
      SelectMenuItem(SplitTabMenuModel::kCloseStartTabMenuItem),
      WaitForHide(kWebContents1Id),
      WaitForHide(kToolbarSplitTabsToolbarButtonElementId), CheckTabCount(1),
      EnsurePresent(kWebContents2Id),
      CheckMenuHistogram(SplitTabMenuModel::CommandId::kCloseStartTab),
      // Create a new split with a third tab.
      AddInstrumentedTab(kWebContents3Id, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      PressButton(kToolbarSplitTabsToolbarButtonElementId),
      WaitForShow(SplitTabMenuModel::kCloseEndTabMenuItem),
      // Selecting close right menu item should close the right tab
      SelectMenuItem(SplitTabMenuModel::kCloseEndTabMenuItem),
      WaitForHide(kWebContents3Id),
      WaitForHide(kToolbarSplitTabsToolbarButtonElementId), CheckTabCount(1),
      EnsurePresent(kWebContents2Id),
      CheckMenuHistogram(SplitTabMenuModel::CommandId::kCloseEndTab));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest, CloseLeftRightTabsInRTL) {
  base::i18n::SetRTLForTesting(true);

  RunTestSequence(
      InstrumentTab(kWebContents1Id),
      AddInstrumentedTab(kWebContents2Id, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      // Open the button's context menu.
      PressButton(kToolbarSplitTabsToolbarButtonElementId),
      WaitForShow(SplitTabMenuModel::kCloseStartTabMenuItem),
      // Selecting close left menu item should close the left tab
      EnsureNotPresent(SplitTabMenuModel::kCloseMenuItem),
      SelectMenuItem(SplitTabMenuModel::kCloseStartTabMenuItem),
      WaitForHide(kWebContents2Id),
      WaitForHide(kToolbarSplitTabsToolbarButtonElementId), CheckTabCount(1),
      EnsurePresent(kWebContents1Id),
      CheckMenuHistogram(SplitTabMenuModel::CommandId::kCloseStartTab),
      // Create a new split with a third tab.
      AddInstrumentedTab(kWebContents3Id, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      PressButton(kToolbarSplitTabsToolbarButtonElementId),
      WaitForShow(SplitTabMenuModel::kCloseEndTabMenuItem),
      // Selecting close right menu item should close the right tab
      SelectMenuItem(SplitTabMenuModel::kCloseEndTabMenuItem),
      WaitForHide(kWebContents1Id),
      WaitForHide(kToolbarSplitTabsToolbarButtonElementId), CheckTabCount(1),
      EnsurePresent(kWebContents3Id),
      CheckMenuHistogram(SplitTabMenuModel::CommandId::kCloseEndTab));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest, ExitSplit) {
  RunTestSequence(
      AddInstrumentedTab(kWebContents2Id, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      // Open the button's context menu.
      PressButton(kToolbarSplitTabsToolbarButtonElementId),
      WaitForShow(SplitTabMenuModel::kReversePositionMenuItem),
      CheckTabInSplit(0, true), CheckTabInSplit(1, true),
      // The split tabs should be separated after selecting the menu item.
      SelectMenuItem(SplitTabMenuModel::kExitSplitMenuItem),
      WaitForHide(kToolbarSplitTabsToolbarButtonElementId),
      CheckTabInSplit(0, false), CheckTabInSplit(1, false),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->active_index(); }, 0),
      CheckMenuHistogram(SplitTabMenuModel::CommandId::kExitSplit));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest, ButtonUpdatesOnSplit) {
  RunTestSequence(AddInstrumentedTab(kWebContents2Id, GetTestUrl()),
                  SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
                  WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
                  SetOnIncompatibleAction(
                      OnIncompatibleAction::kIgnoreAndContinue,
                      "Screenshot can only run in pixel_tests on Windows."),
                  Screenshot(kToolbarSplitTabsToolbarButtonElementId,
                             "SplitTabButton", "6628632"));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest, A11y) {
  RunTestSequence(
      UpdateSplitTabButtonPinState(true),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      CheckSplitTabButtonStrings(IDS_ACCNAME_SPLIT_TABS_TOOLBAR_BUTTON_PINNED),
      CheckSplitTabButtonRole(ax::mojom::Role::kButton),
      PressButton(kToolbarSplitTabsToolbarButtonElementId),
      CheckSplitTabButtonStrings(IDS_ACCNAME_SPLIT_TABS_TOOLBAR_BUTTON_ENABLED),
      CheckSplitTabButtonRole(ax::mojom::Role::kPopUpButton));
}
