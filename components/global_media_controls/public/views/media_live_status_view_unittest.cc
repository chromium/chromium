// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_live_status_view.h"

#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"

namespace global_media_controls {

namespace {

ui::ColorId kForegroundColorId = ui::kColorSysOnPrimary;
ui::ColorId kBackgroundColorId = ui::kColorSysPrimary;

}  // anonymous namespace

class MediaLiveStatusViewTest : public views::ViewsTestBase {
 public:
  MediaLiveStatusViewTest() = default;
  MediaLiveStatusViewTest(const MediaLiveStatusViewTest&) = delete;
  MediaLiveStatusViewTest& operator=(const MediaLiveStatusViewTest&) = delete;
  ~MediaLiveStatusViewTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    view_ = widget_->SetContentsView(std::make_unique<MediaLiveStatusView>(
        kForegroundColorId, kBackgroundColorId));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    widget_->Close();
    views::ViewsTestBase::TearDown();
  }

  MediaLiveStatusView* view() { return view_; }

 private:
  raw_ptr<MediaLiveStatusView> view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(MediaLiveStatusViewTest, ViewCheck) {
  EXPECT_TRUE(view()->GetLineViewForTesting()->GetVisible());
  EXPECT_TRUE(view()->GetLiveLabelForTesting()->GetVisible());
  EXPECT_EQ(kForegroundColorId,
            view()->GetLiveLabelForTesting()->GetEnabledColorId());
}

}  // namespace global_media_controls
