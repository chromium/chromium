// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/webui_browser/webui_browser_renderer_extension.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

// TODO(webium): Test with the feature kWebium enabled. Currently, the browser
// test times out on shutdown. Will be fixed in https://crrev.com/c/6819168.
using WebUIBrowserRendererExtensionBrowserTest = InProcessBrowserTest;

// Tests that the API is not exposed on non-WebUI pages.
IN_PROC_BROWSER_TEST_F(WebUIBrowserRendererExtensionBrowserTest,
                       ApiNotExposedOnRegularPages) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a non-WebUI page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  // Check that the API is not exposed.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(false,
            content::EvalJs(web_contents, "!!chrome?.browser").ExtractBool());
}
