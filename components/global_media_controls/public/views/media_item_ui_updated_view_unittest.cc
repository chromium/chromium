// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"

#include "components/global_media_controls/public/test/mock_media_item_ui_device_selector.h"
#include "components/global_media_controls/public/test/mock_media_item_ui_footer.h"
#include "components/global_media_controls/public/test/mock_media_item_ui_observer.h"
#include "components/global_media_controls/public/views/media_progress_view.h"
#include "components/media_message_center/mock_media_notification_item.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"

namespace global_media_controls {

using ::global_media_controls::test::MockMediaItemUIDeviceSelector;
using ::global_media_controls::test::MockMediaItemUIFooter;
using ::global_media_controls::test::MockMediaItemUIObserver;
using ::media_message_center::test::MockMediaNotificationItem;
using ::media_session::mojom::MediaSessionAction;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

const char kTestId[] = "test-id";

}  // anonymous namespace

class MediaItemUIUpdatedViewTest : public views::ViewsTestBase {
 public:
  MediaItemUIUpdatedViewTest() = default;
  MediaItemUIUpdatedViewTest(const MediaItemUIUpdatedViewTest&) = delete;
  MediaItemUIUpdatedViewTest& operator=(const MediaItemUIUpdatedViewTest&) =
      delete;
  ~MediaItemUIUpdatedViewTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    item_ = std::make_unique<NiceMock<MockMediaNotificationItem>>();
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    auto device_selector =
        std::make_unique<NiceMock<MockMediaItemUIDeviceSelector>>();
    device_selector_ = device_selector.get();
    view_ = widget_->SetContentsView(std::make_unique<MediaItemUIUpdatedView>(
        kTestId, item_->GetWeakPtr(), media_message_center::MediaColorTheme(),
        std::move(device_selector), /*footer_view=*/nullptr));

    observer_ = std::make_unique<NiceMock<MockMediaItemUIObserver>>();
    view_->AddObserver(observer_.get());
    widget_->Show();
  }

  void TearDown() override {
    view_->RemoveObserver(observer_.get());
    device_selector_ = nullptr;
    view_ = nullptr;
    widget_.reset();
    views::ViewsTestBase::TearDown();
  }

  void EnableAllMediaActions() {
    actions_.insert(MediaSessionAction::kPlay);
    actions_.insert(MediaSessionAction::kPause);
    actions_.insert(MediaSessionAction::kPreviousTrack);
    actions_.insert(MediaSessionAction::kNextTrack);
    actions_.insert(MediaSessionAction::kSeekForward);
    actions_.insert(MediaSessionAction::kSeekBackward);
    actions_.insert(MediaSessionAction::kEnterPictureInPicture);
    actions_.insert(MediaSessionAction::kExitPictureInPicture);
    view_->UpdateWithMediaActions(actions_);
  }

  bool IsMediaActionButtonVisible(MediaSessionAction action) const {
    auto* button = view_->GetMediaActionButtonForTesting(action);
    return button && button->GetVisible();
  }

  void SimulateButtonClick(MediaSessionAction action) {
    auto* button = view_->GetMediaActionButtonForTesting(action);
    EXPECT_TRUE(button && button->GetVisible());
    views::test::ButtonTestApi(button).NotifyClick(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0));
  }

  MediaItemUIUpdatedView* view() { return view_; }
  MockMediaNotificationItem& item() { return *item_; }
  MockMediaItemUIObserver& observer() { return *observer_; }
  MockMediaItemUIDeviceSelector* device_selector() { return device_selector_; }

 private:
  base::flat_set<MediaSessionAction> actions_;
  raw_ptr<MediaItemUIUpdatedView> view_;
  std::unique_ptr<MockMediaNotificationItem> item_;
  std::unique_ptr<MockMediaItemUIObserver> observer_;
  raw_ptr<MockMediaItemUIDeviceSelector> device_selector_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(MediaItemUIUpdatedViewTest, ProgressViewCheck) {
  // Check that progress position can be updated.
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(10),
      /*position=*/base::Seconds(5), /*end_of_media=*/false);
  view()->UpdateWithMediaPosition(media_position);
  EXPECT_NEAR(view()->GetProgressViewForTesting()->current_value_for_testing(),
              0.5f, 0.01f);

  // Check that key event on the view can seek the progress.
  ui::KeyEvent key_event{ui::ET_KEY_PRESSED,       ui::VKEY_RIGHT,
                         ui::DomCode::ARROW_RIGHT, ui::EF_NONE,
                         ui::DomKey::ARROW_RIGHT,  ui::EventTimeForNow()};
  EXPECT_CALL(item(), SeekTo(testing::_));
  view()->OnKeyPressed(key_event);
}

TEST_F(MediaItemUIUpdatedViewTest, OnMousePressed) {
  ui::MouseEvent mouse_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             0);
  EXPECT_CALL(observer(),
              OnMediaItemUIClicked(kTestId, /*activate_original_media=*/true));
  view()->OnMousePressed(mouse_event);
}

TEST_F(MediaItemUIUpdatedViewTest,
       UpdateWithMediaSessionInfoForPlayPauseButton) {
  EnableAllMediaActions();
  auto session_info = media_session::mojom::MediaSessionInfo::New();
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kPause));

  EXPECT_CALL(item(),
              OnMediaSessionActionButtonPressed(MediaSessionAction::kPause));
  SimulateButtonClick(MediaSessionAction::kPause);

  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPaused;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kPlay));

  EXPECT_CALL(item(),
              OnMediaSessionActionButtonPressed(MediaSessionAction::kPlay));
  SimulateButtonClick(MediaSessionAction::kPlay);
}

TEST_F(MediaItemUIUpdatedViewTest, UpdateWithMediaSessionInfoForPiPButton) {
  EnableAllMediaActions();
  auto session_info = media_session::mojom::MediaSessionInfo::New();
  session_info->picture_in_picture_state =
      media_session::mojom::MediaPictureInPictureState::kInPictureInPicture;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());
  EXPECT_TRUE(
      IsMediaActionButtonVisible(MediaSessionAction::kExitPictureInPicture));

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kExitPictureInPicture));
  SimulateButtonClick(MediaSessionAction::kExitPictureInPicture);

  session_info->picture_in_picture_state =
      media_session::mojom::MediaPictureInPictureState::kNotInPictureInPicture;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());
  EXPECT_TRUE(
      IsMediaActionButtonVisible(MediaSessionAction::kEnterPictureInPicture));

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kEnterPictureInPicture));
  SimulateButtonClick(MediaSessionAction::kEnterPictureInPicture);
}

TEST_F(MediaItemUIUpdatedViewTest, UpdateWithMediaMetadata) {
  EXPECT_EQ(view()->GetSourceLabelForTesting()->GetText(), u"");
  EXPECT_EQ(view()->GetArtistLabelForTesting()->GetText(), u"");
  EXPECT_EQ(view()->GetTitleLabelForTesting()->GetText(), u"");

  media_session::MediaMetadata metadata;
  metadata.source_title = u"source title";
  metadata.title = u"title";
  metadata.artist = u"artist";

  EXPECT_CALL(observer(), OnMediaItemUIMetadataChanged());
  view()->UpdateWithMediaMetadata(metadata);

  EXPECT_EQ(view()->GetSourceLabelForTesting()->GetText(),
            metadata.source_title);
  EXPECT_EQ(view()->GetArtistLabelForTesting()->GetText(), metadata.artist);
  EXPECT_EQ(view()->GetTitleLabelForTesting()->GetText(), metadata.title);
}

TEST_F(MediaItemUIUpdatedViewTest, UpdateWithMediaActions) {
  EXPECT_CALL(observer(), OnMediaItemUIActionsChanged());
  EnableAllMediaActions();

  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kPlay));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kSeekBackward));
  EXPECT_TRUE(
      IsMediaActionButtonVisible(MediaSessionAction::kEnterPictureInPicture));
}

TEST_F(MediaItemUIUpdatedViewTest, UpdateWithMediaArtwork) {
  EXPECT_FALSE(view()->GetArtworkViewForTesting()->GetVisible());

  SkBitmap image;
  image.allocN32Pixels(10, 10);
  view()->UpdateWithMediaArtwork(gfx::ImageSkia::CreateFrom1xBitmap(image));
  EXPECT_TRUE(view()->GetArtworkViewForTesting()->GetVisible());

  view()->UpdateWithMediaArtwork(gfx::ImageSkia());
  EXPECT_FALSE(view()->GetArtworkViewForTesting()->GetVisible());
}

TEST_F(MediaItemUIUpdatedViewTest, MediaActionButtonPressed) {
  EnableAllMediaActions();
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(10),
      /*position=*/base::Seconds(5), /*end_of_media=*/false);
  view()->UpdateWithMediaPosition(media_position);

  EXPECT_CALL(item(), SeekTo(testing::_));
  SimulateButtonClick(MediaSessionAction::kSeekForward);
  EXPECT_CALL(item(), SeekTo(testing::_));
  SimulateButtonClick(MediaSessionAction::kSeekBackward);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kPreviousTrack));
  SimulateButtonClick(MediaSessionAction::kPreviousTrack);
  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kNextTrack));
  SimulateButtonClick(MediaSessionAction::kNextTrack);
}

TEST_F(MediaItemUIUpdatedViewTest, DeviceSelectorViewCheck) {
  EXPECT_NE(view()->GetStartCastingButtonForTesting(), nullptr);
  EXPECT_FALSE(view()->GetStartCastingButtonForTesting()->GetVisible());
  EXPECT_EQ(view()->GetDeviceSelectorForTesting(), device_selector());

  // Add devices to the list to show the start casting button.
  view()->UpdateDeviceSelectorAvailability(/*has_devices=*/true);
  EXPECT_TRUE(view()->GetStartCastingButtonForTesting()->GetVisible());

  // Click the start casting button to show devices.
  EXPECT_CALL(*device_selector(), IsDeviceSelectorExpanded())
      .WillOnce(Return(false));
  EXPECT_CALL(*device_selector(), ShowDevices());
  views::test::ButtonTestApi(view()->GetStartCastingButtonForTesting())
      .NotifyClick(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));

  // Device selector view will call to update visibility.
  EXPECT_CALL(*device_selector(), IsDeviceSelectorExpanded())
      .WillOnce(Return(true));
  EXPECT_CALL(observer(), OnMediaItemUISizeChanged());
  view()->UpdateDeviceSelectorVisibility(/*visible=*/true);
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            views::InkDrop::Get(view()->GetStartCastingButtonForTesting())
                ->GetInkDrop()
                ->GetTargetInkDropState());

  // Click the start casting button to hide devices.
  EXPECT_CALL(*device_selector(), IsDeviceSelectorExpanded())
      .WillOnce(Return(true));
  EXPECT_CALL(*device_selector(), HideDevices());
  views::test::ButtonTestApi(view()->GetStartCastingButtonForTesting())
      .NotifyClick(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));

  // Device selector view will call to update visibility.
  EXPECT_CALL(*device_selector(), IsDeviceSelectorExpanded())
      .WillOnce(Return(false));
  EXPECT_CALL(observer(), OnMediaItemUISizeChanged());
  view()->UpdateDeviceSelectorVisibility(/*visible=*/false);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            views::InkDrop::Get(view()->GetStartCastingButtonForTesting())
                ->GetInkDrop()
                ->GetTargetInkDropState());
}

TEST_F(MediaItemUIUpdatedViewTest, FooterViewCheck) {
  EnableAllMediaActions();
  auto* pip_button = view()->GetMediaActionButtonForTesting(
      MediaSessionAction::kEnterPictureInPicture);
  EXPECT_TRUE(pip_button->GetVisible());

  auto footer_view = std::make_unique<NiceMock<MockMediaItemUIFooter>>();
  auto* footer_ptr = footer_view.get();
  view()->UpdateFooterView(std::move(footer_view));
  EXPECT_FALSE(pip_button->GetVisible());
  EXPECT_EQ(footer_ptr, view()->GetFooterForTesting());
}

TEST_F(MediaItemUIUpdatedViewTest, DragProgressForPlayingMedia) {
  EnableAllMediaActions();
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(10),
      /*position=*/base::Seconds(5), /*end_of_media=*/false);
  view()->UpdateWithMediaPosition(media_position);
  auto* progress_view = view()->GetProgressViewForTesting();

  // Starts dragging the progress view should pause the media.
  gfx::Point point(progress_view->width() / 2, progress_view->height() / 2);
  ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(item(), SeekTo(testing::_));
  EXPECT_CALL(item(),
              OnMediaSessionActionButtonPressed(MediaSessionAction::kPause));
  progress_view->OnMousePressed(pressed_event);

  // Starts dragging should hide these media action buttons.
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kSeekBackward));

  // Ends dragging the progress view should resume the media.
  ui::MouseEvent released_event = ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(item(), SeekTo(testing::_));
  EXPECT_CALL(item(),
              OnMediaSessionActionButtonPressed(MediaSessionAction::kPlay));
  progress_view->OnMouseReleased(released_event);

  // Ends dragging should show these media action buttons.
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kSeekBackward));
}

TEST_F(MediaItemUIUpdatedViewTest, DragProgressForPausedMedia) {
  EnableAllMediaActions();
  media_session::MediaPosition media_position(
      /*playback_rate=*/0, /*duration=*/base::Seconds(10),
      /*position=*/base::Seconds(5), /*end_of_media=*/false);
  view()->UpdateWithMediaPosition(media_position);
  auto* progress_view = view()->GetProgressViewForTesting();

  // Starts dragging the progress view.
  gfx::Point point(progress_view->width() / 2, progress_view->height() / 2);
  ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(item(), SeekTo(testing::_));
  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(testing::_)).Times(0);
  progress_view->OnMousePressed(pressed_event);

  // Starts dragging should hide these media action buttons.
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kSeekBackward));

  // Ends dragging the progress view.
  ui::MouseEvent released_event = ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(item(), SeekTo(testing::_));
  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(testing::_)).Times(0);
  progress_view->OnMouseReleased(released_event);

  // Ends dragging should show these media action buttons.
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kSeekBackward));
}

TEST_F(MediaItemUIUpdatedViewTest, TimestampLabelsCheck) {
  // The timestamp labels should be hidden initially.
  EXPECT_FALSE(view()->GetCurrentTimestampLabelForTesting()->GetVisible());
  EXPECT_FALSE(view()->GetDurationTimestampLabelForTesting()->GetVisible());

  media_session::MediaPosition media_position(
      /*playback_rate=*/0, /*duration=*/base::Seconds(10),
      /*position=*/base::Seconds(5), /*end_of_media=*/false);
  view()->UpdateWithMediaPosition(media_position);
  auto* progress_view = view()->GetProgressViewForTesting();

  // Starts dragging the progress view should show the timestamp labels.
  gfx::Point point(progress_view->width() / 2, progress_view->height() / 2);
  ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(item(), SeekTo(testing::_));
  progress_view->OnMousePressed(pressed_event);
  EXPECT_TRUE(view()->GetCurrentTimestampLabelForTesting()->GetVisible());
  EXPECT_TRUE(view()->GetDurationTimestampLabelForTesting()->GetVisible());
  EXPECT_EQ(u"0:05", view()->GetCurrentTimestampLabelForTesting()->GetText());
  EXPECT_EQ(u"0:10", view()->GetDurationTimestampLabelForTesting()->GetText());

  // Ends dragging the progress view should hide the timestamp labels.
  ui::MouseEvent released_event = ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(item(), SeekTo(testing::_));
  progress_view->OnMouseReleased(released_event);
  EXPECT_FALSE(view()->GetCurrentTimestampLabelForTesting()->GetVisible());
  EXPECT_FALSE(view()->GetDurationTimestampLabelForTesting()->GetVisible());
}

}  // namespace global_media_controls
