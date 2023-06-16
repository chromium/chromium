// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "content/public/test/browser_test.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/test/ink_drop_host_test_api.h"

// TODO (spqchan): Refine tests. See crbug.com/770873.
class LocationIconViewBrowserTest : public InProcessBrowserTest {
 public:
  LocationIconViewBrowserTest() {}

  LocationIconViewBrowserTest(const LocationIconViewBrowserTest&) = delete;
  LocationIconViewBrowserTest& operator=(const LocationIconViewBrowserTest&) =
      delete;

  ~LocationIconViewBrowserTest() override {}

 protected:
  void SetUpOnMainThread() override {
    gfx::FontList font_list;
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    location_bar_ = browser_view->GetLocationBarView();
    icon_view_ = location_bar_->AddChildView(std::make_unique<LocationIconView>(
        font_list, location_bar_, location_bar_));
  }

  void TearDownOnMainThread() override {
    location_bar_ = nullptr;
    icon_view_ = nullptr;
  }

  LocationBarView* location_bar() const { return location_bar_; }

  LocationIconView* icon_view() const { return icon_view_; }

 private:
  raw_ptr<LocationBarView> location_bar_;
  raw_ptr<LocationIconView> icon_view_;
};

// Check to see if the InkDropMode is off when the omnibox is editing.
// Otherwise, it should be on.
IN_PROC_BROWSER_TEST_F(LocationIconViewBrowserTest, InkDropMode) {
  OmniboxEditModel* model = location_bar()->GetOmniboxView()->model();
  model->SetInputInProgress(true);
  icon_view()->Update(/*suppress_animations=*/true);

  EXPECT_EQ(views::InkDropHost::InkDropMode::OFF,
            views::test::InkDropHostTestApi(views::InkDrop::Get(icon_view()))
                .ink_drop_mode());

  model->SetInputInProgress(false);
  icon_view()->Update(/*suppress_animations=*/true);

  EXPECT_EQ(views::InkDropHost::InkDropMode::ON,
            views::test::InkDropHostTestApi(views::InkDrop::Get(icon_view()))
                .ink_drop_mode());
}
