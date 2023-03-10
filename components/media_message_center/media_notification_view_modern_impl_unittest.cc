// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_view_modern_impl.h"

#include <memory>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/media_message_center/media_controls_progress_view.h"
#include "components/media_message_center/media_notification_background_impl.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_message_center/mock_media_notification_item.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/base_event_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"

namespace media_message_center {

using media_session::mojom::MediaSessionAction;
using testing::_;
using testing::Expectation;
using testing::Invoke;
using testing::Return;

namespace {

const int kMediaButtonIconSize = 20;
const int kPipButtonIconSize = 18;

const gfx::Size kWidgetSize(500, 500);

constexpr int kViewWidth = 350;
const gfx::Size kViewSize(kViewWidth, 400);

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

class MediaNotificationViewModernImplTest : public views::ViewsTestBase {
 public:
  MediaNotificationViewModernImplTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  MediaNotificationViewModernImplTest(
      const MediaNotificationViewModernImplTest&) = delete;
  MediaNotificationViewModernImplTest& operator=(
      const MediaNotificationViewModernImplTest&) = delete;
  ~MediaNotificationViewModernImplTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    // Create a widget to show on the screen for testing screen coordinates and
    // focus.
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(kWidgetSize);
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

  MediaNotificationViewModernImpl* view() const { return view_; }

  const std::u16string& accessible_name() const {
    return view()->GetAccessibleName();
  }

  test::MockMediaNotificationItem& item() { return item_; }

  views::Label* title_label() const { return view()->title_label_; }

  views::Label* subtitle_label() const { return view()->subtitle_label_; }

  views::View* artwork_container() const { return view()->artwork_container_; }

  views::View* media_controls_container() const {
    return view()->media_controls_container_;
  }

  views::Button* picture_in_picture_button() const {
    return view()->picture_in_picture_button_for_testing();
  }

  std::vector<views::Button*> media_control_buttons() const {
    std::vector<views::Button*> buttons;
    auto children = view()->media_controls_container_->children();
    base::ranges::transform(
        children, std::back_inserter(buttons),
        [](auto* view) { return views::Button::AsButton(view); });
    buttons.push_back(views::Button::AsButton(picture_in_picture_button()));
    return buttons;
  }

  MediaControlsProgressView* progress_view() const { return view()->progress_; }

  views::Button* GetButtonForAction(MediaSessionAction action) const {
    auto buttons = media_control_buttons();
    const auto i = base::ranges::find(buttons, static_cast<int>(action),
                                      &views::Button::tag);
    return (i == buttons.end()) ? nullptr : *i;
  }

  bool IsActionButtonVisible(MediaSessionAction action) const {
    return GetButtonForAction(action)->GetVisible();
  }

  const gfx::ImageSkia& GetArtworkImage() const {
    return static_cast<MediaNotificationBackgroundImpl*>(
               view()->GetMediaNotificationBackground())
        ->artwork_;
  }

  void SimulateButtonClick(MediaSessionAction action) {
    views::Button* button = GetButtonForAction(action);
    EXPECT_TRUE(button->GetVisible());

    views::test::ButtonTestApi(button).NotifyClick(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0));
  }

  void SimulateTab() {
    ui::KeyEvent pressed_tab(ui::ET_KEY_PRESSED, ui::VKEY_TAB, ui::EF_NONE);
    view()->GetFocusManager()->OnKeyEvent(pressed_tab);
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

 private:
  void NotifyUpdatedActions() { view_->UpdateWithMediaActions(actions_); }

  void CreateView() {
    // On creation, the view should notify |item_|.
    auto view = std::make_unique<MediaNotificationViewModernImpl>(
        &container_, item_.GetWeakPtr(), std::make_unique<views::View>(),
        std::make_unique<views::View>(), kViewWidth);
    view->SetSize(kViewSize);

    media_session::MediaMetadata metadata;
    metadata.title = u"title";
    metadata.artist = u"artist";
    metadata.source_title = u"source title";
    view->UpdateWithMediaMetadata(metadata);

    view->UpdateWithMediaActions(actions_);

    // Display it in |widget_|. Widget now owns |view|.
    view_ = widget_->SetContentsView(std::move(view));
  }

  base::HistogramTester histogram_tester_;

  base::flat_set<MediaSessionAction> actions_;

  MockMediaNotificationContainer container_;
  test::MockMediaNotificationItem item_;
  raw_ptr<MediaNotificationViewModernImpl> view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(MediaNotificationViewModernImplTest, ButtonsSanityCheck) {
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

#if BUILDFLAG(IS_WIN)
#define MAYBE_ButtonsFocusCheck DISABLED_ButtonsFocusCheck
#else
#define MAYBE_ButtonsFocusCheck ButtonsFocusCheck
#endif
TEST_F(MediaNotificationViewModernImplTest, MAYBE_ButtonsFocusCheck) {
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

TEST_F(MediaNotificationViewModernImplTest, PlayPauseButtonTooltipCheck) {
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  EXPECT_CALL(container(), OnMediaSessionInfoChanged(_));

  auto* button = GetButtonForAction(MediaSessionAction::kPlay);
  std::u16string tooltip = button->GetTooltipText(gfx::Point());
  EXPECT_FALSE(tooltip.empty());

  auto session_info = media_session::mojom::MediaSessionInfo::New();
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  view()->UpdateWithMediaSessionInfo(std::move(session_info));

  std::u16string new_tooltip = button->GetTooltipText(gfx::Point());
  EXPECT_FALSE(new_tooltip.empty());
  EXPECT_NE(tooltip, new_tooltip);
}

TEST_F(MediaNotificationViewModernImplTest, NextTrackButtonClick) {
  EnableAction(MediaSessionAction::kNextTrack);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kNextTrack));
  SimulateButtonClick(MediaSessionAction::kNextTrack);
}

TEST_F(MediaNotificationViewModernImplTest, PlayButtonClick) {
  EnableAction(MediaSessionAction::kPlay);

  EXPECT_CALL(item(),
              OnMediaSessionActionButtonPressed(MediaSessionAction::kPlay));
  SimulateButtonClick(MediaSessionAction::kPlay);
}

TEST_F(MediaNotificationViewModernImplTest, PauseButtonClick) {
  EnableAction(MediaSessionAction::kPause);

  auto session_info = media_session::mojom::MediaSessionInfo::New();
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;

  EXPECT_CALL(container(), OnMediaSessionInfoChanged(_));
  view()->UpdateWithMediaSessionInfo(session_info.Clone());
  testing::Mock::VerifyAndClearExpectations(&container());

  EXPECT_CALL(item(),
              OnMediaSessionActionButtonPressed(MediaSessionAction::kPause));
  SimulateButtonClick(MediaSessionAction::kPause);
}

TEST_F(MediaNotificationViewModernImplTest, PreviousTrackButtonClick) {
  EnableAction(MediaSessionAction::kPreviousTrack);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kPreviousTrack));
  SimulateButtonClick(MediaSessionAction::kPreviousTrack);
}

TEST_F(MediaNotificationViewModernImplTest, SeekBackwardButtonClick) {
  EnableAction(MediaSessionAction::kSeekBackward);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kSeekBackward));
  SimulateButtonClick(MediaSessionAction::kSeekBackward);
}

TEST_F(MediaNotificationViewModernImplTest, SeekForwardButtonClick) {
  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kSeekForward));
  SimulateButtonClick(MediaSessionAction::kSeekForward);
}

TEST_F(MediaNotificationViewModernImplTest, PlayToggle_FromObserver_Empty) {
  EnableAction(MediaSessionAction::kPlay);

  {
    views::Button* button = GetButtonForAction(MediaSessionAction::kPlay);
    EXPECT_NE(button, nullptr);
    EXPECT_EQ(button->tag(), static_cast<int>(MediaSessionAction::kPlay));
  }

  view()->UpdateWithMediaSessionInfo(
      media_session::mojom::MediaSessionInfo::New());

  {
    views::Button* button = GetButtonForAction(MediaSessionAction::kPlay);
    EXPECT_NE(button, nullptr);
    EXPECT_EQ(button->tag(), static_cast<int>(MediaSessionAction::kPlay));
  }
}

TEST_F(MediaNotificationViewModernImplTest,
       PlayToggle_FromObserver_PlaybackState) {
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  {
    views::Button* button = GetButtonForAction(MediaSessionAction::kPlay);
    EXPECT_NE(button, nullptr);
    EXPECT_EQ(button->tag(), static_cast<int>(MediaSessionAction::kPlay));
  }

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());

  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());

  {
    views::Button* button = GetButtonForAction(MediaSessionAction::kPause);
    EXPECT_NE(button, nullptr);
    EXPECT_EQ(button->tag(), static_cast<int>(MediaSessionAction::kPause));
  }

  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPaused;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());

  {
    views::Button* button = GetButtonForAction(MediaSessionAction::kPlay);
    EXPECT_NE(button, nullptr);
    EXPECT_EQ(button->tag(), static_cast<int>(MediaSessionAction::kPlay));
  }
}

TEST_F(MediaNotificationViewModernImplTest, MetadataIsDisplayed) {
  EnableAllActions();

  EXPECT_TRUE(title_label()->GetVisible());
  EXPECT_TRUE(subtitle_label()->GetVisible());

  EXPECT_EQ(u"title", title_label()->GetText());
  EXPECT_EQ(u"source title", subtitle_label()->GetText());
}

TEST_F(MediaNotificationViewModernImplTest, UpdateMetadata_FromObserver) {
  EnableAllActions();

  ExpectHistogramMetadataRecorded(
      MediaNotificationViewModernImpl::Metadata::kTitle, 1);
  ExpectHistogramMetadataRecorded(
      MediaNotificationViewModernImpl::Metadata::kSource, 1);
  ExpectHistogramMetadataRecorded(
      MediaNotificationViewModernImpl::Metadata::kCount, 1);

  media_session::MediaMetadata metadata;
  metadata.title = u"title2";
  metadata.source_title = u"source title2";
  metadata.artist = u"artist2";
  metadata.album = u"album";

  EXPECT_CALL(container(), OnMediaSessionMetadataChanged(_));
  view()->UpdateWithMediaMetadata(metadata);
  testing::Mock::VerifyAndClearExpectations(&container());

  EXPECT_TRUE(title_label()->GetVisible());
  EXPECT_TRUE(subtitle_label()->GetVisible());

  EXPECT_EQ(metadata.title, title_label()->GetText());
  EXPECT_EQ(metadata.source_title, subtitle_label()->GetText());

  EXPECT_EQ(u"title2 - artist2 - album", accessible_name());

  ExpectHistogramMetadataRecorded(
      MediaNotificationViewModernImpl::Metadata::kTitle, 2);
  ExpectHistogramMetadataRecorded(
      MediaNotificationViewModernImpl::Metadata::kSource, 2);
  ExpectHistogramMetadataRecorded(
      MediaNotificationViewModernImpl::Metadata::kCount, 2);
}

TEST_F(MediaNotificationViewModernImplTest, ActionButtonsHiddenByDefault) {
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPlay));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekBackward));
}

TEST_F(MediaNotificationViewModernImplTest, ActionButtonsToggleVisibility) {
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));

  EnableAction(MediaSessionAction::kNextTrack);

  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kNextTrack));

  DisableAction(MediaSessionAction::kNextTrack);

  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
}

TEST_F(MediaNotificationViewModernImplTest, UpdateArtworkFromItem) {
  int labels_container_width = title_label()->parent()->width();
  gfx::Size size = view()->size();
  EXPECT_CALL(container(), OnMediaArtworkChanged(_)).Times(2);
  EXPECT_CALL(container(), OnColorsChanged(_, _, _)).Times(2);

  SkBitmap image;
  image.allocN32Pixels(10, 10);
  image.eraseColor(SK_ColorGREEN);

  EXPECT_TRUE(GetArtworkImage().isNull());

  view()->UpdateWithMediaArtwork(gfx::ImageSkia::CreateFrom1xBitmap(image));

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

  view()->UpdateWithMediaArtwork(
      gfx::ImageSkia::CreateFrom1xBitmap(SkBitmap()));

  ExpectHistogramArtworkRecorded(false, 1);

  // Ensure the labels container goes back to the original width now that we
  // do not have any artwork.
  EXPECT_EQ(labels_container_width, title_label()->parent()->width());

  // Ensure that the artwork was reset and the size was still not
  // affected.
  EXPECT_TRUE(GetArtworkImage().isNull());
  EXPECT_EQ(size, view()->size());
}

TEST_F(MediaNotificationViewModernImplTest, UpdateProgressBar) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1.0, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(0), /*end_of_media=*/false);
  view()->UpdateWithMediaPosition(media_position);
  EXPECT_EQ(progress_view()->duration_for_testing(), u"10:00");
}

TEST_F(MediaNotificationViewModernImplTest, AccessibleNodeData) {
  ui::AXNodeData data;
  view()->GetAccessibleNodeData(&data);

  EXPECT_TRUE(
      data.HasStringAttribute(ax::mojom::StringAttribute::kRoleDescription));
  EXPECT_EQ(u"title - artist", accessible_name());
}

class MediaNotificationViewModernImplCastTest
    : public MediaNotificationViewModernImplTest {
 public:
  void SetUp() override {
    EXPECT_CALL(item(), SourceType())
        .WillRepeatedly(Return(media_message_center::SourceType::kCast));
    MediaNotificationViewModernImplTest::SetUp();
  }
};

TEST_F(MediaNotificationViewModernImplCastTest, PictureInPictureButton) {
  // We should not create picture-in-picture button for cast session.
  EXPECT_EQ(picture_in_picture_button(), nullptr);
}

}  // namespace media_message_center
