// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/strings/string_util.h"
#include "chrome/browser/ui/webui/web_ui_all_urls_browser_test.h"
#include "chrome/browser/ui/webui/webui_urls_for_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using WebUIUrlBrowserTest = ::InProcessBrowserTest;

std::string GetOrigin(std::string_view url) {
  return url::Origin::Resolve(GURL(url), url::Origin()).Serialize();
}

}  // namespace

// Tests that all registered WebUIs have their URL listed in kChromeUrls or in
// kChromeUntestedUrls, so that basic sanity checks can run on them.
IN_PROC_BROWSER_TEST_F(WebUIUrlBrowserTest, UrlsInTestList) {
  const absl::flat_hash_set<std::string> origins_in_test_list = []() {
    absl::flat_hash_set<std::string> origins;
    for (std::string_view url : GetChromeUrlsForTest()) {
      origins.insert(GetOrigin(url));
    }
    for (std::string_view url : GetUntestedChromeUrlsForTest()) {
      origins.insert(GetOrigin(url));
    }
    return origins;
  }();

  std::vector<std::string> missing_entries;
  for (const content::WebUIConfigInfo& config_info :
       content::WebUIConfigMap::GetInstance().GetWebUIConfigList(
           /*browser_context=*/nullptr)) {
    const std::string registered_url = config_info.origin.Serialize();
    if (!origins_in_test_list.contains(registered_url)) {
      missing_entries.push_back(registered_url);
    }
  }

  EXPECT_TRUE(missing_entries.empty())
      << "Please add this URL to kChromeUrls in "
         "//chrome/browser/ui/webui/webui_urls_for_test.h:"
      << std::endl
      << base::JoinString(missing_entries, "\n");
}

static const char* const kConsoleErrorUrls[] = {
#if BUILDFLAG(IS_CHROMEOS)
    "chrome-untrusted://os-feedback",
    // TODO(b/300875336): Navigating to chrome://cloud-upload causes an
    // assertion failure because there are no dialog args.
    "chrome://cloud-upload",
    "chrome://crostini-installer",
    // TODO(https://crbug.com/487113801): Fix file manager flaky console
    // errors on load.
    "chrome://file-manager",
    "chrome://office-fallback",
    "chrome://os-feedback",
    "chrome://parent-access",
    "chrome://personalization",
    "chrome://smb-credentials-dialog",
#else
    "chrome://signin-email-confirmation",
#endif
};

class WebUIUrlNoConsoleErrorsTest : public WebUIAllUrlsBrowserTest {
 public:
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
// Note: If one test case fails, move the failing WebUI URL to the
// untested list in webui_urls_for_test.h or to the list of failures
// in this file. DO NOT globally disable all tests in this suite, this
// causes valuable test coverage to be lost for new and existing UIs.
IN_PROC_BROWSER_TEST_P(WebUIUrlNoConsoleErrorsTest, NoConsoleErrors) {
  CheckNoConsoleErrors(GetParam());
  WaitBeforeNavigation();
}

INSTANTIATE_TEST_SUITE_P(,
                         WebUIUrlNoConsoleErrorsTest,
                         testing::ValuesIn(GetChromeUrlsForTest()),
                         WebUIAllUrlsBrowserTest::ParamInfoToString);
