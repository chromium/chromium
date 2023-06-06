// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/promos/ios_promo_password_bubble.h"
#include "content/public/test/browser_test.h"

class IOSPromoPasswordBubbleTest : public DialogBrowserTest {
 public:
  IOSPromoPasswordBubbleTest() = default;

  IOSPromoPasswordBubbleTest(const IOSPromoPasswordBubbleTest&) = delete;

  IOSPromoPasswordBubbleTest& operator=(const IOSPromoPasswordBubbleTest&) =
      delete;

  // DialogBrowserTest
  void ShowUi(const std::string& name) override {
    ToolbarButtonProvider* button_provider =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider();

    if (name == "qr_code") {
      IOSPromoPasswordBubble::ShowBubble(
          button_provider->GetAnchorView(PageActionIconType::kManagePasswords),
          button_provider->GetPageActionIconView(
              PageActionIconType::kManagePasswords),
          IOSPromoPasswordBubble::PromoVariant::QR_CODE_VARIANT, browser());
    } else {
      // default
      IOSPromoPasswordBubble::ShowBubble(
          button_provider->GetAnchorView(PageActionIconType::kManagePasswords),
          button_provider->GetPageActionIconView(
              PageActionIconType::kManagePasswords),
          IOSPromoPasswordBubble::PromoVariant::GET_STARTED_BUTTON_VARIANT,
          browser());
    }
  }
};

IN_PROC_BROWSER_TEST_F(IOSPromoPasswordBubbleTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(IOSPromoPasswordBubbleTest, InvokeUi_qr_code) {
  ShowAndVerifyUi();
}
