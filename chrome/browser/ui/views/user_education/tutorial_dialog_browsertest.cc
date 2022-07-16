// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_view.h"
#include "content/public/test/browser_test.h"

class TutorialDialogTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    BrowserView* const browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());

    FeaturePromoBubbleView::CreateParams params;
    params.has_close_button = true;
    params.anchor_view = browser_view->toolbar()->app_menu_button();
    params.arrow = views::BubbleBorder::TOP_RIGHT;
    params.body_text = u"Hello world, I am a tutorial";
    params.persist_on_blur = true;
    params.tutorial_progress_current = 3;
    params.tutorial_progress_max = 5;
    params.timeout = base::TimeDelta();

    FeaturePromoBubbleView::Create(std::move(params));
  }
};

IN_PROC_BROWSER_TEST_F(TutorialDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
