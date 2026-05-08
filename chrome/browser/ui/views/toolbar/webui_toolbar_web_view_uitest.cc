// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/collaboration/public/features.h"
#include "components/contextual_tasks/public/features.h"
#include "components/data_sharing/public/features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_class_properties.h"

namespace {

WebUIToolbarWebView* GetWebUIToolbarWebView(Browser* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser)
      ->toolbar_button_provider()
      ->GetWebUIToolbarViewForTesting();
}

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
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kWebUILocationBar,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2,
         features::kInitialWebUISyncNavStartToCommit},
        {});
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
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
      ExecuteJsAt(kWebUIToolbarWebViewId,
                  DeepQuery{"toolbar-app", "location-bar", "location-icon",
                            "#container"},
                  "el => el.click()"),
      WaitForShow(PageInfoBubbleViewBase::kPageInfoBubbleElementIdentifier));
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
        browser()->command_controller()->ExecuteCommand(IDC_FOCUS_TOOLBAR);
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
         features::kInitialWebUISyncNavStartToCommit,
         tabs::kHorizontalTabStripComboButton, features::kWebUIReloadButton},
        {});
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    // Set tested actions as pinnable.
    auto* action_item = actions::ActionManager::Get().FindAction(
        kActionPrint, browser()->GetActions()->root_action_item());
    if (action_item) {
      action_item->SetProperty(
          actions::kActionItemPinnableKey,
          static_cast<int>(actions::ActionPinnableState::kPinnable));
    }
    action_item = actions::ActionManager::Get().FindAction(
        kActionSidePanelShowBookmarks,
        browser()->GetActions()->root_action_item());
    if (action_item) {
      action_item->SetProperty(
          actions::kActionItemPinnableKey,
          static_cast<int>(actions::ActionPinnableState::kPinnable));
    }

    model_ = PinnedToolbarActionsModel::Get(browser()->profile());
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
