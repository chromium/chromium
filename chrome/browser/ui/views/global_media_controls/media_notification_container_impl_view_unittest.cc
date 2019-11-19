// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl_view.h"

#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_observer.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/test/scoped_screen_override.h"
#include "ui/display/test/test_screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_utils.h"

using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionAction;
using ::testing::_;

namespace {

const char kTestNotificationId[] = "testid";
const char kOtherTestNotificationId[] = "othertestid";

class MockMediaNotificationContainerObserver
    : public MediaNotificationContainerObserver {
 public:
  MockMediaNotificationContainerObserver() = default;
  ~MockMediaNotificationContainerObserver() = default;

  // MediaNotificationContainerObserver implementation.
  MOCK_METHOD1(OnContainerExpanded, void(bool expanded));
  MOCK_METHOD0(OnContainerMetadataChanged, void());
  MOCK_METHOD1(OnContainerClicked, void(const std::string& id));
  MOCK_METHOD1(OnContainerDismissed, void(const std::string& id));
  MOCK_METHOD1(OnContainerDestroyed, void(const std::string& id));
  MOCK_METHOD2(OnContainerDraggedOut,
               void(const std::string& id, gfx::Rect bounds));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaNotificationContainerObserver);
};

// Fake display::Screen implementation that allows us to set a cursor location.
class FakeCursorLocationScreen : public display::test::TestScreen {
 public:
  FakeCursorLocationScreen() = default;
  ~FakeCursorLocationScreen() override = default;

  void SetCursorScreenPoint(gfx::Point point) { cursor_position_ = point; }

  // display::test::TestScreen implementation.
  gfx::Point GetCursorScreenPoint() override { return cursor_position_; }

 private:
  gfx::Point cursor_position_;

  DISALLOW_COPY_AND_ASSIGN(FakeCursorLocationScreen);
};

}  // anonymous namespace

class MediaNotificationContainerImplViewTest : public views::ViewsTestBase {
 public:
  MediaNotificationContainerImplViewTest() : screen_override_(&fake_screen_) {}
  ~MediaNotificationContainerImplViewTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();

    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(400, 300);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_.Init(std::move(params));

    auto notification_container =
        std::make_unique<MediaNotificationContainerImplView>(
            kTestNotificationId, nullptr);
    notification_container_ = notification_container.get();
    widget_.SetContentsView(notification_container.release());

    observer_ = std::make_unique<MockMediaNotificationContainerObserver>();
    notification_container_->AddObserver(observer_.get());

    SimulateMediaSessionData();

    widget_.Show();
  }

  void TearDown() override {
    notification_container_->RemoveObserver(observer_.get());
    widget_.Close();
    ViewsTestBase::TearDown();
  }

  void SimulateNotificationSwipedToDismiss() {
    // When the notification is swiped, the SlideOutController sends this to the
    // MediaNotificationContainerImplView.
    notification_container()->OnSlideOut();
  }

  bool IsDismissButtonVisible() { return GetDismissButton()->IsDrawn(); }

  void SimulateHoverOverContainer() {
    fake_screen_.SetCursorScreenPoint(
        notification_container_->GetBoundsInScreen().CenterPoint());

    ui::MouseEvent event(ui::ET_MOUSE_ENTERED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    notification_container_->OnMouseEntered(event);
  }

  void SimulateNotHoveringOverContainer() {
    gfx::Rect container_bounds = notification_container_->GetBoundsInScreen();
    gfx::Point point_outside_container =
        container_bounds.bottom_right() + gfx::Vector2d(1, 1);
    fake_screen_.SetCursorScreenPoint(point_outside_container);

    ui::MouseEvent event(ui::ET_MOUSE_EXITED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    notification_container_->OnMouseExited(event);
  }

  void SimulateContainerClicked() {
    ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(notification_container_).NotifyClick(event);
  }

  void SimulateHeaderClicked() {
    ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(GetView()->GetHeaderRowForTesting())
        .NotifyClick(event);
  }

  void SimulateDismissButtonClicked() {
    ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(GetDismissButton()).NotifyClick(event);
  }

  void SimulatePressingDismissButtonWithKeyboard() {
    GetFocusManager()->SetFocusedView(
        notification_container_->GetDismissButtonForTesting());

// On Mac OS, we need to use the space bar to press a button.
#if defined(OS_MACOSX)
    ui::KeyboardCode button_press_keycode = ui::VKEY_SPACE;
#else
    ui::KeyboardCode button_press_keycode = ui::VKEY_RETURN;
#endif  // defined(OS_MACOSX)

    ui::test::EventGenerator generator(GetRootWindow(&widget_));
    generator.PressKey(button_press_keycode, 0);
  }

  void SimulateSessionPlaying() { SimulateSessionInfo(true); }

  void SimulateSessionPaused() { SimulateSessionInfo(false); }

  void SimulateMetadataChanged() {
    media_session::MediaMetadata metadata;
    metadata.source_title = base::ASCIIToUTF16("source_title2");
    metadata.title = base::ASCIIToUTF16("title2");
    metadata.artist = base::ASCIIToUTF16("artist2");
    GetView()->UpdateWithMediaMetadata(metadata);
  }

  void SimulateAllActionsEnabled() {
    actions_.insert(MediaSessionAction::kPlay);
    actions_.insert(MediaSessionAction::kPause);
    actions_.insert(MediaSessionAction::kPreviousTrack);
    actions_.insert(MediaSessionAction::kNextTrack);
    actions_.insert(MediaSessionAction::kSeekBackward);
    actions_.insert(MediaSessionAction::kSeekForward);
    actions_.insert(MediaSessionAction::kStop);

    NotifyUpdatedActions();
  }

  void SimulateOnlyPlayPauseEnabled() {
    actions_.clear();
    actions_.insert(MediaSessionAction::kPlay);
    actions_.insert(MediaSessionAction::kPause);
    NotifyUpdatedActions();
  }

  void SimulateHasArtwork() {
    SkBitmap image;
    image.allocN32Pixels(10, 10);
    image.eraseColor(SK_ColorMAGENTA);
    GetView()->UpdateWithMediaArtwork(
        gfx::ImageSkia::CreateFrom1xBitmap(image));
  }

  void SimulateHasNoArtwork() {
    GetView()->UpdateWithMediaArtwork(gfx::ImageSkia());
  }

  views::FocusManager* GetFocusManager() {
    return notification_container_->GetFocusManager();
  }

  MockMediaNotificationContainerObserver& observer() { return *observer_; }

  MediaNotificationContainerImplView* notification_container() {
    return notification_container_;
  }

 private:
  void SimulateSessionInfo(bool playing) {
    media_session::mojom::MediaSessionInfoPtr session_info(
        media_session::mojom::MediaSessionInfo::New());
    session_info->playback_state =
        playing ? MediaPlaybackState::kPlaying : MediaPlaybackState::kPaused;
    session_info->is_controllable = true;

    GetView()->UpdateWithMediaSessionInfo(std::move(session_info));
  }

  void SimulateMediaSessionData() {
    SimulateSessionInfo(true);

    media_session::MediaMetadata metadata;
    metadata.source_title = base::ASCIIToUTF16("source_title");
    metadata.title = base::ASCIIToUTF16("title");
    metadata.artist = base::ASCIIToUTF16("artist");
    GetView()->UpdateWithMediaMetadata(metadata);

    SimulateOnlyPlayPauseEnabled();
  }

  void NotifyUpdatedActions() { GetView()->UpdateWithMediaActions(actions_); }

  media_message_center::MediaNotificationView* GetView() {
    return notification_container()->view_for_testing();
  }

  views::ImageButton* GetDismissButton() {
    return notification_container()->GetDismissButtonForTesting();
  }

  views::Widget widget_;
  MediaNotificationContainerImplView* notification_container_ = nullptr;
  std::unique_ptr<MockMediaNotificationContainerObserver> observer_;

  // Set of actions currently enabled.
  base::flat_set<MediaSessionAction> actions_;

  FakeCursorLocationScreen fake_screen_;
  display::test::ScopedScreenOverride screen_override_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationContainerImplViewTest);
};

// TODO(https://crbug.com/1022452): Remove this class once
// |kGlobalMediaControlsOverlayControls| is enabled by default.
class MediaNotificationContainerImplViewOverlayControlsTest
    : public MediaNotificationContainerImplViewTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        media::kGlobalMediaControlsOverlayControls);
    MediaNotificationContainerImplViewTest::SetUp();
  }

  void SimulateMouseDrag(gfx::Vector2d drag_distance) {
    gfx::Rect start_bounds = notification_container()->bounds();
    gfx::Point drag_start = start_bounds.CenterPoint();
    gfx::Point drag_end = drag_start + drag_distance;

    notification_container()->OnMousePressed(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, drag_start, drag_start,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    notification_container()->OnMouseDragged(
        ui::MouseEvent(ui::ET_MOUSE_DRAGGED, drag_end, drag_end,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    notification_container()->OnMouseReleased(
        ui::MouseEvent(ui::ET_MOUSE_RELEASED, drag_end, drag_end,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(MediaNotificationContainerImplViewTest, SwipeToDismiss) {
  EXPECT_CALL(observer(), OnContainerDismissed(kTestNotificationId));
  SimulateNotificationSwipedToDismiss();
}

TEST_F(MediaNotificationContainerImplViewTest, ClickToDismiss) {
  // Ensure that the mouse is not over the container and that nothing is
  // focused. The dismiss button should not be visible.
  SimulateNotHoveringOverContainer();
  ASSERT_EQ(nullptr, GetFocusManager()->GetFocusedView());
  ASSERT_FALSE(notification_container()->IsMouseHovered());
  EXPECT_FALSE(IsDismissButtonVisible());

  // Hovering over the notification should show the dismiss button.
  SimulateHoverOverContainer();
  EXPECT_TRUE(IsDismissButtonVisible());

  // Moving the mouse away from the notification should hide the dismiss button.
  SimulateNotHoveringOverContainer();
  EXPECT_FALSE(IsDismissButtonVisible());

  // Moving the mouse back over the notification should re-show it.
  SimulateHoverOverContainer();
  EXPECT_TRUE(IsDismissButtonVisible());

  // Clicking it should inform observers that we've been dismissed.
  EXPECT_CALL(observer(), OnContainerDismissed(kTestNotificationId));
  SimulateDismissButtonClicked();
  testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(MediaNotificationContainerImplViewTest, KeyboardToDismiss) {
  // Ensure that the mouse is not over the container and that nothing is
  // focused. The dismiss button should not be visible.
  SimulateNotHoveringOverContainer();
  ASSERT_EQ(nullptr, GetFocusManager()->GetFocusedView());
  ASSERT_FALSE(notification_container()->IsMouseHovered());
  EXPECT_FALSE(IsDismissButtonVisible());

  // When the notification receives keyboard focus, the dismiss button should be
  // visible.
  GetFocusManager()->SetFocusedView(notification_container());
  EXPECT_TRUE(IsDismissButtonVisible());

  // When the notification loses keyboard focus, the dismiss button should be
  // hidden.
  GetFocusManager()->SetFocusedView(nullptr);
  EXPECT_FALSE(IsDismissButtonVisible());

  // If it gets focus again, it should re-show the dismiss button.
  GetFocusManager()->SetFocusedView(notification_container());
  EXPECT_TRUE(IsDismissButtonVisible());

  // Clicking it should inform observers that we've been dismissed.
  EXPECT_CALL(observer(), OnContainerDismissed(kTestNotificationId));
  SimulatePressingDismissButtonWithKeyboard();
  testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(MediaNotificationContainerImplViewTest, ForceExpandedState) {
  bool notification_expanded = false;
  EXPECT_CALL(observer(), OnContainerExpanded(_))
      .WillRepeatedly([&notification_expanded](bool expanded) {
        notification_expanded = expanded;
      });

  // When we have many actions enabled, we should be forced into the expanded
  // state.
  SimulateAllActionsEnabled();
  EXPECT_TRUE(notification_expanded);

  // When we don't have many actions enabled, we should be forced out of the
  // expanded state.
  SimulateOnlyPlayPauseEnabled();
  EXPECT_FALSE(notification_expanded);

  // We will also be forced into the expanded state when artwork is present.
  SimulateHasArtwork();
  EXPECT_TRUE(notification_expanded);

  // Once the artwork is gone, we should be forced back out of the expanded
  // state.
  SimulateHasNoArtwork();
  EXPECT_FALSE(notification_expanded);
}

TEST_F(MediaNotificationContainerImplViewTest, SendsMetadataUpdates) {
  EXPECT_CALL(observer(), OnContainerMetadataChanged());
  SimulateMetadataChanged();
}

TEST_F(MediaNotificationContainerImplViewTest, SendsDestroyedUpdates) {
  auto container = std::make_unique<MediaNotificationContainerImplView>(
      kOtherTestNotificationId, nullptr);
  MockMediaNotificationContainerObserver observer;
  container->AddObserver(&observer);

  // When the container is destroyed, it should notify the observer.
  EXPECT_CALL(observer, OnContainerDestroyed(kOtherTestNotificationId));
  container.reset();
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(MediaNotificationContainerImplViewTest, SendsClicks) {
  // When the container is clicked directly, it should notify its observers.
  EXPECT_CALL(observer(), OnContainerClicked(kTestNotificationId));
  SimulateContainerClicked();
  testing::Mock::VerifyAndClearExpectations(&observer());

  // It should also notify its observers when the header is clicked.
  EXPECT_CALL(observer(), OnContainerClicked(kTestNotificationId));
  SimulateHeaderClicked();
}

TEST_F(MediaNotificationContainerImplViewOverlayControlsTest,
       Dragging_VeryShortSendsClick) {
  // If the user presses and releases the mouse with only a very short drag,
  // then it should be considered a click.
  EXPECT_CALL(observer(), OnContainerClicked(kTestNotificationId));
  EXPECT_CALL(observer(), OnContainerDraggedOut(kTestNotificationId, _))
      .Times(0);
  SimulateMouseDrag(gfx::Vector2d(1, 1));
  testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(MediaNotificationContainerImplViewOverlayControlsTest,
       Dragging_ShortDoesNothing) {
  // If the user presses and releases the mouse with a drag that doesn't go
  // outside of the container, then it should just return to its initial
  // position and do nothing.
  EXPECT_CALL(observer(), OnContainerClicked(kTestNotificationId)).Times(0);
  EXPECT_CALL(observer(), OnContainerDraggedOut(kTestNotificationId, _))
      .Times(0);
  SimulateMouseDrag(gfx::Vector2d(20, 20));
  testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(MediaNotificationContainerImplViewOverlayControlsTest,
       Dragging_LongFiresDraggedOut) {
  // If the user presses and releases the mouse with a long enough drag to pull
  // the container out of the dialog, then it should fire an
  // |OnContainerDraggedOut()| notification.
  EXPECT_CALL(observer(), OnContainerClicked(kTestNotificationId)).Times(0);
  EXPECT_CALL(observer(), OnContainerDraggedOut(kTestNotificationId, _));
  SimulateMouseDrag(gfx::Vector2d(300, 300));
  testing::Mock::VerifyAndClearExpectations(&observer());
}
