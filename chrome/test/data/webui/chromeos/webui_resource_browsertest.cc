// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/test_data_source.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/resource/resource_bundle.h"

class WebUIResourceBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // Setup chrome://test/ data source.
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
    content::URLDataSource::Add(profile,
                                std::make_unique<TestDataSource>("webui"));
  }

  void LoadTestUrl(const std::string& file) {
    GURL url(std::string("chrome://test/") + file);
    RunTest(url);
  }

 private:
  void RunTest(const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    EXPECT_TRUE(ExecuteWebUIResourceTest(web_contents));
  }
};

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, CrTest) {
  LoadTestUrl("chromeos/ash_common/cr_test.html");
}
