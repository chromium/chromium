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
  GURL kUrl(content::GetWebUIURL(chrome::kChromeUIPrivacySandboxInternalsHost));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_THAT(web_contents->GetLastCommittedURL(), Eq(kUrl));
  EXPECT_FALSE(web_contents->IsCrashed());
  EXPECT_THAT(web_contents->GetTitle(), Eq(u"Privacy Sandbox Internals"));
}

class PrivacySandboxInternalsDisabledTest : public InProcessBrowserTest {
 public:
  PrivacySandboxInternalsDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        privacy_sandbox::kPrivacySandboxInternalsDevUI);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsDisabledTest,
                       PageDoesNotLoadWhenFeatureIsOff) {
  GURL kUrl(content::GetWebUIURL(chrome::kChromeUIPrivacySandboxInternalsHost));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_THAT(web_contents->GetLastCommittedURL(), Eq(kUrl));
  EXPECT_FALSE(web_contents->IsCrashed());
  EXPECT_THAT(web_contents->GetTitle(), Ne(u"Privacy Sandbox Internals"));
}
}  // namespace
