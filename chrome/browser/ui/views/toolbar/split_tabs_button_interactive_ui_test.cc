// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
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
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/test/split_view_interactive_test_mixin.h"
#include "chrome/browser/ui/views/toolbar/split_tabs_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_accessibility_test.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tabs/public/split_tab_collection.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/common/content_features.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"
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
    if (selection.active_tab_changed() ||
        change.type() == TabStripModelChange::kMoved) {
      OnStateObserverStateChanged(true);
    }
  }

 private:
  raw_ptr<TabStripModel> tab_strip_model_;
};

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents1Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents2Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents3Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kToolbarWebContentsId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kToolbarWebViewId);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ActiveTabObserver, kActiveTabChanged);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<int>,
                                    kTabCountState);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingElementStateObserver<bool>,
                                    kHasSplitTabsAxNode);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
    ui::test::PollingElementStateObserver<std::u16string>,
    kSplitTabButtonNameState);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
    ui::test::PollingElementStateObserver<ax::mojom::Role>,
    kSplitTabButtonRoleState);
}  // namespace

class SplitTabButtonInteractiveTest
    : public SplitViewInteractiveTestMixin<ToolbarAccessibilityTest> {
 public:
  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    std::vector<base::test::FeatureRefAndParams> features;
    if (GetParam()) {
      features.push_back({::features::kInitialWebUI, {}});
      features.push_back({::features::kWebUIReloadButton, {}});
      features.push_back({::features::kWebUISplitTabsButton, {}});
    }
    return features;
  }

  void SetUpOnMainThread() override {
    SplitViewInteractiveTestMixin::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    WaitForInitialWebUI();

    if (GetParam()) {
      RunTestSequence(
          WaitForShow(kWebUIToolbarElementIdentifier),
          WithView(kWebUIToolbarElementIdentifier,
                   [](WebUIToolbarWebView* view) {
                     view->GetWebViewForTesting()->SetProperty(
                         views::kElementIdentifierKey, kToolbarWebViewId);
                   }),
          InstrumentNonTabWebView(kToolbarWebContentsId, kToolbarWebViewId));
    }

    ConfigureAccessibilityForWebUITest(GetParam());
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

  std::string GetWebUIIconName(const gfx::VectorIcon& icon) {
    if (&icon == &kSplitSceneIcon) {
      return "split-tabs-button:split-scene";
    }
    if (&icon == &kSplitSceneLeftIcon) {
      return "split-tabs-button:split-scene-left";
    }
    if (&icon == &kSplitSceneRightIcon) {
      return "split-tabs-button:split-scene-right";
    }
    if (&icon == &kSplitSceneUpIcon) {
      return "split-tabs-button:split-scene-up";
    }
    if (&icon == &kSplitSceneDownIcon) {
      return "split-tabs-button:split-scene-down";
    }
    return "";
  }

  auto CheckSplitTabButtonIcon(const gfx::VectorIcon& expected_icon) {
    if (GetParam()) {
      return Steps(
          WaitForJsResultAt(kToolbarWebContentsId,
                            {"toolbar-app", "split-tabs-button", "#button"},
                            "async (btn) => { "
                            "  await btn.updateComplete;"
                            "  return btn.getAttribute('iron-icon') || '';"
                            "}",
                            GetWebUIIconName(expected_icon)));
    }
    return Steps(CheckView(
        kToolbarSplitTabsToolbarButtonElementId,
        [](SplitTabsToolbarButton* button) {
          auto vector_icons = button->GetIconsForTesting();
          CHECK(vector_icons.has_value());
          return vector_icons->icon.name;
        },
        expected_icon.name));
  }

  auto CheckSplitTabButtonStrings(int string_id) {
    std::u16string string = l10n_util::GetStringUTF16(string_id);
    if (GetParam()) {
      return Steps(
          WaitForJsResultAt(kToolbarWebContentsId,
                            {"toolbar-app", "split-tabs-button", "#button"},
                            "async (btn) => { "
                            "  await btn.updateComplete;"
                            "  return btn.getAttribute('aria-label') || '';"
                            "}",
                            base::UTF16ToUTF8(string)));
    }
    return Steps(
        PollElement(kSplitTabButtonNameState,
                    kToolbarSplitTabsToolbarButtonElementId,
                    base::BindRepeating([](const ui::TrackedElement* el) {
                      return el->AsA<views::TrackedElementViews>()
                          ->view()
                          ->GetViewAccessibility()
                          .GetCachedName();
                    })),
        WaitForState(kSplitTabButtonNameState, string),
        StopObservingState(kSplitTabButtonNameState));
  }

  auto CheckSplitTabButtonRole(ax::mojom::Role role) {
    return Steps(PollElement(kSplitTabButtonRoleState,
                             kToolbarSplitTabsToolbarButtonElementId,
                             [role](const ui::TrackedElement* el) {
                               return GetSplitTabsAXRole(el, role);
                             }),
                 WaitForState(kSplitTabButtonRoleState, role),
                 StopObservingState(kSplitTabButtonRoleState));
  }

  auto WaitForTabCount(int expected_count) {
    return Steps(
        PollState(kTabCountState,
                  [this]() { return browser()->tab_strip_model()->count(); }),
        WaitForState(kTabCountState, expected_count),
        StopObservingState(kTabCountState));
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
    return Steps(MoveMouseToElement(kToolbarSplitTabsToolbarButtonElementId),
                 ClickMouse(ui_controls::RIGHT));
  }

  auto CheckMenuHistogram(SplitTabMenuModel::CommandId command_id) {
    return Do([this, command_id] {
      histogram_tester().ExpectBucketCount("Tabs.SplitViewMenu.ToolbarButton",
                                           command_id, 1);
    });
  }

  static std::u16string GetSplitTabsButtonName() {
    return l10n_util::GetStringUTF16(
        IDS_ACCNAME_SPLIT_TABS_TOOLBAR_BUTTON_PINNED);
  }

  static std::u16string GetSplitTabsButtonEnabledName() {
    return l10n_util::GetStringUTF16(
        IDS_ACCNAME_SPLIT_TABS_TOOLBAR_BUTTON_ENABLED);
  }

  static ax::mojom::Role GetSplitTabsAXRole(const ui::TrackedElement* el,
                                            ax::mojom::Role role) {
    if (auto* const view_el = el->AsA<views::TrackedElementViews>()) {
      return view_el->view()->GetViewAccessibility().GetCachedRole();
    }
    for (const std::u16string& name :
         {GetSplitTabsButtonName(), GetSplitTabsButtonEnabledName()}) {
      if (ui::AXNode* node =
              ToolbarAccessibilityTest::GetAXNode(el, role, name)) {
        return node->data().role;
      }
    }
    return ax::mojom::Role::kNone;
  }

  auto ClickSplitTabButton() {
    return Steps(PressButton(kToolbarSplitTabsToolbarButtonElementId));
  }

  auto WaitForAXNode() {
    return Steps(
        PollElement(
            kHasSplitTabsAxNode, kToolbarSplitTabsToolbarButtonElementId,
            base::BindRepeating(
                [](SplitTabButtonInteractiveTest* test,
                   const ui::TrackedElement* el) {
                  const bool is_split =
                      test->browser()->tab_strip_model()->GetActiveTab() &&
                      test->browser()
                          ->tab_strip_model()
                          ->GetActiveTab()
                          ->IsSplit();
                  const ax::mojom::Role role =
                      is_split ? ax::mojom::Role::kPopUpButton
                               : ax::mojom::Role::kButton;
                  const std::u16string name =
                      is_split ? GetSplitTabsButtonEnabledName()
                               : GetSplitTabsButtonName();

                  if (auto* const view_el =
                          el->AsA<views::TrackedElementViews>()) {
                    auto& view_accessibility =
                        view_el->view()->GetViewAccessibility();
                    return view_accessibility.GetCachedRole() == role &&
                           view_accessibility.GetCachedName() == name;
                  }
                  return !!ToolbarAccessibilityTest::GetAXNode(el, role, name);
                },
                base::Unretained(this))),
        WaitForState(kHasSplitTabsAxNode, true),
        StopObservingState(kHasSplitTabsAxNode));
  }

 private:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveTest, PinSplitTabButton) {
  RunTestSequence(
      EnsureNotPresent(kToolbarSplitTabsToolbarButtonElementId),
      UpdateSplitTabButtonPinState(true),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(), UpdateSplitTabButtonPinState(false),
      WaitForHide(kToolbarSplitTabsToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveTest,
                       UnpinSplitTabWhileActive) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(
      InstrumentTab(kWebContents1Id),
      NavigateWebContents(kWebContents1Id, url1),
      AddInstrumentedTab(kWebContents2Id, url1),
      AddInstrumentedTab(kWebContents3Id, url1),
      UpdateSplitTabButtonPinState(true),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(), SelectTab(kTabStripElementId, 0),
      EnterSplitView(0, 1),
      EnsurePresent(kToolbarSplitTabsToolbarButtonElementId),
      UpdateSplitTabButtonPinState(false),
      EnsurePresent(kToolbarSplitTabsToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveTest, PinButtonWithMenu) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(
      AddInstrumentedTab(kWebContents2Id, url1),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(),
      // The split tab button should be unpinned and the menu should reflect
      // that.
      MayInvolveNativeContextMenu(
          RightClickSplitTabsButton(),
          WaitForShow(kPinnedActionToolbarCustomizeElementId),
          CheckSplitTabButtonPinState(false),
          CheckMenuString(kPinnedActionToolbarPinElementId,
                          IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_PIN),
          CheckMenuIcon(kPinnedActionToolbarPinElementId, kKeepIcon),
          SelectMenuItem(kPinnedActionToolbarPinElementId)),
      WaitForHide(kPinnedActionToolbarPinElementId),
      // Verify that the split tab button is pinned.
      MayInvolveNativeContextMenu(
          RightClickSplitTabsButton(),
          WaitForShow(kPinnedActionToolbarCustomizeElementId),
          CheckSplitTabButtonPinState(true),
          CheckMenuString(kPinnedActionToolbarUnpinElementId,
                          IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_UNPIN),
          CheckMenuIcon(kPinnedActionToolbarUnpinElementId, kKeepOffIcon),
          SelectMenuItem(kPinnedActionToolbarUnpinElementId)));
}

IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveTest, UnpinButtonWithMenu) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(
      AddInstrumentedTab(kWebContents2Id, url1),
      UpdateSplitTabButtonPinState(true),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(), SelectTab(kTabStripElementId, 0),
      EnterSplitView(0, 1),
      // Unpin the split tab button with the menu item
      MayInvolveNativeContextMenu(
          RightClickSplitTabsButton(),
          WaitForShow(kPinnedActionToolbarUnpinElementId),
          SelectMenuItem(kPinnedActionToolbarUnpinElementId)),
      WaitForHide(kPinnedActionToolbarUnpinElementId),
      // Verify the button is now unpinned and the menu should have
      // the pin option
      MayInvolveNativeContextMenu(
          RightClickSplitTabsButton(),
          WaitForShow(kPinnedActionToolbarPinElementId),
          CheckSplitTabButtonPinState(false),
          CheckMenuString(kPinnedActionToolbarPinElementId,
                          IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_PIN),
          CheckMenuIcon(kPinnedActionToolbarPinElementId, kKeepIcon)));
}

IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveTest,
                       OpenCustomizedChromeSidePanel) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(
      AddInstrumentedTab(kWebContents2Id, url1),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(),
      MayInvolveNativeContextMenu(
          RightClickSplitTabsButton(),
          WaitForShow(kPinnedActionToolbarCustomizeElementId),
          EnsureNotPresent(kCustomizeChromeSidePanelWebViewElementId),
          SelectMenuItem(kPinnedActionToolbarCustomizeElementId)),
      WaitForHide(kPinnedActionToolbarCustomizeElementId),
      WaitForShow(kCustomizeChromeSidePanelWebViewElementId));
}

IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveTest, DefaultButtonIcon) {
  RunTestSequence(
      UpdateSplitTabButtonPinState(true),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), CheckSplitTabButtonIcon(kSplitSceneIcon));
}

IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveTest, ButtonIconUpdates) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(
      InstrumentTab(kWebContents1Id),
      NavigateWebContents(kWebContents1Id, url1),
      AddInstrumentedTab(kWebContents2Id, url1),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(),
      CheckSplitTabButtonIcon(kSplitSceneLeftIcon),
      ObserveState(kActiveTabChanged, browser()->tab_strip_model()),
      FocusInactiveTabInSplit(), WaitForState(kActiveTabChanged, true),
      EnsurePresent(kToolbarSplitTabsToolbarButtonElementId),
      CheckSplitTabButtonIcon(kSplitSceneRightIcon));
}

IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveTest, EnterSplitView) {
  RunTestSequence(
      UpdateSplitTabButtonPinState(true),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(), WaitForTabCount(1),
      ClickSplitTabButton(), WaitForTabCount(2), CheckTabInSplit(0, true),
      CheckTabInSplit(1, true));
}

IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveTest, ToggleMenu) {
  RunTestSequence(
      UpdateSplitTabButtonPinState(true),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(), CheckTabInSplit(0, false),
      WaitForTabCount(1),
      // Since the active tab isn't in a split, the button press
      // should create an empty split tab.
      ClickSplitTabButton(), WaitForTabCount(2),
      // Pressing the button while we are in a split should open the
      // menu instead.
      ClickSplitTabButton(),
      MayInvolveNativeContextMenu(
          WaitForShow(SplitTabMenuModel::kReversePositionMenuItem),
          WaitForTabCount(2),
          // Click on the button again while the menu for the split
          // button is open and confirm it hides the menu.
          ClickSplitTabButton(),
          WaitForHide(SplitTabMenuModel::kReversePositionMenuItem)),
      WaitForTabCount(2));
}

IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveTest,
                       ReversePositionMenuItemUpdates) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(
      AddInstrumentedTab(kWebContents2Id, url1),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(), ClickSplitTabButton(),
      MayInvolveNativeContextMenu(
          WaitForShow(SplitTabMenuModel::kReversePositionMenuItem),
          CheckMenuString(SplitTabMenuModel::kReversePositionMenuItem,
                          IDS_SPLIT_TAB_REVERSE_VIEWS),
          CheckMenuIcon(SplitTabMenuModel::kReversePositionMenuItem,
                        kSplitSceneRightIcon),
          DismissContextMenu(kToolbarSplitTabsToolbarButtonElementId,
                             SplitTabMenuModel::kReversePositionMenuItem)),
      WaitForHide(SplitTabMenuModel::kReversePositionMenuItem),
      // Change the focus and reopen the menu
      ObserveState(kActiveTabChanged, browser()->tab_strip_model()),
      FocusInactiveTabInSplit(), WaitForState(kActiveTabChanged, true),
      ClickSplitTabButton(),
      WaitForShow(SplitTabMenuModel::kReversePositionMenuItem),
      CheckMenuString(SplitTabMenuModel::kReversePositionMenuItem,
                      IDS_SPLIT_TAB_REVERSE_VIEWS),
      CheckMenuIcon(SplitTabMenuModel::kReversePositionMenuItem,
                    kSplitSceneLeftIcon));
}

IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveTest, ReverseSplitTabPosition) {
  RunTestSequence(
      InstrumentTab(kWebContents1Id),
      AddInstrumentedTab(kWebContents2Id, GetTestUrl("/links.html")),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(),
      // The newly created split tab should be active
      CheckSplitTabButtonIcon(kSplitSceneLeftIcon),
      NavigateWebContents(kWebContents1Id, GetTestUrl()),
      // Reversing the tab positions should move the active tab to the left.
      ClickSplitTabButton(),
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

IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveTest, CloseLeftRightTabs) {
  RunTestSequence(
      InstrumentTab(kWebContents1Id),
      AddInstrumentedTab(kWebContents2Id, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(),
      // Open the button's context menu.
      ClickSplitTabButton(),
      WaitForShow(SplitTabMenuModel::kCloseStartTabMenuItem),
      // Selecting close left menu item should close the left tab
      EnsureNotPresent(SplitTabMenuModel::kCloseMenuItem),
      SelectMenuItem(SplitTabMenuModel::kCloseStartTabMenuItem),
      WaitForHide(SplitTabMenuModel::kCloseStartTabMenuItem),
      WaitForHide(kWebContents1Id),
      WaitForHide(kToolbarSplitTabsToolbarButtonElementId), WaitForTabCount(1),
      EnsurePresent(kWebContents2Id),
      CheckMenuHistogram(SplitTabMenuModel::CommandId::kCloseStartTab),
      // Create a new split with a third tab.
      AddInstrumentedTab(kWebContents3Id, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(), ClickSplitTabButton(),
      WaitForShow(SplitTabMenuModel::kCloseEndTabMenuItem),
      // Selecting close right menu item should close the right tab
      SelectMenuItem(SplitTabMenuModel::kCloseEndTabMenuItem),
      WaitForHide(SplitTabMenuModel::kCloseEndTabMenuItem),
      WaitForHide(kWebContents3Id),
      WaitForHide(kToolbarSplitTabsToolbarButtonElementId), WaitForTabCount(1),
      EnsurePresent(kWebContents2Id),
      CheckMenuHistogram(SplitTabMenuModel::CommandId::kCloseEndTab));
}

class SplitTabButtonInteractiveRTLTest : public SplitTabButtonInteractiveTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SplitTabButtonInteractiveTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(::switches::kForceUIDirection,
                                    ::switches::kForceDirectionRTL);
    command_line->AppendSwitchASCII(::switches::kForceTextDirection,
                                    ::switches::kForceDirectionRTL);
  }
};

IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveRTLTest,
                       CloseLeftRightTabsInRTL) {
  RunTestSequence(
      InstrumentTab(kWebContents1Id),
      AddInstrumentedTab(kWebContents2Id, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(),
      // Open the button's context menu.
      ClickSplitTabButton(),
      WaitForShow(SplitTabMenuModel::kCloseStartTabMenuItem),
      // Selecting close left menu item should close the left tab
      EnsureNotPresent(SplitTabMenuModel::kCloseMenuItem),
      SelectMenuItem(SplitTabMenuModel::kCloseStartTabMenuItem),
      WaitForHide(kWebContents2Id),
      WaitForHide(kToolbarSplitTabsToolbarButtonElementId), WaitForTabCount(1),
      EnsurePresent(kWebContents1Id),
      CheckMenuHistogram(SplitTabMenuModel::CommandId::kCloseStartTab),
      // Create a new split with a third tab.
      AddInstrumentedTab(kWebContents3Id, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(), ClickSplitTabButton(),
      WaitForShow(SplitTabMenuModel::kCloseEndTabMenuItem),
      // Selecting close right menu item should close the right tab
      SelectMenuItem(SplitTabMenuModel::kCloseEndTabMenuItem),
      WaitForHide(kWebContents1Id),
      WaitForHide(kToolbarSplitTabsToolbarButtonElementId), WaitForTabCount(1),
      EnsurePresent(kWebContents3Id),
      CheckMenuHistogram(SplitTabMenuModel::CommandId::kCloseEndTab));
}

IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveTest, ExitSplit) {
  RunTestSequence(
      AddInstrumentedTab(kWebContents2Id, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(),
      // Open the button's context menu.
      ClickSplitTabButton(),
      WaitForShow(SplitTabMenuModel::kReversePositionMenuItem),
      CheckTabInSplit(0, true), CheckTabInSplit(1, true),
      // The split tabs should be separated after selecting the menu item.
      SelectMenuItem(SplitTabMenuModel::kExitSplitMenuItem),
      WaitForHide(SplitTabMenuModel::kExitSplitMenuItem),
      WaitForHide(kToolbarSplitTabsToolbarButtonElementId),
      CheckTabInSplit(0, false), CheckTabInSplit(1, false),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->active_index(); }, 0),
      CheckMenuHistogram(SplitTabMenuModel::CommandId::kExitSplit));
}

IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveTest, ButtonUpdatesOnSplit) {
  RunTestSequence(
      AddInstrumentedTab(kWebContents2Id, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      If([param = GetParam()]() { return param; },
         Then(ScreenshotWebUi(kToolbarWebContentsId,
                              {"toolbar-app", "split-tabs-button"},
                              "SplitTabButton", "6628632")),
         Else(Screenshot(kToolbarSplitTabsToolbarButtonElementId,
                         "SplitTabButton", "6628632"))));
}

IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveTest, A11y) {
  RunTestSequence(
      UpdateSplitTabButtonPinState(true),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(),
      CheckSplitTabButtonStrings(IDS_ACCNAME_SPLIT_TABS_TOOLBAR_BUTTON_PINNED),
      CheckSplitTabButtonRole(ax::mojom::Role::kButton), ClickSplitTabButton(),
      CheckSplitTabButtonStrings(IDS_ACCNAME_SPLIT_TABS_TOOLBAR_BUTTON_ENABLED),
      CheckSplitTabButtonRole(ax::mojom::Role::kPopUpButton));
}

// Regression test for https://crbug.com/495303521
IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveTest,
                       MenuHidesOnActiveTabChange) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(
      UpdateSplitTabButtonPinState(true),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(),
      AddInstrumentedTab(kWebContents1Id, url1),
      AddInstrumentedTab(kWebContents2Id, url1),
      AddInstrumentedTab(kWebContents3Id, url1),
      SelectTab(kTabStripElementId, 1), EnterSplitView(1, 2),
      ClickSplitTabButton(),
      WaitForShow(SplitTabMenuModel::kReversePositionMenuItem),
      SelectTab(kTabStripElementId, 0),
      WaitForHide(SplitTabMenuModel::kReversePositionMenuItem));
}

IN_PROC_BROWSER_TEST_P(SplitTabButtonInteractiveTest, AccessibilityNode) {
  GURL url = GetTestUrl();
  RunTestSequence(
      InstrumentTab(kWebContents1Id), NavigateWebContents(kWebContents1Id, url),
      UpdateSplitTabButtonPinState(true),
      WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
      WaitForElementNonzeroSize(kToolbarSplitTabsToolbarButtonElementId),
      WaitForAXNode(), DoWaitForLayout(),
      CheckSplitTabButtonRole(ax::mojom::Role::kButton),
      CheckSplitTabButtonStrings(IDS_ACCNAME_SPLIT_TABS_TOOLBAR_BUTTON_PINNED));

  // Also test pop-up state
  RunTestSequence(ClickSplitTabButton(), WaitForTabCount(2), WaitForAXNode(),
                  CheckSplitTabButtonRole(ax::mojom::Role::kPopUpButton),
                  CheckSplitTabButtonStrings(
                      IDS_ACCNAME_SPLIT_TABS_TOOLBAR_BUTTON_ENABLED));
}

INSTANTIATE_TEST_SUITE_P(All, SplitTabButtonInteractiveTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         SplitTabButtonInteractiveRTLTest,
                         testing::Bool());
