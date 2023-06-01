// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_view.h"

#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/global_media_controls/public/test/mock_media_item_manager.h"
#include "components/global_media_controls/public/test/mock_media_item_ui_device_selector.h"
#include "components/global_media_controls/public/test/mock_media_item_ui_footer.h"
#include "components/global_media_controls/public/test/mock_media_item_ui_observer.h"
#include "components/media_message_center/mock_media_notification_item.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/test/test_screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/animation/slide_out_controller.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/view_metadata_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_utils.h"

using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionAction;
using ::testing::_;
using ::testing::NiceMock;

namespace global_media_controls {

namespace {

const char kTestNotificationId[] = "testid";
const char kOtherTestNotificationId[] = "othertestid";

}  // anonymous namespace

class MediaItemUIViewTest : public views::ViewsTestBase {
 public:
  MediaItemUIViewTest() { display::Screen::SetScreenInstance(&fake_screen_); }
  MediaItemUIViewTest(const MediaItemUIViewTest&) = delete;
  MediaItemUIViewTest& operator=(const MediaItemUIViewTest&) = delete;
  ~MediaItemUIViewTest() override {
    display::Screen::SetScreenInstance(nullptr);
  }

  // views::ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    item_ = std::make_unique<
        NiceMock<media_message_center::test::MockMediaNotificationItem>>();
    widget_ = CreateTestWidget();

    auto footer = std::make_unique<NiceMock<test::MockMediaItemUIFooter>>();
    footer_ = footer.get();

    auto device_selector =
        std::make_unique<NiceMock<test::MockMediaItemUIDeviceSelector>>();
    device_selector_ = device_selector.get();
    device_selector_->SetPreferredSize(gfx::Size(400, 50));

    item_ui_ = widget_->SetContentsView(std::make_unique<MediaItemUIView>(
        kTestNotificationId, item_->GetWeakPtr(), std::move(footer),
        std::move(device_selector)));

    observer_ = std::make_unique<
        NiceMock<global_media_controls::test::MockMediaItemUIObserver>>();
    item_ui_->AddObserver(observer_.get());

    SimulateMediaSessionData();

    widget_->Show();
  }

  void TearDown() override {
    item_ui_->RemoveObserver(observer_.get());
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  void SimulateItemUISwipedToDismiss() {
    // When the item ui is swiped, the SlideOutController sends this to the
    // MediaItemUIView.
    item_ui()->OnSlideOut();
  }

  bool IsDismissButtonVisible() { return GetDismissButton()->IsDrawn(); }

  void SimulateHoverOverItemUI() {
    fake_screen_.set_cursor_screen_point(
        item_ui_->GetBoundsInScreen().CenterPoint());

    ui::MouseEvent event(ui::ET_MOUSE_ENTERED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    item_ui_->OnMouseEntered(event);
  }

  void SimulateNotHoveringOverItemUI() {
    gfx::Rect container_bounds = item_ui_->GetBoundsInScreen();
    gfx::Point point_outside_container =
        container_bounds.bottom_right() + gfx::Vector2d(1, 1);
    fake_screen_.set_cursor_screen_point(point_outside_container);

    ui::MouseEvent event(ui::ET_MOUSE_EXITED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    item_ui_->OnMouseExited(event);
  }

  void SimulateItemUIClicked() {
    ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(item_ui_).NotifyClick(event);
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
    GetFocusManager()->SetFocusedView(item_ui_->GetDismissButtonForTesting());

// On Mac OS, we need to use the space bar to press a button.
#if BUILDFLAG(IS_MAC)
    ui::KeyboardCode button_press_keycode = ui::VKEY_SPACE;
#else
    ui::KeyboardCode button_press_keycode = ui::VKEY_RETURN;
#endif  // BUILDFLAG(IS_MAC)

    ui::test::EventGenerator generator(GetRootWindow(widget_.get()));
    generator.PressKey(button_press_keycode, 0);
  }

  void SimulateSessionPlaying() { SimulateSessionInfo(true); }

  void SimulateSessionPaused() { SimulateSessionInfo(false); }

  void SimulateMetadataChanged() {
    media_session::MediaMetadata metadata;
    metadata.source_title = u"source_title2";
    metadata.title = u"title2";
    metadata.artist = u"artist2";
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

  views::FocusManager* GetFocusManager() { return item_ui_->GetFocusManager(); }

  test::MockMediaItemUIObserver& observer() { return *observer_; }

  MediaItemUIView* item_ui() { return item_ui_; }

  base::WeakPtr<media_message_center::test::MockMediaNotificationItem>
  notification_item() {
    return item_->GetWeakPtr();
  }
  test::MockMediaItemUIFooter* footer() { return footer_; }
  test::MockMediaItemUIDeviceSelector* device_selector() {
    return device_selector_;
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
    metadata.source_title = u"source_title";
    metadata.title = u"title";
    metadata.artist = u"artist";
    GetView()->UpdateWithMediaMetadata(metadata);

    SimulateOnlyPlayPauseEnabled();
  }

  void NotifyUpdatedActions() { GetView()->UpdateWithMediaActions(actions_); }

  media_message_center::MediaNotificationViewImpl* GetView() {
    return item_ui()->view_for_testing();
  }

  views::ImageButton* GetDismissButton() {
    return item_ui()->GetDismissButtonForTesting();
  }

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<test::MockMediaItemUIFooter, DanglingUntriaged> footer_ = nullptr;
  raw_ptr<test::MockMediaItemUIDeviceSelector, DanglingUntriaged>
      device_selector_ = nullptr;
  raw_ptr<MediaItemUIView, DanglingUntriaged> item_ui_ = nullptr;
  std::unique_ptr<global_media_controls::test::MockMediaItemUIObserver>
      observer_;
  std::unique_ptr<media_message_center::test::MockMediaNotificationItem> item_;

  // Set of actions currently enabled.
  base::flat_set<MediaSessionAction> actions_;

  display::test::TestScreen fake_screen_;
};

TEST_F(MediaItemUIViewTest, SwipeToDismiss) {
  EXPECT_CALL(observer(), OnMediaItemUIDismissed(kTestNotificationId));
  SimulateItemUISwipedToDismiss();
}

TEST_F(MediaItemUIViewTest, ClickToDismiss) {
  // Ensure that the mouse is not over the container and that nothing is
  // focused. The dismiss button should not be visible.
  SimulateNotHoveringOverItemUI();
  ASSERT_EQ(nullptr, GetFocusManager()->GetFocusedView());
  ASSERT_FALSE(item_ui()->IsMouseHovered());
  EXPECT_FALSE(IsDismissButtonVisible());

  // Hovering over the notification should show the dismiss button.
  SimulateHoverOverItemUI();
  EXPECT_TRUE(IsDismissButtonVisible());

  // Moving the mouse away from the notification should hide the dismiss button.
  SimulateNotHoveringOverItemUI();
  EXPECT_FALSE(IsDismissButtonVisible());

  // Moving the mouse back over the notification should re-show it.
  SimulateHoverOverItemUI();
  EXPECT_TRUE(IsDismissButtonVisible());

  // Clicking it should inform observers that we've been dismissed.
  EXPECT_CALL(observer(), OnMediaItemUIDismissed(kTestNotificationId));
  SimulateDismissButtonClicked();
  testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(MediaItemUIViewTest, KeyboardToDismiss) {
  // Ensure that the mouse is not over the container and that nothing is
  // focused. The dismiss button should not be visible.
  SimulateNotHoveringOverItemUI();
  ASSERT_EQ(nullptr, GetFocusManager()->GetFocusedView());
  ASSERT_FALSE(item_ui()->IsMouseHovered());
  EXPECT_FALSE(IsDismissButtonVisible());

  // When the notification receives keyboard focus, the dismiss button should be
  // visible.
  GetFocusManager()->SetFocusedView(item_ui());
  EXPECT_TRUE(IsDismissButtonVisible());

  // When the notification loses keyboard focus, the dismiss button should be
  // hidden.
  GetFocusManager()->SetFocusedView(nullptr);
  EXPECT_FALSE(IsDismissButtonVisible());

  // If it gets focus again, it should re-show the dismiss button.
  GetFocusManager()->SetFocusedView(item_ui());
  EXPECT_TRUE(IsDismissButtonVisible());

  // Clicking it should inform observers that we've been dismissed.
  EXPECT_CALL(observer(), OnMediaItemUIDismissed(kTestNotificationId));
  SimulatePressingDismissButtonWithKeyboard();
  testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(MediaItemUIViewTest, ForceExpandedState) {
  // When we have many actions enabled, we should be forced into the expanded
  // state.
  SimulateAllActionsEnabled();
  EXPECT_TRUE(item_ui()->is_expanded_for_testing());

  // When we don't have many actions enabled, we should be forced out of the
  // expanded state.
  SimulateOnlyPlayPauseEnabled();
  EXPECT_FALSE(item_ui()->is_expanded_for_testing());

  // We will also be forced into the expanded state when artwork is present.
  SimulateHasArtwork();
  EXPECT_TRUE(item_ui()->is_expanded_for_testing());

  // Once the artwork is gone, we should be forced back out of the expanded
  // state.
  SimulateHasNoArtwork();
  EXPECT_FALSE(item_ui()->is_expanded_for_testing());
}

TEST_F(MediaItemUIViewTest, SendsMetadataUpdates) {
  EXPECT_CALL(observer(), OnMediaItemUIMetadataChanged());
  SimulateMetadataChanged();
}

TEST_F(MediaItemUIViewTest, SendsDestroyedUpdates) {
  auto container = std::make_unique<MediaItemUIView>(
      kOtherTestNotificationId, notification_item(), nullptr, nullptr);
  global_media_controls::test::MockMediaItemUIObserver observer;
  container->AddObserver(&observer);

  // When the container is destroyed, it should notify the observer.
  EXPECT_CALL(observer, OnMediaItemUIDestroyed(kOtherTestNotificationId));
  container.reset();
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(MediaItemUIViewTest, SendsClicks) {
  // When the container is clicked directly, it should notify its observers.
  EXPECT_CALL(observer(), OnMediaItemUIClicked(kTestNotificationId));
  SimulateItemUIClicked();
  testing::Mock::VerifyAndClearExpectations(&observer());

  // It should also notify its observers when the header is clicked.
  EXPECT_CALL(observer(), OnMediaItemUIClicked(kTestNotificationId));
  SimulateHeaderClicked();
}

TEST_F(MediaItemUIViewTest, GestureScrollDisabledWhenSlidingOut) {
  auto scroll_view = std::make_unique<views::ScrollView>();
  item_ui()->SetScrollView(scroll_view.get());

  // Vertical scroll bar should be enabled initially.
  EXPECT_EQ(scroll_view->GetVerticalScrollBarMode(),
            views::ScrollView::ScrollBarMode::kEnabled);

  // Send a gesture scroll update event with some horizontal value.
  gfx::Point point;
  ui::GestureEvent gesture_scroll_update(
      point.x(), point.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, /*delta_x=*/1.0,
                              /*delta_y=*/0.0));
  item_ui()->slide_out_controller_for_testing()->OnGestureEvent(
      &gesture_scroll_update);

  // Vertical scroll bar should be disabled because of the sliding.
  EXPECT_EQ(scroll_view->GetVerticalScrollBarMode(),
            views::ScrollView::ScrollBarMode::kDisabled);

  // Slide ending should re-enabled the vertical scroll bar.
  ui::GestureEvent gesture_scroll_end(
      point.x(), point.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));
  item_ui()->slide_out_controller_for_testing()->OnGestureEvent(
      &gesture_scroll_end);
  EXPECT_EQ(scroll_view->GetVerticalScrollBarMode(),
            views::ScrollView::ScrollBarMode::kEnabled);
}

TEST_F(MediaItemUIViewTest, MetadataTest) {
  auto container_view = std::make_unique<MediaItemUIView>(
      kOtherTestNotificationId, notification_item(), nullptr, nullptr);
  views::test::TestViewMetadata(container_view.get());
}

TEST_F(MediaItemUIViewTest, UpdateView) {
  EXPECT_CALL(*footer(), Die);
  item_ui()->UpdateFooterView(
      std::make_unique<NiceMock<test::MockMediaItemUIFooter>>());

  EXPECT_CALL(*device_selector(), Die);
  item_ui()->UpdateDeviceSelector(
      std::make_unique<NiceMock<test::MockMediaItemUIDeviceSelector>>());
}

}  // namespace global_media_controls
