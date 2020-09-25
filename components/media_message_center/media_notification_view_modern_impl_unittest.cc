// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_view_modern_impl.h"

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
#include "components/media_message_center/media_notification_util.h"
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

const int kMediaButtonIconSize = 20;
const int kPipButtonIconSize = 18;

const gfx::Size kWidgetSize(500, 500);

constexpr int kViewWidth = 350;
const gfx::Size kViewSize(kViewWidth, 400);

class MockMediaNotificationController : public MediaNotificationController {
 public:
  MockMediaNotificationController() = default;
  ~MockMediaNotificationController() override = default;

  // MediaNotificationController implementation.
  MOCK_METHOD1(ShowNotification, void(const std::string& id));
  MOCK_METHOD1(HideNotification, void(const std::string& id));
  MOCK_METHOD1(RemoveItem, void(const std::string& id));
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() const override {
    return nullptr;
  }
  MOCK_METHOD2(LogMediaSessionActionButtonPressed,
               void(const std::string& id,
                    media_session::mojom::MediaSessionAction action));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaNotificationController);
};

class MockMediaNotificationContainer : public MediaNotificationContainer {
 public:
  MockMediaNotificationContainer() = default;
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
  MOCK_METHOD2(OnColorsChanged, void(SkColor foreground, SkColor background));
  MOCK_METHOD0(OnHeaderClicked, void());

  MediaNotificationViewModernImpl* view() const { return view_; }
  void SetView(MediaNotificationViewModernImpl* view) { view_ = view; }

 private:
  MediaNotificationViewModernImpl* view_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaNotificationContainer);
};

}  // namespace

class MediaNotificationViewModernImplTest : public views::ViewsTestBase {
 public:
  MediaNotificationViewModernImplTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~MediaNotificationViewModernImplTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    request_id_ = base::UnguessableToken::Create();

    // Create a new MediaNotificationViewModernImpl whenever the
    // MediaSessionNotificationItem says to show the notification.
    EXPECT_CALL(controller_, ShowNotification(request_id_.ToString()))
        .WillRepeatedly(InvokeWithoutArgs(
            this, &MediaNotificationViewModernImplTest::CreateView));

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
    metadata.source_title = base::ASCIIToUTF16("source title");
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

  MediaNotificationViewModernImpl* view() const { return container_.view(); }

  TestMediaController* media_controller() const {
    return media_controller_.get();
  }

  const base::string16& accessible_name() const {
    return view()->accessible_name_;
  }

  views::Label* title_label() const { return view()->title_label_; }

  views::Label* subtitle_label() const { return view()->subtitle_label_; }

  views::View* artwork_container() const { return view()->artwork_container_; }

  views::View* media_controls_container() const {
    return view()->media_controls_container_;
  }

  std::vector<views::Button*> media_control_buttons() const {
    std::vector<views::Button*> buttons;
    auto children = view()->media_controls_container_->children();
    std::transform(
        children.begin(), children.end(), std::back_inserter(buttons),
        [](views::View* child) { return views::Button::AsButton(child); });
    buttons.push_back(
        views::Button::AsButton(view()->picture_in_picture_button_));
    return buttons;
  }

  views::Button* picture_in_picture_button() const {
    return view()->picture_in_picture_button_;
  }

  views::Button* GetButtonForAction(MediaSessionAction action) const {
    auto buttons = media_control_buttons();
    const auto i = std::find_if(
        buttons.begin(), buttons.end(), [action](const views::Button* button) {
          return button->tag() == static_cast<int>(action);
        });
    return (i == buttons.end()) ? nullptr : *i;
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

  void SimulateButtonClick(MediaSessionAction action) {
    views::Button* button = GetButtonForAction(action);
    EXPECT_TRUE(button->GetVisible());

    view()->ButtonPressed(
        button, ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
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
        MediaNotificationViewModernImpl::kArtworkHistogramName,
        static_cast<base::HistogramBase::Sample>(present), count);
  }

  void ExpectHistogramMetadataRecorded(
      MediaNotificationViewModernImpl::Metadata metadata,
      int count) {
    histogram_tester_.ExpectBucketCount(
        MediaNotificationViewModernImpl::kMetadataHistogramName,
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
    // Create a MediaNotificationViewModernImpl.
    auto view = std::make_unique<MediaNotificationViewModernImpl>(
        &container_, item_->GetWeakPtr(), std::make_unique<views::View>(),
        kViewWidth);
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

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationViewModernImplTest);
};

// TODO(crbug.com/1009287): many of these tests are failing on TSan builds.
#if defined(THREAD_SANITIZER)
#define MAYBE_MediaNotificationViewModernImplTest \
  DISABLED_MediaNotificationViewModernImplTest
class DISABLED_MediaNotificationViewModernImplTest
    : public MediaNotificationViewModernImplTest {};
#else
#define MAYBE_MediaNotificationViewModernImplTest \
  MediaNotificationViewModernImplTest
#endif

TEST_F(MAYBE_MediaNotificationViewModernImplTest, ButtonsSanityCheck) {
  EnableAllActions();

  EXPECT_TRUE(media_controls_container()->GetVisible());
  EXPECT_GT(media_controls_container()->width(), 0);
  EXPECT_GT(media_controls_container()->height(), 0);

  auto buttons = media_control_buttons();
  EXPECT_EQ(6u, buttons.size());

  for (auto* button : buttons) {
    EXPECT_TRUE(button->GetVisible());
    if (button == picture_in_picture_button()) {
      EXPECT_LT(kPipButtonIconSize, button->width());
      EXPECT_LT(kPipButtonIconSize, button->height());
    } else {
      EXPECT_LT(kMediaButtonIconSize, button->width());
      EXPECT_LT(kMediaButtonIconSize, button->height());
    }
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
TEST_F(MAYBE_MediaNotificationViewModernImplTest, MAYBE_ButtonsFocusCheck) {
  // Expand and enable all actions to show all buttons.
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

TEST_F(MAYBE_MediaNotificationViewModernImplTest, PlayPauseButtonTooltipCheck) {
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

TEST_F(MAYBE_MediaNotificationViewModernImplTest, NextTrackButtonClick) {
  EXPECT_CALL(controller(), LogMediaSessionActionButtonPressed(
                                _, MediaSessionAction::kNextTrack));
  EnableAction(MediaSessionAction::kNextTrack);

  EXPECT_EQ(0, media_controller()->next_track_count());

  SimulateButtonClick(MediaSessionAction::kNextTrack);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->next_track_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kNextTrack);
}

TEST_F(MAYBE_MediaNotificationViewModernImplTest, PlayButtonClick) {
  EXPECT_CALL(controller(),
              LogMediaSessionActionButtonPressed(_, MediaSessionAction::kPlay));
  EnableAction(MediaSessionAction::kPlay);

  EXPECT_EQ(0, media_controller()->resume_count());

  SimulateButtonClick(MediaSessionAction::kPlay);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->resume_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kPlay);
}

TEST_F(MAYBE_MediaNotificationViewModernImplTest, PauseButtonClick) {
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

TEST_F(MAYBE_MediaNotificationViewModernImplTest, PreviousTrackButtonClick) {
  EXPECT_CALL(controller(), LogMediaSessionActionButtonPressed(
                                _, MediaSessionAction::kPreviousTrack));
  EnableAction(MediaSessionAction::kPreviousTrack);

  EXPECT_EQ(0, media_controller()->previous_track_count());

  SimulateButtonClick(MediaSessionAction::kPreviousTrack);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->previous_track_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kPreviousTrack);
}

TEST_F(MAYBE_MediaNotificationViewModernImplTest, SeekBackwardButtonClick) {
  EXPECT_CALL(controller(), LogMediaSessionActionButtonPressed(
                                _, MediaSessionAction::kSeekBackward));
  EnableAction(MediaSessionAction::kSeekBackward);

  EXPECT_EQ(0, media_controller()->seek_backward_count());

  SimulateButtonClick(MediaSessionAction::kSeekBackward);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->seek_backward_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kSeekBackward);
}

TEST_F(MAYBE_MediaNotificationViewModernImplTest, SeekForwardButtonClick) {
  EXPECT_CALL(controller(), LogMediaSessionActionButtonPressed(
                                _, MediaSessionAction::kSeekForward));
  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_EQ(0, media_controller()->seek_forward_count());

  SimulateButtonClick(MediaSessionAction::kSeekForward);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->seek_forward_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kSeekForward);
}

TEST_F(MAYBE_MediaNotificationViewModernImplTest,
       PlayToggle_FromObserver_Empty) {
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

TEST_F(MAYBE_MediaNotificationViewModernImplTest,
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

TEST_F(MAYBE_MediaNotificationViewModernImplTest, MetadataIsDisplayed) {
  EnableAllActions();

  EXPECT_TRUE(title_label()->GetVisible());
  EXPECT_TRUE(subtitle_label()->GetVisible());

  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("source title"), subtitle_label()->GetText());
}

TEST_F(MAYBE_MediaNotificationViewModernImplTest, UpdateMetadata_FromObserver) {
  EnableAllActions();

  ExpectHistogramMetadataRecorded(
      MediaNotificationViewModernImpl::Metadata::kTitle, 1);
  ExpectHistogramMetadataRecorded(
      MediaNotificationViewModernImpl::Metadata::kSource, 1);
  ExpectHistogramMetadataRecorded(
      MediaNotificationViewModernImpl::Metadata::kCount, 1);

  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title2");
  metadata.source_title = base::ASCIIToUTF16("source title2");
  metadata.artist = base::ASCIIToUTF16("artist2");
  metadata.album = base::ASCIIToUTF16("album");

  EXPECT_CALL(container(), OnMediaSessionMetadataChanged(_));
  GetItem()->MediaSessionMetadataChanged(metadata);

  EXPECT_TRUE(title_label()->GetVisible());
  EXPECT_TRUE(subtitle_label()->GetVisible());

  EXPECT_EQ(metadata.title, title_label()->GetText());
  EXPECT_EQ(metadata.source_title, subtitle_label()->GetText());

  EXPECT_EQ(base::ASCIIToUTF16("title2 - artist2 - album"), accessible_name());

  ExpectHistogramMetadataRecorded(
      MediaNotificationViewModernImpl::Metadata::kTitle, 2);
  ExpectHistogramMetadataRecorded(
      MediaNotificationViewModernImpl::Metadata::kSource, 2);
  ExpectHistogramMetadataRecorded(
      MediaNotificationViewModernImpl::Metadata::kCount, 2);
}

TEST_F(MAYBE_MediaNotificationViewModernImplTest,
       ActionButtonsHiddenByDefault) {
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPlay));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekBackward));
}

TEST_F(MAYBE_MediaNotificationViewModernImplTest,
       ActionButtonsToggleVisibility) {
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));

  EnableAction(MediaSessionAction::kNextTrack);

  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kNextTrack));

  DisableAction(MediaSessionAction::kNextTrack);

  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
}

TEST_F(MAYBE_MediaNotificationViewModernImplTest, UpdateArtworkFromItem) {
  int labels_container_width = title_label()->parent()->width();
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

  // The size of the labels container should not change when there is artwork.
  EXPECT_EQ(labels_container_width, title_label()->parent()->width());

  // Ensure that the labels container does not extend into the artwork bounds.
  EXPECT_FALSE(artwork_container()->bounds().Intersects(
      title_label()->parent()->bounds()));

  // Ensure that when the image is displayed that the size of the notification
  // was not affected.
  EXPECT_FALSE(GetArtworkImage().isNull());
  EXPECT_EQ(gfx::Size(10, 10), GetArtworkImage().size());
  EXPECT_EQ(size, view()->size());

  GetItem()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, SkBitmap());

  ExpectHistogramArtworkRecorded(false, 1);

  // Ensure the labels container goes back to the original width now that we
  // do not have any artwork.
  EXPECT_EQ(labels_container_width, title_label()->parent()->width());

  // Ensure that the artwork was reset and the size was still not
  // affected.
  EXPECT_TRUE(GetArtworkImage().isNull());
  EXPECT_EQ(size, view()->size());
}

TEST_F(MAYBE_MediaNotificationViewModernImplTest, AccessibleNodeData) {
  ui::AXNodeData data;
  view()->GetAccessibleNodeData(&data);

  EXPECT_TRUE(
      data.HasStringAttribute(ax::mojom::StringAttribute::kRoleDescription));
  EXPECT_EQ(base::ASCIIToUTF16("title - artist"), accessible_name());
}

TEST_F(MAYBE_MediaNotificationViewModernImplTest,
       Freezing_DoNotUpdateMetadata) {
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title2");
  metadata.artist = base::ASCIIToUTF16("artist2");
  metadata.album = base::ASCIIToUTF16("album");

  EXPECT_CALL(container(), OnMediaSessionMetadataChanged(_)).Times(0);
  GetItem()->Freeze(base::DoNothing());
  GetItem()->MediaSessionMetadataChanged(metadata);

  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("source title"), subtitle_label()->GetText());
}

TEST_F(MAYBE_MediaNotificationViewModernImplTest, Freezing_DoNotUpdateImage) {
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

TEST_F(MAYBE_MediaNotificationViewModernImplTest,
       Freezing_DoNotUpdatePlaybackState) {
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

TEST_F(MAYBE_MediaNotificationViewModernImplTest, Freezing_DoNotUpdateActions) {
  EXPECT_FALSE(
      GetButtonForAction(MediaSessionAction::kSeekForward)->GetVisible());

  GetItem()->Freeze(base::DoNothing());
  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_FALSE(
      GetButtonForAction(MediaSessionAction::kSeekForward)->GetVisible());
}

TEST_F(MAYBE_MediaNotificationViewModernImplTest, Freezing_DisableInteraction) {
  EnableAllActions();

  EXPECT_EQ(0, media_controller()->next_track_count());

  GetItem()->Freeze(base::DoNothing());

  SimulateButtonClick(MediaSessionAction::kNextTrack);
  GetItem()->FlushForTesting();

  EXPECT_EQ(0, media_controller()->next_track_count());
}

TEST_F(MAYBE_MediaNotificationViewModernImplTest, UnfreezingDoesntMissUpdates) {
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
  EXPECT_EQ(base::ASCIIToUTF16("source title"), subtitle_label()->GetText());

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
  EXPECT_EQ(base::ASCIIToUTF16("source title"), subtitle_label()->GetText());

  // Update the metadata.
  EXPECT_CALL(unfrozen_callback, Run);
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title2");
  metadata.source_title = base::ASCIIToUTF16("source title 2");
  GetItem()->MediaSessionMetadataChanged(metadata);

  // The item should no longer be frozen, and we should see the updated data.
  EXPECT_FALSE(GetItem()->frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_EQ(base::ASCIIToUTF16("title2"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("source title 2"), subtitle_label()->GetText());
}

TEST_F(MAYBE_MediaNotificationViewModernImplTest,
       UnfreezingWaitsForArtwork_Timeout) {
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
  EXPECT_EQ(base::ASCIIToUTF16("source title"), subtitle_label()->GetText());
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
  EXPECT_EQ(base::ASCIIToUTF16("source title"), subtitle_label()->GetText());
  EXPECT_FALSE(GetArtworkImage().isNull());

  // Update the metadata.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title2");
  metadata.source_title = base::ASCIIToUTF16("source title 2");
  GetItem()->MediaSessionMetadataChanged(metadata);

  // The item should still be frozen, and waiting for a new image.
  EXPECT_TRUE(GetItem()->frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("source title"), subtitle_label()->GetText());
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
  EXPECT_EQ(base::ASCIIToUTF16("source title 2"), subtitle_label()->GetText());
  EXPECT_TRUE(GetArtworkImage().isNull());
}

TEST_F(MAYBE_MediaNotificationViewModernImplTest, UnfreezingWaitsForActions) {
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
  EXPECT_EQ(base::ASCIIToUTF16("source title"), subtitle_label()->GetText());

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
  EXPECT_EQ(base::ASCIIToUTF16("source title"), subtitle_label()->GetText());

  // Update the metadata.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title2");
  metadata.source_title = base::ASCIIToUTF16("source title 2");
  GetItem()->MediaSessionMetadataChanged(metadata);

  // The item should still be frozen, and waiting for new actions.
  EXPECT_TRUE(GetItem()->frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPreviousTrack));
  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("source title"), subtitle_label()->GetText());

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
  EXPECT_EQ(base::ASCIIToUTF16("source title 2"), subtitle_label()->GetText());
}

TEST_F(MAYBE_MediaNotificationViewModernImplTest,
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
  EXPECT_EQ(base::ASCIIToUTF16("source title"), subtitle_label()->GetText());
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
  EXPECT_EQ(base::ASCIIToUTF16("source title"), subtitle_label()->GetText());
  EXPECT_FALSE(GetArtworkImage().isNull());

  // Update the metadata.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title2");
  metadata.source_title = base::ASCIIToUTF16("source title 2");
  GetItem()->MediaSessionMetadataChanged(metadata);

  // The item should still be frozen, and waiting for a new image.
  EXPECT_TRUE(GetItem()->frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("source title"), subtitle_label()->GetText());
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
  EXPECT_EQ(base::ASCIIToUTF16("source title 2"), subtitle_label()->GetText());
  EXPECT_FALSE(GetArtworkImage().isNull());
}

}  // namespace media_message_center
