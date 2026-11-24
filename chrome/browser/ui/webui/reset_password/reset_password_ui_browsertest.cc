// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ResetPasswordUIBrowserTest = PlatformBrowserTest;

// The "Reset Password" button opens the enterprise change password page.
IN_PROC_BROWSER_TEST_F(ResetPasswordUIBrowserTest, ResetPasswordButton) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL change_password_url =
      embedded_test_server()->GetURL("/title1.html");
  chrome_test_utils::GetProfile(this)->GetPrefs()->SetString(
      prefs::kPasswordProtectionChangePasswordURL, change_password_url.spec());

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(content::NavigateToURL(web_contents,
                                     GURL(chrome::kChromeUIResetPasswordURL)));

  content::TestNavigationObserver observer(web_contents);
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      "document.getElementById('reset-password-button').click();"));
  observer.Wait();
  EXPECT_EQ(change_password_url, web_contents->GetLastCommittedURL());
}
