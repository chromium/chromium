// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

class WebNNInternalsBrowserTest : public InProcessBrowserTest {
 public:
  WebNNInternalsBrowserTest() = default;
  ~WebNNInternalsBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(WebNNInternalsBrowserTest, PageLoads) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIWebNNInternalsURL)));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(u"WebNN Internals", web_contents->GetTitle());

  bool checkbox_exists =
      content::EvalJs(web_contents,
                      "!!document.getElementById('record-graph-checkbox')")
          .ExtractBool();
  EXPECT_TRUE(checkbox_exists);
}
