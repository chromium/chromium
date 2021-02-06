// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "content/public/test/browser_test.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"

// TODO (spqchan): Refine tests. See crbug.com/770873.
class LocationIconViewBrowserTest : public InProcessBrowserTest {
 public:
  LocationIconViewBrowserTest() {}
  ~LocationIconViewBrowserTest() override {}

 protected:
  void SetUpOnMainThread() override {
    gfx::FontList font_list;
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    location_bar_ = browser_view->GetLocationBarView();
    icon_view_ = std::make_unique<LocationIconView>(font_list, location_bar_,
                                                    location_bar_);
  }

  LocationBarView* location_bar() const { return location_bar_; }

  LocationIconView* icon_view() const { return icon_view_.get(); }

 private:
  LocationBarView* location_bar_;

  std::unique_ptr<LocationIconView> icon_view_;

  DISALLOW_COPY_AND_ASSIGN(LocationIconViewBrowserTest);
};

// Check to see if the InkDropMode is off when the omnibox is editing.
// Otherwise, it should be on.
IN_PROC_BROWSER_TEST_F(LocationIconViewBrowserTest, InkDropMode) {
  OmniboxEditModel* model = location_bar()->GetOmniboxView()->model();
  model->SetInputInProgress(true);
  icon_view()->Update(/*suppress_animations=*/true);

  EXPECT_EQ(IconLabelBubbleView::InkDropMode::OFF,
            views::test::InkDropHostViewTestApi(icon_view()).ink_drop_mode());

  model->SetInputInProgress(false);
  icon_view()->Update(/*suppress_animations=*/true);

  EXPECT_EQ(IconLabelBubbleView::InkDropMode::ON,
            views::test::InkDropHostViewTestApi(icon_view()).ink_drop_mode());
}
