// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/permission_chip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/permissions/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

class PermissionChipBrowserTest : public UiBrowserTest {
 public:
  PermissionChipBrowserTest() {
    feature_list_.InitAndEnableFeature(permissions::features::kPermissionChip);
  }

  PermissionChipBrowserTest(const PermissionChipBrowserTest&) = delete;
  PermissionChipBrowserTest& operator=(const PermissionChipBrowserTest&) =
      delete;

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_ =
        std::make_unique<test::PermissionRequestManagerTestApi>(browser());
    EXPECT_TRUE(test_api_->manager());
    test_api_->AddSimpleRequest(GetActiveMainFrame(),
                                ContentSettingsType::GEOLOCATION);

    base::RunLoop().RunUntilIdle();
  }

  bool VerifyUi() override {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    PermissionChip* permission_chip =
        browser_view->toolbar()->location_bar()->permission_chip();

    return permission_chip && permission_chip->GetVisible();
  }

  void WaitForUserDismissal() override {
    // Consider closing the browser to be dismissal.
    ui_test_utils::WaitForBrowserToClose();
  }

  content::RenderFrameHost* GetActiveMainFrame() {
    return browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PermissionChipBrowserTest, InvokeUi_geolocation) {
  ShowAndVerifyUi();
}
