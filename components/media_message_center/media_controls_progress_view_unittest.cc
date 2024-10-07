// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_controls_progress_view.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/test/views_test_base.h"

namespace media_message_center {

class MediaControlsProgressViewTest : public views::ViewsTestBase {
 public:
  MediaControlsProgressViewTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  MediaControlsProgressViewTest(const MediaControlsProgressViewTest&) = delete;
  MediaControlsProgressViewTest& operator=(
      const MediaControlsProgressViewTest&) = delete;

  ~MediaControlsProgressViewTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();

    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(300, 300);
    widget_.Init(std::move(params));
    views::View* container =
        widget_.SetContentsView(std::make_unique<views::View>());

    progress_view_ =
        std::make_unique<MediaControlsProgressView>(base::BindRepeating(
            &MediaControlsProgressViewTest::SeekTo, base::Unretained(this)));

    container->AddChildView(progress_view_.get());

    widget_.Show();
  }

  void TearDown() override {
    widget_.Close();
    progress_view_.reset();
    ViewsTestBase::TearDown();
  }

  MOCK_METHOD1(SeekTo, void(double));

 protected:
  std::unique_ptr<MediaControlsProgressView> progress_view_;

 private:
  views::Widget widget_;
};

// TODO(crbug.com/40650520): many of these tests are failing on TSan builds.
#if defined(THREAD_SANITIZER)
#define MAYBE_MediaControlsProgressViewTest \
  DISABLED_MediaControlsProgressViewTest
class DISABLED_MediaControlsProgressViewTest
    : public MediaControlsProgressViewTest {};
#else
#define MAYBE_MediaControlsProgressViewTest MediaControlsProgressViewTest
#endif

TEST_F(MAYBE_MediaControlsProgressViewTest, InitProgress) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"05:00");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, InitProgressOverHour) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Hours(2),
      /*position=*/base::Minutes(30), /*end_of_media=*/false);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(), u"2:00:00");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"0:30:00");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .25);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, InitProgressOverDay) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Hours(25),
      /*position=*/base::Hours(5), /*end_of_media=*/false);

  progress_view_->UpdateProgress(media_position);

  // Verify that base::DURATION_WIDTH_NARROW time format is used here.
  EXPECT_EQ(progress_view_->duration_for_testing(), u"25h 0m 0s");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"5h 0m 0s");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .2);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, UpdateProgress) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"05:00");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);

  task_environment()->FastForwardBy(base::Seconds(30));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"05:30");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .55);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, UpdateProgressFastPlayback) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/2, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"05:00");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);

  task_environment()->FastForwardBy(base::Seconds(15));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"05:30");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .55);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, UpdateProgressSlowPlayback) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/.5, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"05:00");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);

  task_environment()->FastForwardBy(base::Seconds(60));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"05:30");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .55);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, UpdateProgressNegativePlayback) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/-1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"05:00");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);

  task_environment()->FastForwardBy(base::Seconds(30));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"04:30");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .45);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, UpdateProgressPastDuration) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"05:00");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);

  // Move forward in time past the duration.
  task_environment()->FastForwardBy(base::Minutes(6));
  task_environment()->RunUntilIdle();

  // Verify the progress does not go past the duration.
  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), 1);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, UpdateProgressBeforeStart) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/-1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"05:00");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);

  // Move forward in time before the start using negative playback rate.
  task_environment()->FastForwardBy(base::Minutes(6));
  task_environment()->RunUntilIdle();

  // Verify the progress does not go below 0.
  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"00:00");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), 0);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, UpdateProgressPaused) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/0, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"05:00");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);

  task_environment()->FastForwardBy(base::Minutes(6));
  task_environment()->RunUntilIdle();

  // Verify the progress does not change while media is paused.
  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"05:00");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, UpdateProgressTwice) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  // Simulate first position change.
  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"05:00");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);

  media_session::MediaPosition new_media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(200),
      /*position=*/base::Seconds(50), /*end_of_media=*/false);

  // Simulate second position change.
  progress_view_->UpdateProgress(new_media_position);

  // Verify that the progress reflects the most recent position update.
  EXPECT_EQ(progress_view_->duration_for_testing(), u"03:20");
  EXPECT_EQ(progress_view_->progress_time_for_testing(), u"00:50");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .25);
}

TEST_F(MAYBE_MediaControlsProgressViewTest,
       UpdateProgressWithInfiniteDuration) {
  media_session::MediaPosition media_position(/*playback_rate=*/1,
                                              /*duration=*/base::Seconds(600),
                                              /*position=*/base::Seconds(300),
                                              /*end_of_media=*/false);

  // Simulate a non-live position change.
  progress_view_->UpdateProgress(media_position);

  // Verify that the progress view reflects the position update.
  EXPECT_TRUE(progress_view_->is_duration_visible_for_testing());
  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);

  media_session::MediaPosition media_position_live(
      /*playback_rate=*/1,
      /*duration=*/base::TimeDelta::Max(),
      /*position=*/base::Seconds(300),
      /*end_of_media=*/false);

  // Simulate a position change with infinite duration. i.e. a live media.
  progress_view_->UpdateProgress(media_position_live);

  // Verify that duration view is hidden and progress bar is set correctly.
  EXPECT_FALSE(progress_view_->is_duration_visible_for_testing());
  EXPECT_EQ(progress_view_->duration_for_testing(), u"");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), 1.0);

  // Simulate another non-live position change.
  progress_view_->UpdateProgress(media_position);

  // Verify that the progress view is back to its normal state.
  EXPECT_TRUE(progress_view_->is_duration_visible_for_testing());
  EXPECT_EQ(progress_view_->duration_for_testing(), u"10:00");
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, SeekTo) {
  progress_view_->SetBounds(0, 0, 300, 40);

  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);
  progress_view_->UpdateProgress(media_position);

  gfx::Point point(progress_view_->width() / 2, progress_view_->height() / 2);
  ui::MouseEvent pressed_event(ui::EventType::kMousePressed, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);

  // Simulate a mouse click event and SeekTo callback should be called.
  EXPECT_CALL(*this, SeekTo(0.5));
  progress_view_->OnMousePressed(pressed_event);

  media_session::MediaPosition media_position_live(
      /*playback_rate=*/1, /*duration=*/base::TimeDelta::Max(),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  // Simulate a position change with infinite duration. i.e. a live media.
  progress_view_->UpdateProgress(media_position_live);

  // Simulate a mouse click event and SeekTo callback should not be called.
  EXPECT_CALL(*this, SeekTo(testing::_)).Times(0);
  progress_view_->OnMousePressed(pressed_event);
}

}  // namespace media_message_center
