// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl_view.h"

#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_item.h"
#include "chrome/browser/ui/global_media_controls/cast_media_session_controller.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_observer.h"
#include "chrome/browser/ui/global_media_controls/test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/media_message_center/media_notification_controller.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/test/scoped_screen_override.h"
#include "ui/display/test/test_screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/view_metadata_test_utils.h"
#include "ui/views/widget/widget_utils.h"

using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionAction;
using ::testing::_;

namespace {

const char kTestNotificationId[] = "testid";
const char kOtherTestNotificationId[] = "othertestid";
const char kRouteId[] = "route_id";

class MockMediaNotificationContainerObserver
    : public MediaNotificationContainerObserver {
 public:
  MockMediaNotificationContainerObserver() = default;
  ~MockMediaNotificationContainerObserver() = default;

  // MediaNotificationContainerObserver implementation.
  MOCK_METHOD0(OnContainerSizeChanged, void());
  MOCK_METHOD0(OnContainerMetadataChanged, void());
  MOCK_METHOD0(OnContainerActionsChanged, void());
  MOCK_METHOD1(OnContainerClicked, void(const std::string& id));
  MOCK_METHOD1(OnContainerDismissed, void(const std::string& id));
  MOCK_METHOD1(OnContainerDestroyed, void(const std::string& id));
  MOCK_METHOD2(OnContainerDraggedOut,
               void(const std::string& id, gfx::Rect bounds));
  MOCK_METHOD2(OnAudioSinkChosen,
               void(const std::string& id, const std::string& sink_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaNotificationContainerObserver);
};

media_router::MediaRoute CreateMediaRoute() {
  media_router::MediaRoute route(kRouteId,
                                 media_router::MediaSource("source_id"),
                                 "sink_id", "route_description",
                                 /* is_local */ true, /* for_display */ true);
  route.set_media_sink_name("sink_name");
  return route;
}

class MockSessionController : public CastMediaSessionController {
 public:
  MockSessionController(
      mojo::Remote<media_router::mojom::MediaController> remote)
      : CastMediaSessionController(std::move(remote)) {}

  MOCK_METHOD1(Send, void(media_session::mojom::MediaSessionAction));
  MOCK_METHOD1(OnMediaStatusUpdated, void(media_router::mojom::MediaStatusPtr));
};

class MockMediaNotificationController
    : public media_message_center::MediaNotificationController {
 public:
  MockMediaNotificationController() = default;
  ~MockMediaNotificationController() = default;

  MOCK_METHOD(void, ShowNotification, (const std::string&));
  MOCK_METHOD(void, HideNotification, (const std::string&));
  MOCK_METHOD(void, RemoveItem, (const std::string&));

  MOCK_METHOD(scoped_refptr<base::SequencedTaskRunner>,
              GetTaskRunner,
              (),
              (const));
  MOCK_METHOD(void,
              LogMediaSessionActionButtonPressed,
              (const std::string&, MediaSessionAction));
};
}  // anonymous namespace

class MediaNotificationContainerImplViewTest : public ChromeViewsTestBase {
 public:
  MediaNotificationContainerImplViewTest() : screen_override_(&fake_screen_) {}
  ~MediaNotificationContainerImplViewTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();
    item_ = std::make_unique<MockMediaNotificationItem>();
    SetUpCommon(std::make_unique<MediaNotificationContainerImplView>(
        kTestNotificationId, item_->GetWeakPtr(), nullptr,
        GlobalMediaControlsEntryPoint::kToolbarIcon));
  }

  void SetUpCommon(std::unique_ptr<MediaNotificationContainerImplView>
                       notification_container) {
    widget_ = CreateTestWidget();

    notification_container_ =
        widget_->SetContentsView(std::move(notification_container));

    observer_ = std::make_unique<MockMediaNotificationContainerObserver>();
    notification_container_->AddObserver(observer_.get());

    SimulateMediaSessionData();

    widget_->Show();
  }

  void TearDown() override {
    notification_container_->RemoveObserver(observer_.get());
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  void SimulateNotificationSwipedToDismiss() {
    // When the notification is swiped, the SlideOutController sends this to the
    // MediaNotificationContainerImplView.
    notification_container()->OnSlideOut();
  }

  bool IsDismissButtonVisible() { return GetDismissButton()->IsDrawn(); }

  void SimulateHoverOverContainer() {
    fake_screen_.set_cursor_screen_point(
        notification_container_->GetBoundsInScreen().CenterPoint());

    ui::MouseEvent event(ui::ET_MOUSE_ENTERED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    notification_container_->OnMouseEntered(event);
  }

  void SimulateNotHoveringOverContainer() {
    gfx::Rect container_bounds = notification_container_->GetBoundsInScreen();
    gfx::Point point_outside_container =
        container_bounds.bottom_right() + gfx::Vector2d(1, 1);
    fake_screen_.set_cursor_screen_point(point_outside_container);

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
#if defined(OS_MAC)
    ui::KeyboardCode button_press_keycode = ui::VKEY_SPACE;
#else
    ui::KeyboardCode button_press_keycode = ui::VKEY_RETURN;
#endif  // defined(OS_MAC)

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

  views::FocusManager* GetFocusManager() {
    return notification_container_->GetFocusManager();
  }

  MockMediaNotificationContainerObserver& observer() { return *observer_; }

  MediaNotificationContainerImplView* notification_container() {
    return notification_container_;
  }

  base::WeakPtr<MockMediaNotificationItem> notification_item() {
    return item_->GetWeakPtr();
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
    return notification_container()->view_for_testing();
  }

  views::ImageButton* GetDismissButton() {
    return notification_container()->GetDismissButtonForTesting();
  }

  std::unique_ptr<views::Widget> widget_;
  MediaNotificationContainerImplView* notification_container_ = nullptr;
  std::unique_ptr<MockMediaNotificationContainerObserver> observer_;
  std::unique_ptr<MockMediaNotificationItem> item_;

  // Set of actions currently enabled.
  base::flat_set<MediaSessionAction> actions_;

  display::test::TestScreen fake_screen_;
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

  void SimulateMouseDragAndRelease(gfx::Vector2d drag_distance) {
    gfx::Rect start_bounds = notification_container()->bounds();
    gfx::Point drag_start = start_bounds.CenterPoint();
    gfx::Point drag_end = drag_start + drag_distance;

    SimulateMousePressed(drag_start);
    SimulateMouseDragged(drag_end);
    SimulateMouseReleased(drag_end);
  }

  void SimulateMousePressed(gfx::Point point) {
    notification_container()->OnMousePressed(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, point, point,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  void SimulateMouseDragged(gfx::Point point) {
    notification_container()->OnMouseDragged(
        ui::MouseEvent(ui::ET_MOUSE_DRAGGED, point, point,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  void SimulateMouseReleased(gfx::Point point) {
    notification_container()->OnMouseReleased(
        ui::MouseEvent(ui::ET_MOUSE_RELEASED, point, point,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  views::Widget* GetDragImageWidget() {
    return notification_container()->drag_image_widget_for_testing();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(b/185139027): Remove this class once
// |media_router::kGlobalMediaControlsCastStartStop| is enabled by default.
class MediaNotificationContainerImplViewCastTest
    : public MediaNotificationContainerImplViewTest {
 public:
  void SetUp() override {
    ViewsTestBase::SetUp();
    feature_list_.InitWithFeatures(
        {media::kGlobalMediaControlsForCast,
         media_router::kGlobalMediaControlsCastStartStop,
         media::kGlobalMediaControlsOverlayControls},
        {});

    media_router::MediaRouterFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&media_router::MockMediaRouter::Create));

    auto session_controller = std::make_unique<MockSessionController>(
        mojo::Remote<media_router::mojom::MediaController>());
    session_controller_ = session_controller.get();
    item_ = std::make_unique<CastMediaNotificationItem>(
        CreateMediaRoute(), &notification_controller_,
        std::move(session_controller), &profile_);

    SetUpCommon(std::make_unique<MediaNotificationContainerImplView>(
        kTestNotificationId, item_->GetWeakPtr(), nullptr,
        GlobalMediaControlsEntryPoint::kToolbarIcon));
  }

  void TearDown() override {
    // Delete |item_| before |notification_controller_|.
    item_.reset();
    MediaNotificationContainerImplViewTest::TearDown();
  }

  void SimulateStopCastingButtonClicked() {
    ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(
        notification_container()->GetStopCastingButtonForTesting())
        .NotifyClick(event);
  }

  CastMediaNotificationItem* item() { return item_.get(); }
  Profile* profile() { return &profile_; }
  MockMediaNotificationController* notification_controller() {
    return &notification_controller_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  std::unique_ptr<CastMediaNotificationItem> item_;
  MockMediaNotificationController notification_controller_;
  MockSessionController* session_controller_ = nullptr;
};

TEST_F(MediaNotificationContainerImplViewCastTest, StopCasting) {
  auto* mock_router = static_cast<media_router::MockMediaRouter*>(
      media_router::MediaRouterFactory::GetApiForBrowserContext(profile()));
  EXPECT_CALL(*mock_router, TerminateRoute(kRouteId));

  SimulateStopCastingButtonClicked();
}

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
  // When we have many actions enabled, we should be forced into the expanded
  // state.
  SimulateAllActionsEnabled();
  EXPECT_TRUE(notification_container()->is_expanded_for_testing());

  // When we don't have many actions enabled, we should be forced out of the
  // expanded state.
  SimulateOnlyPlayPauseEnabled();
  EXPECT_FALSE(notification_container()->is_expanded_for_testing());

  // We will also be forced into the expanded state when artwork is present.
  SimulateHasArtwork();
  EXPECT_TRUE(notification_container()->is_expanded_for_testing());

  // Once the artwork is gone, we should be forced back out of the expanded
  // state.
  SimulateHasNoArtwork();
  EXPECT_FALSE(notification_container()->is_expanded_for_testing());
}

TEST_F(MediaNotificationContainerImplViewTest, SendsMetadataUpdates) {
  EXPECT_CALL(observer(), OnContainerMetadataChanged());
  SimulateMetadataChanged();
}

TEST_F(MediaNotificationContainerImplViewTest, SendsDestroyedUpdates) {
  auto container = std::make_unique<MediaNotificationContainerImplView>(
      kOtherTestNotificationId, notification_item(), nullptr,
      GlobalMediaControlsEntryPoint::kToolbarIcon);
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

TEST_F(MediaNotificationContainerImplViewTest, SendsSinkUpdates) {
  // The container should notify its observers when an audio output device has
  // been chosen.
  EXPECT_CALL(observer(), OnAudioSinkChosen(kTestNotificationId, "foobar"));
  notification_container()->OnAudioSinkChosen("foobar");
}

TEST_F(MediaNotificationContainerImplViewTest, MetadataTest) {
  auto container_view = std::make_unique<MediaNotificationContainerImplView>(
      kOtherTestNotificationId, notification_item(), nullptr,
      GlobalMediaControlsEntryPoint::kToolbarIcon);
  views::test::TestViewMetadata(container_view.get());
}

TEST_F(MediaNotificationContainerImplViewOverlayControlsTest,
       Dragging_VeryShortSendsClick) {
  // If the user presses and releases the mouse with only a very short drag,
  // then it should be considered a click.
  EXPECT_CALL(observer(), OnContainerClicked(kTestNotificationId));
  EXPECT_CALL(observer(), OnContainerDraggedOut(kTestNotificationId, _))
      .Times(0);
  SimulateMouseDragAndRelease(gfx::Vector2d(1, 1));
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
  SimulateMouseDragAndRelease(gfx::Vector2d(20, 20));
  testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(MediaNotificationContainerImplViewOverlayControlsTest,
       Dragging_LongFiresDraggedOut) {
  // If the user presses and releases the mouse with a long enough drag to pull
  // the container out of the dialog, then it should fire an
  // |OnContainerDraggedOut()| notification.
  EXPECT_CALL(observer(), OnContainerClicked(kTestNotificationId)).Times(0);
  EXPECT_CALL(observer(), OnContainerDraggedOut(kTestNotificationId, _));
  SimulateMouseDragAndRelease(
      notification_container()->bounds().bottom_right().OffsetFromOrigin());
  testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(MediaNotificationContainerImplViewOverlayControlsTest, DragImage) {
  gfx::Point start_point =
      notification_container()->GetBoundsInScreen().CenterPoint();
  gfx::Point end_point = start_point + gfx::Vector2d(50, 50);

  EXPECT_EQ(GetDragImageWidget(), nullptr);

  SimulateMousePressed(start_point);
  SimulateMouseDragged(end_point);
  EXPECT_NE(GetDragImageWidget(), nullptr);
  EXPECT_EQ(GetDragImageWidget()->GetWindowBoundsInScreen().CenterPoint(),
            end_point);

  SimulateMouseReleased(end_point);
  EXPECT_EQ(GetDragImageWidget(), nullptr);
}
