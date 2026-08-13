// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings_internals {
namespace {

using ::testing::Eq;

class ContentSettingsUITest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    g_browser_process->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);
    InProcessBrowserTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(ContentSettingsUITest, PageLoads) {
  GURL initial_url(content::GetWebUIURL(chrome::kChromeUIContentSettingsHost));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  const std::string default_page_name = content::EvalJs(web_contents, R"(
        (async function() {
          await customElements.whenDefined('app-element');
          const appElement = document.querySelector('app-element');
          const selectedTab =
              appElement.shadowRoot.querySelector('[slot="tab"][selected]');
          return selectedTab.dataset.pageName;
        })();
      )")
                                            .ExtractString();

  ASSERT_FALSE(default_page_name.empty());
  GURL final_url(initial_url.spec() + "?page=" + default_page_name);
  EXPECT_THAT(web_contents->GetLastCommittedURL(), Eq(final_url));
  EXPECT_FALSE(web_contents->IsCrashed());
  EXPECT_THAT(web_contents->GetTitle(), Eq(u"Content Settings"));
}

}  // namespace
}  // namespace content_settings_internals
