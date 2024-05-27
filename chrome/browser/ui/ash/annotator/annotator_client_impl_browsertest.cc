// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/annotator/annotator_client_impl.h"

#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

class AnnotatorClientTest : public InProcessBrowserTest {
 public:
  AnnotatorClientTest() = default;

  ~AnnotatorClientTest() override = default;
  AnnotatorClientTest(const AnnotatorClientTest&) = delete;
  AnnotatorClientTest& operator=(const AnnotatorClientTest&) = delete;

  // This test helper verifies that navigating to the |url| doesn't result in a
  // 404 error.
  void VerifyUrlValid(const char* url) {
    GURL gurl(url);
    EXPECT_TRUE(gurl.is_valid()) << "url isn't valid: " << url;
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl))
        << "navigating to url failed: " << url;
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(tab->GetController().GetLastCommittedEntry()->GetPageType(),
              content::PAGE_TYPE_NORMAL)
        << "page has unexpected errors: " << url;
  }
};

// This test verifies that the annotator WebUI URL is valid.
IN_PROC_BROWSER_TEST_F(AnnotatorClientTest, AppUrlsValid) {
  VerifyUrlValid(kChromeUIUntrustedAnnotatorUrl);
}

}  // namespace ash
