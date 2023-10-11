// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_notification_view_ash_impl.h"

#include "components/global_media_controls/public/test/mock_media_item_ui_device_selector.h"
#include "components/global_media_controls/public/test/mock_media_item_ui_footer.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/mock_media_notification_item.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"

namespace global_media_controls {

using ::global_media_controls::test::MockMediaItemUIDeviceSelector;
using ::global_media_controls::test::MockMediaItemUIFooter;
using ::media_message_center::test::MockMediaNotificationItem;
using ::media_session::mojom::MediaSessionAction;
using ::testing::_;
using ::testing::NiceMock;

namespace {

class MockMediaNotificationContainer
    : public media_message_center::MediaNotificationContainer {
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

    container_ = std::make_unique<NiceMock<MockMediaNotificationContainer>>();
    item_ = std::make_unique<NiceMock<MockMediaNotificationItem>>();

    auto device_selector =
        std::make_unique<NiceMock<MockMediaItemUIDeviceSelector>>();
    device_selector_ = device_selector.get();
    device_selector_->SetPreferredSize(gfx::Size(400, 50));

    // Create a widget and add the view to show on the screen for testing screen
    // coordinates and focus.
    widget_ = CreateTestWidget();
    view_ =
        widget_->SetContentsView(std::make_unique<MediaNotificationViewAshImpl>(
            container_.get(), item_->GetWeakPtr(), /*footer_view=*/nullptr,
            std::move(device_selector), /*dismiss_button=*/nullptr,
            media_message_center::MediaColorTheme(),
            MediaDisplayPage::kQuickSettingsMediaDetailedView));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    device_selector_ = nullptr;
    widget_.reset();
    actions_.clear();

    views::ViewsTestBase::TearDown();
  }

  std::unique_ptr<MediaNotificationViewAshImpl> CreateView(
      MediaDisplayPage media_display_page) {
    return std::make_unique<MediaNotificationViewAshImpl>(
        container_.get(), item_->GetWeakPtr(), /*footer_view=*/nullptr,
        /*device_selector_view=*/nullptr, /*dismiss_button=*/nullptr,
        media_message_center::MediaColorTheme(), media_display_page);
  }

  std::unique_ptr<MediaNotificationViewAshImpl> CreateViewWithFooter(
      std::unique_ptr<MediaItemUIFooter> footer) {
    auto device_selector =
        std::make_unique<NiceMock<MockMediaItemUIDeviceSelector>>();
    return std::make_unique<MediaNotificationViewAshImpl>(
        container_.get(), item_->GetWeakPtr(), std::move(footer),
        std::move(device_selector), /*dismiss_button=*/nullptr,
        media_message_center::MediaColorTheme(),
        MediaDisplayPage::kQuickSettingsMediaView);
  }

  std::unique_ptr<MediaNotificationViewAshImpl> CreateLockScreenMediaView() {
    return std::make_unique<MediaNotificationViewAshImpl>(
        container_.get(), /*item=*/nullptr, /*footer_view=*/nullptr,
        /*device_selector_view=*/nullptr,
        /*dismiss_button=*/std::make_unique<views::View>(),
        media_message_center::MediaColorTheme(),
        MediaDisplayPage::kLockScreenMediaView);
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

  MockMediaNotificationContainer& container() { return *container_; }

  MediaNotificationViewAshImpl* view() const { return view_; }

  MockMediaNotificationItem& item() { return *item_; }

  MockMediaItemUIDeviceSelector* device_selector() { return device_selector_; }

  bool IsActionButtonVisible(MediaSessionAction action) const {
    views::Button* button = view()->GetActionButtonForTesting(action);
    return button && button->GetVisible();
  }

  void SimulateButtonClick(MediaSessionAction action) {
    views::Button* button = view()->GetActionButtonForTesting(action);
    EXPECT_TRUE(button && button->GetVisible());

    views::test::ButtonTestApi(button).NotifyClick(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0));
  }

 private:
  void NotifyUpdatedActions() { view_->UpdateWithMediaActions(actions_); }

  base::flat_set<MediaSessionAction> actions_;
  std::unique_ptr<MockMediaNotificationContainer> container_;
  std::unique_ptr<MockMediaNotificationItem> item_;
  raw_ptr<MediaNotificationViewAshImpl> view_;
  raw_ptr<MockMediaItemUIDeviceSelector> device_selector_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(MediaNotificationViewAshImplTest, ChevronIconVisibilityCheck) {
  auto view = CreateView(MediaDisplayPage::kQuickSettingsMediaView);
  EXPECT_NE(view->GetChevronIconForTesting(), nullptr);

  view = CreateView(MediaDisplayPage::kQuickSettingsMediaDetailedView);
  EXPECT_EQ(view->GetChevronIconForTesting(), nullptr);

  view = CreateView(MediaDisplayPage::kSystemShelfMediaDetailedView);
  EXPECT_EQ(view->GetChevronIconForTesting(), nullptr);

  view = CreateLockScreenMediaView();
  EXPECT_EQ(view->GetChevronIconForTesting(), nullptr);
}

TEST_F(MediaNotificationViewAshImplTest, DeviceSelectorViewCheck) {
  EXPECT_NE(view()->GetStartCastingButtonForTesting(), nullptr);
  EXPECT_FALSE(view()->GetStartCastingButtonForTesting()->GetVisible());
  EXPECT_EQ(view()->GetDeviceSelectorForTesting(), device_selector());
  EXPECT_FALSE(view()->GetDeviceSelectorForTesting()->GetVisible());
  EXPECT_NE(view()->GetDeviceSelectorSeparatorForTesting(), nullptr);
  EXPECT_FALSE(view()->GetDeviceSelectorSeparatorForTesting()->GetVisible());

  EXPECT_CALL(*device_selector(), IsDeviceSelectorExpanded())
      .WillOnce(testing::Return(false));
  view()->UpdateDeviceSelectorAvailability(/*has_devices=*/true);
  EXPECT_TRUE(view()->GetStartCastingButtonForTesting()->GetVisible());
  EXPECT_TRUE(view()->GetDeviceSelectorForTesting()->GetVisible());
  EXPECT_FALSE(view()->GetDeviceSelectorSeparatorForTesting()->GetVisible());

  EXPECT_CALL(*device_selector(), ShowOrHideDeviceList());
  EXPECT_CALL(*device_selector(), IsDeviceSelectorExpanded())
      .WillOnce(testing::Return(true));
  views::test::ButtonTestApi(view()->GetStartCastingButtonForTesting())
      .NotifyClick(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  EXPECT_TRUE(view()->GetDeviceSelectorSeparatorForTesting()->GetVisible());
}

TEST_F(MediaNotificationViewAshImplTest, FooterViewCheck) {
  auto footer = std::make_unique<NiceMock<MockMediaItemUIFooter>>();
  auto* footer_ptr = footer.get();
  auto view = CreateViewWithFooter(std::move(footer));

  EXPECT_EQ(view->GetFooterForTesting(), footer_ptr);
  EXPECT_NE(view->GetStartCastingButtonForTesting(), nullptr);
  EXPECT_FALSE(view->GetStartCastingButtonForTesting()->GetVisible());

  base::flat_set<MediaSessionAction> actions;
  actions.insert(MediaSessionAction::kEnterPictureInPicture);
  view->UpdateWithMediaActions(actions);

  views::Button* button = view->GetActionButtonForTesting(
      MediaSessionAction::kEnterPictureInPicture);
  EXPECT_NE(button, nullptr);
  EXPECT_FALSE(button->GetVisible());
}

TEST_F(MediaNotificationViewAshImplTest, MetadataUpdated) {
  EXPECT_EQ(view()->GetSourceLabelForTesting()->GetText(), u"");
  EXPECT_EQ(view()->GetArtistLabelForTesting()->GetText(), u"");
  EXPECT_EQ(view()->GetTitleLabelForTesting()->GetText(), u"");

  media_session::MediaMetadata metadata;
  metadata.source_title = u"source title";
  metadata.title = u"title";
  metadata.artist = u"artist";

  EXPECT_CALL(container(), OnMediaSessionMetadataChanged(_));
  view()->UpdateWithMediaMetadata(metadata);

  EXPECT_EQ(view()->GetSourceLabelForTesting()->GetText(),
            metadata.source_title);
  EXPECT_EQ(view()->GetArtistLabelForTesting()->GetText(), metadata.artist);
  EXPECT_EQ(view()->GetTitleLabelForTesting()->GetText(), metadata.title);
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
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPlay));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPause));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kStop));
  EXPECT_FALSE(
      IsActionButtonVisible(MediaSessionAction::kEnterPictureInPicture));
  EXPECT_FALSE(
      IsActionButtonVisible(MediaSessionAction::kExitPictureInPicture));

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

}  // namespace global_media_controls
