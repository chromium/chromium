// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/promos/promos_pref_names.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/promos/ios_promo_password_bubble.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "content/public/test/browser_test.h"

class IOSPromoPasswordBubbleTest : public DialogBrowserTest {
 public:
  IOSPromoPasswordBubbleTest() = default;

  IOSPromoPasswordBubbleTest(const IOSPromoPasswordBubbleTest&) = delete;

  IOSPromoPasswordBubbleTest& operator=(const IOSPromoPasswordBubbleTest&) =
      delete;

  void SetUp() override {
    // Activate kIPHiOSPasswordPromoDesktopFeature FET feature to successfully
    // dismiss it when the view closes.
    iph_features_.InitAndEnableFeaturesWithParameters(
        {{feature_engagement::kIPHiOSPasswordPromoDesktopFeature, {}}});

    InProcessBrowserTest::SetUp();
  }

  // DialogBrowserTest
  void ShowUi(const std::string& name) override {
    // Set a dummy value of 1 in promo impressions otherwise a
    // NOTREACHED() is hit.
    PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
    prefs->SetInteger(
        promos_prefs::kDesktopToiOSPasswordPromoImpressionsCounter, 1);

    ToolbarButtonProvider* button_provider =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider();

    IOSPromoPasswordBubble::ShowBubble(
        button_provider->GetAnchorView(PageActionIconType::kManagePasswords),
        button_provider->GetPageActionIconView(
            PageActionIconType::kManagePasswords),
        browser());
  }

 private:
  feature_engagement::test::ScopedIphFeatureList iph_features_;
};

IN_PROC_BROWSER_TEST_F(IOSPromoPasswordBubbleTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
