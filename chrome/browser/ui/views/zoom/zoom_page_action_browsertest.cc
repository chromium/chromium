// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/views/view.h"

namespace zoom {
namespace {

class ZoomPageActionBrowserTest : public InProcessBrowserTest {
 public:
  ZoomPageActionBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kPageActionsMigration);
  }

 protected:
  page_actions::PageActionView* GetZoomIcon(ToolbarButtonProvider* provider) {
    return provider->GetPageActionView(kActionZoomNormal);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ZoomPageActionBrowserTest, ZoomStateUpdates) {
  page_actions::PageActionView* zoom_icon =
      GetZoomIcon(BrowserView::GetBrowserViewForBrowser(browser())
                      ->toolbar_button_provider());

  chrome::Zoom(browser(), content::PAGE_ZOOM_IN);
  EXPECT_TRUE(zoom_icon->GetVisible());
  EXPECT_EQ(zoom_icon->GetTooltipText(), u"Zoom: 110%");
  ui::ImageModel zoom_in_image =
      zoom_icon->GetImageModel(views::Button::STATE_NORMAL).value();

  chrome::Zoom(browser(), content::PAGE_ZOOM_RESET);
  EXPECT_FALSE(zoom_icon->GetVisible());
  EXPECT_EQ(zoom_icon->GetTooltipText(), u"Zoom: 100%");

  chrome::Zoom(browser(), content::PAGE_ZOOM_OUT);
  EXPECT_TRUE(zoom_icon->GetVisible());
  EXPECT_EQ(zoom_icon->GetTooltipText(), u"Zoom: 90%");
  ui::ImageModel zoom_out_image =
      zoom_icon->GetImageModel(views::Button::STATE_NORMAL).value();
  EXPECT_NE(zoom_out_image, zoom_in_image);
}

}  // namespace
}  // namespace zoom
