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
        switches::kReplaceManageProfileWithDialog, GetParam());
  }

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
  if (GetParam()) {
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

INSTANTIATE_TEST_SUITE_P(,
                         ShowManageProfileBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Dialog" : "SettingsPage";
                         });

}  // namespace chrome
