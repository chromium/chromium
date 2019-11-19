// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/feature_promos/reopen_tab_promo_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/animation/ink_drop.h"
#include "url/gurl.h"

class ReopenTabPromoControllerDialogBrowserTest : public DialogBrowserTest {
 public:
  ReopenTabPromoControllerDialogBrowserTest() {
    feature_list_.InitAndEnableFeature(
        feature_engagement::kIPHReopenTabFeature);
  }

  void SetUpOnMainThread() override {
    promo_controller_ = std::make_unique<ReopenTabPromoController>(
        BrowserView::GetBrowserViewForBrowser(browser()));
    promo_controller_->disable_bubble_timeout_for_test();
  }

  void ShowUi(const std::string& name) override {
    promo_controller_->ShowPromo();
  }

 private:
  std::unique_ptr<ReopenTabPromoController> promo_controller_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReopenTabPromoControllerDialogBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
