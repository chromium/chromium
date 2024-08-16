// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_view_impl.h"

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
#include "components/media_message_center/media_notification_background_impl.h"
#include "components/media_message_center/media_notification_container.h"
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
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_utils.h"

namespace media_message_center {

using media_session::mojom::MediaSessionAction;
using testing::_;
using testing::Expectation;
using testing::Invoke;

namespace {

// The icons size is 24 and INSETS_VECTOR_IMAGE_BUTTON will add padding around
// the image.
const int kMediaButtonIconSize = 24;

// The title artist row should always have the same height.
const int kMediaTitleArtistRowExpectedHeight = 48;

const char16_t kTestDefaultAppName[] = u"default app name";
const char16_t kTestAppName[] = u"app name";

const gfx::Size kWidgetSize(500, 500);

constexpr int kViewWidth = 400;
constexpr int kViewArtworkWidth = kViewWidth * 0.4;
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
  MOCK_METHOD1(OnHeaderClicked, void(bool activate_original_media));
};

}  // namespace

class MediaNotificationViewImplTest : public views::ViewsTestBase {
 public:
  MediaNotificationViewImplTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  MediaNotificationViewImplTest(const MediaNotificationViewImplTest&) = delete;
  MediaNotificationViewImplTest& operator=(
      const MediaNotificationViewImplTest&) = delete;
  ~MediaNotificationViewImplTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    // Create a widget to show on the screen for testing screen coordinates and
    // focus.
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
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

  views::Widget* widget() { return widget_.get(); }

  MediaNotificationViewImpl* view() const { return view_; }

  message_center::NotificationHeaderView* GetHeaderRow(
      MediaNotificationViewImpl* view) const {
    return view->header_row_;
  }

  message_center::NotificationHeaderView* header_row() const {
    return GetHeaderRow(view());
  }

  std::u16string accessible_name() const {
    return view()->GetViewAccessibility().GetCachedName();
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
    const auto i = base::ranges::find(
        buttons, static_cast<int>(action),
        [](const views::View* v) { return views::Button::AsButton(v)->tag(); });
    return (i == buttons.end()) ? nullptr : views::Button::AsButton(*i);
  }

  bool IsActionButtonVisible(MediaSessionAction action) const {
    return GetButtonForAction(action)->GetVisible();
  }

  test::MockMediaNotificationItem& item() { return item_; }

  const gfx::ImageSkia& GetArtworkImage() const {
    return static_cast<MediaNotificationBackgroundImpl*>(
               view()->GetMediaNotificationBackground())
        ->artwork_;
  }

  gfx::ImageSkia GetAppIcon() const {
    return header_row()->app_icon_for_testing();
  }

  bool expand_button_enabled() const {
    return header_row()->expand_button()->GetVisible();
  }

  bool GetActuallyExpanded() const { return view()->GetActuallyExpanded(); }

  void SimulateButtonClick(MediaSessionAction action) {
    views::Button* button = GetButtonForAction(action);
    EXPECT_TRUE(button->GetVisible());

    views::test::ButtonTestApi(button).NotifyClick(
        ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0));
  }

  void SimulateHeaderClick() {
    views::test::ButtonTestApi(header_row())
        .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), ui::EventTimeForNow(), 0, 0));
  }

  void SimulateTab() {
    ui::KeyEvent pressed_tab(ui::EventType::kKeyPressed, ui::VKEY_TAB,
                             ui::EF_NONE);
    view()->GetFocusManager()->OnKeyEvent(pressed_tab);
  }

 private:
  void NotifyUpdatedActions() { view_->UpdateWithMediaActions(actions_); }

  void CreateView() {
    // On creation, the view should notify |item_|.
    EXPECT_CALL(item_, SetView(_));
    auto view = std::make_unique<MediaNotificationViewImpl>(
        &container_, item_.GetWeakPtr(), nullptr /* header_row_controls_view */,
        kTestDefaultAppName, kViewWidth,
        /*should_show_icon=*/true);
    testing::Mock::VerifyAndClearExpectations(&item_);

    view->SetSize(kViewSize);

    media_session::MediaMetadata metadata;
    metadata.title = u"title";
    metadata.artist = u"artist";
    view->UpdateWithMediaMetadata(metadata);

    // Display it in |widget_|. Widget now owns |view|.
    view_ = widget_->SetContentsView(std::move(view));
  }

  base::HistogramTester histogram_tester_;

  base::flat_set<MediaSessionAction> actions_;

  MockMediaNotificationContainer container_;
  test::MockMediaNotificationItem item_;
  raw_ptr<MediaNotificationViewImpl> view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(MediaNotificationViewImplTest, ButtonsSanityCheck) {
  view()->SetExpanded(true);
  EnableAllActions();
  widget()->LayoutRootViewIfNecessary();

  EXPECT_TRUE(button_row()->GetVisible());
  EXPECT_GT(button_row()->width(), 0);
  EXPECT_GT(button_row()->height(), 0);

  auto buttons = view()->get_buttons_for_testing();
  EXPECT_EQ(6u, buttons.size());

  for (views::View* button : buttons) {
    EXPECT_TRUE(button->GetVisible());
    EXPECT_LT(kMediaButtonIconSize, button->width());
    EXPECT_LT(kMediaButtonIconSize, button->height());
    EXPECT_FALSE(views::Button::AsButton(button)
                     ->GetViewAccessibility()
                     .GetCachedName()
                     .empty());
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
TEST_F(MediaNotificationViewImplTest, MAYBE_ButtonsFocusCheck) {
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

TEST_F(MediaNotificationViewImplTest, PlayPauseButtonTooltipCheck) {
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  EXPECT_CALL(container(), OnMediaSessionInfoChanged(_));

  auto* button = GetButtonForAction(MediaSessionAction::kPlay);
  std::u16string tooltip = button->GetTooltipText(gfx::Point());
  EXPECT_FALSE(tooltip.empty());

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  view()->UpdateWithMediaSessionInfo(std::move(session_info));

  std::u16string new_tooltip = button->GetTooltipText(gfx::Point());
  EXPECT_FALSE(new_tooltip.empty());
  EXPECT_NE(tooltip, new_tooltip);
}

TEST_F(MediaNotificationViewImplTest, NextTrackButtonClick) {
  EnableAction(MediaSessionAction::kNextTrack);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kNextTrack));
  SimulateButtonClick(MediaSessionAction::kNextTrack);
}

TEST_F(MediaNotificationViewImplTest, PlayButtonClick) {
  EnableAction(MediaSessionAction::kPlay);

  EXPECT_CALL(item(),
              OnMediaSessionActionButtonPressed(MediaSessionAction::kPlay));
  SimulateButtonClick(MediaSessionAction::kPlay);
}

TEST_F(MediaNotificationViewImplTest, PauseButtonClick) {
  EnableAction(MediaSessionAction::kPause);

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;

  EXPECT_CALL(container(), OnMediaSessionInfoChanged(_));
  view()->UpdateWithMediaSessionInfo(std::move(session_info));
  testing::Mock::VerifyAndClearExpectations(&container());

  EXPECT_CALL(item(),
              OnMediaSessionActionButtonPressed(MediaSessionAction::kPause));
  SimulateButtonClick(MediaSessionAction::kPause);
}

TEST_F(MediaNotificationViewImplTest, PreviousTrackButtonClick) {
  EnableAction(MediaSessionAction::kPreviousTrack);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kPreviousTrack));
  SimulateButtonClick(MediaSessionAction::kPreviousTrack);
}

TEST_F(MediaNotificationViewImplTest, SeekBackwardButtonClick) {
  EnableAction(MediaSessionAction::kSeekBackward);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kSeekBackward));
  SimulateButtonClick(MediaSessionAction::kSeekBackward);
}

TEST_F(MediaNotificationViewImplTest, SeekForwardButtonClick) {
  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_CALL(item(), OnMediaSessionActionButtonPressed(
                          MediaSessionAction::kSeekForward));
  SimulateButtonClick(MediaSessionAction::kSeekForward);
}

TEST_F(MediaNotificationViewImplTest, PlayToggle_FromObserver_Empty) {
  EnableAction(MediaSessionAction::kPlay);

  {
    views::ToggleImageButton* button =
        views::AsViewClass<views::ToggleImageButton>(
            GetButtonForAction(MediaSessionAction::kPlay));
    ASSERT_TRUE(button);
    EXPECT_FALSE(button->GetToggled());
  }

  view()->UpdateWithMediaSessionInfo(
      media_session::mojom::MediaSessionInfo::New());

  {
    views::ToggleImageButton* button =
        views::AsViewClass<views::ToggleImageButton>(
            GetButtonForAction(MediaSessionAction::kPlay));
    ASSERT_TRUE(button);
    EXPECT_FALSE(button->GetToggled());
  }
}

TEST_F(MediaNotificationViewImplTest, PlayToggle_FromObserver_PlaybackState) {
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  {
    views::ToggleImageButton* button =
        views::AsViewClass<views::ToggleImageButton>(
            GetButtonForAction(MediaSessionAction::kPlay));
    ASSERT_TRUE(button);
    EXPECT_FALSE(button->GetToggled());
  }

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());

  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());

  {
    views::ToggleImageButton* button =
        views::AsViewClass<views::ToggleImageButton>(
            GetButtonForAction(MediaSessionAction::kPause));
    ASSERT_TRUE(button);
    EXPECT_TRUE(button->GetToggled());
  }

  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPaused;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());

  {
    views::ToggleImageButton* button =
        views::AsViewClass<views::ToggleImageButton>(
            GetButtonForAction(MediaSessionAction::kPlay));
    ASSERT_TRUE(button);
    EXPECT_FALSE(button->GetToggled());
  }
}

TEST_F(MediaNotificationViewImplTest, MetadataIsDisplayed) {
  view()->SetExpanded(true);
  EnableAllActions();
  widget()->LayoutRootViewIfNecessary();

  EXPECT_TRUE(title_artist_row()->GetVisible());
  EXPECT_TRUE(title_label()->GetVisible());
  EXPECT_TRUE(artist_label()->GetVisible());

  EXPECT_EQ(u"title", title_label()->GetText());
  EXPECT_EQ(u"artist", artist_label()->GetText());

  EXPECT_EQ(kMediaTitleArtistRowExpectedHeight, title_artist_row()->height());
}

TEST_F(MediaNotificationViewImplTest, UpdateMetadata_FromObserver) {
  EnableAllActions();
  widget()->LayoutRootViewIfNecessary();

  EXPECT_FALSE(header_row()->summary_text_for_testing()->GetVisible());

  media_session::MediaMetadata metadata;
  metadata.title = u"title2";
  metadata.artist = u"artist2";
  metadata.album = u"album";

  EXPECT_CALL(container(), OnMediaSessionMetadataChanged(_));
  view()->UpdateWithMediaMetadata(metadata);
  testing::Mock::VerifyAndClearExpectations(&container());

  view()->SetExpanded(true);
  widget()->LayoutRootViewIfNecessary();

  EXPECT_TRUE(title_artist_row()->GetVisible());
  EXPECT_TRUE(title_label()->GetVisible());
  EXPECT_TRUE(artist_label()->GetVisible());
  EXPECT_TRUE(header_row()->summary_text_for_testing()->GetVisible());

  EXPECT_EQ(metadata.title, title_label()->GetText());
  EXPECT_EQ(metadata.artist, artist_label()->GetText());
  EXPECT_EQ(metadata.album,
            header_row()->summary_text_for_testing()->GetText());

  EXPECT_EQ(kMediaTitleArtistRowExpectedHeight, title_artist_row()->height());

  EXPECT_EQ(u"title2 - artist2 - album", accessible_name());
}

TEST_F(MediaNotificationViewImplTest, UpdateMetadata_AppName) {
  EXPECT_EQ(kTestDefaultAppName, header_row()->app_name_for_testing());

  {
    media_session::MediaMetadata metadata;
    metadata.title = u"title";
    metadata.artist = u"artist";
    metadata.source_title = kTestAppName;
    view()->UpdateWithMediaMetadata(metadata);
  }

  EXPECT_EQ(kTestAppName, header_row()->app_name_for_testing());

  {
    media_session::MediaMetadata metadata;
    metadata.title = u"title";
    metadata.artist = u"artist";
    view()->UpdateWithMediaMetadata(metadata);
  }

  EXPECT_EQ(kTestDefaultAppName, header_row()->app_name_for_testing());
}

TEST_F(MediaNotificationViewImplTest, Buttons_WhenCollapsed) {
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

  EXPECT_FALSE(GetActuallyExpanded());

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

TEST_F(MediaNotificationViewImplTest, Buttons_WhenExpanded) {
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

  EXPECT_TRUE(GetActuallyExpanded());

  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPlay));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kSeekBackward));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kSeekForward));
}

TEST_F(MediaNotificationViewImplTest, ClickHeader_ToggleExpand) {
  view()->SetExpanded(true);
  EnableAllActions();

  EXPECT_TRUE(GetActuallyExpanded());

  SimulateHeaderClick();

  EXPECT_FALSE(GetActuallyExpanded());

  SimulateHeaderClick();

  EXPECT_TRUE(GetActuallyExpanded());
}

TEST_F(MediaNotificationViewImplTest, ActionButtonsHiddenByDefault) {
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPlay));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekBackward));
}

TEST_F(MediaNotificationViewImplTest, ActionButtonsToggleVisibility) {
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));

  EnableAction(MediaSessionAction::kNextTrack);

  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kNextTrack));

  DisableAction(MediaSessionAction::kNextTrack);

  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
}

TEST_F(MediaNotificationViewImplTest, UpdateArtworkFromItem) {
  int title_artist_width = title_artist_row()->width();
  const SkColor accent = header_row()->color_for_testing().value();
  gfx::Size size = view()->size();
  EXPECT_CALL(container(), OnMediaArtworkChanged(_)).Times(2);
  EXPECT_CALL(container(), OnColorsChanged(_, _, _)).Times(2);

  SkBitmap image;
  image.allocN32Pixels(10, 10);
  image.eraseColor(SK_ColorGREEN);

  EXPECT_TRUE(GetArtworkImage().isNull());

  view()->UpdateWithMediaArtwork(gfx::ImageSkia::CreateFrom1xBitmap(image));

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
  auto accent_color = header_row()->color_for_testing();
  ASSERT_TRUE(accent_color.has_value());
  EXPECT_NE(accent, accent_color.value());

  view()->UpdateWithMediaArtwork(
      gfx::ImageSkia::CreateFrom1xBitmap(SkBitmap()));

  // Ensure the title artist row goes back to the original width now that we
  // do not have any artwork.
  EXPECT_EQ(title_artist_width, title_artist_row()->width());

  // Ensure that the background artwork was reset and the size was still not
  // affected.
  EXPECT_TRUE(GetArtworkImage().isNull());
  EXPECT_EQ(size, view()->size());
  accent_color = header_row()->color_for_testing();
  ASSERT_TRUE(accent_color.has_value());
  EXPECT_EQ(accent, accent_color.value());
}

TEST_F(MediaNotificationViewImplTest, ExpandableDefaultState) {
  EXPECT_FALSE(GetActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());
}

TEST_F(MediaNotificationViewImplTest, ExpandablePlayPauseActionCountsOnce) {
  view()->SetExpanded(true);

  EXPECT_FALSE(GetActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAction(MediaSessionAction::kPreviousTrack);
  EnableAction(MediaSessionAction::kNextTrack);
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  EXPECT_FALSE(GetActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  view()->UpdateWithMediaSessionInfo(session_info);

  EXPECT_FALSE(GetActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_TRUE(GetActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());
}

TEST_F(MediaNotificationViewImplTest, BecomeExpandableAndWasNotExpandable) {
  view()->SetExpanded(true);

  EXPECT_FALSE(GetActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAllActions();

  EXPECT_TRUE(GetActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());
}

TEST_F(MediaNotificationViewImplTest, BecomeExpandableButWasAlreadyExpandable) {
  view()->SetExpanded(true);

  EXPECT_FALSE(GetActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAllActions();

  EXPECT_TRUE(GetActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());

  DisableAction(MediaSessionAction::kSeekForward);

  EXPECT_TRUE(GetActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());
}

TEST_F(MediaNotificationViewImplTest, BecomeNotExpandableAndWasExpandable) {
  view()->SetExpanded(true);

  EXPECT_FALSE(GetActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAllActions();

  EXPECT_TRUE(GetActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());

  DisableAction(MediaSessionAction::kPreviousTrack);
  DisableAction(MediaSessionAction::kNextTrack);
  DisableAction(MediaSessionAction::kSeekBackward);
  DisableAction(MediaSessionAction::kSeekForward);

  EXPECT_FALSE(GetActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());
}

TEST_F(MediaNotificationViewImplTest,
       BecomeNotExpandableButWasAlreadyNotExpandable) {
  view()->SetExpanded(true);

  EXPECT_FALSE(GetActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_FALSE(GetActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());
}

TEST_F(MediaNotificationViewImplTest, ActionButtonRowSizeAndAlignment) {
  EnableAction(MediaSessionAction::kPlay);

  views::Button* button = GetButtonForAction(MediaSessionAction::kPlay);
  int button_x = button->GetBoundsInScreen().x();

  // When collapsed the button row should be a fixed width.
  EXPECT_FALSE(GetActuallyExpanded());
  EXPECT_EQ(124, button_row()->width());

  EnableAllActions();
  view()->SetExpanded(true);

  // When expanded the button row should be wider and the play button should
  // have shifted to the left.
  EXPECT_TRUE(GetActuallyExpanded());
  EXPECT_LT(124, button_row()->width());
  EXPECT_GT(button_x, button->GetBoundsInScreen().x());
}

TEST_F(MediaNotificationViewImplTest, NotifysContainerOfExpandedState) {
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

TEST_F(MediaNotificationViewImplTest, AccessibleProperties) {
  ui::AXNodeData data;
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);

  EXPECT_TRUE(
      data.HasStringAttribute(ax::mojom::StringAttribute::kRoleDescription));
  EXPECT_EQ(u"title - artist", accessible_name());

  EXPECT_EQ(view()->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kListItem);
}

TEST_F(MediaNotificationViewImplTest, ForcedExpandedState) {
  // Make the view expandable.
  EnableAllActions();

  // Force it to be expanded.
  bool expanded_state = true;
  view()->SetForcedExpandedState(&expanded_state);
  EXPECT_TRUE(GetActuallyExpanded());

  // Since it's forced, clicking on the header should not toggle the expanded
  // state.
  SimulateHeaderClick();
  EXPECT_TRUE(GetActuallyExpanded());

  // Force it to be not expanded.
  expanded_state = false;
  view()->SetForcedExpandedState(&expanded_state);
  EXPECT_FALSE(GetActuallyExpanded());

  // Since it's forced, clicking on the header should not toggle the expanded
  // state.
  SimulateHeaderClick();
  EXPECT_FALSE(GetActuallyExpanded());

  // Stop forcing expanded state.
  view()->SetForcedExpandedState(nullptr);
  EXPECT_FALSE(GetActuallyExpanded());

  // Clicking on the header should toggle the expanded state.
  SimulateHeaderClick();
  EXPECT_TRUE(GetActuallyExpanded());
}

TEST_F(MediaNotificationViewImplTest, AllowsHidingOfAppIcon) {
  MediaNotificationViewImpl shows_icon(&container(), nullptr, nullptr,
                                       std::u16string(), kViewWidth,
                                       /*should_show_icon=*/true);
  MediaNotificationViewImpl hides_icon(&container(), nullptr, nullptr,
                                       std::u16string(), kViewWidth,
                                       /*should_show_icon=*/false);

  EXPECT_TRUE(
      GetHeaderRow(&shows_icon)->app_icon_view_for_testing()->GetVisible());
  EXPECT_FALSE(
      GetHeaderRow(&hides_icon)->app_icon_view_for_testing()->GetVisible());
}

TEST_F(MediaNotificationViewImplTest, ClickHeader_NotifyContainer) {
  EXPECT_CALL(container(), OnHeaderClicked(/*activate_original_media=*/true));
  SimulateHeaderClick();
}

}  // namespace media_message_center
