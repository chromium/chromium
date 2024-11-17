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

class IOSPasswordPromoBubbleTest : public DialogBrowserTest {
 public:
  IOSPasswordPromoBubbleTest() = default;

  IOSPasswordPromoBubbleTest(const IOSPasswordPromoBubbleTest&) = delete;

  IOSPasswordPromoBubbleTest& operator=(const IOSPasswordPromoBubbleTest&) =
      delete;

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
        browser()->profile(), IOSPromoType::kPassword);
  }
};

class IOSAddressPromoBubbleTest : public DialogBrowserTest {
 public:
  IOSAddressPromoBubbleTest() = default;

  IOSAddressPromoBubbleTest(const IOSAddressPromoBubbleTest&) = delete;

  IOSAddressPromoBubbleTest& operator=(const IOSAddressPromoBubbleTest&) =
      delete;

  // DialogBrowserTest
  void ShowUi(const std::string& name) override {
    ToolbarButtonProvider* button_provider =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider();
    // Test for iOS Promo Bubble for Desktop Address promo.
    IOSPromoBubble::ShowPromoBubble(
        button_provider->GetAnchorView(PageActionIconType::kAutofillAddress),
        button_provider->GetPageActionIconView(
            PageActionIconType::kAutofillAddress),
        browser()->profile(), IOSPromoType::kAddress);
  }
};

class IOSPaymentPromoBubbleTest : public DialogBrowserTest {
 public:
  IOSPaymentPromoBubbleTest() = default;

  IOSPaymentPromoBubbleTest(const IOSPaymentPromoBubbleTest&) = delete;

  IOSPaymentPromoBubbleTest& operator=(const IOSPaymentPromoBubbleTest&) =
      delete;

  // DialogBrowserTest
  void ShowUi(const std::string& name) override {
    ToolbarButtonProvider* button_provider =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider();
    // Test for iOS Promo Bubble for Desktop Payment promo.
    IOSPromoBubble::ShowPromoBubble(
        button_provider->GetAnchorView(PageActionIconType::kSaveCard),
        button_provider->GetPageActionIconView(PageActionIconType::kSaveCard),
        browser()->profile(), IOSPromoType::kPayment);
  }
};

IN_PROC_BROWSER_TEST_F(IOSPasswordPromoBubbleTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(IOSAddressPromoBubbleTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(IOSPaymentPromoBubbleTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
