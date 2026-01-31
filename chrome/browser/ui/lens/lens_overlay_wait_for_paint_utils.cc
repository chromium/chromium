// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_wait_for_paint_utils.h"

#include "base/test/run_until.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lens {

namespace {

// `content::ExecJs` can handle promises, so queue a promise that only succeeds
// after the contents have been rendered.
constexpr char kPaintWorkaroundFunction[] =
    "() => new Promise(resolve => requestAnimationFrame(() => resolve(true)))";

}  // namespace

void WaitForPaint(Browser* browser,
                  const GURL& url,
                  WindowOpenDisposition disposition,
                  int browser_test_flags) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser, url, disposition, browser_test_flags));
  const bool first_paint_completed =
      browser->tab_strip_model()
          ->GetActiveTab()
          ->GetContents()
          ->CompletedFirstVisuallyNonEmptyPaint();

  // Return early if first paint is already completed.
  if (first_paint_completed) {
    return;
  }
  // Wait for the first paint to complete. The below code works for a majority
  // of cases, but loading non-html files can lead to the workaround failing, so
  // this check is still needed.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser->tab_strip_model()
        ->GetActiveTab()
        ->GetContents()
        ->CompletedFirstVisuallyNonEmptyPaint();
  }));
  // If the first paint was not mark as completed by the WebContents, use a
  // workaround to request a frame on the WebContents. This function will only
  // return when the promise is resolved and thus there is content painted on
  // the WebContents to allow screenshotting. See crbug.com/334747109 for
  // details on this possible race condition and the workaround used in
  // interactive tests.
  ASSERT_TRUE(
      content::ExecJs(browser->tab_strip_model()->GetActiveTab()->GetContents(),
                      kPaintWorkaroundFunction));
}

}  // namespace lens
