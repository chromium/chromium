// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/content_setting_site_row_view.h"

#include "base/test/mock_callback.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "net/base/schemeful_site.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget.h"

class ContentSettingSiteRowViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  views::Widget* widget() { return widget_.get(); }

  ui::MouseEvent click_event() {
    return ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                          gfx::Point(), ui::EventTimeForNow(), 0, 0);
  }

 private:
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(ContentSettingSiteRowViewTest, ClickToggle) {
  net::SchemefulSite site(GURL("https://example.com"));
  base::MockRepeatingCallback<void(const net::SchemefulSite& site,
                                   bool allowed)>
      callback;
  EXPECT_CALL(callback, Run(site, false));

  ContentSettingSiteRowView* view =
      widget()->SetContentsView(std::make_unique<ContentSettingSiteRowView>(
          /*favicon_service=*/nullptr, site, true, callback.Get()));
  EXPECT_TRUE(view->GetToggleForTesting()->GetIsOn());
  views::test::ButtonTestApi(view->GetToggleForTesting())
      .NotifyClick(click_event());
  EXPECT_FALSE(view->GetToggleForTesting()->GetIsOn());
}
