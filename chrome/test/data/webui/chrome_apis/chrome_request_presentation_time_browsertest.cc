// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

using ChromeRequestPresentationTimeBrowserTest = WebUIMochaBrowserTest;

namespace {

// Paints a visible change (a fixed-position box that moves on every call, so
// each frame has damage) and resolves with the typeof of the
// requestPresentationTime() result: 'bigint' when a frame was presented,
// 'object' for null. A mutation with no visible effect would be dropped by
// the compositor as a no-damage frame and legitimately yield null.
constexpr char kPaintAndRequest[] = R"(
    (() => {
      window.pptMarkerCount = (window.pptMarkerCount || 0) + 1;
      const box = document.createElement('div');
      box.style.cssText = 'position:fixed;top:0;width:40px;height:40px;' +
          'background:red;z-index:2147483647;left:' +
          (window.pptMarkerCount * 50) + 'px';
      document.body.appendChild(box);
      return chrome.requestPresentationTime().then(v => typeof v);
    })())";

}  // namespace

IN_PROC_BROWSER_TEST_F(ChromeRequestPresentationTimeBrowserTest, All) {
  // Spawn the test from chrome://settings (or any WebUI surface) so that
  // the chrome.requestPresentationTime API exists.
  set_test_loader_host(chrome::kChromeUISettingsHost);
  RunTest("chrome_apis/chrome_request_presentation_time_test.js",
          "mocha.run()");
}

// The promise must resolve with a timestamp while the page is visible, null
// immediately while it is hidden (a hidden page produces no frames, so the
// request would otherwise attach to the frame of a later show), and with a
// timestamp again when called from the visibilitychange handler that runs as
// the page becomes visible. That last call happens before the RenderWidget is
// shown, which is why visibility is judged by the Page and not the widget.
IN_PROC_BROWSER_TEST_F(ChromeRequestPresentationTimeBrowserTest,
                       FollowsPageVisibility) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUISettingsURL)));
  content::WebContents* settings =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_EQ(content::Visibility::VISIBLE, settings->GetVisibility());
  EXPECT_EQ("bigint", content::EvalJs(settings, kPaintAndRequest));

  // Hide the WebContents (same path as backgrounding its tab) and wait for
  // the renderer to observe the change.
  settings->WasHidden();
  ASSERT_EQ(content::Visibility::HIDDEN, settings->GetVisibility());
  ASSERT_EQ("hidden", content::EvalJs(settings, R"(
      new Promise(resolve => {
        if (document.visibilityState === 'hidden') {
          resolve('hidden');
          return;
        }
        document.addEventListener(
            'visibilitychange', () => resolve(document.visibilityState),
            {once: true});
      }))"));
  EXPECT_EQ("object", content::EvalJs(settings, kPaintAndRequest));

  // Arm a request that fires from the visibilitychange handler on the way
  // back to 'visible', then show the WebContents again. The script ends with
  // a plain value because ExecJs would otherwise await the armed promise.
  ASSERT_TRUE(content::ExecJs(
      settings, base::StrCat({R"(
      window.showResult = new Promise(resolve => {
        document.addEventListener('visibilitychange', () => {
          resolve()", kPaintAndRequest, R"();
        }, {once: true});
      });
      true;)"})));
  settings->WasShown();
  ASSERT_EQ(content::Visibility::VISIBLE, settings->GetVisibility());
  EXPECT_EQ("bigint", content::EvalJs(settings, "window.showResult"));
}
