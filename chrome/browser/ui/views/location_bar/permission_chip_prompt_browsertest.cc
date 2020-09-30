// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/permission_chip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/button_test_api.h"

class PermissionChipPromptBrowserTest : public DialogBrowserTest {
 public:
  PermissionChipPromptBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kPermissionChip);
  }

  PermissionChipPromptBrowserTest(const PermissionChipPromptBrowserTest&) =
      delete;
  PermissionChipPromptBrowserTest& operator=(
      const PermissionChipPromptBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_ =
        std::make_unique<test::PermissionRequestManagerTestApi>(browser());
    EXPECT_TRUE(test_api_->manager());
    test_api_->AddSimpleRequest(GetActiveMainFrame(),
                                ContentSettingsType::GEOLOCATION);

    base::RunLoop().RunUntilIdle();

    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    PermissionChip* permission_chip =
        browser_view->toolbar()->location_bar()->permission_chip();
    ASSERT_TRUE(permission_chip);

    views::test::ButtonTestApi(permission_chip->button())
        .NotifyClick(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                    gfx::Point(), ui::EventTimeForNow(),
                                    ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  content::RenderFrameHost* GetActiveMainFrame() {
    return browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PermissionChipPromptBrowserTest, InvokeUi_geolocation) {
  ShowAndVerifyUi();
}
