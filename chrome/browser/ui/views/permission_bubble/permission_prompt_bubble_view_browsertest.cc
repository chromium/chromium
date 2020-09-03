// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/ax_event_counter.h"

class PermissionPromptBubbleViewBrowserTest : public DialogBrowserTest {
 public:
  PermissionPromptBubbleViewBrowserTest() = default;

  PermissionPromptBubbleViewBrowserTest(
      const PermissionPromptBubbleViewBrowserTest&) = delete;
  PermissionPromptBubbleViewBrowserTest& operator=(
      const PermissionPromptBubbleViewBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_ =
        std::make_unique<test::PermissionRequestManagerTestApi>(browser());
    EXPECT_TRUE(test_api_->manager());
    test_api_->AddSimpleRequest(
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
        ContentSettingsType::GEOLOCATION);

    base::RunLoop().RunUntilIdle();
  }
};

IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleViewBrowserTest,
                       InvokeUi_geolocation) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleViewBrowserTest,
                       AlertAccessibleEvent) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));
  ShowUi("PermissionPromptBubble");
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
}
