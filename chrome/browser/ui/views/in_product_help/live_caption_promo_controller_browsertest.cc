// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/in_product_help/live_caption_promo_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "ui/events/base_event_utils.h"

class LiveCaptionPromoControllerTest : public InProcessBrowserTest {
 public:
  LiveCaptionPromoControllerTest() = default;
  ~LiveCaptionPromoControllerTest() override = default;
  LiveCaptionPromoControllerTest(const LiveCaptionPromoControllerTest&) =
      delete;
  LiveCaptionPromoControllerTest& operator=(
      const LiveCaptionPromoControllerTest&) = delete;

  // InProcessBrowserTest:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {media::kGlobalMediaControls,
         feature_engagement::kIPHLiveCaptionFeature},
        {});
    InProcessBrowserTest::SetUp();
  }

  bool PromoBubbleVisible() {
    FeaturePromoBubbleView* bubble = GetPromoController()->promo_bubble_;
    return bubble && bubble->GetVisible();
  }

  void ShowPromo() { GetPromoController()->ShowPromo(); }

  void DisableMediaButton() { GetPromoController()->OnMediaButtonDisabled(); }

  void OpenMediaDialog() { GetPromoController()->OnMediaDialogOpened(); }

 private:
  LiveCaptionPromoController* GetPromoController() {
    if (!controller_) {
      controller_ = std::make_unique<LiveCaptionPromoController>(
          BrowserView::GetBrowserViewForBrowser(browser()));
    }
    return controller_.get();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<LiveCaptionPromoController> controller_;
};

IN_PROC_BROWSER_TEST_F(LiveCaptionPromoControllerTest, ShowPromo) {
  EXPECT_FALSE(PromoBubbleVisible());

  ShowPromo();
  EXPECT_TRUE(PromoBubbleVisible());
}

IN_PROC_BROWSER_TEST_F(LiveCaptionPromoControllerTest, OpenMediaDialog) {
  EXPECT_FALSE(PromoBubbleVisible());

  ShowPromo();
  EXPECT_TRUE(PromoBubbleVisible());

  // Promo disappears when media dialog opens.
  OpenMediaDialog();
  EXPECT_FALSE(PromoBubbleVisible());
}

IN_PROC_BROWSER_TEST_F(LiveCaptionPromoControllerTest, DisableMediaButton) {
  EXPECT_FALSE(PromoBubbleVisible());

  ShowPromo();
  EXPECT_TRUE(PromoBubbleVisible());

  // Promo disappears when media button is disabled.
  DisableMediaButton();
  EXPECT_FALSE(PromoBubbleVisible());
}
