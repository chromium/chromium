// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"

class FeaturePromoDialogTest : public DialogBrowserTest {
 public:
  FeaturePromoDialogTest() = default;
  ~FeaturePromoDialogTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto* app_menu_button = BrowserView::GetBrowserViewForBrowser(browser())
                                ->toolbar_button_provider()
                                ->GetAppMenuButton();
    // We use one of the strings for the new tab feature promo since there is
    // currently no infrastructure for test-only string resources.
    int placeholder_string = IDS_NEWTAB_PROMO_0;
    FeaturePromoBubbleView::CreateOwned(
        app_menu_button, views::BubbleBorder::TOP_RIGHT,
        FeaturePromoBubbleView::ActivationAction::ACTIVATE, placeholder_string);
  }
};

IN_PROC_BROWSER_TEST_F(FeaturePromoDialogTest, InvokeUi_FeaturePromo) {
  ShowAndVerifyUi();
}
