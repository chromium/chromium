// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/timer/mock_timer.h"
#include "components/global_media_controls/public/test/mock_media_item_ui_device_selector.h"
#include "components/global_media_controls/public/test/mock_media_item_ui_footer.h"
#include "components/global_media_controls/public/test/mock_media_item_ui_observer.h"
#include "components/global_media_controls/public/views/media_progress_view.h"
#include "components/media_message_center/mock_media_notification_item.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/accessibility/view_accessibility.h"
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
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    auto device_selector =
        std::make_unique<NiceMock<MockMediaItemUIDeviceSelector>>();
    device_selector_ = device_selector.get();
    view_ = widget_->SetContentsView(std::make_unique<MediaItemUIUpdatedView>(
        kTestId, item_->GetWeakPtr(), media_message_center::MediaColorTheme(),
        std::move(device_selector), /*footer_view=*/nullptr));

    observer_ = std::make_unique<NiceMock<MockMediaItemUIObserver>>();
    view_->AddObserver(observer_.get());

    // This timer needs to be fired if the test is sending a mouse or gesture
    // event that should be dragging rather than clicking the progress view.
    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    progress_drag_timer_ = mock_timer.get();
    view_->GetProgressViewForTesting()
        ->set_progress_drag_started_delay_timer_for_testing(
            std::move(mock_timer));

    widget_->Show();
  }

  void TearDown() override {
    view_->RemoveObserver(observer_.get());
    device_selector_ = nullptr;
    view_ = nullptr;
    progress_drag_timer_ = nullptr;
    widget_->Close();
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

  bool IsMediaActionButtonDisabled(MediaSessionAction action) const {
    auto* button = view_->GetMediaActionButtonForTesting(action);
    return button && button->GetVisible() && !button->GetEnabled();
  }

  void SimulateButtonClick(MediaSessionAction action) {
    auto* button = view_->GetMediaActionButtonForTesting(action);
    EXPECT_TRUE(button && button->GetVisible());
    views::test::ButtonTestApi(button).NotifyClick(
        ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0));
  }

  void ExpectActionHistogramCount(MediaItemUIUpdatedViewAction action,
                                  int count = 1) {
    histogram_tester_.ExpectBucketCount(kMediaItemUIUpdatedViewActionHistogram,
                                        action, count);
  }

  MediaItemUIUpdatedView* view() { return view_; }
  MockMediaNotificationItem& item() { return *item_; }
  MockMediaItemUIObserver& observer() { return *observer_; }
  MockMediaItemUIDeviceSelector* device_selector() { return device_selector_; }
  base::MockOneShotTimer* progress_drag_timer() const {
    return progress_drag_timer_;
  }

 private:
  base::flat_set<MediaSessionAction> actions_;
  raw_ptr<MediaItemUIUpdatedView> view_ = nullptr;
  std::unique_ptr<MockMediaNotificationItem> item_;
  std::unique_ptr<MockMediaItemUIObserver> observer_;
  raw_ptr<MockMediaItemUIDeviceSelector> device_selector_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
  base::HistogramTester histogram_tester_;
  raw_ptr<base::MockOneShotTimer> progress_drag_timer_ = nullptr;
};

TEST_F(MediaItemUIUpdatedViewTest, AccessibleProperties) {
  EXPECT_EQ(view()->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kListItem);
  EXPECT_EQ(view()->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(
                IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACCESSIBLE_NAME));
}

TEST_F(MediaItemUIUpdatedViewTest, ProgressRowCheck) {
  // Check that progress position can be updated.
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(10),
      /*position=*/base::Seconds(5), /*end_of_media=*/false);
  view()->UpdateWithMediaPosition(media_position);
  EXPECT_NEAR(view()->GetProgressViewForTesting()->current_value_for_testing(),
              0.5f, 0.01f);

  // Check that key event on the view can seek the progress.
  ui::KeyEvent key_event{ui::EventType::kKeyPressed, ui::VKEY_RIGHT,
                         ui::DomCode::ARROW_RIGHT,   ui::EF_NONE,
                         ui::DomKey::ARROW_RIGHT,    ui::EventTimeForNow()};
  EXPECT_CALL(item(), SeekTo(testing::_));
  view()->OnKeyPressed(key_event);

  // The progress row media action buttons should be disabled instead of
  // invisible if the actions are not supported.
  EXPECT_TRUE(IsMediaActionButtonDisabled(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(IsMediaActionButtonDisabled(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(IsMediaActionButtonDisabled(MediaSessionAction::kSeekForward));
  EXPECT_TRUE(IsMediaActionButtonDisabled(MediaSessionAction::kSeekBackward));
}

TEST_F(MediaItemUIUpdatedViewTest, OnMousePressed) {
  ui::MouseEvent mouse_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
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
  ExpectActionHistogramCount(MediaItemUIUpdatedViewAction::kPause);

  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPaused;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kPlay));

  EXPECT_CALL(item(),
              OnMediaSessionActionButtonPressed(MediaSessionAction::kPlay));
  SimulateButtonClick(MediaSessionAction::kPlay);
  ExpectActionHistogramCount(MediaItemUIUpdatedViewAction::kPlay);
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
  ExpectActionHistogramCount(
      MediaItemUIUpdatedViewAction::kExitPictureInPicture);

  session_info->picture_in_picture_state =
      media_session::mojom::MediaPictureInPictureState::kNotInPictureInPicture;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());
  EXPECT_TRUE(
      IsMediaActionButtonVisible(MediaSessionAction::kEnterPictureInPicture));

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kEnterPictureInPicture));
  SimulateButtonClick(MediaSessionAction::kEnterPictureInPicture);
  ExpectActionHistogramCount(
      MediaItemUIUpdatedViewAction::kEnterPictureInPicture);
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

  ui::AXNodeData data;
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            metadata.title);
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
  EXPECT_TRUE(view()->GetArtworkViewForTesting()->GetImageModel().IsImage());

  view()->UpdateWithMediaArtwork(gfx::ImageSkia());
  EXPECT_FALSE(view()->GetArtworkViewForTesting()->GetVisible());
}

TEST_F(MediaItemUIUpdatedViewTest, UpdateWithFavicon) {
  EXPECT_TRUE(
      view()->GetFaviconViewForTesting()->GetImageModel().IsVectorIcon());

  SkBitmap image;
  image.allocN32Pixels(10, 10);
  view()->UpdateWithFavicon(gfx::ImageSkia::CreateFrom1xBitmap(image));
  EXPECT_TRUE(view()->GetFaviconViewForTesting()->GetImageModel().IsImage());

  view()->UpdateWithFavicon(gfx::ImageSkia());
  EXPECT_TRUE(
      view()->GetFaviconViewForTesting()->GetImageModel().IsVectorIcon());
}

TEST_F(MediaItemUIUpdatedViewTest, MediaActionButtonPressed) {
  EnableAllMediaActions();
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(10),
      /*position=*/base::Seconds(5), /*end_of_media=*/false);
  view()->UpdateWithMediaPosition(media_position);

  EXPECT_CALL(item(), SeekTo(testing::_));
  SimulateButtonClick(MediaSessionAction::kSeekForward);
  ExpectActionHistogramCount(MediaItemUIUpdatedViewAction::kForward10Seconds);

  EXPECT_CALL(item(), SeekTo(testing::_));
  SimulateButtonClick(MediaSessionAction::kSeekBackward);
  ExpectActionHistogramCount(MediaItemUIUpdatedViewAction::kReplay10Seconds);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kPreviousTrack));
  SimulateButtonClick(MediaSessionAction::kPreviousTrack);
  ExpectActionHistogramCount(MediaItemUIUpdatedViewAction::kPreviousTrack);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kNextTrack));
  SimulateButtonClick(MediaSessionAction::kNextTrack);
  ExpectActionHistogramCount(MediaItemUIUpdatedViewAction::kNextTrack);
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
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  ExpectActionHistogramCount(
      MediaItemUIUpdatedViewAction::kShowDeviceListForCasting);

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
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  ExpectActionHistogramCount(
      MediaItemUIUpdatedViewAction::kHideDeviceListForCasting);

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

TEST_F(MediaItemUIUpdatedViewTest, DeviceSelectorViewIssueCheck) {
  SkBitmap bitmap = *view()
                         ->GetStartCastingButtonForTesting()
                         ->GetImage(views::Button::STATE_NORMAL)
                         .bitmap();
  view()->UpdateDeviceSelectorIssue(/*has_issue=*/true);
  SkBitmap bitmap_with_issue = *view()
                                    ->GetStartCastingButtonForTesting()
                                    ->GetImage(views::Button::STATE_NORMAL)
                                    .bitmap();
  EXPECT_FALSE(gfx::test::AreBitmapsEqual(bitmap, bitmap_with_issue));
  view()->UpdateDeviceSelectorIssue(/*has_issue=*/false);
  SkBitmap bitmap_without_issue = *view()
                                       ->GetStartCastingButtonForTesting()
                                       ->GetImage(views::Button::STATE_NORMAL)
                                       .bitmap();
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(bitmap, bitmap_without_issue));
}

TEST_F(MediaItemUIUpdatedViewTest, FooterViewCheck) {
  EnableAllMediaActions();
  auto* pip_button = view()->GetMediaActionButtonForTesting(
      MediaSessionAction::kEnterPictureInPicture);
  EXPECT_TRUE(pip_button->GetVisible());
  EXPECT_FALSE(view()->GetCastingIndicatorViewForTesting()->GetVisible());

  auto footer_view = std::make_unique<NiceMock<MockMediaItemUIFooter>>();
  auto* footer_ptr = footer_view.get();
  view()->UpdateFooterView(std::move(footer_view));
  EXPECT_FALSE(pip_button->GetVisible());
  EXPECT_TRUE(view()->GetCastingIndicatorViewForTesting()->GetVisible());
  EXPECT_EQ(footer_ptr, view()->GetFooterForTesting());
}

TEST_F(MediaItemUIUpdatedViewTest, DragProgressBackwardForPlayingMedia) {
  EnableAllMediaActions();
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(10),
      /*position=*/base::Seconds(7), /*end_of_media=*/false);
  view()->UpdateWithMediaPosition(media_position);
  auto* progress_view = view()->GetProgressViewForTesting();

  // Starts dragging the progress view should pause the media.
  gfx::Point point(progress_view->width() / 2, progress_view->height() / 2);
  ui::MouseEvent pressed_event(ui::EventType::kMousePressed, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(item(), SeekTo(testing::_));
  EXPECT_CALL(item(),
              OnMediaSessionActionButtonPressed(MediaSessionAction::kPause));
  progress_view->OnMousePressed(pressed_event);
  progress_drag_timer()->Fire();

  // Starts dragging should hide these media action buttons.
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kSeekBackward));

  // Ends dragging the progress view should resume the media.
  media_session::MediaPosition media_position_released(
      /*playback_rate=*/1, /*duration=*/base::Seconds(10),
      /*position=*/base::Seconds(5), /*end_of_media=*/false);
  view()->UpdateWithMediaPosition(media_position_released);
  ui::MouseEvent released_event =
      ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(item(), SeekTo(testing::_));
  EXPECT_CALL(item(),
              OnMediaSessionActionButtonPressed(MediaSessionAction::kPlay));
  progress_view->OnMouseReleased(released_event);

  // Ends dragging should show these media action buttons.
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kSeekBackward));

  ExpectActionHistogramCount(
      MediaItemUIUpdatedViewAction::kProgressViewSeekBackward);
}

TEST_F(MediaItemUIUpdatedViewTest, DragProgressForwardForPausedMedia) {
  EnableAllMediaActions();
  media_session::MediaPosition media_position(
      /*playback_rate=*/0, /*duration=*/base::Seconds(10),
      /*position=*/base::Seconds(3), /*end_of_media=*/false);
  view()->UpdateWithMediaPosition(media_position);
  auto* progress_view = view()->GetProgressViewForTesting();

  // Starts dragging the progress view.
  gfx::Point point(progress_view->width() / 2, progress_view->height() / 2);
  ui::MouseEvent pressed_event(ui::EventType::kMousePressed, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(item(), SeekTo(testing::_));
  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(testing::_)).Times(0);
  progress_view->OnMousePressed(pressed_event);
  progress_drag_timer()->Fire();

  // Starts dragging should hide these media action buttons.
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kSeekBackward));

  // Ends dragging the progress view.
  media_session::MediaPosition media_position_released(
      /*playback_rate=*/1, /*duration=*/base::Seconds(10),
      /*position=*/base::Seconds(5), /*end_of_media=*/false);
  view()->UpdateWithMediaPosition(media_position_released);
  ui::MouseEvent released_event =
      ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(item(), SeekTo(testing::_));
  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(testing::_)).Times(0);
  progress_view->OnMouseReleased(released_event);

  // Ends dragging should show these media action buttons.
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kSeekBackward));

  ExpectActionHistogramCount(
      MediaItemUIUpdatedViewAction::kProgressViewSeekForward);
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
  ui::MouseEvent pressed_event(ui::EventType::kMousePressed, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(item(), SeekTo(testing::_));
  progress_view->OnMousePressed(pressed_event);
  progress_drag_timer()->Fire();
  EXPECT_TRUE(view()->GetCurrentTimestampLabelForTesting()->GetVisible());
  EXPECT_TRUE(view()->GetDurationTimestampLabelForTesting()->GetVisible());
  EXPECT_EQ(u"0:05", view()->GetCurrentTimestampLabelForTesting()->GetText());
  EXPECT_EQ(u"0:10", view()->GetDurationTimestampLabelForTesting()->GetText());

  // Ends dragging the progress view should hide the timestamp labels.
  ui::MouseEvent released_event =
      ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(item(), SeekTo(testing::_));
  progress_view->OnMouseReleased(released_event);
  EXPECT_FALSE(view()->GetCurrentTimestampLabelForTesting()->GetVisible());
  EXPECT_FALSE(view()->GetDurationTimestampLabelForTesting()->GetVisible());
}

TEST_F(MediaItemUIUpdatedViewTest, LiveMediaViewCheck) {
  EXPECT_TRUE(view()->GetProgressViewForTesting()->GetVisible());
  EXPECT_FALSE(view()->GetLiveStatusViewForTesting()->GetVisible());

  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::TimeDelta::Max(),
      /*position=*/base::Seconds(5), /*end_of_media=*/false);
  view()->UpdateWithMediaPosition(media_position);
  EXPECT_FALSE(view()->GetProgressViewForTesting()->GetVisible());
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kSeekBackward));
  EXPECT_TRUE(view()->GetLiveStatusViewForTesting()->GetVisible());
}

TEST_F(MediaItemUIUpdatedViewTest, LiveMediaViewWithMediaActionButtons) {
  EnableAllMediaActions();
  EXPECT_TRUE(view()->GetProgressViewForTesting()->GetVisible());
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kSeekBackward));
  EXPECT_FALSE(view()->GetLiveStatusViewForTesting()->GetVisible());

  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::TimeDelta::Max(),
      /*position=*/base::Seconds(5), /*end_of_media=*/false);
  view()->UpdateWithMediaPosition(media_position);
  EXPECT_FALSE(view()->GetProgressViewForTesting()->GetVisible());
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(IsMediaActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_FALSE(IsMediaActionButtonVisible(MediaSessionAction::kSeekBackward));
  EXPECT_TRUE(view()->GetLiveStatusViewForTesting()->GetVisible());
}

}  // namespace global_media_controls
