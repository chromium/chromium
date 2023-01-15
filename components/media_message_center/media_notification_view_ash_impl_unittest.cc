// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_view_ash_impl.h"

#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/mock_media_notification_item.h"
#include "components/media_message_center/notification_theme.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"

namespace media_message_center {

using media_session::mojom::MediaSessionAction;
using testing::_;

namespace {

class MockMediaNotificationContainer : public MediaNotificationContainer {
 public:
  MockMediaNotificationContainer() = default;
  MockMediaNotificationContainer(const MockMediaNotificationContainer&) =
      delete;
  MockMediaNotificationContainer& operator=(
      const MockMediaNotificationContainer&) = delete;
  ~MockMediaNotificationContainer() override = default;

  // MediaNotificationContainer implementation.
  MOCK_METHOD1(OnExpanded, void(bool expanded));
  MOCK_METHOD1(
      OnMediaSessionInfoChanged,
      void(const media_session::mojom::MediaSessionInfoPtr& session_info));
  MOCK_METHOD1(OnMediaSessionMetadataChanged,
               void(const media_session::MediaMetadata& metadata));
  MOCK_METHOD1(OnVisibleActionsChanged,
               void(const base::flat_set<MediaSessionAction>& actions));
  MOCK_METHOD1(OnMediaArtworkChanged, void(const gfx::ImageSkia& image));
  MOCK_METHOD3(OnColorsChanged,
               void(SkColor foreground,
                    SkColor foreground_disabled,
                    SkColor background));
  MOCK_METHOD0(OnHeaderClicked, void());
};

}  // namespace

class MediaNotificationViewAshImplTest : public views::ViewsTestBase {
 public:
  MediaNotificationViewAshImplTest() = default;
  MediaNotificationViewAshImplTest(const MediaNotificationViewAshImplTest&) =
      delete;
  MediaNotificationViewAshImplTest& operator=(
      const MediaNotificationViewAshImplTest&) = delete;
  ~MediaNotificationViewAshImplTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    // Create a widget to show on the screen for testing screen coordinates and
    // focus.
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_->Init(std::move(params));
    widget_->Show();

    // Creates the view and adds it to the widget.
    CreateView();
  }

  void TearDown() override {
    view_ = nullptr;
    widget_.reset();
    actions_.clear();

    views::ViewsTestBase::TearDown();
  }

  void EnableAllActions() {
    actions_.insert(MediaSessionAction::kPlay);
    actions_.insert(MediaSessionAction::kPause);
    actions_.insert(MediaSessionAction::kPreviousTrack);
    actions_.insert(MediaSessionAction::kNextTrack);
    actions_.insert(MediaSessionAction::kStop);
    actions_.insert(MediaSessionAction::kEnterPictureInPicture);
    actions_.insert(MediaSessionAction::kExitPictureInPicture);

    NotifyUpdatedActions();
  }

  void DisableAllActions() {
    actions_.clear();
    NotifyUpdatedActions();
  }

  void EnableAction(MediaSessionAction action) {
    actions_.insert(action);
    NotifyUpdatedActions();
  }

  void DisableAction(MediaSessionAction action) {
    actions_.erase(action);
    NotifyUpdatedActions();
  }

  MockMediaNotificationContainer& container() { return container_; }

  MediaNotificationViewAshImpl* view() const { return view_; }

  test::MockMediaNotificationItem& item() { return item_; }

  views::Label* source_label() const { return view()->source_label_; }

  views::Label* title_label() const { return view()->title_label_; }

  views::Label* artist_label() const { return view()->artist_label_; }

  std::vector<views::Button*> media_control_buttons() const {
    return view()->action_buttons_;
  }

  views::Button* GetButtonForAction(MediaSessionAction action) const {
    auto buttons = media_control_buttons();
    const auto i = base::ranges::find(buttons, static_cast<int>(action),
                                      &views::Button::tag);
    return (i == buttons.end()) ? nullptr : *i;
  }

  bool IsActionButtonVisible(MediaSessionAction action) const {
    auto* button = GetButtonForAction(action);
    return button && button->GetVisible();
  }

  void SimulateButtonClick(MediaSessionAction action) {
    views::Button* button = GetButtonForAction(action);
    EXPECT_TRUE(button->GetVisible());

    views::test::ButtonTestApi(button).NotifyClick(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0));
  }

 private:
  void NotifyUpdatedActions() { view_->UpdateWithMediaActions(actions_); }

  void CreateView() {
    // On creation, the view should notify |item_|.
    auto view = std::make_unique<MediaNotificationViewAshImpl>(
        &container_, item_.GetWeakPtr(), std::make_unique<views::View>(),
        NotificationTheme());

    media_session::MediaMetadata metadata;
    metadata.title = u"title";
    metadata.artist = u"artist";
    metadata.source_title = u"source title";
    view->UpdateWithMediaMetadata(metadata);

    view->UpdateWithMediaActions(actions_);

    // Display it in |widget_|. Widget now owns |view|.
    view_ = widget_->SetContentsView(std::move(view));
  }

  base::flat_set<MediaSessionAction> actions_;

  MockMediaNotificationContainer container_;
  test::MockMediaNotificationItem item_;
  raw_ptr<MediaNotificationViewAshImpl> view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(MediaNotificationViewAshImplTest, MetadataUpdated) {
  media_session::MediaMetadata metadata;
  metadata.source_title = u"source title2";
  metadata.title = u"title2";
  metadata.artist = u"artist2";

  EXPECT_CALL(container(), OnMediaSessionMetadataChanged(_));
  view()->UpdateWithMediaMetadata(metadata);

  EXPECT_EQ(source_label()->GetText(), metadata.source_title);
  EXPECT_EQ(title_label()->GetText(), metadata.title);
  EXPECT_EQ(artist_label()->GetText(), metadata.artist);
}

TEST_F(MediaNotificationViewAshImplTest, PlayPauseButtonDisplay) {
  EnableAllActions();

  auto session_info = media_session::mojom::MediaSessionInfo::New();
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;

  EXPECT_CALL(container(), OnMediaSessionInfoChanged(_));
  view()->UpdateWithMediaSessionInfo(session_info.Clone());
  testing::Mock::VerifyAndClearExpectations(&container());

  // Check that the pause button is shown.
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPause));

  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPaused;
  EXPECT_CALL(container(), OnMediaSessionInfoChanged(_));
  view()->UpdateWithMediaSessionInfo(session_info.Clone());
  testing::Mock::VerifyAndClearExpectations(&container());

  // Check that the play button is shown.
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPlay));
}

TEST_F(MediaNotificationViewAshImplTest, PictureInPictureButtonDisplay) {
  EnableAllActions();

  auto session_info = media_session::mojom::MediaSessionInfo::New();
  session_info->picture_in_picture_state =
      media_session::mojom::MediaPictureInPictureState::kNotInPictureInPicture;
  session_info->is_controllable = true;

  EXPECT_CALL(container(), OnMediaSessionInfoChanged(_));
  view()->UpdateWithMediaSessionInfo(session_info.Clone());
  testing::Mock::VerifyAndClearExpectations(&container());

  // Check that the enter picture-in-picture button is shown.
  EXPECT_TRUE(
      IsActionButtonVisible(MediaSessionAction::kEnterPictureInPicture));

  session_info->picture_in_picture_state =
      media_session::mojom::MediaPictureInPictureState::kInPictureInPicture;
  EXPECT_CALL(container(), OnMediaSessionInfoChanged(_));
  view()->UpdateWithMediaSessionInfo(session_info.Clone());
  testing::Mock::VerifyAndClearExpectations(&container());

  // Check that the exit picture-in-picture button is shown.
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kExitPictureInPicture));
}

TEST_F(MediaNotificationViewAshImplTest, ButtonVisibilityCheck) {
  auto session_info = media_session::mojom::MediaSessionInfo::New();
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->picture_in_picture_state =
      media_session::mojom::MediaPictureInPictureState::kNotInPictureInPicture;
  session_info->is_controllable = true;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());

  DisableAllActions();
  for (auto* button : media_control_buttons()) {
    EXPECT_FALSE(button->GetVisible());
  }

  EnableAction(MediaSessionAction::kPause);
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPause));

  EnableAction(MediaSessionAction::kPreviousTrack);
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));

  EnableAction(MediaSessionAction::kNextTrack);
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kNextTrack));

  EnableAction(MediaSessionAction::kEnterPictureInPicture);
  EXPECT_TRUE(
      IsActionButtonVisible(MediaSessionAction::kEnterPictureInPicture));
}

TEST_F(MediaNotificationViewAshImplTest, NextTrackButtonClick) {
  EnableAction(MediaSessionAction::kNextTrack);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kNextTrack));
  SimulateButtonClick(MediaSessionAction::kNextTrack);
}

TEST_F(MediaNotificationViewAshImplTest, PlayButtonClick) {
  EnableAction(MediaSessionAction::kPlay);

  EXPECT_CALL(item(),
              OnMediaSessionActionButtonPressed(MediaSessionAction::kPlay));
  SimulateButtonClick(MediaSessionAction::kPlay);
}

TEST_F(MediaNotificationViewAshImplTest, PauseButtonClick) {
  EnableAction(MediaSessionAction::kPause);

  auto session_info = media_session::mojom::MediaSessionInfo::New();
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());

  EXPECT_CALL(item(),
              OnMediaSessionActionButtonPressed(MediaSessionAction::kPause));
  SimulateButtonClick(MediaSessionAction::kPause);
}

TEST_F(MediaNotificationViewAshImplTest, PreviousTrackButtonClick) {
  EnableAction(MediaSessionAction::kPreviousTrack);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kPreviousTrack));
  SimulateButtonClick(MediaSessionAction::kPreviousTrack);
}

TEST_F(MediaNotificationViewAshImplTest, EnterPictureInPictureButtonClick) {
  EnableAction(MediaSessionAction::kEnterPictureInPicture);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kEnterPictureInPicture));
  SimulateButtonClick(MediaSessionAction::kEnterPictureInPicture);
}

TEST_F(MediaNotificationViewAshImplTest, ExitPictureInPictureButtonClick) {
  EnableAction(MediaSessionAction::kExitPictureInPicture);

  auto session_info = media_session::mojom::MediaSessionInfo::New();
  session_info->picture_in_picture_state =
      media_session::mojom::MediaPictureInPictureState::kInPictureInPicture;
  session_info->is_controllable = true;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kExitPictureInPicture));
  SimulateButtonClick(MediaSessionAction::kExitPictureInPicture);
}

}  // namespace media_message_center
