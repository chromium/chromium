// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_squiggly_progress_view.h"

#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/views_test_base.h"

namespace media_message_center {

class MediaSquigglyProgressViewTest : public views::ViewsTestBase {
 public:
  MediaSquigglyProgressViewTest() = default;
  MediaSquigglyProgressViewTest(const MediaSquigglyProgressViewTest&) = delete;
  MediaSquigglyProgressViewTest& operator=(
      const MediaSquigglyProgressViewTest&) = delete;
  ~MediaSquigglyProgressViewTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    view_ =
        widget_->SetContentsView(std::make_unique<MediaSquigglyProgressView>(
            SK_ColorRED, SK_ColorRED,
            base::BindRepeating(&MediaSquigglyProgressViewTest::SeekTo,
                                base::Unretained(this))));

    widget_->SetBounds(gfx::Rect(500, 500));
    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  MediaSquigglyProgressView* view() const { return view_; }

  MOCK_METHOD1(SeekTo, void(double));

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<MediaSquigglyProgressView> view_ = nullptr;
};

TEST_F(MediaSquigglyProgressViewTest, MediaPlaying) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  view()->UpdateProgress(media_position);

  EXPECT_NEAR(view()->current_value_for_testing(), 0.5, 0.001);
  EXPECT_FALSE(view()->is_paused_for_testing());
  EXPECT_FALSE(view()->is_live_for_testing());
}

TEST_F(MediaSquigglyProgressViewTest, MediaPaused) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/0, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(150), /*end_of_media=*/false);

  view()->UpdateProgress(media_position);

  EXPECT_NEAR(view()->current_value_for_testing(), 0.25, 0.001);
  EXPECT_TRUE(view()->is_paused_for_testing());
  EXPECT_FALSE(view()->is_live_for_testing());
}

TEST_F(MediaSquigglyProgressViewTest, SeekTo) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);
  view()->UpdateProgress(media_position);

  // Simulate a mouse click event and SeekTo() should be called.
  gfx::Point point(view()->width() / 2, view()->height() / 2);
  ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, SeekTo(0.5));
  view()->OnMousePressed(pressed_event);

  // Simulate a gesture tap event and SeekTo() should be called.
  ui::GestureEvent tapped_event(point.x(), point.y(), 0, ui::EventTimeForNow(),
                                ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  EXPECT_CALL(*this, SeekTo(0.5));
  view()->OnGestureEvent(&tapped_event);

  // Simulate a position change with infinite duration. i.e. a live media.
  media_session::MediaPosition media_position_live(
      /*playback_rate=*/1, /*duration=*/base::TimeDelta::Max(),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);
  view()->UpdateProgress(media_position_live);

  // Simulate a mouse click event and SeekTo() should not be called.
  EXPECT_CALL(*this, SeekTo(testing::_)).Times(0);
  view()->OnMousePressed(pressed_event);
  EXPECT_TRUE(view()->is_live_for_testing());
}

}  // namespace media_message_center
