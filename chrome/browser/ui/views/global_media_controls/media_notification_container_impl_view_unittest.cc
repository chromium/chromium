// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl_view.h"

#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_item.h"
#include "chrome/browser/ui/global_media_controls/cast_media_session_controller.h"
#include "chrome/browser/ui/global_media_controls/test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/global_media_controls/public/test/mock_media_item_manager.h"
#include "components/global_media_controls/public/test/mock_media_item_ui_observer.h"
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
using ::testing::NiceMock;

namespace {

const char kTestNotificationId[] = "testid";
const char kOtherTestNotificationId[] = "othertestid";
const char kRouteId[] = "route_id";

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

}  // anonymous namespace

class MediaNotificationContainerImplViewTest : public ChromeViewsTestBase {
 public:
  MediaNotificationContainerImplViewTest() : screen_override_(&fake_screen_) {}
  MediaNotificationContainerImplViewTest(
      const MediaNotificationContainerImplViewTest&) = delete;
  MediaNotificationContainerImplViewTest& operator=(
      const MediaNotificationContainerImplViewTest&) = delete;
  ~MediaNotificationContainerImplViewTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();
    item_ = std::make_unique<NiceMock<MockMediaNotificationItem>>();
    SetUpCommon(std::make_unique<MediaNotificationContainerImplView>(
        kTestNotificationId, item_->GetWeakPtr(), nullptr,
        GlobalMediaControlsEntryPoint::kToolbarIcon, nullptr));
  }

  void SetUpCommon(std::unique_ptr<MediaNotificationContainerImplView>
                       notification_container) {
    widget_ = CreateTestWidget();

    notification_container_ =
        widget_->SetContentsView(std::move(notification_container));

    observer_ = std::make_unique<
        NiceMock<global_media_controls::test::MockMediaItemUIObserver>>();
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

  global_media_controls::test::MockMediaItemUIObserver& observer() {
    return *observer_;
  }

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
  std::unique_ptr<global_media_controls::test::MockMediaItemUIObserver>
      observer_;
  std::unique_ptr<MockMediaNotificationItem> item_;

  // Set of actions currently enabled.
  base::flat_set<MediaSessionAction> actions_;

  display::test::TestScreen fake_screen_;
  display::test::ScopedScreenOverride screen_override_;
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
         media_router::kGlobalMediaControlsCastStartStop},
        {});

    media_router::MediaRouterFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&media_router::MockMediaRouter::Create));

    auto session_controller = std::make_unique<MockSessionController>(
        mojo::Remote<media_router::mojom::MediaController>());
    session_controller_ = session_controller.get();
    item_ = std::make_unique<CastMediaNotificationItem>(
        CreateMediaRoute(), &item_manager_, std::move(session_controller),
        &profile_);

    SetUpCommon(std::make_unique<MediaNotificationContainerImplView>(
        kTestNotificationId, item_->GetWeakPtr(), nullptr,
        GlobalMediaControlsEntryPoint::kToolbarIcon, profile()));
  }

  void TearDown() override {
    // Delete |item_| before |item_manager_|.
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
  global_media_controls::test::MockMediaItemManager* item_manager() {
    return &item_manager_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  std::unique_ptr<CastMediaNotificationItem> item_;
  NiceMock<global_media_controls::test::MockMediaItemManager> item_manager_;
  MockSessionController* session_controller_ = nullptr;
};

TEST_F(MediaNotificationContainerImplViewCastTest, StopCasting) {
  auto* mock_router = static_cast<media_router::MockMediaRouter*>(
      media_router::MediaRouterFactory::GetApiForBrowserContext(profile()));
  EXPECT_CALL(*mock_router, TerminateRoute(kRouteId));

  SimulateStopCastingButtonClicked();
}

TEST_F(MediaNotificationContainerImplViewTest, SwipeToDismiss) {
  EXPECT_CALL(observer(), OnMediaItemUIDismissed(kTestNotificationId));
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
  EXPECT_CALL(observer(), OnMediaItemUIDismissed(kTestNotificationId));
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
  EXPECT_CALL(observer(), OnMediaItemUIDismissed(kTestNotificationId));
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
  EXPECT_CALL(observer(), OnMediaItemUIMetadataChanged());
  SimulateMetadataChanged();
}

TEST_F(MediaNotificationContainerImplViewTest, SendsDestroyedUpdates) {
  auto container = std::make_unique<MediaNotificationContainerImplView>(
      kOtherTestNotificationId, notification_item(), nullptr,
      GlobalMediaControlsEntryPoint::kToolbarIcon, nullptr);
  global_media_controls::test::MockMediaItemUIObserver observer;
  container->AddObserver(&observer);

  // When the container is destroyed, it should notify the observer.
  EXPECT_CALL(observer, OnMediaItemUIDestroyed(kOtherTestNotificationId));
  container.reset();
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(MediaNotificationContainerImplViewTest, SendsClicks) {
  // When the container is clicked directly, it should notify its observers.
  EXPECT_CALL(observer(), OnMediaItemUIClicked(kTestNotificationId));
  SimulateContainerClicked();
  testing::Mock::VerifyAndClearExpectations(&observer());

  // It should also notify its observers when the header is clicked.
  EXPECT_CALL(observer(), OnMediaItemUIClicked(kTestNotificationId));
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
      GlobalMediaControlsEntryPoint::kToolbarIcon, nullptr);
  views::test::TestViewMetadata(container_view.get());
}
