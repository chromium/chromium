// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_pinned_toolbar_actions.h"

#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/translate/translate_bubble_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_pinned_toolbar_actions_test_base.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/actions/actions.h"
#include "ui/views/controls/webview/webview.h"

class WebUIPinnedToolbarActionsInteractiveUiTest
    : public WebUIPinnedToolbarActionsTestBase,
      public testing::WithParamInterface<bool> {};

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsInteractiveUiTest,
                       HighlightOnShowTranslateBubble) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  content::WebContents* web_ui_contents =
      webui_toolbar_view->GetWebViewForTesting()->GetWebContents();

  actions::ActionId action_id = kActionShowTranslate;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action =
      toolbar_ui_api::mojom::PinnedToolbarAction::kShowTranslate;

  // Pin Translate action.
  PinAction(action_id, mojom_action);

  // Show translate bubble.
  BrowserWindow::FromBrowser(browser())->ShowTranslateBubble(
      browser()->GetTabStripModel()->GetActiveWebContents(),
      translate::TRANSLATE_STEP_BEFORE_TRANSLATE, "fr", "en",
      translate::TranslateErrors::NONE, true);

  // Verify it's highlighted.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return EvalJsOnPinnedButton(web_ui_contents, mojom_action,
                                "return !!btn && "
                                "btn.hasAttribute('is-menu-open');")
        .ExtractBool();
  }));
}

IN_PROC_BROWSER_TEST_P(WebUIPinnedToolbarActionsInteractiveUiTest,
                       ClickSuppressionOnTranslateBubble) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  content::WebContents* web_ui_contents =
      webui_toolbar_view->GetWebViewForTesting()->GetWebContents();

  // Pin another action.
  PinAction(kActionCopyUrl,
            toolbar_ui_api::mojom::PinnedToolbarAction::kCopyUrl);

  actions::ActionId action_id = kActionShowTranslate;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action =
      toolbar_ui_api::mojom::PinnedToolbarAction::kShowTranslate;

  if (GetParam()) {
    // Pin Translate action.
    PinAction(action_id, mojom_action);
  } else {
    // Pop out Translate action.
    GetPinnedToolbarActions()->ShowActionEphemerallyInToolbar(action_id, true);
    base::RunLoop run_loop;
    GetPinnedToolbarActions()->PostOrQueueActionAfterAnimation(
        run_loop.QuitClosure());
    run_loop.Run();
  }

  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      action_id, BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(action_item);

  for (bool close_before_pointerdown : {false, true}) {
    // Show translate bubble.
    BrowserWindow::FromBrowser(browser())->ShowTranslateBubble(
        browser()->GetTabStripModel()->GetActiveWebContents(),
        translate::TRANSLATE_STEP_BEFORE_TRANSLATE, "fr", "en",
        translate::TranslateErrors::NONE, true);

    // Verify it's highlighted and action item is showing bubble.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return action_item->GetIsShowingBubble() &&
             EvalJsOnPinnedButton(web_ui_contents, mojom_action,
                                  "return !!btn && "
                                  "btn.hasAttribute('is-menu-open') && "
                                  "actionEl.trackedHighlighted;")
                 .ExtractBool();
    }));

    if (close_before_pointerdown) {
      ASSERT_TRUE(
          EvalJsOnPinnedButton(
              web_ui_contents, mojom_action,
              "window.beforeCloseTime = performance.now(); return true;")
              .ExtractBool());
      translate::test_utils::CloseCurrentBubble(browser());
      ASSERT_TRUE(base::test::RunUntil([&]() {
        return EvalJsOnPinnedButton(web_ui_contents, mojom_action,
                                    "return actionEl.lastUnhighlightedTime >= "
                                    "window.beforeCloseTime;")
            .ExtractBool();
      }));
      // Set lastUnhighlightedTime to 10s in the future in case of a slow bot.
      ASSERT_TRUE(EvalJsOnPinnedButton(
                      web_ui_contents, mojom_action,
                      "actionEl.lastUnhighlightedTime = performance.now() + "
                      "10000; return true;")
                      .ExtractBool());
    }

    EXPECT_TRUE(EvalJsOnPinnedButton(
                    web_ui_contents, mojom_action,
                    "btn.dispatchEvent(new PointerEvent('pointerdown', "
                    "{bubbles: true, cancelable: true, view: window, "
                    "button: 0, pointerType: 'mouse'}));"
                    "return true;")
                    .ExtractBool());
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return EvalJsOnPinnedButton(web_ui_contents, mojom_action,
                                  "return actionEl.skipNextClick_;")
          .ExtractBool();
    }));

    if (!close_before_pointerdown) {
      translate::test_utils::CloseCurrentBubble(browser());
    }

    EXPECT_TRUE(
        EvalJsOnPinnedButton(web_ui_contents, mojom_action,
                             "btn.dispatchEvent(new PointerEvent('click', "
                             "{bubbles: true, cancelable: true, view: window, "
                             "button: 0, pointerType: 'mouse'}));"
                             "return true;")
            .ExtractBool());

    // Verify that the bubble remains closed and suppression prevents it from
    // reopening.
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return !action_item->GetIsShowingBubble() &&
             EvalJsOnPinnedButton(web_ui_contents, mojom_action,
                                  "return !!btn && "
                                  "!actionEl.trackedHighlighted;")
                 .ExtractBool();
    }));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebUIPinnedToolbarActionsInteractiveUiTest,
                         testing::Bool());
