// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_controls_progress_view.h"

#include "base/bind_helpers.h"
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
  ~MediaControlsProgressViewTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();

    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(300, 300);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_.Init(std::move(params));
    views::View* container = new views::View();
    widget_.SetContentsView(container);

    progress_view_ = new MediaControlsProgressView(base::DoNothing());
    container->AddChildView(progress_view_);

    widget_.Show();
  }

  void TearDown() override {
    widget_.Close();
    ViewsTestBase::TearDown();
  }

 protected:
  MediaControlsProgressView* progress_view_ = nullptr;

 private:
  views::Widget widget_;

  DISALLOW_COPY_AND_ASSIGN(MediaControlsProgressViewTest);
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
      1 /* playback_rate */, base::TimeDelta::FromSeconds(600) /* duration */,
      base::TimeDelta::FromSeconds(300) /* position */);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("10:00"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("05:00"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, InitProgressOverHour) {
  media_session::MediaPosition media_position(
      1 /* playback_rate */, base::TimeDelta::FromHours(2) /* duration */,
      base::TimeDelta::FromMinutes(30) /* position */);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("2:00:00"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("0:30:00"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .25);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, InitProgressOverDay) {
  media_session::MediaPosition media_position(
      1 /* playback_rate */, base::TimeDelta::FromHours(25) /* duration */,
      base::TimeDelta::FromHours(5) /* position */);

  progress_view_->UpdateProgress(media_position);

  // Verify that base::DURATION_WIDTH_NARROW time format is used here.
  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("25h 0m 0s"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("5h 0m 0s"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .2);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, UpdateProgress) {
  media_session::MediaPosition media_position(
      1 /* playback_rate */, base::TimeDelta::FromSeconds(600) /* duration */,
      base::TimeDelta::FromSeconds(300) /* position */);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("10:00"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("05:00"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);

  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(30));
  task_environment_->RunUntilIdle();

  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("10:00"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("05:30"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .55);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, UpdateProgressFastPlayback) {
  media_session::MediaPosition media_position(
      2 /* playback_rate */, base::TimeDelta::FromSeconds(600) /* duration */,
      base::TimeDelta::FromSeconds(300) /* position */);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("10:00"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("05:00"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);

  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(15));
  task_environment_->RunUntilIdle();

  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("10:00"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("05:30"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .55);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, UpdateProgressSlowPlayback) {
  media_session::MediaPosition media_position(
      .5 /* playback_rate */, base::TimeDelta::FromSeconds(600) /* duration */,
      base::TimeDelta::FromSeconds(300) /* position */);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("10:00"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("05:00"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);

  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(60));
  task_environment_->RunUntilIdle();

  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("10:00"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("05:30"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .55);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, UpdateProgressNegativePlayback) {
  media_session::MediaPosition media_position(
      -1 /* playback_rate */, base::TimeDelta::FromSeconds(600) /* duration */,
      base::TimeDelta::FromSeconds(300) /* position */);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("10:00"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("05:00"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);

  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(30));
  task_environment_->RunUntilIdle();

  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("10:00"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("04:30"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .45);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, UpdateProgressPastDuration) {
  media_session::MediaPosition media_position(
      1 /* playback_rate */, base::TimeDelta::FromSeconds(600) /* duration */,
      base::TimeDelta::FromSeconds(300) /* position */);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("10:00"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("05:00"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);

  // Move forward in time past the duration.
  task_environment_->FastForwardBy(base::TimeDelta::FromMinutes(6));
  task_environment_->RunUntilIdle();

  // Verify the progress does not go past the duration.
  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("10:00"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("10:00"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), 1);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, UpdateProgressBeforeStart) {
  media_session::MediaPosition media_position(
      -1 /* playback_rate */, base::TimeDelta::FromSeconds(600) /* duration */,
      base::TimeDelta::FromSeconds(300) /* position */);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("10:00"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("05:00"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);

  // Move forward in time before the start using negative playback rate.
  task_environment_->FastForwardBy(base::TimeDelta::FromMinutes(6));
  task_environment_->RunUntilIdle();

  // Verify the progress does not go below 0.
  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("10:00"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("00:00"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), 0);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, UpdateProgressPaused) {
  media_session::MediaPosition media_position(
      0 /* playback_rate */, base::TimeDelta::FromSeconds(600) /* duration */,
      base::TimeDelta::FromSeconds(300) /* position */);

  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("10:00"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("05:00"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);

  task_environment_->FastForwardBy(base::TimeDelta::FromMinutes(6));
  task_environment_->RunUntilIdle();

  // Verify the progress does not change while media is paused.
  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("10:00"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("05:00"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);
}

TEST_F(MAYBE_MediaControlsProgressViewTest, UpdateProgressTwice) {
  media_session::MediaPosition media_position(
      1 /* playback_rate */, base::TimeDelta::FromSeconds(600) /* duration */,
      base::TimeDelta::FromSeconds(300) /* position */);

  // Simulate first position change.
  progress_view_->UpdateProgress(media_position);

  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("10:00"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("05:00"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .5);

  media_session::MediaPosition new_media_position(
      1 /* playback_rate */, base::TimeDelta::FromSeconds(200) /* duration */,
      base::TimeDelta::FromSeconds(50) /* position */);

  // Simulate second position change.
  progress_view_->UpdateProgress(new_media_position);

  // Verify that the progress reflects the most recent position update.
  EXPECT_EQ(progress_view_->duration_for_testing(),
            base::ASCIIToUTF16("03:20"));
  EXPECT_EQ(progress_view_->progress_time_for_testing(),
            base::ASCIIToUTF16("00:50"));
  EXPECT_EQ(progress_view_->progress_bar_for_testing()->GetValue(), .25);
}

}  // namespace media_message_center
