// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_view_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/media_message_center/media_notification_background_impl.h"
#include "components/media_message_center/media_notification_constants.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/media_notification_controller.h"
#include "components/media_message_center/media_session_notification_item.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/base_event_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/test/views_test_base.h"

namespace media_message_center {

using media_session::mojom::MediaSessionAction;
using media_session::test::TestMediaController;
using testing::_;
using testing::Expectation;
using testing::Invoke;

namespace {

// The icons size is 24 and INSETS_VECTOR_IMAGE_BUTTON will add padding around
// the image.
const int kMediaButtonIconSize = 24;

// The title artist row should always have the same height.
const int kMediaTitleArtistRowExpectedHeight = 48;

const char kTestDefaultAppName[] = "default app name";
const char kTestAppName[] = "app name";

const gfx::Size kWidgetSize(500, 500);

constexpr int kViewWidth = 400;
constexpr int kViewArtworkWidth = kViewWidth * 0.4;
const gfx::Size kViewSize(kViewWidth, 400);

class MockMediaNotificationController : public MediaNotificationController {
 public:
  MockMediaNotificationController() = default;
  ~MockMediaNotificationController() = default;

  // MediaNotificationController implementation.
  MOCK_METHOD(void, ShowNotification, (const std::string& id));
  MOCK_METHOD(void, HideNotification, (const std::string& id));
  MOCK_METHOD(void, RemoveItem, (const std::string& id));
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() const override {
    return nullptr;
  }
  MOCK_METHOD(void,
              LogMediaSessionActionButtonPressed,
              (const std::string& id,
               media_session::mojom::MediaSessionAction action));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaNotificationController);
};

class MockMediaNotificationContainer : public MediaNotificationContainer {
 public:
  MockMediaNotificationContainer() = default;
  ~MockMediaNotificationContainer() = default;

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
  MOCK_METHOD2(OnColorsChanged, void(SkColor foreground, SkColor background));
  MOCK_METHOD0(OnHeaderClicked, void());

  MediaNotificationViewImpl* view() const { return view_; }
  void SetView(MediaNotificationViewImpl* view) { view_ = view; }

 private:
  MediaNotificationViewImpl* view_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaNotificationContainer);
};

}  // namespace

class MediaNotificationViewImplTest : public views::ViewsTestBase {
 public:
  MediaNotificationViewImplTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~MediaNotificationViewImplTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    request_id_ = base::UnguessableToken::Create();

    // Create a new MediaNotificationViewImpl whenever the
    // MediaSessionNotificationItem says to show the notification.
    EXPECT_CALL(controller_, ShowNotification(request_id_.ToString()))
        .WillRepeatedly(InvokeWithoutArgs(
            this, &MediaNotificationViewImplTest::CreateView));

    // Create a widget to show on the screen for testing screen coordinates and
    // focus.
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(kWidgetSize);
    widget_->Init(std::move(params));
    widget_->Show();

    CreateViewFromMediaSessionInfo(
        media_session::mojom::MediaSessionInfo::New());
  }

  void CreateViewFromMediaSessionInfo(
      media_session::mojom::MediaSessionInfoPtr session_info) {
    session_info->is_controllable = true;
    mojo::Remote<media_session::mojom::MediaController> controller;
    item_ = std::make_unique<MediaSessionNotificationItem>(
        &controller_, request_id_.ToString(), std::string(),
        std::move(controller), std::move(session_info));

    // Update the metadata.
    media_session::MediaMetadata metadata;
    metadata.title = base::ASCIIToUTF16("title");
    metadata.artist = base::ASCIIToUTF16("artist");
    item_->MediaSessionMetadataChanged(metadata);

    // Inject the test media controller into the item.
    media_controller_ = std::make_unique<TestMediaController>();
    item_->SetMediaControllerForTesting(
        media_controller_->CreateMediaControllerRemote());
  }

  void TearDown() override {
    container_.SetView(nullptr);
    widget_.reset();

    actions_.clear();

    views::ViewsTestBase::TearDown();
  }

  void EnableAllActions() {
    actions_.insert(MediaSessionAction::kPlay);
    actions_.insert(MediaSessionAction::kPause);
    actions_.insert(MediaSessionAction::kPreviousTrack);
    actions_.insert(MediaSessionAction::kNextTrack);
    actions_.insert(MediaSessionAction::kSeekBackward);
    actions_.insert(MediaSessionAction::kSeekForward);
    actions_.insert(MediaSessionAction::kStop);
    actions_.insert(MediaSessionAction::kEnterPictureInPicture);
    actions_.insert(MediaSessionAction::kExitPictureInPicture);

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

  MockMediaNotificationController& controller() { return controller_; }

  MediaNotificationViewImpl* view() const { return container_.view(); }

  TestMediaController* media_controller() const {
    return media_controller_.get();
  }

  message_center::NotificationHeaderView* GetHeaderRow(
      MediaNotificationViewImpl* view) const {
    return view->header_row_;
  }

  message_center::NotificationHeaderView* header_row() const {
    return GetHeaderRow(view());
  }

  const base::string16& accessible_name() const {
    return view()->accessible_name_;
  }

  views::View* button_row() const { return view()->button_row_; }

  const views::View* playback_button_container() const {
    return view()->playback_button_container_;
  }

  views::View* title_artist_row() const { return view()->title_artist_row_; }

  views::Label* title_label() const { return view()->title_label_; }

  views::Label* artist_label() const { return view()->artist_label_; }

  views::Button* GetButtonForAction(MediaSessionAction action) const {
    auto buttons = view()->get_buttons_for_testing();
    const auto i = std::find_if(
        buttons.begin(), buttons.end(), [action](const views::View* v) {
          return views::Button::AsButton(v)->tag() == static_cast<int>(action);
        });
    return (i == buttons.end()) ? nullptr : views::Button::AsButton(*i);
  }

  bool IsActionButtonVisible(MediaSessionAction action) const {
    return GetButtonForAction(action)->GetVisible();
  }

  MediaSessionNotificationItem* GetItem() const { return item_.get(); }

  const gfx::ImageSkia& GetArtworkImage() const {
    return static_cast<MediaNotificationBackgroundImpl*>(
               view()->GetMediaNotificationBackground())
        ->artwork_;
  }

  const gfx::ImageSkia& GetAppIcon() const {
    return header_row()->app_icon_for_testing();
  }

  bool expand_button_enabled() const {
    return header_row()->expand_button()->GetVisible();
  }

  bool IsActuallyExpanded() const { return view()->IsActuallyExpanded(); }

  void SimulateButtonClick(MediaSessionAction action) {
    views::Button* button = GetButtonForAction(action);
    EXPECT_TRUE(button->GetVisible());

    view()->ButtonPressed(
        button, ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               ui::EventTimeForNow(), 0, 0));
  }

  void SimulateHeaderClick() {
    view()->ButtonPressed(
        header_row(),
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0));
  }

  void SimulateTab() {
    ui::KeyEvent pressed_tab(ui::ET_KEY_PRESSED, ui::VKEY_TAB, ui::EF_NONE);
    view()->GetFocusManager()->OnKeyEvent(pressed_tab);
  }

  void ExpectHistogramActionRecorded(MediaSessionAction action) {
    histogram_tester_.ExpectUniqueSample(
        MediaSessionNotificationItem::kUserActionHistogramName,
        static_cast<base::HistogramBase::Sample>(action), 1);
  }

  void ExpectHistogramArtworkRecorded(bool present, int count) {
    histogram_tester_.ExpectBucketCount(
        MediaNotificationViewImpl::kArtworkHistogramName,
        static_cast<base::HistogramBase::Sample>(present), count);
  }

  void ExpectHistogramMetadataRecorded(
      MediaNotificationViewImpl::Metadata metadata,
      int count) {
    histogram_tester_.ExpectBucketCount(
        MediaNotificationViewImpl::kMetadataHistogramName,
        static_cast<base::HistogramBase::Sample>(metadata), count);
  }

  void AdvanceClockMilliseconds(int milliseconds) {
    task_environment()->FastForwardBy(
        base::TimeDelta::FromMilliseconds(milliseconds));
  }

 private:
  void NotifyUpdatedActions() {
    item_->MediaSessionActionsChanged(
        std::vector<MediaSessionAction>(actions_.begin(), actions_.end()));
  }

  void CreateView() {
    // Create a MediaNotificationViewImpl.
    auto view = std::make_unique<MediaNotificationViewImpl>(
        &container_, item_->GetWeakPtr(),
        nullptr /* header_row_controls_view */,
        base::ASCIIToUTF16(kTestDefaultAppName), kViewWidth,
        /*should_show_icon=*/true);
    view->SetSize(kViewSize);

    // Display it in |widget_|. Widget now owns |view|.
    // And associate it with |container_|.
    container_.SetView(widget_->SetContentsView(std::move(view)));
  }

  base::UnguessableToken request_id_;

  base::HistogramTester histogram_tester_;

  base::flat_set<MediaSessionAction> actions_;

  std::unique_ptr<TestMediaController> media_controller_;
  MockMediaNotificationContainer container_;
  MockMediaNotificationController controller_;
  std::unique_ptr<MediaSessionNotificationItem> item_;
  std::unique_ptr<views::Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationViewImplTest);
};

// TODO(crbug.com/1009287): many of these tests are failing on TSan builds.
#if defined(THREAD_SANITIZER)
#define MAYBE_MediaNotificationViewImplTest \
  DISABLED_MediaNotificationViewImplTest
class DISABLED_MediaNotificationViewImplTest
    : public MediaNotificationViewImplTest {};
#else
#define MAYBE_MediaNotificationViewImplTest MediaNotificationViewImplTest
#endif

TEST_F(MAYBE_MediaNotificationViewImplTest, ButtonsSanityCheck) {
  view()->SetExpanded(true);

  EnableAllActions();

  EXPECT_TRUE(button_row()->GetVisible());
  EXPECT_GT(button_row()->width(), 0);
  EXPECT_GT(button_row()->height(), 0);

  auto buttons = view()->get_buttons_for_testing();
  EXPECT_EQ(6u, buttons.size());

  for (auto* button : buttons) {
    EXPECT_TRUE(button->GetVisible());
    EXPECT_LT(kMediaButtonIconSize, button->width());
    EXPECT_LT(kMediaButtonIconSize, button->height());
    EXPECT_FALSE(views::Button::AsButton(button)->GetAccessibleName().empty());
  }

  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kSeekBackward));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kSeekForward));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kEnterPictureInPicture));

  // |kPause| cannot be present if |kPlay| is.
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kExitPictureInPicture));
}

#if defined(OS_WIN)
#define MAYBE_ButtonsFocusCheck DISABLED_ButtonsFocusCheck
#else
#define MAYBE_ButtonsFocusCheck ButtonsFocusCheck
#endif
TEST_F(MAYBE_MediaNotificationViewImplTest, MAYBE_ButtonsFocusCheck) {
  // Expand and enable all actions to show all buttons.
  view()->SetExpanded(true);
  EnableAllActions();

  views::FocusManager* focus_manager = view()->GetFocusManager();

  {
    // Focus the first action button.
    auto* button = GetButtonForAction(MediaSessionAction::kPreviousTrack);
    focus_manager->SetFocusedView(button);
    EXPECT_EQ(button, focus_manager->GetFocusedView());
  }

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kSeekBackward),
            focus_manager->GetFocusedView());

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kPlay),
            focus_manager->GetFocusedView());

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kSeekForward),
            focus_manager->GetFocusedView());

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kNextTrack),
            focus_manager->GetFocusedView());
}

TEST_F(MAYBE_MediaNotificationViewImplTest, PlayPauseButtonTooltipCheck) {
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  EXPECT_CALL(container(), OnMediaSessionInfoChanged(_));

  auto* button = GetButtonForAction(MediaSessionAction::kPlay);
  base::string16 tooltip = button->GetTooltipText(gfx::Point());
  EXPECT_FALSE(tooltip.empty());

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  GetItem()->MediaSessionInfoChanged(session_info.Clone());

  base::string16 new_tooltip = button->GetTooltipText(gfx::Point());
  EXPECT_FALSE(new_tooltip.empty());
  EXPECT_NE(tooltip, new_tooltip);
}

TEST_F(MAYBE_MediaNotificationViewImplTest, NextTrackButtonClick) {
  EXPECT_CALL(controller(), LogMediaSessionActionButtonPressed(
                                _, MediaSessionAction::kNextTrack));
  EnableAction(MediaSessionAction::kNextTrack);

  EXPECT_EQ(0, media_controller()->next_track_count());

  SimulateButtonClick(MediaSessionAction::kNextTrack);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->next_track_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kNextTrack);
}

TEST_F(MAYBE_MediaNotificationViewImplTest, PlayButtonClick) {
  EXPECT_CALL(controller(),
              LogMediaSessionActionButtonPressed(_, MediaSessionAction::kPlay));
  EnableAction(MediaSessionAction::kPlay);

  EXPECT_EQ(0, media_controller()->resume_count());

  SimulateButtonClick(MediaSessionAction::kPlay);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->resume_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kPlay);
}

TEST_F(MAYBE_MediaNotificationViewImplTest, PauseButtonClick) {
  EXPECT_CALL(controller(), LogMediaSessionActionButtonPressed(
                                _, MediaSessionAction::kPause));
  EnableAction(MediaSessionAction::kPause);
  EXPECT_CALL(container(), OnMediaSessionInfoChanged(_));

  EXPECT_EQ(0, media_controller()->suspend_count());

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  GetItem()->MediaSessionInfoChanged(session_info.Clone());

  SimulateButtonClick(MediaSessionAction::kPause);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->suspend_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kPause);
}

TEST_F(MAYBE_MediaNotificationViewImplTest, PreviousTrackButtonClick) {
  EXPECT_CALL(controller(), LogMediaSessionActionButtonPressed(
                                _, MediaSessionAction::kPreviousTrack));
  EnableAction(MediaSessionAction::kPreviousTrack);

  EXPECT_EQ(0, media_controller()->previous_track_count());

  SimulateButtonClick(MediaSessionAction::kPreviousTrack);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->previous_track_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kPreviousTrack);
}

TEST_F(MAYBE_MediaNotificationViewImplTest, SeekBackwardButtonClick) {
  EXPECT_CALL(controller(), LogMediaSessionActionButtonPressed(
                                _, MediaSessionAction::kSeekBackward));
  EnableAction(MediaSessionAction::kSeekBackward);

  EXPECT_EQ(0, media_controller()->seek_backward_count());

  SimulateButtonClick(MediaSessionAction::kSeekBackward);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->seek_backward_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kSeekBackward);
}

TEST_F(MAYBE_MediaNotificationViewImplTest, SeekForwardButtonClick) {
  EXPECT_CALL(controller(), LogMediaSessionActionButtonPressed(
                                _, MediaSessionAction::kSeekForward));
  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_EQ(0, media_controller()->seek_forward_count());

  SimulateButtonClick(MediaSessionAction::kSeekForward);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->seek_forward_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kSeekForward);
}

TEST_F(MAYBE_MediaNotificationViewImplTest, PlayToggle_FromObserver_Empty) {
  EnableAction(MediaSessionAction::kPlay);

  {
    views::ToggleImageButton* button = static_cast<views::ToggleImageButton*>(
        GetButtonForAction(MediaSessionAction::kPlay));
    ASSERT_EQ(views::ToggleImageButton::kViewClassName, button->GetClassName());
    EXPECT_FALSE(button->GetToggled());
  }

  view()->UpdateWithMediaSessionInfo(
      media_session::mojom::MediaSessionInfo::New());

  {
    views::ToggleImageButton* button = static_cast<views::ToggleImageButton*>(
        GetButtonForAction(MediaSessionAction::kPlay));
    ASSERT_EQ(views::ToggleImageButton::kViewClassName, button->GetClassName());
    EXPECT_FALSE(button->GetToggled());
  }
}

TEST_F(MAYBE_MediaNotificationViewImplTest,
       PlayToggle_FromObserver_PlaybackState) {
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  {
    views::ToggleImageButton* button = static_cast<views::ToggleImageButton*>(
        GetButtonForAction(MediaSessionAction::kPlay));
    ASSERT_EQ(views::ToggleImageButton::kViewClassName, button->GetClassName());
    EXPECT_FALSE(button->GetToggled());
  }

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());

  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());

  {
    views::ToggleImageButton* button = static_cast<views::ToggleImageButton*>(
        GetButtonForAction(MediaSessionAction::kPause));
    ASSERT_EQ(views::ToggleImageButton::kViewClassName, button->GetClassName());
    EXPECT_TRUE(button->GetToggled());
  }

  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPaused;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());

  {
    views::ToggleImageButton* button = static_cast<views::ToggleImageButton*>(
        GetButtonForAction(MediaSessionAction::kPlay));
    ASSERT_EQ(views::ToggleImageButton::kViewClassName, button->GetClassName());
    EXPECT_FALSE(button->GetToggled());
  }
}

TEST_F(MAYBE_MediaNotificationViewImplTest, MetadataIsDisplayed) {
  view()->SetExpanded(true);

  EnableAllActions();

  EXPECT_TRUE(title_artist_row()->GetVisible());
  EXPECT_TRUE(title_label()->GetVisible());
  EXPECT_TRUE(artist_label()->GetVisible());

  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist"), artist_label()->GetText());

  EXPECT_EQ(kMediaTitleArtistRowExpectedHeight, title_artist_row()->height());
}

TEST_F(MAYBE_MediaNotificationViewImplTest, UpdateMetadata_FromObserver) {
  EnableAllActions();

  ExpectHistogramMetadataRecorded(MediaNotificationViewImpl::Metadata::kTitle,
                                  1);
  ExpectHistogramMetadataRecorded(MediaNotificationViewImpl::Metadata::kArtist,
                                  1);
  ExpectHistogramMetadataRecorded(MediaNotificationViewImpl::Metadata::kAlbum,
                                  0);
  ExpectHistogramMetadataRecorded(MediaNotificationViewImpl::Metadata::kCount,
                                  1);

  EXPECT_FALSE(header_row()->summary_text_for_testing()->GetVisible());

  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title2");
  metadata.artist = base::ASCIIToUTF16("artist2");
  metadata.album = base::ASCIIToUTF16("album");

  EXPECT_CALL(container(), OnMediaSessionMetadataChanged(_));
  GetItem()->MediaSessionMetadataChanged(metadata);
  view()->SetExpanded(true);

  EXPECT_TRUE(title_artist_row()->GetVisible());
  EXPECT_TRUE(title_label()->GetVisible());
  EXPECT_TRUE(artist_label()->GetVisible());
  EXPECT_TRUE(header_row()->summary_text_for_testing()->GetVisible());

  EXPECT_EQ(metadata.title, title_label()->GetText());
  EXPECT_EQ(metadata.artist, artist_label()->GetText());
  EXPECT_EQ(metadata.album,
            header_row()->summary_text_for_testing()->GetText());

  EXPECT_EQ(kMediaTitleArtistRowExpectedHeight, title_artist_row()->height());

  EXPECT_EQ(base::ASCIIToUTF16("title2 - artist2 - album"), accessible_name());

  ExpectHistogramMetadataRecorded(MediaNotificationViewImpl::Metadata::kTitle,
                                  2);
  ExpectHistogramMetadataRecorded(MediaNotificationViewImpl::Metadata::kArtist,
                                  2);
  ExpectHistogramMetadataRecorded(MediaNotificationViewImpl::Metadata::kAlbum,
                                  1);
  ExpectHistogramMetadataRecorded(MediaNotificationViewImpl::Metadata::kCount,
                                  2);
}

TEST_F(MAYBE_MediaNotificationViewImplTest, UpdateMetadata_AppName) {
  EXPECT_EQ(base::ASCIIToUTF16(kTestDefaultAppName),
            header_row()->app_name_for_testing());

  {
    media_session::MediaMetadata metadata;
    metadata.title = base::ASCIIToUTF16("title");
    metadata.artist = base::ASCIIToUTF16("artist");
    metadata.source_title = base::ASCIIToUTF16(kTestAppName);
    GetItem()->MediaSessionMetadataChanged(metadata);
  }

  EXPECT_EQ(base::ASCIIToUTF16(kTestAppName),
            header_row()->app_name_for_testing());

  {
    media_session::MediaMetadata metadata;
    metadata.title = base::ASCIIToUTF16("title");
    metadata.artist = base::ASCIIToUTF16("artist");
    GetItem()->MediaSessionMetadataChanged(metadata);
  }

  EXPECT_EQ(base::ASCIIToUTF16(kTestDefaultAppName),
            header_row()->app_name_for_testing());
}

TEST_F(MAYBE_MediaNotificationViewImplTest, Buttons_WhenCollapsed) {
  EXPECT_CALL(
      container(),
      OnVisibleActionsChanged(base::flat_set<MediaSessionAction>(
          {MediaSessionAction::kPlay, MediaSessionAction::kPreviousTrack,
           MediaSessionAction::kNextTrack, MediaSessionAction::kSeekBackward,
           MediaSessionAction::kSeekForward,
           MediaSessionAction::kEnterPictureInPicture})));
  EnableAllActions();
  view()->SetExpanded(false);
  testing::Mock::VerifyAndClearExpectations(&container());

  EXPECT_FALSE(IsActuallyExpanded());

  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPlay));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekBackward));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekForward));

  EXPECT_CALL(
      container(),
      OnVisibleActionsChanged(base::flat_set<MediaSessionAction>(
          {MediaSessionAction::kPlay, MediaSessionAction::kSeekBackward,
           MediaSessionAction::kNextTrack, MediaSessionAction::kSeekForward,
           MediaSessionAction::kEnterPictureInPicture})));
  DisableAction(MediaSessionAction::kPreviousTrack);
  testing::Mock::VerifyAndClearExpectations(&container());
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));

  EXPECT_CALL(
      container(),
      OnVisibleActionsChanged(base::flat_set<MediaSessionAction>(
          {MediaSessionAction::kPlay, MediaSessionAction::kPreviousTrack,
           MediaSessionAction::kNextTrack, MediaSessionAction::kSeekBackward,
           MediaSessionAction::kSeekForward,
           MediaSessionAction::kEnterPictureInPicture})));
  EnableAction(MediaSessionAction::kPreviousTrack);
  testing::Mock::VerifyAndClearExpectations(&container());
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));

  EXPECT_CALL(
      container(),
      OnVisibleActionsChanged(base::flat_set<MediaSessionAction>(
          {MediaSessionAction::kPlay, MediaSessionAction::kPreviousTrack,
           MediaSessionAction::kNextTrack, MediaSessionAction::kSeekBackward,
           MediaSessionAction::kEnterPictureInPicture})));
  DisableAction(MediaSessionAction::kSeekForward);
  testing::Mock::VerifyAndClearExpectations(&container());
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekForward));

  EXPECT_CALL(
      container(),
      OnVisibleActionsChanged(base::flat_set<MediaSessionAction>(
          {MediaSessionAction::kPlay, MediaSessionAction::kPreviousTrack,
           MediaSessionAction::kNextTrack, MediaSessionAction::kSeekBackward,
           MediaSessionAction::kSeekForward,
           MediaSessionAction::kEnterPictureInPicture})));
  EnableAction(MediaSessionAction::kSeekForward);
  testing::Mock::VerifyAndClearExpectations(&container());
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekForward));
}

TEST_F(MAYBE_MediaNotificationViewImplTest, Buttons_WhenExpanded) {
  EXPECT_CALL(
      container(),
      OnVisibleActionsChanged(base::flat_set<MediaSessionAction>(
          {MediaSessionAction::kPlay, MediaSessionAction::kPreviousTrack,
           MediaSessionAction::kNextTrack, MediaSessionAction::kSeekBackward,
           MediaSessionAction::kSeekForward,
           MediaSessionAction::kEnterPictureInPicture})));
  EnableAllActions();
  testing::Mock::VerifyAndClearExpectations(&container());

  EXPECT_CALL(
      container(),
      OnVisibleActionsChanged(base::flat_set<MediaSessionAction>(
          {MediaSessionAction::kPlay, MediaSessionAction::kPreviousTrack,
           MediaSessionAction::kNextTrack, MediaSessionAction::kSeekBackward,
           MediaSessionAction::kSeekForward,
           MediaSessionAction::kEnterPictureInPicture})));
  view()->SetExpanded(true);
  testing::Mock::VerifyAndClearExpectations(&container());

  EXPECT_TRUE(IsActuallyExpanded());

  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPlay));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kSeekBackward));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kSeekForward));
}

TEST_F(MAYBE_MediaNotificationViewImplTest, ClickHeader_ToggleExpand) {
  view()->SetExpanded(true);
  EnableAllActions();

  EXPECT_TRUE(IsActuallyExpanded());

  SimulateHeaderClick();

  EXPECT_FALSE(IsActuallyExpanded());

  SimulateHeaderClick();

  EXPECT_TRUE(IsActuallyExpanded());
}

TEST_F(MAYBE_MediaNotificationViewImplTest, ActionButtonsHiddenByDefault) {
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPlay));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekBackward));
}

TEST_F(MAYBE_MediaNotificationViewImplTest, ActionButtonsToggleVisibility) {
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));

  EnableAction(MediaSessionAction::kNextTrack);

  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kNextTrack));

  DisableAction(MediaSessionAction::kNextTrack);

  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
}

TEST_F(MAYBE_MediaNotificationViewImplTest, UpdateArtworkFromItem) {
  int title_artist_width = title_artist_row()->width();
  const SkColor accent = header_row()->accent_color_for_testing().value();
  gfx::Size size = view()->size();
  EXPECT_CALL(container(), OnMediaArtworkChanged(_)).Times(2);
  EXPECT_CALL(container(), OnColorsChanged(_, _)).Times(2);

  SkBitmap image;
  image.allocN32Pixels(10, 10);
  image.eraseColor(SK_ColorGREEN);

  EXPECT_TRUE(GetArtworkImage().isNull());

  GetItem()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, image);

  ExpectHistogramArtworkRecorded(true, 1);

  // Ensure the title artist row has a small width than before now that we
  // have artwork.
  EXPECT_GT(title_artist_width, title_artist_row()->width());

  // Ensure that the title artist row does not extend into the artwork bounds.
  EXPECT_LE(kViewWidth - kViewArtworkWidth, title_artist_row()->width());

  // Ensure that the image is displayed in the background artwork and that the
  // size of the notification was not affected.
  EXPECT_FALSE(GetArtworkImage().isNull());
  EXPECT_EQ(gfx::Size(10, 10), GetArtworkImage().size());
  EXPECT_EQ(size, view()->size());
  auto accent_color = header_row()->accent_color_for_testing();
  ASSERT_TRUE(accent_color.has_value());
  EXPECT_NE(accent, accent_color.value());

  GetItem()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, SkBitmap());

  ExpectHistogramArtworkRecorded(false, 1);

  // Ensure the title artist row goes back to the original width now that we
  // do not have any artwork.
  EXPECT_EQ(title_artist_width, title_artist_row()->width());

  // Ensure that the background artwork was reset and the size was still not
  // affected.
  EXPECT_TRUE(GetArtworkImage().isNull());
  EXPECT_EQ(size, view()->size());
  accent_color = header_row()->accent_color_for_testing();
  ASSERT_TRUE(accent_color.has_value());
  EXPECT_EQ(accent, accent_color.value());
}

TEST_F(MAYBE_MediaNotificationViewImplTest, ExpandableDefaultState) {
  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());
}

TEST_F(MAYBE_MediaNotificationViewImplTest,
       ExpandablePlayPauseActionCountsOnce) {
  view()->SetExpanded(true);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAction(MediaSessionAction::kPreviousTrack);
  EnableAction(MediaSessionAction::kNextTrack);
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  view()->UpdateWithMediaSessionInfo(session_info);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());
}

TEST_F(MAYBE_MediaNotificationViewImplTest,
       BecomeExpandableAndWasNotExpandable) {
  view()->SetExpanded(true);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAllActions();

  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());
}

TEST_F(MAYBE_MediaNotificationViewImplTest,
       BecomeExpandableButWasAlreadyExpandable) {
  view()->SetExpanded(true);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAllActions();

  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());

  DisableAction(MediaSessionAction::kSeekForward);

  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());
}

TEST_F(MAYBE_MediaNotificationViewImplTest,
       BecomeNotExpandableAndWasExpandable) {
  view()->SetExpanded(true);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAllActions();

  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());

  DisableAction(MediaSessionAction::kPreviousTrack);
  DisableAction(MediaSessionAction::kNextTrack);
  DisableAction(MediaSessionAction::kSeekBackward);
  DisableAction(MediaSessionAction::kSeekForward);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());
}

TEST_F(MAYBE_MediaNotificationViewImplTest,
       BecomeNotExpandableButWasAlreadyNotExpandable) {
  view()->SetExpanded(true);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());
}

TEST_F(MAYBE_MediaNotificationViewImplTest, ActionButtonRowSizeAndAlignment) {
  EnableAction(MediaSessionAction::kPlay);

  views::Button* button = GetButtonForAction(MediaSessionAction::kPlay);
  int button_x = button->GetBoundsInScreen().x();

  // When collapsed the button row should be a fixed width.
  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_EQ(124, button_row()->width());

  EnableAllActions();
  view()->SetExpanded(true);

  // When expanded the button row should be wider and the play button should
  // have shifted to the left.
  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_LT(124, button_row()->width());
  EXPECT_GT(button_x, button->GetBoundsInScreen().x());
}

TEST_F(MAYBE_MediaNotificationViewImplTest, NotifysContainerOfExpandedState) {
  // Track the expanded state given to |container_|.
  bool expanded = false;
  EXPECT_CALL(container(), OnExpanded(_))
      .WillRepeatedly(Invoke([&expanded](bool exp) { expanded = exp; }));

  // Expand the view implicitly via |EnableAllActions()|.
  view()->SetExpanded(true);
  EnableAllActions();
  EXPECT_TRUE(expanded);

  // Explicitly contract the view.
  view()->SetExpanded(false);
  EXPECT_FALSE(expanded);

  // Explicitly expand the view.
  view()->SetExpanded(true);
  EXPECT_TRUE(expanded);

  // Implicitly contract the view by removing available actions.
  DisableAction(MediaSessionAction::kPreviousTrack);
  DisableAction(MediaSessionAction::kNextTrack);
  DisableAction(MediaSessionAction::kSeekBackward);
  DisableAction(MediaSessionAction::kSeekForward);
  EXPECT_FALSE(expanded);
}

TEST_F(MAYBE_MediaNotificationViewImplTest, AccessibleNodeData) {
  ui::AXNodeData data;
  view()->GetAccessibleNodeData(&data);

  EXPECT_TRUE(
      data.HasStringAttribute(ax::mojom::StringAttribute::kRoleDescription));
  EXPECT_EQ(base::ASCIIToUTF16("title - artist"), accessible_name());
}

TEST_F(MAYBE_MediaNotificationViewImplTest, Freezing_DoNotUpdateMetadata) {
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title2");
  metadata.artist = base::ASCIIToUTF16("artist2");
  metadata.album = base::ASCIIToUTF16("album");

  EXPECT_CALL(container(), OnMediaSessionMetadataChanged(_)).Times(0);
  GetItem()->Freeze(base::DoNothing());
  GetItem()->MediaSessionMetadataChanged(metadata);

  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist"), artist_label()->GetText());
}

TEST_F(MAYBE_MediaNotificationViewImplTest, Freezing_DoNotUpdateImage) {
  SkBitmap image;
  image.allocN32Pixels(10, 10);
  image.eraseColor(SK_ColorMAGENTA);
  EXPECT_CALL(container(), OnMediaArtworkChanged(_)).Times(0);
  EXPECT_CALL(container(), OnColorsChanged(_, _)).Times(0);

  GetItem()->Freeze(base::DoNothing());
  GetItem()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, image);

  EXPECT_TRUE(GetArtworkImage().isNull());
}

TEST_F(MAYBE_MediaNotificationViewImplTest, Freezing_DoNotUpdatePlaybackState) {
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  EXPECT_CALL(container(), OnMediaSessionInfoChanged(_)).Times(0);

  GetItem()->Freeze(base::DoNothing());

  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  GetItem()->MediaSessionInfoChanged(session_info.Clone());

  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
}

TEST_F(MAYBE_MediaNotificationViewImplTest, Freezing_DoNotUpdateActions) {
  EXPECT_FALSE(
      GetButtonForAction(MediaSessionAction::kSeekForward)->GetVisible());

  GetItem()->Freeze(base::DoNothing());
  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_FALSE(
      GetButtonForAction(MediaSessionAction::kSeekForward)->GetVisible());
}

TEST_F(MAYBE_MediaNotificationViewImplTest, Freezing_DisableInteraction) {
  EnableAllActions();

  EXPECT_EQ(0, media_controller()->next_track_count());

  GetItem()->Freeze(base::DoNothing());

  SimulateButtonClick(MediaSessionAction::kNextTrack);
  GetItem()->FlushForTesting();

  EXPECT_EQ(0, media_controller()->next_track_count());
}

TEST_F(MAYBE_MediaNotificationViewImplTest, UnfreezingDoesntMissUpdates) {
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  // Freeze the item and clear the metadata.
  base::MockOnceClosure unfrozen_callback;
  EXPECT_CALL(unfrozen_callback, Run).Times(0);
  GetItem()->Freeze(unfrozen_callback.Get());
  GetItem()->MediaSessionInfoChanged(nullptr);
  GetItem()->MediaSessionMetadataChanged(base::nullopt);

  // The item should be frozen and the view should contain the old data.
  EXPECT_TRUE(GetItem()->frozen());
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist"), artist_label()->GetText());

  // Bind the item to a new controller that's playing instead of paused.
  auto new_media_controller = std::make_unique<TestMediaController>();
  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  GetItem()->SetController(new_media_controller->CreateMediaControllerRemote(),
                           session_info.Clone());

  // The item will receive a MediaSessionInfoChanged.
  GetItem()->MediaSessionInfoChanged(session_info.Clone());

  // The item should still be frozen, and the view should contain the old data.
  EXPECT_TRUE(GetItem()->frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist"), artist_label()->GetText());

  // Update the metadata.
  EXPECT_CALL(unfrozen_callback, Run);
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title2");
  metadata.artist = base::ASCIIToUTF16("artist2");
  GetItem()->MediaSessionMetadataChanged(metadata);

  // The item should no longer be frozen, and we should see the updated data.
  EXPECT_FALSE(GetItem()->frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_EQ(base::ASCIIToUTF16("title2"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist2"), artist_label()->GetText());
}

TEST_F(MAYBE_MediaNotificationViewImplTest, UnfreezingWaitsForArtwork_Timeout) {
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  // Set an image before freezing.
  SkBitmap initial_image;
  initial_image.allocN32Pixels(10, 10);
  initial_image.eraseColor(SK_ColorMAGENTA);
  GetItem()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, initial_image);
  EXPECT_FALSE(GetArtworkImage().isNull());

  // Freeze the item and clear the metadata.
  base::MockOnceClosure unfrozen_callback;
  EXPECT_CALL(unfrozen_callback, Run).Times(0);
  GetItem()->Freeze(unfrozen_callback.Get());
  GetItem()->MediaSessionInfoChanged(nullptr);
  GetItem()->MediaSessionMetadataChanged(base::nullopt);
  GetItem()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, SkBitmap());

  // The item should be frozen and the view should contain the old data.
  EXPECT_TRUE(GetItem()->frozen());
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist"), artist_label()->GetText());
  EXPECT_FALSE(GetArtworkImage().isNull());

  // Bind the item to a new controller that's playing instead of paused.
  auto new_media_controller = std::make_unique<TestMediaController>();
  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  GetItem()->SetController(new_media_controller->CreateMediaControllerRemote(),
                           session_info.Clone());

  // The item will receive a MediaSessionInfoChanged.
  GetItem()->MediaSessionInfoChanged(session_info.Clone());

  // The item should still be frozen, and the view should contain the old data.
  EXPECT_TRUE(GetItem()->frozen());
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist"), artist_label()->GetText());
  EXPECT_FALSE(GetArtworkImage().isNull());

  // Update the metadata.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title2");
  metadata.artist = base::ASCIIToUTF16("artist2");
  GetItem()->MediaSessionMetadataChanged(metadata);

  // The item should still be frozen, and waiting for a new image.
  EXPECT_TRUE(GetItem()->frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist"), artist_label()->GetText());
  EXPECT_FALSE(GetArtworkImage().isNull());

  // Once the freeze timer fires, the item should unfreeze even if there's no
  // artwork.
  EXPECT_CALL(unfrozen_callback, Run);
  AdvanceClockMilliseconds(2600);

  EXPECT_FALSE(GetItem()->frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_EQ(base::ASCIIToUTF16("title2"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist2"), artist_label()->GetText());
  EXPECT_TRUE(GetArtworkImage().isNull());
}

TEST_F(MAYBE_MediaNotificationViewImplTest, UnfreezingWaitsForActions) {
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);
  EnableAction(MediaSessionAction::kNextTrack);
  EnableAction(MediaSessionAction::kPreviousTrack);

  // Freeze the item and clear the metadata and actions.
  base::MockOnceClosure unfrozen_callback;
  EXPECT_CALL(unfrozen_callback, Run).Times(0);
  GetItem()->Freeze(unfrozen_callback.Get());
  GetItem()->MediaSessionInfoChanged(nullptr);
  GetItem()->MediaSessionMetadataChanged(base::nullopt);
  DisableAction(MediaSessionAction::kPlay);
  DisableAction(MediaSessionAction::kPause);
  DisableAction(MediaSessionAction::kNextTrack);
  DisableAction(MediaSessionAction::kPreviousTrack);

  // The item should be frozen and the view should contain the old data.
  EXPECT_TRUE(GetItem()->frozen());
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPreviousTrack));
  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist"), artist_label()->GetText());

  // Bind the item to a new controller that's playing instead of paused.
  auto new_media_controller = std::make_unique<TestMediaController>();
  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  GetItem()->SetController(new_media_controller->CreateMediaControllerRemote(),
                           session_info.Clone());

  // The item will receive a MediaSessionInfoChanged.
  GetItem()->MediaSessionInfoChanged(session_info.Clone());

  // The item should still be frozen, and the view should contain the old data.
  EXPECT_TRUE(GetItem()->frozen());
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPreviousTrack));
  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist"), artist_label()->GetText());

  // Update the metadata.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title2");
  metadata.artist = base::ASCIIToUTF16("artist2");
  GetItem()->MediaSessionMetadataChanged(metadata);

  // The item should still be frozen, and waiting for new actions.
  EXPECT_TRUE(GetItem()->frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPreviousTrack));
  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist"), artist_label()->GetText());

  // Once we receive actions, the item should unfreeze.
  EXPECT_CALL(unfrozen_callback, Run);
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);
  EnableAction(MediaSessionAction::kSeekForward);
  EnableAction(MediaSessionAction::kSeekBackward);

  EXPECT_FALSE(GetItem()->frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kSeekForward));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kSeekBackward));
  EXPECT_EQ(base::ASCIIToUTF16("title2"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist2"), artist_label()->GetText());
}

TEST_F(MAYBE_MediaNotificationViewImplTest,
       UnfreezingWaitsForArtwork_ReceiveArtwork) {
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  // Set an image before freezing.
  SkBitmap initial_image;
  initial_image.allocN32Pixels(10, 10);
  initial_image.eraseColor(SK_ColorMAGENTA);
  GetItem()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, initial_image);
  EXPECT_FALSE(GetArtworkImage().isNull());

  // Freeze the item and clear the metadata.
  base::MockOnceClosure unfrozen_callback;
  EXPECT_CALL(unfrozen_callback, Run).Times(0);
  GetItem()->Freeze(unfrozen_callback.Get());
  GetItem()->MediaSessionInfoChanged(nullptr);
  GetItem()->MediaSessionMetadataChanged(base::nullopt);
  GetItem()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, SkBitmap());

  // The item should be frozen and the view should contain the old data.
  EXPECT_TRUE(GetItem()->frozen());
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist"), artist_label()->GetText());
  EXPECT_FALSE(GetArtworkImage().isNull());

  // Bind the item to a new controller that's playing instead of paused.
  auto new_media_controller = std::make_unique<TestMediaController>();
  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  GetItem()->SetController(new_media_controller->CreateMediaControllerRemote(),
                           session_info.Clone());

  // The item will receive a MediaSessionInfoChanged.
  GetItem()->MediaSessionInfoChanged(session_info.Clone());

  // The item should still be frozen, and the view should contain the old data.
  EXPECT_TRUE(GetItem()->frozen());
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist"), artist_label()->GetText());
  EXPECT_FALSE(GetArtworkImage().isNull());

  // Update the metadata.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title2");
  metadata.artist = base::ASCIIToUTF16("artist2");
  GetItem()->MediaSessionMetadataChanged(metadata);

  // The item should still be frozen, and waiting for a new image.
  EXPECT_TRUE(GetItem()->frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist"), artist_label()->GetText());
  EXPECT_FALSE(GetArtworkImage().isNull());

  // Once we receive artwork, the item should unfreeze.
  EXPECT_CALL(unfrozen_callback, Run);
  SkBitmap new_image;
  new_image.allocN32Pixels(10, 10);
  new_image.eraseColor(SK_ColorYELLOW);
  GetItem()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, new_image);

  EXPECT_FALSE(GetItem()->frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_EQ(base::ASCIIToUTF16("title2"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("artist2"), artist_label()->GetText());
  EXPECT_FALSE(GetArtworkImage().isNull());
}

TEST_F(MAYBE_MediaNotificationViewImplTest, ForcedExpandedState) {
  // Make the view expandable.
  EnableAllActions();

  // Force it to be expanded.
  bool expanded_state = true;
  view()->SetForcedExpandedState(&expanded_state);
  EXPECT_TRUE(IsActuallyExpanded());

  // Since it's forced, clicking on the header should not toggle the expanded
  // state.
  SimulateHeaderClick();
  EXPECT_TRUE(IsActuallyExpanded());

  // Force it to be not expanded.
  expanded_state = false;
  view()->SetForcedExpandedState(&expanded_state);
  EXPECT_FALSE(IsActuallyExpanded());

  // Since it's forced, clicking on the header should not toggle the expanded
  // state.
  SimulateHeaderClick();
  EXPECT_FALSE(IsActuallyExpanded());

  // Stop forcing expanded state.
  view()->SetForcedExpandedState(nullptr);
  EXPECT_FALSE(IsActuallyExpanded());

  // Clicking on the header should toggle the expanded state.
  SimulateHeaderClick();
  EXPECT_TRUE(IsActuallyExpanded());
}

TEST_F(MAYBE_MediaNotificationViewImplTest, AllowsHidingOfAppIcon) {
  MediaNotificationViewImpl shows_icon(&container(), nullptr, nullptr,
                                       base::string16(), kViewWidth,
                                       /*should_show_icon=*/true);
  MediaNotificationViewImpl hides_icon(&container(), nullptr, nullptr,
                                       base::string16(), kViewWidth,
                                       /*should_show_icon=*/false);

  EXPECT_TRUE(
      GetHeaderRow(&shows_icon)->app_icon_view_for_testing()->GetVisible());
  EXPECT_FALSE(
      GetHeaderRow(&hides_icon)->app_icon_view_for_testing()->GetVisible());
}

TEST_F(MAYBE_MediaNotificationViewImplTest, ClickHeader_NotifyContainer) {
  EXPECT_CALL(container(), OnHeaderClicked());
  SimulateHeaderClick();
}

}  // namespace media_message_center
