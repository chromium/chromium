// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "chrome/browser/ui/webui/web_ui_all_urls_browser_test.h"
#include "chrome/browser/ui/webui/webui_urls_for_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

using WebUIUrlBrowserTest = InProcessBrowserTest;

// Tests that all registered WebUIs have their URL listed in kChromeUrls or in
// kChromeUntestedUrls, so that basic sanity checks can run on them.
IN_PROC_BROWSER_TEST_F(WebUIUrlBrowserTest, UrlsInTestList) {
  content::WebUIConfigMap& map = content::WebUIConfigMap::GetInstance();
  std::set<std::string> missing_entries;
  for (const content::WebUIConfigInfo& config_info :
       map.GetWebUIConfigList(nullptr)) {
    std::string url = config_info.origin.Serialize();
    missing_entries.insert(url);
  }

  for (const char* url : kChromeUrls) {
    missing_entries.erase(url);
  }

  for (const char* url : kChromeUntestedUrls) {
    missing_entries.erase(url);
  }

  EXPECT_TRUE(missing_entries.empty())
      << "Please add this URL to kChromeUrls in "
         "//chrome/browser/ui/webui/webui_urls_for_test.h:"
      << std::endl
      << base::JoinString(
             std::vector(missing_entries.begin(), missing_entries.end()), "\n");
}

static const char* const kConsoleErrorUrls[] = {
#if BUILDFLAG(IS_CHROMEOS)
    "chrome-untrusted://os-feedback",
    // TODO(b/300875336): Navigating to chrome://cloud-upload causes an
    // assertion failure because there are no dialog args.
    "chrome://cloud-upload",
    "chrome://crostini-installer",
    "chrome://office-fallback/",
    "chrome://os-feedback",
    "chrome://parent-access",
    "chrome://personalization",
    "chrome://smb-credentials-dialog/",
#else
    "chrome://signin-email-confirmation",
#endif
};

class WebUIUrlNoConsoleErrorsTest : public WebUIAllUrlsBrowserTest {
 public:
  WebUIUrlNoConsoleErrorsTest() = default;

  void CheckNoConsoleErrors(std::string_view url) {
    for (const char* broken_url : kConsoleErrorUrls) {
      if (url == broken_url) {
        return;
      }
    }
    auto console_error_filter =
        [](const content::WebContentsConsoleObserver::Message& message) {
          return message.log_level == blink::mojom::ConsoleMessageLevel::kError;
        };
    content::WebContents* content =
        browser()->tab_strip_model()->GetActiveWebContents();
    // Watches for any network load errors.
    content::DevToolsInspectorLogWatcher log_watcher(content);
    // Watches for console errors.
    content::WebContentsConsoleObserver console_observer(content);
    console_observer.SetFilter(base::BindRepeating(console_error_filter));

    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
    ASSERT_TRUE(content::WaitForLoadStop(content));
    EXPECT_TRUE(console_observer.messages().empty());
    log_watcher.FlushAndStopWatching();
    EXPECT_EQ(log_watcher.last_message(), "");
  }
};

// Verify that there's no console errors when loading any `kChromeUrls`.
// TODO(crbug.com/487122203): Fix the issue (see the bug entry for details) and
// re-enable the test.
IN_PROC_BROWSER_TEST_P(WebUIUrlNoConsoleErrorsTest, DISABLED_NoConsoleErrors) {
  CheckNoConsoleErrors(GetParam());
  WaitBeforeNavigation();
}

INSTANTIATE_TEST_SUITE_P(,
                         WebUIUrlNoConsoleErrorsTest,
                         ::testing::ValuesIn(kChromeUrls),
                         WebUIAllUrlsBrowserTest::ParamInfoToString);
