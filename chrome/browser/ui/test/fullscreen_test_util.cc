// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/fullscreen_test_util.h"

#include "content/public/test/browser_test_utils.h"

namespace content {

void WaitForHTMLFullscreen(WebContents* web_contents) {
  WaitForLoadStop(web_contents);
  ASSERT_TRUE(EvalJs(web_contents, R"JS(
        (new Promise((resolve, reject) => {
          if (!!document.fullscreenElement) {
            resolve();
          } else {
            document.addEventListener(`fullscreenchange`,
              () => { if (!!document.fullscreenElement) resolve(); }
            );
            document.addEventListener(`fullscreenerror`, e => { reject(e); });
          }
        })))JS")
                  .error.empty());
}

void WaitForHTMLFullscreenExit(WebContents* web_contents) {
  WaitForLoadStop(web_contents);
  ASSERT_TRUE(EvalJs(web_contents, R"JS(
        (new Promise((resolve, reject) => {
          if (!document.fullscreenElement) {
            resolve();
          } else {
            document.addEventListener(`fullscreenchange`,
              () => { if (!document.fullscreenElement) resolve(); }
            );
            document.addEventListener(`fullscreenerror`, e => { reject(e); });
          }
        })))JS")
                  .error.empty());
}

}  // namespace content
