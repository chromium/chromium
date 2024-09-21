// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_progress_view.h"

#include "base/i18n/rtl.h"
#include "base/timer/mock_timer.h"
#include "components/strings/grit/components_strings.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/views/accessibility/view_accessibility.h"
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
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    // This test just needs to construct a progress view, without caring about
    // what specific color IDs are used, so just use an arbitrary value.
    ui::ColorId id = ui::kUiColorsStart;
    view_ = widget_->SetContentsView(std::make_unique<MediaProgressView>(
        true, id, id, id, id, id,
        base::BindRepeating(&MediaProgressViewTest::OnProgressDragStateChange,
                            base::Unretained(this)),
        base::BindRepeating(
            &MediaProgressViewTest::OnPlaybackStateChangeForProgressDrag,
            base::Unretained(this)),
        base::BindRepeating(&MediaProgressViewTest::SeekTo,
                            base::Unretained(this)),
        base::BindRepeating(&MediaProgressViewTest::OnProgressUpdated,
                            base::Unretained(this))));

    widget_->SetBounds(gfx::Rect(500, 500));
    widget_->Show();

    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    update_progress_timer_ = mock_timer.get();
    view_->set_update_progress_timer_for_testing(std::move(mock_timer));

    mock_timer = std::make_unique<base::MockOneShotTimer>();
    switch_progress_colors_delay_timer_ = mock_timer.get();
    view_->set_switch_progress_colors_delay_timer_for_testing(
        std::move(mock_timer));

    mock_timer = std::make_unique<base::MockOneShotTimer>();
    progress_drag_started_delay_timer_ = mock_timer.get();
    view_->set_progress_drag_started_delay_timer_for_testing(
        std::move(mock_timer));

    default_locale_ = base::i18n::GetConfiguredLocale();
  }

  void TearDown() override {
    update_progress_timer_ = nullptr;
    switch_progress_colors_delay_timer_ = nullptr;
    progress_drag_started_delay_timer_ = nullptr;
    view_ = nullptr;
    widget_->Close();
    base::i18n::SetICUDefaultLocale(default_locale_);
    ViewsTestBase::TearDown();
  }

  MediaProgressView* view() const { return view_; }
  base::MockOneShotTimer* update_progress_timer() const {
    return update_progress_timer_;
  }
  base::MockOneShotTimer* switch_progress_colors_delay_timer() const {
    return switch_progress_colors_delay_timer_;
  }
  base::MockOneShotTimer* progress_drag_started_delay_timer() const {
    return progress_drag_started_delay_timer_;
  }

  MOCK_METHOD1(OnProgressDragStateChange, void(DragState));
  MOCK_METHOD1(OnPlaybackStateChangeForProgressDrag,
               void(PlaybackStateChangeForDragging));
  MOCK_METHOD1(SeekTo, void(double));
  MOCK_METHOD1(OnProgressUpdated, void(base::TimeDelta));

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<MediaProgressView> view_ = nullptr;
  raw_ptr<base::MockOneShotTimer> update_progress_timer_ = nullptr;
  raw_ptr<base::MockOneShotTimer> switch_progress_colors_delay_timer_ = nullptr;
  raw_ptr<base::MockOneShotTimer> progress_drag_started_delay_timer_ = nullptr;
  std::string default_locale_;
};

TEST_F(MediaProgressViewTest, MediaPlaying) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);
  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(media_position);

  EXPECT_NEAR(view()->current_value_for_testing(), 0.5, 0.001);
  EXPECT_FALSE(view()->is_paused_for_testing());
  EXPECT_FALSE(view()->is_live_for_testing());
  EXPECT_TRUE(update_progress_timer()->IsRunning());
}

TEST_F(MediaProgressViewTest, AccessibleProperties) {
  ui::AXNodeData data;
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kSlider);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(
                IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_TIME_SCRUBBER));
}

TEST_F(MediaProgressViewTest, MediaPaused) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/0, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(150), /*end_of_media=*/false);
  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(media_position);

  EXPECT_NEAR(view()->current_value_for_testing(), 0.25, 0.001);
  EXPECT_TRUE(view()->is_paused_for_testing());
  EXPECT_FALSE(view()->is_live_for_testing());
  EXPECT_FALSE(update_progress_timer()->IsRunning());
}

TEST_F(MediaProgressViewTest, MouseClickEventSeekTo) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(100), /*end_of_media=*/false);
  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(media_position);

  // Simulate mouse click and release events and SeekTo() should be called once.
  gfx::Point point(view()->width() / 2, view()->height() / 2);
  ui::MouseEvent pressed_event(ui::EventType::kMousePressed, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, OnProgressDragStateChange(testing::_)).Times(0);
  EXPECT_CALL(*this, OnPlaybackStateChangeForProgressDrag(testing::_)).Times(0);
  EXPECT_CALL(*this, SeekTo(testing::_)).Times(0);
  view()->OnMousePressed(pressed_event);
  EXPECT_TRUE(progress_drag_started_delay_timer()->IsRunning());

  ui::MouseEvent released_event = ui::MouseEvent(
      ui::EventType::kMouseReleased, point, point, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, OnProgressDragStateChange(testing::_)).Times(0);
  EXPECT_CALL(*this, OnPlaybackStateChangeForProgressDrag(testing::_)).Times(0);
  EXPECT_CALL(*this, SeekTo(0.5));
  view()->OnMouseReleased(released_event);
  EXPECT_FALSE(progress_drag_started_delay_timer()->IsRunning());
}

TEST_F(MediaProgressViewTest, MouseLongPressEventSeekTo) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(100), /*end_of_media=*/false);
  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(media_position);

  // Simulate mouse press and release events and SeekTo() should be called
  // twice.
  gfx::Point point(view()->width() / 2, view()->height() / 2);
  ui::MouseEvent pressed_event(ui::EventType::kMousePressed, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, OnProgressDragStateChange(DragState::kDragStarted));
  EXPECT_CALL(*this,
              OnPlaybackStateChangeForProgressDrag(
                  PlaybackStateChangeForDragging::kPauseForDraggingStarted));
  EXPECT_CALL(*this, SeekTo(0.5));
  view()->OnMousePressed(pressed_event);
  progress_drag_started_delay_timer()->Fire();

  ui::MouseEvent released_event = ui::MouseEvent(
      ui::EventType::kMouseReleased, point, point, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, OnProgressDragStateChange(DragState::kDragEnded));
  EXPECT_CALL(*this,
              OnPlaybackStateChangeForProgressDrag(
                  PlaybackStateChangeForDragging::kResumeForDraggingEnded));
  EXPECT_CALL(*this, SeekTo(0.5));
  view()->OnMouseReleased(released_event);
}

TEST_F(MediaProgressViewTest, MouseLongPressEventSeekToForRTL) {
  base::i18n::SetICUDefaultLocale("he");

  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(100), /*end_of_media=*/false);
  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(media_position);

  // Simulate mouse press and release events and SeekTo() should be called
  // twice.
  gfx::Point point(view()->width() / 4 + kWidthInset / 2, view()->height() / 2);
  ui::MouseEvent pressed_event(ui::EventType::kMousePressed, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, OnProgressDragStateChange(DragState::kDragStarted));
  EXPECT_CALL(*this,
              OnPlaybackStateChangeForProgressDrag(
                  PlaybackStateChangeForDragging::kPauseForDraggingStarted));
  EXPECT_CALL(*this, SeekTo(testing::DoubleNear(0.75, 0.01)));
  view()->OnMousePressed(pressed_event);
  progress_drag_started_delay_timer()->Fire();

  ui::MouseEvent released_event = ui::MouseEvent(
      ui::EventType::kMouseReleased, point, point, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, OnProgressDragStateChange(DragState::kDragEnded));
  EXPECT_CALL(*this,
              OnPlaybackStateChangeForProgressDrag(
                  PlaybackStateChangeForDragging::kResumeForDraggingEnded));
  EXPECT_CALL(*this, SeekTo(testing::DoubleNear(0.75, 0.01)));
  view()->OnMouseReleased(released_event);
}

TEST_F(MediaProgressViewTest, MouseSeekToForLiveMedia) {
  // Simulate a position change with infinite duration. i.e. a live media.
  media_session::MediaPosition media_position_live(
      /*playback_rate=*/1, /*duration=*/base::TimeDelta::Max(),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);
  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(media_position_live);

  // Simulate a mouse click event and SeekTo() should not be called.
  gfx::Point point(view()->width() / 2, view()->height() / 2);
  ui::MouseEvent pressed_event(ui::EventType::kMousePressed, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, OnProgressDragStateChange(testing::_)).Times(0);
  EXPECT_CALL(*this, OnPlaybackStateChangeForProgressDrag(testing::_)).Times(0);
  EXPECT_CALL(*this, SeekTo(testing::_)).Times(0);
  view()->OnMousePressed(pressed_event);
  EXPECT_TRUE(view()->is_live_for_testing());
  EXPECT_FALSE(progress_drag_started_delay_timer()->IsRunning());
}

TEST_F(MediaProgressViewTest, GestureTapEventSeekTo) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(100), /*end_of_media=*/false);
  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(media_position);

  // Simulate gesture tap and release events and SeekTo() should be called once.
  gfx::Point point(view()->width() / 2, view()->height() / 2);
  ui::GestureEvent tapped_event(
      point.x(), point.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureTapDown));
  EXPECT_CALL(*this, OnProgressDragStateChange(testing::_)).Times(0);
  EXPECT_CALL(*this, OnPlaybackStateChangeForProgressDrag(testing::_)).Times(0);
  EXPECT_CALL(*this, SeekTo(testing::_)).Times(0);
  view()->OnGestureEvent(&tapped_event);
  EXPECT_TRUE(progress_drag_started_delay_timer()->IsRunning());

  ui::GestureEvent released_event(
      point.x(), point.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureEnd));
  EXPECT_CALL(*this, OnProgressDragStateChange(testing::_)).Times(0);
  EXPECT_CALL(*this, OnPlaybackStateChangeForProgressDrag(testing::_)).Times(0);
  EXPECT_CALL(*this, SeekTo(0.5));
  view()->OnGestureEvent(&released_event);
  EXPECT_FALSE(progress_drag_started_delay_timer()->IsRunning());
}

TEST_F(MediaProgressViewTest, GestureScrollEventSeekTo) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(100), /*end_of_media=*/false);
  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(media_position);

  // Simulate a gesture tap event.
  gfx::Point point(view()->width() / 4, view()->height() / 2);
  ui::GestureEvent tapped_event(
      point.x(), point.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureTapDown));
  EXPECT_CALL(*this, OnProgressDragStateChange(DragState::kDragStarted));
  EXPECT_CALL(*this,
              OnPlaybackStateChangeForProgressDrag(
                  PlaybackStateChangeForDragging::kPauseForDraggingStarted));
  EXPECT_CALL(*this, SeekTo(testing::DoubleNear(0.25, 0.01)));
  view()->OnGestureEvent(&tapped_event);
  progress_drag_started_delay_timer()->Fire();

  // Simulate a gesture scroll begin event.
  ui::GestureEvent scroll_begin_event(
      point.x(), point.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));
  EXPECT_CALL(*this, SeekTo(testing::DoubleNear(0.25, 0.01)));
  view()->OnGestureEvent(&scroll_begin_event);

  // Simulate a gesture scroll update event.
  ui::GestureEvent scroll_update_event(
      view()->width() / 2, point.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate));
  EXPECT_CALL(*this, SeekTo(0.5));
  view()->OnGestureEvent(&scroll_update_event);

  // Simulate a gesture scroll end event.
  ui::GestureEvent released_event(
      view()->width() / 2, point.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureEnd));
  EXPECT_CALL(*this, OnProgressDragStateChange(DragState::kDragEnded));
  EXPECT_CALL(*this,
              OnPlaybackStateChangeForProgressDrag(
                  PlaybackStateChangeForDragging::kResumeForDraggingEnded));
  EXPECT_CALL(*this, SeekTo(0.5));
  view()->OnGestureEvent(&released_event);
}

TEST_F(MediaProgressViewTest, KeyEventSeekBackward) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(60),
      /*position=*/base::Seconds(35), /*end_of_media=*/false);
  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(media_position);

  ui::KeyEvent key_event{ui::EventType::kKeyPressed, ui::VKEY_LEFT,
                         ui::DomCode::ARROW_LEFT,    ui::EF_NONE,
                         ui::DomKey::ARROW_LEFT,     ui::EventTimeForNow()};
  EXPECT_CALL(*this, SeekTo(testing::DoubleNear(0.5, 0.01)));
  view()->OnKeyPressed(key_event);
}

TEST_F(MediaProgressViewTest, KeyEventSeekBackwardToBeginning) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(60),
      /*position=*/base::Seconds(3), /*end_of_media=*/false);
  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(media_position);

  ui::KeyEvent key_event{ui::EventType::kKeyPressed, ui::VKEY_DOWN,
                         ui::DomCode::ARROW_DOWN,    ui::EF_NONE,
                         ui::DomKey::ARROW_DOWN,     ui::EventTimeForNow()};
  EXPECT_CALL(*this, SeekTo(0));
  view()->OnKeyPressed(key_event);
}

TEST_F(MediaProgressViewTest, KeyEventSeekForward) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(60),
      /*position=*/base::Seconds(25), /*end_of_media=*/false);
  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(media_position);

  ui::KeyEvent key_event{ui::EventType::kKeyPressed, ui::VKEY_RIGHT,
                         ui::DomCode::ARROW_RIGHT,   ui::EF_NONE,
                         ui::DomKey::ARROW_RIGHT,    ui::EventTimeForNow()};
  EXPECT_CALL(*this, SeekTo(testing::DoubleNear(0.5, 0.01)));
  view()->OnKeyPressed(key_event);
}

TEST_F(MediaProgressViewTest, KeyEventSeekForwardToEnd) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(60),
      /*position=*/base::Seconds(57), /*end_of_media=*/false);
  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(media_position);

  ui::KeyEvent key_event{ui::EventType::kKeyPressed, ui::VKEY_UP,
                         ui::DomCode::ARROW_UP,      ui::EF_NONE,
                         ui::DomKey::ARROW_UP,       ui::EventTimeForNow()};
  EXPECT_CALL(*this, SeekTo(1));
  view()->OnKeyPressed(key_event);
}

TEST_F(MediaProgressViewTest, DragProgressForPlayingMedia) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(100), /*end_of_media=*/false);
  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(media_position);
  EXPECT_TRUE(switch_progress_colors_delay_timer()->IsRunning());
  switch_progress_colors_delay_timer()->Fire();
  EXPECT_FALSE(view()->use_paused_colors_for_testing());

  gfx::Point point(view()->width() / 2, view()->height() / 2);
  ui::MouseEvent pressed_event(ui::EventType::kMousePressed, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, OnProgressDragStateChange(DragState::kDragStarted));
  EXPECT_CALL(*this,
              OnPlaybackStateChangeForProgressDrag(
                  PlaybackStateChangeForDragging::kPauseForDraggingStarted));
  EXPECT_CALL(*this, SeekTo(0.5));
  view()->OnMousePressed(pressed_event);
  progress_drag_started_delay_timer()->Fire();
  EXPECT_TRUE(view()->use_paused_colors_for_testing());

  ui::MouseEvent dragged_event(ui::EventType::kMouseDragged, gfx::Point(),
                               gfx::Point(), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, SeekTo(0));
  view()->OnMouseDragged(dragged_event);

  ui::MouseEvent released_event =
      ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, OnProgressDragStateChange(DragState::kDragEnded));
  EXPECT_CALL(*this,
              OnPlaybackStateChangeForProgressDrag(
                  PlaybackStateChangeForDragging::kResumeForDraggingEnded));
  EXPECT_CALL(*this, SeekTo(0));
  view()->OnMouseReleased(released_event);
  EXPECT_FALSE(view()->use_paused_colors_for_testing());
}

TEST_F(MediaProgressViewTest, DragProgressForPausedMedia) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/0, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(100), /*end_of_media=*/false);
  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(media_position);

  gfx::Point point(view()->width() / 2, view()->height() / 2);
  ui::MouseEvent pressed_event(ui::EventType::kMousePressed, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, OnProgressDragStateChange(DragState::kDragStarted));
  EXPECT_CALL(*this, OnPlaybackStateChangeForProgressDrag(testing::_)).Times(0);
  EXPECT_CALL(*this, SeekTo(0.5));
  view()->OnMousePressed(pressed_event);
  progress_drag_started_delay_timer()->Fire();

  gfx::Point new_point(view()->width() / 4, view()->height() / 2);
  ui::MouseEvent dragged_event(
      ui::EventType::kMouseDragged, new_point, new_point, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, SeekTo(testing::DoubleNear(0.25, 0.01)));
  view()->OnMouseDragged(dragged_event);

  ui::MouseEvent released_event =
      ui::MouseEvent(ui::EventType::kMouseReleased, new_point, new_point,
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(*this, OnProgressDragStateChange(DragState::kDragEnded));
  EXPECT_CALL(*this, OnPlaybackStateChangeForProgressDrag(testing::_)).Times(0);
  EXPECT_CALL(*this, SeekTo(testing::DoubleNear(0.25, 0.01)));
  view()->OnMouseReleased(released_event);
}

TEST_F(MediaProgressViewTest, UpdateProgressColorsForPlaybackRateChanges) {
  media_session::MediaPosition playing_media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);
  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(playing_media_position);
  EXPECT_TRUE(switch_progress_colors_delay_timer()->IsRunning());
  EXPECT_TRUE(view()->use_paused_colors_for_testing());

  switch_progress_colors_delay_timer()->Fire();
  EXPECT_FALSE(view()->use_paused_colors_for_testing());

  media_session::MediaPosition paused_media_position(
      /*playback_rate=*/0, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);
  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(paused_media_position);
  EXPECT_TRUE(switch_progress_colors_delay_timer()->IsRunning());
  EXPECT_FALSE(view()->use_paused_colors_for_testing());

  switch_progress_colors_delay_timer()->Fire();
  EXPECT_TRUE(view()->use_paused_colors_for_testing());
}

TEST_F(MediaProgressViewTest, MediaProgressAccessibleValue) {
  ui::AXNodeData data;
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kValue),
            u"0:00");

  media_session::MediaPosition media_position(
      /*playback_rate=*/0, /*duration=*/base::Seconds(6),
      /*position=*/base::Seconds(3), /*end_of_media=*/false);
  EXPECT_CALL(*this, OnProgressUpdated(testing::_));
  view()->UpdateProgress(media_position);

  data = ui::AXNodeData();
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kValue),
            u"0:03");
}

}  // namespace global_media_controls
