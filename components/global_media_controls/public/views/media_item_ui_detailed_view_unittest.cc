// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_detailed_view.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/global_media_controls/public/test/mock_media_item_ui_device_selector.h"
#include "components/global_media_controls/public/test/mock_media_item_ui_footer.h"
#include "components/global_media_controls/public/views/media_progress_view.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/mock_media_notification_item.h"
#include "components/strings/grit/components_strings.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/accessibility/view_accessibility.h"
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
using ::testing::Return;

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
  MOCK_METHOD1(OnHeaderClicked, void(bool activate_original_media));
};

}  // namespace

class MediaItemUIDetailedViewTest : public views::ViewsTestBase {
 public:
  MediaItemUIDetailedViewTest() = default;
  MediaItemUIDetailedViewTest(const MediaItemUIDetailedViewTest&) = delete;
  MediaItemUIDetailedViewTest& operator=(const MediaItemUIDetailedViewTest&) =
      delete;
  ~MediaItemUIDetailedViewTest() override = default;

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
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    view_ = widget_->SetContentsView(std::make_unique<MediaItemUIDetailedView>(
        container_.get(), item_->GetWeakPtr(), /*footer_view=*/nullptr,
        std::move(device_selector), /*dismiss_button=*/nullptr,
        media_message_center::MediaColorTheme(),
        MediaDisplayPage::kQuickSettingsMediaDetailedView));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    device_selector_ = nullptr;
    widget_->Close();
    actions_.clear();

    views::ViewsTestBase::TearDown();
  }

  std::unique_ptr<MediaItemUIDetailedView> CreateView(
      MediaDisplayPage media_display_page) {
    auto device_selector =
        std::make_unique<NiceMock<MockMediaItemUIDeviceSelector>>();
    return std::make_unique<MediaItemUIDetailedView>(
        container_.get(), item_->GetWeakPtr(), /*footer_view=*/nullptr,
        /*device_selector_view=*/std::move(device_selector),
        /*dismiss_button=*/nullptr, media_message_center::MediaColorTheme(),
        media_display_page);
  }

  std::unique_ptr<MediaItemUIDetailedView> CreateViewWithFooter(
      std::unique_ptr<MediaItemUIFooter> footer) {
    auto device_selector =
        std::make_unique<NiceMock<MockMediaItemUIDeviceSelector>>();
    return std::make_unique<MediaItemUIDetailedView>(
        container_.get(), item_->GetWeakPtr(), std::move(footer),
        std::move(device_selector), /*dismiss_button=*/nullptr,
        media_message_center::MediaColorTheme(),
        MediaDisplayPage::kQuickSettingsMediaView);
  }

  std::unique_ptr<MediaItemUIDetailedView> CreateLockScreenMediaView() {
    return std::make_unique<MediaItemUIDetailedView>(
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
    actions_.insert(MediaSessionAction::kSeekForward);
    actions_.insert(MediaSessionAction::kSeekBackward);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

  MediaItemUIDetailedView* view() const { return view_; }

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
        ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0));
  }

 private:
  void NotifyUpdatedActions() { view_->UpdateWithMediaActions(actions_); }

  base::flat_set<MediaSessionAction> actions_;
  std::unique_ptr<MockMediaNotificationContainer> container_;
  std::unique_ptr<MockMediaNotificationItem> item_;
  raw_ptr<MediaItemUIDetailedView> view_;
  raw_ptr<MockMediaItemUIDeviceSelector> device_selector_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(MediaItemUIDetailedViewTest, ChevronIconVisibilityCheck) {
  auto view = CreateView(MediaDisplayPage::kQuickSettingsMediaView);
  EXPECT_NE(view->GetChevronIconForTesting(), nullptr);

  view = CreateView(MediaDisplayPage::kQuickSettingsMediaDetailedView);
  EXPECT_EQ(view->GetChevronIconForTesting(), nullptr);

  view = CreateView(MediaDisplayPage::kSystemShelfMediaDetailedView);
  EXPECT_EQ(view->GetChevronIconForTesting(), nullptr);

  view = CreateLockScreenMediaView();
  EXPECT_EQ(view->GetChevronIconForTesting(), nullptr);
}

TEST_F(MediaItemUIDetailedViewTest, AccessibleProperties) {
  auto view = CreateView(MediaDisplayPage::kQuickSettingsMediaView);
  EXPECT_EQ(view->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kListItem);
  EXPECT_EQ(view->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(
                IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACCESSIBLE_NAME));
}

TEST_F(MediaItemUIDetailedViewTest, DeviceSelectorViewCheck) {
  EXPECT_NE(view()->GetStartCastingButtonForTesting(), nullptr);
  EXPECT_FALSE(view()->GetStartCastingButtonForTesting()->GetVisible());
  EXPECT_EQ(view()->GetDeviceSelectorForTesting(), device_selector());
  EXPECT_FALSE(view()->GetDeviceSelectorForTesting()->GetVisible());
  EXPECT_NE(view()->GetDeviceSelectorSeparatorForTesting(), nullptr);
  EXPECT_FALSE(view()->GetDeviceSelectorSeparatorForTesting()->GetVisible());

  // Add devices to the list to show the start casting button.
  EXPECT_CALL(*device_selector(), IsDeviceSelectorExpanded())
      .WillOnce(Return(false));
  view()->UpdateDeviceSelectorAvailability(/*has_devices=*/true);
  EXPECT_TRUE(view()->GetStartCastingButtonForTesting()->GetVisible());
  EXPECT_TRUE(view()->GetDeviceSelectorForTesting()->GetVisible());
  EXPECT_FALSE(view()->GetDeviceSelectorSeparatorForTesting()->GetVisible());

  // Click the start casting button to show devices.
  EXPECT_CALL(*device_selector(), ShowDevices());
  EXPECT_CALL(*device_selector(), IsDeviceSelectorExpanded())
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  views::test::ButtonTestApi(view()->GetStartCastingButtonForTesting())
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  EXPECT_TRUE(view()->GetDeviceSelectorSeparatorForTesting()->GetVisible());

  // Click the start casting button to hide devices.
  EXPECT_CALL(*device_selector(), HideDevices());
  EXPECT_CALL(*device_selector(), IsDeviceSelectorExpanded())
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  views::test::ButtonTestApi(view()->GetStartCastingButtonForTesting())
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  EXPECT_FALSE(view()->GetDeviceSelectorSeparatorForTesting()->GetVisible());
}

TEST_F(MediaItemUIDetailedViewTest, FooterViewCheck) {
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

TEST_F(MediaItemUIDetailedViewTest, MetadataUpdated) {
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

TEST_F(MediaItemUIDetailedViewTest, PlayPauseButtonDisplay) {
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

TEST_F(MediaItemUIDetailedViewTest, PictureInPictureButtonDisplay) {
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

TEST_F(MediaItemUIDetailedViewTest, ButtonVisibilityCheck) {
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

TEST_F(MediaItemUIDetailedViewTest, NextTrackButtonClick) {
  EnableAction(MediaSessionAction::kNextTrack);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kNextTrack));
  SimulateButtonClick(MediaSessionAction::kNextTrack);
}

TEST_F(MediaItemUIDetailedViewTest, PlayButtonClick) {
  EnableAction(MediaSessionAction::kPlay);

  EXPECT_CALL(item(),
              OnMediaSessionActionButtonPressed(MediaSessionAction::kPlay));
  SimulateButtonClick(MediaSessionAction::kPlay);
}

TEST_F(MediaItemUIDetailedViewTest, PauseButtonClick) {
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

TEST_F(MediaItemUIDetailedViewTest, PreviousTrackButtonClick) {
  EnableAction(MediaSessionAction::kPreviousTrack);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kPreviousTrack));
  SimulateButtonClick(MediaSessionAction::kPreviousTrack);
}

TEST_F(MediaItemUIDetailedViewTest, EnterPictureInPictureButtonClick) {
  EnableAction(MediaSessionAction::kEnterPictureInPicture);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kEnterPictureInPicture));
  SimulateButtonClick(MediaSessionAction::kEnterPictureInPicture);
}

TEST_F(MediaItemUIDetailedViewTest, ExitPictureInPictureButtonClick) {
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

TEST_F(MediaItemUIDetailedViewTest, ProgressViewCheck) {
  auto view = CreateView(MediaDisplayPage::kQuickSettingsMediaView);
  EXPECT_NE(view->GetProgressViewForTesting(), nullptr);

  // Check that progress position can be updated.
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(10),
      /*position=*/base::Seconds(5), /*end_of_media=*/false);
  view->UpdateWithMediaPosition(media_position);
  EXPECT_NEAR(view->GetProgressViewForTesting()->current_value_for_testing(),
              0.5, 0.001);

  // Check that key event on the view can seek the progress.
  ui::KeyEvent key_event{ui::EventType::kKeyPressed, ui::VKEY_RIGHT,
                         ui::DomCode::ARROW_RIGHT,   ui::EF_NONE,
                         ui::DomKey::ARROW_RIGHT,    ui::EventTimeForNow()};
  EXPECT_CALL(item(), SeekTo(testing::_));
  view->OnKeyPressed(key_event);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(MediaItemUIDetailedViewTest, ChapterList) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(media::kBackgroundListening);
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* view = widget->SetContentsView(
      CreateView(MediaDisplayPage::kSystemShelfMediaDetailedView));

  // Chapter list is not created yet.
  EXPECT_EQ(view->GetTitleLabelForTesting()->GetText(), u"");
  EXPECT_FALSE(!!view->GetChapterListViewForTesting());
  EXPECT_EQ(view->GetChaptersForTesting().find(0),
            view->GetChaptersForTesting().end());
  EXPECT_FALSE(view->GetChapterListButtonForTesting()->GetVisible());

  std::vector<media_session::ChapterInformation> expected_chapters;
  media_session::MediaImage test_image_1;
  test_image_1.src = GURL("https://www.google.com");
  media_session::MediaImage test_image_2;
  test_image_2.src = GURL("https://www.example.org");
  media_session::ChapterInformation test_chapter_1(
      /*title=*/u"chapter1", /*startTime=*/base::Seconds(10),
      /*artwork=*/{test_image_1});
  media_session::ChapterInformation test_chapter_2(
      /*title=*/u"chapter2", /*startTime=*/base::Seconds(20),
      /*artwork=*/{test_image_2});
  expected_chapters.push_back(test_chapter_1);
  expected_chapters.push_back(test_chapter_2);

  media_session::MediaMetadata metadata;
  metadata.source_title = u"source title";
  metadata.title = u"title";
  metadata.artist = u"artist";
  metadata.chapters = expected_chapters;

  EXPECT_CALL(container(), OnMediaSessionMetadataChanged(_));
  view->UpdateWithMediaMetadata(metadata);

  // Before clicking on the chapter list button, the list is not visible.
  EXPECT_TRUE(view->GetChapterListButtonForTesting()->GetVisible());
  EXPECT_FALSE(view->GetChapterListViewForTesting()->GetVisible());

  // Click the start chapter list button to show the chapters.
  views::test::ButtonTestApi(view->GetChapterListButtonForTesting())
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  EXPECT_TRUE(view->GetChapterListViewForTesting()->GetVisible());
  EXPECT_EQ(view->GetTitleLabelForTesting()->GetText(), metadata.title);
  EXPECT_EQ(view->GetChapterListViewForTesting()->children().size(), 2u);
  EXPECT_EQ(view->GetChaptersForTesting().find(0)->second->chapter().title(),
            u"chapter1");
  EXPECT_EQ(view->GetChaptersForTesting().find(1)->second->chapter().title(),
            u"chapter2");
  EXPECT_EQ(view->GetChaptersForTesting()
                .find(0)
                ->second->chapter()
                .startTime()
                .InSeconds(),
            10);
  EXPECT_EQ(view->GetChaptersForTesting()
                .find(1)
                ->second->chapter()
                .startTime()
                .InSeconds(),
            20);

  // Clicking on a chapter item should seek to the start time of that chapter.
  EXPECT_CALL(item(), SeekTo(base::Seconds(10)));
  views::test::ButtonTestApi(view->GetChaptersForTesting().find(0)->second)
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(item(), SeekTo(base::Seconds(20)));
  views::test::ButtonTestApi(view->GetChaptersForTesting().find(1)->second)
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  testing::Mock::VerifyAndClearExpectations(this);

  // Showing cast view should hide the chapter list view.
  view->UpdateDeviceSelectorAvailability(/*has_devices=*/true);
  EXPECT_TRUE(view->GetStartCastingButtonForTesting()->GetVisible());
  views::test::ButtonTestApi(view->GetStartCastingButtonForTesting())
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  EXPECT_TRUE(view->GetChapterListButtonForTesting()->GetVisible());
  EXPECT_FALSE(view->GetChapterListViewForTesting()->GetVisible());

  // Showing the chapter list view should also hide the cast view.
  views::test::ButtonTestApi(view->GetChapterListButtonForTesting())
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  EXPECT_FALSE(view->GetDeviceSelectorForTesting()->IsDeviceSelectorExpanded());
  EXPECT_TRUE(view->GetChapterListViewForTesting()->GetVisible());
  EXPECT_EQ(view->GetChapterListViewForTesting()->children().size(), 2u);

  // Should not show chapter list button when there's no chapters.
  media_session::MediaMetadata metadata_with_0_chpaters;
  metadata_with_0_chpaters.source_title = u"source title 0";
  metadata_with_0_chpaters.title = u"title 0";
  metadata_with_0_chpaters.artist = u"artist 0";

  EXPECT_CALL(container(), OnMediaSessionMetadataChanged(_));
  view->UpdateWithMediaMetadata(metadata_with_0_chpaters);
  EXPECT_FALSE(view->GetChapterListButtonForTesting()->GetVisible());
  EXPECT_FALSE(view->GetChapterListViewForTesting()->GetVisible());
  EXPECT_EQ(view->GetTitleLabelForTesting()->GetText(),
            metadata_with_0_chpaters.title);
  EXPECT_EQ(view->GetChapterListViewForTesting()->children().size(), 0u);
}

TEST_F(MediaItemUIDetailedViewTest, ShouldNotShowDeviceSelectorViewForAsh) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(media::kBackgroundListening);
  auto* start_casting_button = view()->GetStartCastingButtonForTesting();
  auto* separator = view()->GetDeviceSelectorSeparatorForTesting();
  auto* device_selector_view = view()->GetDeviceSelectorForTesting();

  ASSERT_TRUE(start_casting_button);
  EXPECT_FALSE(start_casting_button->GetVisible());
  EXPECT_EQ(device_selector_view, device_selector());
  EXPECT_FALSE(device_selector_view->GetVisible());
  ASSERT_TRUE(separator);
  EXPECT_FALSE(separator->GetVisible());

  // Add devices to the list to show the start casting button.
  EXPECT_CALL(*device_selector(), IsDeviceSelectorExpanded())
      .WillOnce(Return(false));
  view()->UpdateDeviceSelectorAvailability(/*has_devices=*/true);
  EXPECT_TRUE(start_casting_button->GetVisible());
  EXPECT_TRUE(device_selector_view->GetVisible());
  EXPECT_FALSE(separator->GetVisible());

  // Click the start casting button to show devices.
  EXPECT_CALL(*device_selector(), ShowDevices());
  EXPECT_CALL(*device_selector(), IsDeviceSelectorExpanded())
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  views::test::ButtonTestApi(start_casting_button)
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  EXPECT_FALSE(separator->GetVisible());

  // Click the start casting button to hide devices.
  EXPECT_CALL(*device_selector(), HideDevices());
  EXPECT_CALL(*device_selector(), IsDeviceSelectorExpanded())
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  views::test::ButtonTestApi(start_casting_button)
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  EXPECT_FALSE(separator->GetVisible());
}

TEST_F(MediaItemUIDetailedViewTest, Forward10ButtonClick) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(media::kBackgroundListening);
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* view = widget->SetContentsView(
      CreateView(MediaDisplayPage::kSystemShelfMediaDetailedView));
  view->UpdateWithMediaActions({MediaSessionAction::kSeekForward});
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(58),
      /*position=*/base::Seconds(5), /*end_of_media=*/false);
  view->UpdateWithMediaPosition(media_position);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kSeekForward))
      .Times(0);
  EXPECT_CALL(item(),
              SeekTo(::testing::AllOf(::testing::Ge(base::Seconds(15)),
                                      ::testing::Le(base::Seconds(16)))));
  views::Button* button =
      view->GetActionButtonForTesting(MediaSessionAction::kSeekForward);
  EXPECT_TRUE(button && button->GetVisible());
  views::test::ButtonTestApi(button).NotifyClick(
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0));
}

TEST_F(MediaItemUIDetailedViewTest, Backward10ButtonClick) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(media::kBackgroundListening);
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* view = widget->SetContentsView(
      CreateView(MediaDisplayPage::kSystemShelfMediaDetailedView));
  view->UpdateWithMediaActions({MediaSessionAction::kSeekBackward});
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(58),
      /*position=*/base::Seconds(38), /*end_of_media=*/false);
  view->UpdateWithMediaPosition(media_position);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kSeekBackward))
      .Times(0);
  EXPECT_CALL(item(),
              SeekTo(::testing::AllOf(::testing::Ge(base::Seconds(28)),
                                      ::testing::Le(base::Seconds(29)))));
  views::Button* button =
      view->GetActionButtonForTesting(MediaSessionAction::kSeekBackward);
  EXPECT_TRUE(button && button->GetVisible());
  views::test::ButtonTestApi(button).NotifyClick(
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0));
  testing::Mock::VerifyAndClearExpectations(this);
}

TEST_F(MediaItemUIDetailedViewTest, TimestampView) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(media::kBackgroundListening);
  auto view = CreateView(MediaDisplayPage::kSystemShelfMediaDetailedView);
  EXPECT_NE(view->GetProgressViewForTesting(), nullptr);

  // Check that the timestamp gets updated when the progress position is
  // updated.
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(58),
      /*position=*/base::Seconds(5), /*end_of_media=*/false);
  view->UpdateWithMediaPosition(media_position);
  EXPECT_EQ(view->GetCurrentTimestampViewForTesting()->GetText(), u"0:05");
  EXPECT_EQ(view->GetTotalDurationViewForTesting()->GetText(), u" / 0:58");

  media_session::MediaPosition media_position2(
      /*playback_rate=*/1, /*duration=*/base::Seconds(108),
      /*position=*/base::Seconds(66), /*end_of_media=*/false);
  view->UpdateWithMediaPosition(media_position2);
  EXPECT_EQ(view->GetCurrentTimestampViewForTesting()->GetText(), u"1:06");
  EXPECT_EQ(view->GetTotalDurationViewForTesting()->GetText(), u" / 1:48");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace global_media_controls
