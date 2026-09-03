// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/collaboration/public/features.h"
#include "components/contextual_tasks/public/features.h"
#include "components/data_sharing/public/features.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_class_properties.h"

namespace {


std::string GetPinnedButtonJS(
    toolbar_ui_api::mojom::PinnedToolbarAction action) {
  return base::StringPrintf(R"(
    ((action) => {
      const app = document.querySelector('toolbar-app');
      if (!app) return null;
      const pinnedToolbarActions =
        app.shadowRoot.querySelector('#pinnedToolbarActions');
      if (!pinnedToolbarActions) return null;
      const container = pinnedToolbarActions.shadowRoot;
      if (!container) return null;
      const actionEl =
        Array.from(container.querySelectorAll('pinned-toolbar-action'))
             .find(el => el.state && el.state.action === action);
      if (!actionEl) return null;
      return actionEl.shadowRoot.querySelector('cr-icon-button');
    })(%d)
  )",
                            static_cast<int>(action));
}

}  // namespace

class WebUIToolbarWebViewInteractiveTest : public InteractiveBrowserTest {
 public:
  WebUIToolbarWebViewInteractiveTest() {
    // Disable kExtensionsPinnedByDefault to prevent test extensions from being
    // pinned automatically upon installation. This avoids a DCHECK crash when
    // the test subsequently tries to pin them manually.
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kWebUILocationBar, features::kWebUIBackForwardButton,
         features::kWebUIAppMenuButton, features::kWebUIPinnedToolbarActions,
         features::kWebUIExtensionsContainer,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2},
        /*disabled_features=*/{features::kExtensionsPinnedByDefault});
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
  }

  auto CheckElementIsCentered(ui::ElementIdentifier webview_id,
                              const DeepQuery& query) {
    return CheckJsResultAt(
        webview_id, query,
        "el => Math.abs(el.getBoundingClientRect().top - (window.innerHeight "
        "- el.getBoundingClientRect().bottom)) <= 1",
        true);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewInteractiveTest,
                       LocationIconOpensPageInfo) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarWebViewId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInstrumentedWebViewId);
  WaitForInitialWebUIToolbar(browser());
  RunTestSequence(
      WaitForShow(kWebUIToolbarElementIdentifier),
      WithView(kWebUIToolbarElementIdentifier,
               [](WebUIToolbarWebView* parent) {
                 parent->GetWebViewForTesting()->SetProperty(
                     views::kElementIdentifierKey, kInstrumentedWebViewId);
               }),
      InstrumentNonTabWebView(kWebUIToolbarWebViewId, kInstrumentedWebViewId,
                              /*wait_for_ready=*/true),
      ExecuteJsAt(
          kWebUIToolbarWebViewId,
          DeepQuery{"toolbar-app", "location-bar", "location-icon", "#button"},
          "el => el.click()"),
      WaitForShow(PageInfoBubbleViewBase::kPageInfoBubbleElementIdentifier));
}

// Verifies the vertical layout alignment of the WebUI toolbar when maximized.
// In maximized/fullscreen mode:
// 1. The WebUIToolbarWebView should remain centered and limited to the content
// height.
// 2. All WebUI elements (Back, Reload, Location Bar, Pinned actions, App Menu)
//    should be vertically centered relative to the WebUI viewport.
// 3. The container itself is centered in the toolbar (y = (parent_height -
// height) / 2 in C++).
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewInteractiveTest,
                       MaximizedLayoutAlignment) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarWebViewId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInstrumentedWebViewId);

  // Make a test extension
  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(R"({
    "name": "Test Extension",
    "version": "1.0",
    "manifest_version": 3,
    "action": {}
  })");
  extensions::ChromeTestExtensionLoader loader(browser()->GetProfile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Pin the extension to make it visible in the toolbar.
  ToolbarActionsModel::Get(browser()->GetProfile())
      ->SetActionVisibility(extension->id(), true);

  PinnedToolbarActionsModel::Get(browser()->GetProfile())
      ->UpdatePinnedState(kActionPrint, true);
  WaitForInitialWebUIToolbar(browser());

  RunTestSequence(
      WaitForShow(kWebUIToolbarElementIdentifier),
      WithView(kWebUIToolbarElementIdentifier,
               [](WebUIToolbarWebView* parent) {
                 parent->GetWebViewForTesting()->SetProperty(
                     views::kElementIdentifierKey, kInstrumentedWebViewId);
                 parent->GetWidget()->Maximize();
               }),
      // Wait for the window to actually maximize.
      PollUntil(
          [this]() {
            return browser()->GetWindow() &&
                   browser()->GetWindow()->IsMaximized();
          },
          "Wait for window to maximize"),
      // Verify C++ side centering.
      CheckView(kWebUIToolbarElementIdentifier,
                [](WebUIToolbarWebView* view) {
                  return view->bounds().y() ==
                         (view->parent()->height() - view->bounds().height()) /
                             2;
                }),
      InstrumentNonTabWebView(kWebUIToolbarWebViewId, kInstrumentedWebViewId,
                              /*wait_for_ready=*/true),
      // Verify WebUI elements are vertically centered in the viewport.
      CheckElementIsCentered(kWebUIToolbarWebViewId,
                             DeepQuery{"toolbar-app", "#reload"}),
      CheckElementIsCentered(kWebUIToolbarWebViewId,
                             DeepQuery{"toolbar-app", "#back"}),
      CheckElementIsCentered(kWebUIToolbarWebViewId,
                             DeepQuery{"toolbar-app", "#location-bar"}),
      CheckElementIsCentered(kWebUIToolbarWebViewId,
                             DeepQuery{"toolbar-app", "#app-menu"}),
      CheckElementIsCentered(
          kWebUIToolbarWebViewId,
          DeepQuery{"toolbar-app", "#extensions", "webui-toolbar-extension"}),
      CheckElementIsCentered(kWebUIToolbarWebViewId,
                             DeepQuery{"toolbar-app", "#pinnedToolbarActions",
                                       "pinned-toolbar-action"}));
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewInteractiveTest, FocusReloadButton) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarWebViewId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInstrumentedWebViewId);
  WaitForInitialWebUIToolbar(browser());
  RunTestSequence(
      WaitForShow(kWebUIToolbarElementIdentifier),
      WithView(kWebUIToolbarElementIdentifier,
               [](WebUIToolbarWebView* parent) {
                 parent->GetWebViewForTesting()->SetProperty(
                     views::kElementIdentifierKey, kInstrumentedWebViewId);
               }),
      InstrumentNonTabWebView(kWebUIToolbarWebViewId, kInstrumentedWebViewId,
                              /*wait_for_ready=*/true),
      Do([this]() {
        chrome::BrowserCommandController::From(browser())->ExecuteCommand(
            IDC_FOCUS_TOOLBAR);
      }),
      CheckJsResultAt(kWebUIToolbarWebViewId, DeepQuery{},
                      "() => {"
                      "  let active = document.activeElement;"
                      "  while (active && active.shadowRoot && "
                      "active.shadowRoot.activeElement) {"
                      "    active = active.shadowRoot.activeElement;"
                      "  }"
                      "  return active ? active.ariaLabel : null;"
                      "}",
                      l10n_util::GetStringUTF8(IDS_ACCNAME_RELOAD)));
}

class WebUIPinnedToolbarActionsInteractiveTest : public InteractiveBrowserTest {
 public:
  WebUIPinnedToolbarActionsInteractiveTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIPinnedToolbarActions,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2,
         features::kWebUIReloadButton},
        {});
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    // Set tested actions as pinnable.
    auto* action_item = actions::ActionManager::Get().FindAction(
        kActionPrint, BrowserActions::From(browser())->root_action_item());
    if (action_item) {
      action_item->SetProperty(
          actions::kActionItemPinnableKey,
          static_cast<int>(actions::ActionPinnableState::kPinnable));
    }
    action_item = actions::ActionManager::Get().FindAction(
        kActionSidePanelShowBookmarks,
        BrowserActions::From(browser())->root_action_item());
    if (action_item) {
      action_item->SetProperty(
          actions::kActionItemPinnableKey,
          static_cast<int>(actions::ActionPinnableState::kPinnable));
    }

    model_ = PinnedToolbarActionsModel::Get(browser()->GetProfile());
  }

  void TearDownOnMainThread() override {
    model_ = nullptr;
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 protected:
  raw_ptr<PinnedToolbarActionsModel> model_ = nullptr;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsInteractiveTest,
                       RightClickPinnedAction) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarWebViewId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInstrumentedWebViewId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kMenuRunningState);

  actions::ActionId action_id1 = kActionPrint;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action1 =
      toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;

  actions::ActionId action_id2 = kActionSidePanelShowBookmarks;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action2 =
      toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowBookmarks;

  RunTestSequence(
      WaitForShow(kWebUIToolbarElementIdentifier),
      WithView(kWebUIToolbarElementIdentifier,
               [](WebUIToolbarWebView* parent) {
                 parent->GetWebViewForTesting()->SetProperty(
                     views::kElementIdentifierKey, kInstrumentedWebViewId);
               }),
      InstrumentNonTabWebView(kWebUIToolbarWebViewId, kInstrumentedWebViewId,
                              /*wait_for_ready=*/true),
      Do([this, action_id1, action_id2]() {
        model_->UpdatePinnedState(action_id1, true);
        model_->UpdatePinnedState(action_id2, true);
      }),
      WaitForJsResult(
          kWebUIToolbarWebViewId,
          base::StringPrintf(R"(
            () => {
              const btn1 = %s;
              const btn2 = %s;
              return !!btn1 && btn1.checkVisibility() &&
                     !!btn2 && btn2.checkVisibility();
            }
          )",
                             GetPinnedButtonJS(mojom_action1).c_str(),
                             GetPinnedButtonJS(mojom_action2).c_str()),
          true),

      PollState(kMenuRunningState,
                [this]() {
                  WebUIToolbarWebView* webui_toolbar_view =
                      GetWebUIToolbarWebView(browser());
                  WebUIPinnedToolbarActions* actions =
                      webui_toolbar_view->GetPinnedToolbarActions();
                  return actions->menu_runner_ &&
                         actions->menu_runner_->IsRunning();
                }),

      // Open context menu for first action.
      ExecuteJsAt(kWebUIToolbarWebViewId, DeepQuery{},
                  base::StringPrintf(R"(
        () => {
          const btn = %s;
          btn.dispatchEvent(
            new MouseEvent(
              'contextmenu', {button: 2, bubbles: true, composed: true}
            )
          );
        }
      )",
                                     GetPinnedButtonJS(mojom_action1).c_str())),

      // Wait for the menu to be running.
      WaitForState(kMenuRunningState, true),

      // Verify first action is highlighted.
      WaitForJsResult(
          kWebUIToolbarWebViewId,
          base::StringPrintf(R"(
            () => {
              const btn = %s;
              return !!btn && btn.hasAttribute('is-menu-open');
            }
          )",
                             GetPinnedButtonJS(mojom_action1).c_str()),
          true),

      // Open context menu for second action.
      ExecuteJsAt(kWebUIToolbarWebViewId, DeepQuery{},
                  base::StringPrintf(R"(
        () => {
          const btn = %s;
          btn.dispatchEvent(
            new MouseEvent(
              'contextmenu', {button: 2, bubbles: true, composed: true}
            )
          );
        }
      )",
                                     GetPinnedButtonJS(mojom_action2).c_str())),

      // Wait for first action to NOT be highlighted.
      WaitForJsResult(
          kWebUIToolbarWebViewId,
          base::StringPrintf(R"(
            () => {
              const btn = %s;
              return !!btn && !btn.hasAttribute('is-menu-open');
            }
          )",
                             GetPinnedButtonJS(mojom_action1).c_str()),
          true),

      // Verify second action is highlighted.
      WaitForJsResult(
          kWebUIToolbarWebViewId,
          base::StringPrintf(R"(
            () => {
              const btn = %s;
              return !!btn && btn.hasAttribute('is-menu-open');
            }
          )",
                             GetPinnedButtonJS(mojom_action2).c_str()),
          true),

      // Clean menu
      Do([this]() {
        WebUIToolbarWebView* webui_toolbar_view =
            GetWebUIToolbarWebView(browser());
        webui_toolbar_view->GetPinnedToolbarActions()->menu_runner_->Cancel();
      }),

      // Verify second action is not highlighted.
      WaitForJsResult(
          kWebUIToolbarWebViewId,
          base::StringPrintf(R"(
            () => {
              const btn = %s;
              return !!btn && !btn.hasAttribute('is-menu-open');
            }
          )",
                             GetPinnedButtonJS(mojom_action2).c_str()),
          true),

      // Clean up pinned state
      Do([this, action_id1, action_id2]() {
        model_->UpdatePinnedState(action_id1, false);
        model_->UpdatePinnedState(action_id2, false);
      }));
}

// Verifies that restoring a background WebUI tab and switching to it does not
// cause the WebUI toolbar to be occluded or freeze its Omnibox location
// updates. See b/547718312.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewInteractiveTest,
                       TabSwitchStaleBoundsOccludesWebUIToolbar) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarWebViewId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInstrumentedWebViewId);

  WaitForInitialWebUIToolbar(browser());

  // Restore a background WebUI tab (e.g. chrome://version).
  const GURL webui_url("chrome://version/");
  std::vector<sessions::SerializedNavigationEntry> navigations;
  sessions::SerializedNavigationEntry entry;
  entry.set_virtual_url(webui_url);
  entry.set_index(0);
  navigations.push_back(entry);

  content::WebContents* restored_contents = chrome::AddRestoredTab(
      browser(), navigations, /*tab_index=*/1, /*selected_navigation=*/0,
      /*extension_app_id=*/"", /*group=*/std::nullopt, /*select=*/false,
      /*pin=*/false, base::TimeTicks::Now(), base::Time::Now(),
      /*storage_namespace=*/nullptr,
      sessions::SerializedUserAgentOverride(),
      /*extra_data=*/{}, /*from_session_restore=*/true,
      /*is_active_browser=*/std::nullopt);
  ASSERT_TRUE(restored_contents);
  EXPECT_EQ(2, browser()->GetTabStripModel()->count());
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());

  RunTestSequence(
      WaitForShow(kWebUIToolbarElementIdentifier),
      WithView(kWebUIToolbarElementIdentifier,
               [](WebUIToolbarWebView* parent) {
                 parent->GetWebViewForTesting()->SetProperty(
                     views::kElementIdentifierKey, kInstrumentedWebViewId);
               }),
      InstrumentNonTabWebView(kWebUIToolbarWebViewId, kInstrumentedWebViewId,
                              /*wait_for_ready=*/true),
      // Switch to the restored WebUI background tab.
      Do([this]() { browser()->GetTabStripModel()->ActivateTabAt(1); }),
      // Verify that the WebUI Location Bar receives the update and displays the
      // new URL.
      WaitForJsResultAt(kWebUIToolbarWebViewId,
                        DeepQuery{"toolbar-app", "#location-bar", "#omnibox",
                                  "#textInput", "#input"},
                        "el => el.value.includes('chrome://version')", true));
}
