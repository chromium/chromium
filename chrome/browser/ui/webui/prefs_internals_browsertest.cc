// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

using PrefsInternalsTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(PrefsInternalsTest, TestPrefsAreServed) {
  // Set a preference to something very unique so we can look for it in the
  // generated page.
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  GURL fake_homepage_url = GURL("http://example.com/" + guid);
  EXPECT_TRUE(fake_homepage_url.is_valid());
  browser()->profile()->GetPrefs()->SetString(prefs::kHomePage,
                                              fake_homepage_url.spec());

  // First, check that navigation succeeds.
  GURL kUrl(content::GetWebUIURL(chrome::kChromeUIPrefsInternalsHost));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(kUrl, web_contents->GetLastCommittedURL());
  EXPECT_FALSE(web_contents->IsCrashed());

  // It's difficult to test the content of the page without duplicating the
  // implementation, but we can at least assert that something is being shown.
  EXPECT_EQ(true,
            content::EvalJs(web_contents,
                            base::StringPrintf(
                                "document.body.textContent && "
                                "document.body.textContent.indexOf('%s') >= 0;",
                                guid.c_str())));
}
