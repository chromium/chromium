// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_item_ui_cast_footer_view.h"

#include "base/test/mock_callback.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/button_test_api.h"

using testing::NiceMock;

class MediaItemUICastFooterViewTest : public ChromeViewsTestBase {
 public:
  MediaItemUICastFooterViewTest() = default;
  ~MediaItemUICastFooterViewTest() override = default;

  void CreateView() {
    view_ = std::make_unique<MediaItemUICastFooterView>(
        mock_stop_casting_callback_.Get(),
        media_message_center::MediaColorTheme());
  }

  MediaItemUICastFooterView* view() { return view_.get(); }
  views::Button* stop_casting_button() {
    return view_->GetStopCastingButtonForTesting();
  }
  base::MockRepeatingClosure* mock_stop_casting_callback() {
    return &mock_stop_casting_callback_;
  }

 private:
  NiceMock<base::MockRepeatingClosure> mock_stop_casting_callback_;
  std::unique_ptr<MediaItemUICastFooterView> view_;
};

TEST_F(MediaItemUICastFooterViewTest, ClickingOnStopCastingButton) {
  CreateView();
  EXPECT_TRUE(stop_casting_button());
  EXPECT_TRUE(stop_casting_button()->GetEnabled());

  EXPECT_CALL(*mock_stop_casting_callback(), Run());
  views::test::ButtonTestApi(stop_casting_button())
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  EXPECT_FALSE(stop_casting_button()->GetEnabled());
}
