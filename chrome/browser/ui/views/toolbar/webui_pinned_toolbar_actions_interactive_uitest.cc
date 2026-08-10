// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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
    : public WebUIPinnedToolbarActionsTestBase {};

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
      browser()->tab_strip_model()->GetActiveWebContents(),
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
