// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_pages.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace chrome {

// The param is whether kReplaceManageProfileWithDialog is enabled.
class ShowManageProfileBrowserTest : public InProcessBrowserTest,
                                     public testing::WithParamInterface<bool> {
 public:
  ShowManageProfileBrowserTest() {
    feature_list_.InitWithFeatureState(
        switches::kReplaceManageProfileWithDialog,
        ReplaceManageProfileWithDialog());
  }

 protected:
  bool ReplaceManageProfileWithDialog() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Opens the edit profile dialog if kReplaceManageProfileWithDialog is enabled,
// and chrome://settings/manageProfile otherwise.
IN_PROC_BROWSER_TEST_P(ShowManageProfileBrowserTest,
                       OpensDialogOrSettingsPage) {
  const int tab_count = browser()->GetTabStripModel()->count();
  ShowManageProfile(browser());

  SigninViewController* controller = SigninViewController::From(browser());
  if (ReplaceManageProfileWithDialog()) {
    ASSERT_TRUE(controller->ShowsModalDialog());
    EXPECT_EQ(
        GetProfileCustomizationStyle(
            controller->GetModalDialogWebContentsForTesting()->GetVisibleURL()),
        ProfileCustomizationStyle::kEditProfile);
    EXPECT_EQ(browser()->GetTabStripModel()->count(), tab_count);
    controller->CloseModalSignin();
  } else {
    content::WebContents* web_contents =
        browser()->GetTabStripModel()->GetActiveWebContents();
    content::WaitForLoadStop(web_contents);
    EXPECT_EQ(web_contents->GetVisibleURL(),
              GURL("chrome://settings/manageProfile"));
    EXPECT_FALSE(controller->ShowsModalDialog());
  }
}

// Nothing is shown for an Incognito browser: neither the edit profile dialog if
// `kReplaceManageProfileWithDialog` is enabled, nor
// chrome://settings/manageProfile otherwise.
IN_PROC_BROWSER_TEST_P(ShowManageProfileBrowserTest, IncognitoBrowser) {
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  const int incognito_tab_count =
      incognito_browser->GetTabStripModel()->count();
  const int regular_tab_count = browser()->GetTabStripModel()->count();
  const GURL regular_url =
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL();

  ShowManageProfile(incognito_browser);

  if (ReplaceManageProfileWithDialog()) {
    EXPECT_FALSE(
        SigninViewController::From(incognito_browser)->ShowsModalDialog());
  } else {
    // chrome://settings/manageProfile is not opened, neither in the Incognito
    // browser nor in the regular browser (where Settings opened from
    // Incognito are redirected).
    EXPECT_EQ(incognito_browser->GetTabStripModel()->count(),
              incognito_tab_count);
    EXPECT_EQ(browser()->GetTabStripModel()->count(), regular_tab_count);
    EXPECT_EQ(
        browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL(),
        regular_url);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         ShowManageProfileBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Dialog" : "SettingsPage";
                         });

}  // namespace chrome
