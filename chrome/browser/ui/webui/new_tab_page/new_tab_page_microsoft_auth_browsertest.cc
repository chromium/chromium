// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace {

class NewTabPageMicrosoftAuthBrowserTest : public policy::PolicyTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();
    policy::PolicyMap policies;
    SetPolicy(&policies, policy::key::kNTPOutlookCardVisible,
              base::Value(true));
    UpdateProviderPolicy(policies);
  }
};

// The New Tab Page embeds chrome-untrusted://ntp-microsoft-auth, whose scripts
// load and connect it to the page.
IN_PROC_BROWSER_TEST_F(NewTabPageMicrosoftAuthBrowserTest,
                       MicrosoftAuthFrameConnectsToNewTabPage) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(content::NavigateToURL(web_contents,
                                     GURL(chrome::kChromeUINewTabPageURL)));

  ASSERT_TRUE(base::test::RunUntil([&] {
    return content::EvalJs(
               web_contents,
               "import('chrome://new-tab-page/lazy_load.js').then("
               "    m => !!m.ParentTrustedDocumentProxy.getInstance())") ==
           true;
  })) << "chrome-untrusted://ntp-microsoft-auth never connected to the NTP";
}

}  // namespace
