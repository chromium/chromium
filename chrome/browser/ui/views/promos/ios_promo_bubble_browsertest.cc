// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/promos/ios_promo_bubble.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/promos/promos_pref_names.h"
#include "chrome/browser/promos/promos_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "content/public/test/browser_test.h"

class IOSPromoBubbleTest : public DialogBrowserTest {
 public:
  IOSPromoBubbleTest() = default;

  IOSPromoBubbleTest(const IOSPromoBubbleTest&) = delete;

  IOSPromoBubbleTest& operator=(const IOSPromoBubbleTest&) = delete;

  // DialogBrowserTest
  void ShowUi(const std::string& name) override {
    ToolbarButtonProvider* button_provider =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider();
    // Test for iOS Promo Bubble for Desktop Passwords promo.
    IOSPromoBubble::ShowPromoBubble(
        button_provider->GetAnchorView(PageActionIconType::kManagePasswords),
        button_provider->GetPageActionIconView(
            PageActionIconType::kManagePasswords),
        browser(), IOSPromoType::kPassword);
  }
};

IN_PROC_BROWSER_TEST_F(IOSPromoBubbleTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
