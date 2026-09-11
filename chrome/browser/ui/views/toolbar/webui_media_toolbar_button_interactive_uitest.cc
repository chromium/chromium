// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_media_toolbar_button.h"

#include "base/test/run_until.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view_test_base.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/webview/webview.h"

class WebUIMediaToolbarButtonInteractiveTest
    : public WebUIToolbarWebViewTestBase {
 public:
  WebUIMediaToolbarButtonInteractiveTest()
      : WebUIToolbarWebViewTestBase(
            {features::kInitialWebUI, ::features::kWebUIMediaButton,
             features::kSkipIPCChannelPausingForNonGuests,
             features::kWebUIInProcessResourceLoadingV2},
            {}) {}
};

IN_PROC_BROWSER_TEST_F(WebUIMediaToolbarButtonInteractiveTest,
                       MediaButtonClickedAndRightClicked) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  ASSERT_TRUE(webui_toolbar_view);

  auto* media_button = static_cast<WebUIMediaToolbarButton*>(
      webui_toolbar_view->GetMediaToolbarButton());
  ASSERT_TRUE(media_button);

  media_button->GetController()->ShowToolbarButton();

  content::WebContents* web_contents =
      webui_toolbar_view->GetWebViewForTesting()->GetWebContents();
  ASSERT_TRUE(web_contents);

  // 1. Verify media button is visible and not highlighted.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(
               web_contents,
               "(() => {"
               "  const media = "
               "document.querySelector('toolbar-app')?.shadowRoot"
               "                    ?.querySelector('#media');"
               "  const btn = "
               "media?.shadowRoot?.querySelector('cr-icon-button');"
               "  return !!media && !!btn && btn.checkVisibility() &&"
               "         !media.classList.contains('anchor-highlight') &&"
               "         !btn.hasAttribute('is-menu-open');"
               "})()")
        .ExtractBool();
  }));

  // 2. Click the media button to show the bubble.
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "document.querySelector('toolbar-app').shadowRoot"
                              "    .querySelector('#media').shadowRoot"
                              "    .querySelector('cr-icon-button').click();"));

  // 3. Verify MediaDialogView is showing and button is highlighted.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return MediaDialogView::IsShowing() &&
           content::EvalJs(web_contents,
                           "document.querySelector('toolbar-app').shadowRoot"
                           "    .querySelector('#media')"
                           "    .classList.contains('anchor-highlight')")
               .ExtractBool();
  }));

  // 4. Close the dialog and verify highlight is removed.
  MediaDialogView::HideDialog();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !MediaDialogView::IsShowing() &&
           content::EvalJs(web_contents,
                           "!document.querySelector('toolbar-app').shadowRoot"
                           "    .querySelector('#media')"
                           "    .classList.contains('anchor-highlight')")
               .ExtractBool();
  }));

  // 5. Right-click (contextmenu) the media button to show the context menu.
  EXPECT_TRUE(
      content::ExecJs(web_contents,
                      "document.querySelector('toolbar-app').shadowRoot"
                      "    .querySelector('#media').shadowRoot"
                      "    .querySelector('cr-icon-button').dispatchEvent("
                      "        new MouseEvent('contextmenu', {button: 2, "
                      "bubbles: true, composed: true}));"));

  // 6. Verify context menu is running and button has is-menu-open.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return media_button->menu_runner_ &&
           media_button->menu_runner_->IsRunning() &&
           content::EvalJs(web_contents,
                           "document.querySelector('toolbar-app').shadowRoot"
                           "    .querySelector('#media').shadowRoot"
                           "    .querySelector('cr-icon-button')"
                           "    .hasAttribute('is-menu-open')")
               .ExtractBool();
  }));

  // 7. Close context menu and verify is-menu-open is removed.
  media_button->menu_runner_->Cancel();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return (!media_button->menu_runner_ ||
            !media_button->menu_runner_->IsRunning()) &&
           content::EvalJs(web_contents,
                           "!document.querySelector('toolbar-app').shadowRoot"
                           "    .querySelector('#media').shadowRoot"
                           "    .querySelector('cr-icon-button')"
                           "    .hasAttribute('is-menu-open')")
               .ExtractBool();
  }));
}
