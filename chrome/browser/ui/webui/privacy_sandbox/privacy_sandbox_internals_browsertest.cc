// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {
using ::testing::Eq;
using ::testing::Ne;

class PrivacySandboxInternalsTest : public InProcessBrowserTest {
 public:
  PrivacySandboxInternalsTest() {
    scoped_feature_list_.InitAndEnableFeature(
        privacy_sandbox::kPrivacySandboxInternalsDevUI);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsTest, PageLoadsWhenFeatureOn) {
  GURL initial_url(content::GetWebUIURL(chrome::kChromeUIPrivacySandboxInternalsHost));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  const std::string default_page_name =
      content::EvalJs(web_contents, R"(
        (function() {
          const internalsPage = document.querySelector('internals-page');
          const selectedTab = internalsPage.shadowRoot.querySelector('[slot="tab"][selected]');
          return selectedTab.dataset.pageName;
        })();
      )").ExtractString();

  ASSERT_FALSE(default_page_name.empty());
  GURL final_url(initial_url.spec() + "?page=" + default_page_name);
  EXPECT_THAT(web_contents->GetLastCommittedURL(), Eq(final_url));
  EXPECT_FALSE(web_contents->IsCrashed());
  EXPECT_THAT(web_contents->GetTitle(), Eq(u"Privacy Sandbox Internals"));
}
}  // namespace
