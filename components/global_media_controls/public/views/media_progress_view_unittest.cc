// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_progress_view.h"

#include "base/i18n/rtl.h"
#include "base/time/time.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/views/test/views_test_base.h"

namespace global_media_controls {

namespace {

constexpr int kWidthInset = 8;

}  // namespace

class MediaProgressViewTest : public views::ViewsTestBase {
 public:
  MediaProgressViewTest() = default;
  MediaProgressViewTest(const MediaProgressViewTest&) = delete;
  MediaProgressViewTest& operator=(const MediaProgressViewTest&) = delete;
  ~MediaProgressViewTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    // This test just needs to construct a progress view, without caring about
    // what specific color IDs are used, so just use an arbitrary value.
    ui::ColorId id = ui::kUiColorsStart;
    view_ = widget_->SetContentsView(std::make_unique<MediaProgressView>(
        true, id, id, id, id, id,
        base::BindRepeating(&MediaProgressViewTest::OnProgressDragging,
                            base::Unretained(this)),
        base::BindRepeating(&MediaProgressViewTest::SeekTo,
                            base::Unretained(this)),
        base::BindRepeating(&MediaProgressViewTest::OnProgressUpdated,
                            base::Unretained(this))));

    widget_->SetBounds(gfx::Rect(500, 500));
    widget_->Show();
    default_locale_ = base::i18n::GetConfiguredLocale();
  }

  void TearDown() override {
    view_ = nullptr;
    widget_.reset();
    base::i18n::SetICUDefaultLocale(default_locale_);
    ViewsTestBase::TearDown();
  }

  MediaProgressView* view() const { return view_; }

  MOCK_METHOD1(OnProgressDragging, void(bool));
  MOCK_METHOD1(SeekTo, void(double));
  MOCK_METHOD1(OnProgressUpdated, void(base::TimeDelta));

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<MediaProgressView> view_ = nullptr;
  std::string default_locale_;
};

TEST_F(MediaProgressViewTest, MediaPlaying) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  view()->UpdateProgress(media_position);

  EXPECT_NEAR(view()->current_value_for_testing(), 0.5, 0.001);
  EXPECT_FALSE(view()->is_paused_for_testing());
  EXPECT_FALSE(view()->is_live_for_testing());
}

TEST_F(MediaProgressViewTest, MediaPaused) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/0, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(150), /*end_of_media=*/false);

  view()->UpdateProgress(media_position);

  EXPECT_NEAR(view()->current_value_for_testing(), 0.25, 0.001);
  EXPECT_TRUE(view()->is_paused_for_testing());
  EXPECT_FALSE(view()->is_live_for_testing());
}

TEST_F(MediaProgressViewTest, MouseEventSeekTo) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(100), /*end_of_media=*/false);
  view()->UpdateProgress(media_position);

  // Simulate mouse click and release events and SeekTo() should be called.
  gfx::Point point(view()->width() / 2, view()->height() / 2);
  ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, SeekTo(0.5));
  EXPECT_CALL(*this, OnProgressDragging(true));
  view()->OnMousePressed(pressed_event);

  ui::MouseEvent released_event =
      ui::MouseEvent(ui::ET_MOUSE_RELEASED, point, point, ui::EventTimeForNow(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, SeekTo(0.5));
  EXPECT_CALL(*this, OnProgressDragging(false));
  view()->OnMouseReleased(released_event);

  // Simulate a position change with infinite duration. i.e. a live media.
  media_session::MediaPosition media_position_live(
      /*playback_rate=*/1, /*duration=*/base::TimeDelta::Max(),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);
  view()->UpdateProgress(media_position_live);

  // Simulate a mouse click event and SeekTo() should not be called.
  EXPECT_CALL(*this, SeekTo(testing::_)).Times(0);
  EXPECT_CALL(*this, OnProgressDragging(testing::_)).Times(0);
  view()->OnMousePressed(pressed_event);
  EXPECT_TRUE(view()->is_live_for_testing());
}

TEST_F(MediaProgressViewTest, MouseEventSeekToForRTL) {
  base::i18n::SetICUDefaultLocale("he");

  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(100), /*end_of_media=*/false);
  view()->UpdateProgress(media_position);

  // Simulate mouse click and release events and SeekTo() should be called.
  gfx::Point point(view()->width() / 4 + kWidthInset / 2, view()->height() / 2);
  ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, SeekTo(testing::DoubleNear(0.75, 0.01)));
  EXPECT_CALL(*this, OnProgressDragging(true));
  view()->OnMousePressed(pressed_event);

  ui::MouseEvent released_event =
      ui::MouseEvent(ui::ET_MOUSE_RELEASED, point, point, ui::EventTimeForNow(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, SeekTo(testing::DoubleNear(0.75, 0.01)));
  EXPECT_CALL(*this, OnProgressDragging(false));
  view()->OnMouseReleased(released_event);
}

TEST_F(MediaProgressViewTest, GestureEventSeekTo) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(100), /*end_of_media=*/false);
  view()->UpdateProgress(media_position);

  // Simulate gesture tap events and SeekTo() should be called.
  gfx::Point point(view()->width() / 2, view()->height() / 2);
  ui::GestureEvent tapped_event(
      point.x(), point.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));
  EXPECT_CALL(*this, SeekTo(0.5));
  EXPECT_CALL(*this, OnProgressDragging(true));
  view()->OnGestureEvent(&tapped_event);

  ui::GestureEvent released_event(point.x(), point.y(), 0,
                                  ui::EventTimeForNow(),
                                  ui::GestureEventDetails(ui::ET_GESTURE_END));
  EXPECT_CALL(*this, SeekTo(0.5));
  EXPECT_CALL(*this, OnProgressDragging(false));
  view()->OnGestureEvent(&released_event);
}

TEST_F(MediaProgressViewTest, KeyEventSeekBackward) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(60),
      /*position=*/base::Seconds(35), /*end_of_media=*/false);
  view()->UpdateProgress(media_position);

  ui::KeyEvent key_event{ui::ET_KEY_PRESSED,      ui::VKEY_LEFT,
                         ui::DomCode::ARROW_LEFT, ui::EF_NONE,
                         ui::DomKey::ARROW_LEFT,  ui::EventTimeForNow()};
  EXPECT_CALL(*this, SeekTo(testing::DoubleNear(0.5, 0.01)));
  view()->OnKeyPressed(key_event);
}

TEST_F(MediaProgressViewTest, KeyEventSeekBackwardToBeginning) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(60),
      /*position=*/base::Seconds(3), /*end_of_media=*/false);
  view()->UpdateProgress(media_position);

  ui::KeyEvent key_event{ui::ET_KEY_PRESSED,      ui::VKEY_DOWN,
                         ui::DomCode::ARROW_DOWN, ui::EF_NONE,
                         ui::DomKey::ARROW_DOWN,  ui::EventTimeForNow()};
  EXPECT_CALL(*this, SeekTo(0));
  view()->OnKeyPressed(key_event);
}

TEST_F(MediaProgressViewTest, KeyEventSeekForward) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(60),
      /*position=*/base::Seconds(25), /*end_of_media=*/false);
  view()->UpdateProgress(media_position);

  ui::KeyEvent key_event{ui::ET_KEY_PRESSED,       ui::VKEY_RIGHT,
                         ui::DomCode::ARROW_RIGHT, ui::EF_NONE,
                         ui::DomKey::ARROW_RIGHT,  ui::EventTimeForNow()};
  EXPECT_CALL(*this, SeekTo(testing::DoubleNear(0.5, 0.01)));
  view()->OnKeyPressed(key_event);
}

TEST_F(MediaProgressViewTest, KeyEventSeekForwardToEnd) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(60),
      /*position=*/base::Seconds(57), /*end_of_media=*/false);
  view()->UpdateProgress(media_position);

  ui::KeyEvent key_event{ui::ET_KEY_PRESSED,    ui::VKEY_UP,
                         ui::DomCode::ARROW_UP, ui::EF_NONE,
                         ui::DomKey::ARROW_UP,  ui::EventTimeForNow()};
  EXPECT_CALL(*this, SeekTo(1));
  view()->OnKeyPressed(key_event);
}

TEST_F(MediaProgressViewTest, DragProgressForPlayingMedia) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(100), /*end_of_media=*/false);
  view()->UpdateProgress(media_position);

  gfx::Point point(view()->width() / 2, view()->height() / 2);
  ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, SeekTo(0.5));
  EXPECT_CALL(*this, OnProgressDragging(true));
  view()->OnMousePressed(pressed_event);

  ui::MouseEvent dragged_event(ui::ET_MOUSE_DRAGGED, gfx::Point(), gfx::Point(),
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, SeekTo(0));
  view()->OnMouseDragged(dragged_event);

  ui::MouseEvent released_event = ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, SeekTo(0));
  EXPECT_CALL(*this, OnProgressDragging(false));
  view()->OnMouseReleased(released_event);
}

TEST_F(MediaProgressViewTest, DragProgressForPausedMedia) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/0, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(100), /*end_of_media=*/false);
  view()->UpdateProgress(media_position);

  gfx::Point point(view()->width() / 2, view()->height() / 2);
  ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, SeekTo(0.5));
  EXPECT_CALL(*this, OnProgressDragging(testing::_)).Times(0);
  view()->OnMousePressed(pressed_event);

  gfx::Point new_point(view()->width() / 4, view()->height() / 2);
  ui::MouseEvent dragged_event(ui::ET_MOUSE_DRAGGED, new_point, new_point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, SeekTo(testing::DoubleNear(0.25, 0.01)));
  view()->OnMouseDragged(dragged_event);

  ui::MouseEvent released_event = ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, new_point, new_point, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, SeekTo(testing::DoubleNear(0.25, 0.01)));
  EXPECT_CALL(*this, OnProgressDragging(testing::_)).Times(0);
  view()->OnMouseReleased(released_event);
}

TEST_F(MediaProgressViewTest, UpdateProgress) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(150), /*end_of_media=*/false);

  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(media_position);
}

}  // namespace global_media_controls
