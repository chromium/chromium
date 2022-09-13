// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_controls_progress_view.h"

#include <memory>

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
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
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(300, 300);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_.Init(std::move(params));
    views::View* container =
        widget_.SetContentsView(std::make_unique<views::View>());

    progress_view_ = new MediaControlsProgressView(base::DoNothing());
    container->AddChildView(progress_view_.get());

    widget_.Show();
  }

  void TearDown() override {
    widget_.Close();
    ViewsTestBase::TearDown();
  }

 protected:
  raw_ptr<MediaControlsProgressView> progress_view_ = nullptr;

 private:
  views::Widget widget_;
};

// TODO(crbug.com/1009356): many of these tests are failing on TSan builds.
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

// Flaky on multiple platforms. crbug.com/1293864
TEST_F(MAYBE_MediaControlsProgressViewTest,
       DISABLED_UpdateProgressFastPlayback) {
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

}  // namespace media_message_center
