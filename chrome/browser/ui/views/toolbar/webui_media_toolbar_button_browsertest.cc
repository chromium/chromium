// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_media_toolbar_button.h"

#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view_test_base.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/webview/webview.h"

class WebUIMediaButtonBrowserTest : public WebUIToolbarWebViewTestBase {
 public:
  WebUIMediaButtonBrowserTest()
      : WebUIToolbarWebViewTestBase(
            {features::kInitialWebUI, ::features::kWebUIMediaButton,
             features::kSkipIPCChannelPausingForNonGuests,
             features::kWebUIInProcessResourceLoadingV2},
            {}) {}
};

IN_PROC_BROWSER_TEST_F(WebUIMediaButtonBrowserTest, MediaButtonShowHide) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  ASSERT_TRUE(webui_toolbar_view);

  auto* media_button = static_cast<WebUIMediaToolbarButton*>(
      webui_toolbar_view->GetMediaToolbarButton());
  ASSERT_TRUE(media_button);

  WebUIToolbarControlDelegate* delegate = webui_toolbar_view;
  const auto& state = delegate->GetState().media_control_state;
  ASSERT_TRUE(state);

  content::WebContents* webui_web_contents =
      webui_toolbar_view->GetWebViewForTesting()->GetWebContents();
  ASSERT_TRUE(webui_web_contents);
  const std::string media_selector = "#media";

  // Verify the button is initially hidden.
  EXPECT_FALSE(state->should_be_shown);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(
               webui_web_contents,
               base::StringPrintf("(() => { const btn = %s; return !btn || "
                                  "!btn.checkVisibility(); })()",
                                  GetButtonAppJS(media_selector).c_str()))
        .ExtractBool();
  }));

  // Show the button via MediaToolbarButtonController and verify the state and
  // WebUI DOM update.
  media_button->GetController()->ShowToolbarButton();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return delegate->GetState().media_control_state->should_be_shown;
  }));
  EXPECT_TRUE(WaitForButtonVisible(webui_web_contents, media_selector));

  // Hide the button and verify the state and WebUI DOM update.
  media_button->Hide();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !delegate->GetState().media_control_state->should_be_shown;
  }));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(
               webui_web_contents,
               base::StringPrintf("(() => { const btn = %s; return !btn || "
                                  "!btn.checkVisibility(); })()",
                                  GetButtonAppJS(media_selector).c_str()))
        .ExtractBool();
  }));
}

IN_PROC_BROWSER_TEST_F(WebUIMediaButtonBrowserTest, MediaButtonEnableDisable) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  ASSERT_TRUE(webui_toolbar_view);

  auto* media_button = static_cast<WebUIMediaToolbarButton*>(
      webui_toolbar_view->GetMediaToolbarButton());
  ASSERT_TRUE(media_button);

  WebUIToolbarControlDelegate* delegate = webui_toolbar_view;
  const auto& state = delegate->GetState().media_control_state;
  ASSERT_TRUE(state);

  content::WebContents* webui_web_contents =
      webui_toolbar_view->GetWebViewForTesting()->GetWebContents();
  ASSERT_TRUE(webui_web_contents);
  const std::string media_selector = "#media";

  // Show the button via MediaToolbarButtonController first so it is rendered in
  // WebUI.
  media_button->GetController()->ShowToolbarButton();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return delegate->GetState().media_control_state->should_be_shown;
  }));
  EXPECT_TRUE(WaitForButtonVisible(webui_web_contents, media_selector));

  // Verify the button is initially enabled.
  EXPECT_TRUE(state->enabled);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(
               webui_web_contents,
               base::StringPrintf("(() => { const btn = %s?.shadowRoot?"
                                  ".querySelector('cr-icon-button'); "
                                  "return !!btn && !btn.disabled; })()",
                                  GetButtonAppJS(media_selector).c_str()))
        .ExtractBool();
  }));

  // Disable the button and verify the state and WebUI DOM update.
  media_button->Disable();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !delegate->GetState().media_control_state->enabled; }));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(
               webui_web_contents,
               base::StringPrintf("(() => { const btn = %s?.shadowRoot?"
                                  ".querySelector('cr-icon-button'); "
                                  "return !!btn && btn.disabled; })()",
                                  GetButtonAppJS(media_selector).c_str()))
        .ExtractBool();
  }));

  // Enable the button and verify the state and WebUI DOM update.
  media_button->Enable();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return delegate->GetState().media_control_state->enabled; }));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(
               webui_web_contents,
               base::StringPrintf("(() => { const btn = %s?.shadowRoot?"
                                  ".querySelector('cr-icon-button'); "
                                  "return !!btn && !btn.disabled; })()",
                                  GetButtonAppJS(media_selector).c_str()))
        .ExtractBool();
  }));
}
