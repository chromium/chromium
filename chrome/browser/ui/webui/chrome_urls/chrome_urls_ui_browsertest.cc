// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

class ChromeUrlsUiTest : public InProcessBrowserTest {
 private:
  base::test::ScopedFeatureList feature_list_{features::kInternalOnlyUisPref};
};

// Simple test to ensure chrome://chrome-urls loads with the feature flag that
// enables the new version of the page. Performs the same checks for
// TrustedTypes that are performed by chrome_url_data_manager_browsertest.cc
// for the old version of the page.
// TODO (crbug.com/379889249): Move this test to
// chrome_url_data_manager_browsertest.cc once the flag is enabled by default.
IN_PROC_BROWSER_TEST_F(ChromeUrlsUiTest, LoadsWithoutTrustedTypeError) {
  const std::string kMessageFilter = "*Refused to create a TrustedTypePolicy*";
  content::WebContents* content =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContentsConsoleObserver console_observer(content);
  console_observer.SetPattern(kMessageFilter);

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIChromeURLsURL)));
  ASSERT_TRUE(content::WaitForLoadStop(content));
  EXPECT_TRUE(console_observer.messages().empty());

  const char kIsTrustedTypesEnabled[] =
      "(function isTrustedTypesEnabled() {"
      "  try {"
      "    document.body.innerHTML = 'foo';"
      "  } catch(e) {"
      "    return true;"
      "  }"
      "  return false;"
      "})();";

  EXPECT_EQ(true,
            EvalJs(content, kIsTrustedTypesEnabled,
                   content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
}
