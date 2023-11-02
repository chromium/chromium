// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/browser/sharesheet/sharesheet_ui_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller_chromeos_impl.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {

class SharingHubBubbleControllerChromeOsBrowserTest
    : public InProcessBrowserTest {
 public:
  SharingHubBubbleControllerChromeOsBrowserTest() = default;
  ~SharingHubBubbleControllerChromeOsBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SharingHubBubbleControllerChromeOsBrowserTest,
                       OpenSharesheet) {
  sharesheet::SharesheetService* sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(browser()->profile());
  gfx::NativeWindow web_contents_containing_window_ =
      browser()
          ->tab_strip_model()
          ->GetActiveWebContents()
          ->GetTopLevelNativeWindow();

  // Open the sharesheet using the sharing hub controller.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  sharing_hub::SharingHubBubbleControllerChromeOsImpl::
      CreateOrGetFromWebContents(web_contents)
          ->ShowBubble(share::ShareAttempt(web_contents));

  // Verify that the sharesheet is open.
  sharesheet::SharesheetUiDelegate* bubble_delegate =
      sharesheet_service->GetUiDelegateForTesting(
          web_contents_containing_window_);
  EXPECT_NE(bubble_delegate, nullptr);
  ASSERT_TRUE(bubble_delegate->IsBubbleVisible());
}

}  // namespace
