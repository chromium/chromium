// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_view.h"

#include "components/global_media_controls/public/test/mock_media_item_manager.h"
#include "components/global_media_controls/public/test/mock_media_item_ui_device_selector.h"
#include "components/global_media_controls/public/test/mock_media_item_ui_footer.h"
#include "components/global_media_controls/public/test/mock_media_item_ui_observer.h"
#include "components/global_media_controls/public/views/media_item_ui_detailed_view.h"
#include "components/media_message_center/mock_media_notification_item.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/test/test_screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
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
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

    auto footer = std::make_unique<NiceMock<test::MockMediaItemUIFooter>>();
    auto* footer_ptr = footer.get();

    auto device_selector =
        std::make_unique<NiceMock<test::MockMediaItemUIDeviceSelector>>();
    device_selector->SetPreferredSize(gfx::Size(400, 50));
    auto* device_selector_ptr = device_selector.get();

    item_ui_ = widget_->SetContentsView(std::make_unique<MediaItemUIView>(
        kTestNotificationId, item_->GetWeakPtr(), std::move(footer),
        std::move(device_selector), media_message_center::MediaColorTheme(),
        MediaDisplayPage::kQuickSettingsMediaView));

    EXPECT_EQ(footer_ptr, item_ui_->footer_view_for_testing());
    EXPECT_EQ(device_selector_ptr,
              item_ui_->device_selector_view_for_testing());

    observer_ = std::make_unique<
        NiceMock<global_media_controls::test::MockMediaItemUIObserver>>();
    item_ui_->AddObserver(observer_.get());

    SimulateMediaSessionData();

    widget_->Show();
  }

  void TearDown() override {
    item_ui_->RemoveObserver(observer_.get());
    item_ui_ = nullptr;
    widget_->Close();
    ViewsTestBase::TearDown();
  }

  void SimulateHoverOverItemUI() {
    fake_screen_.set_cursor_screen_point(
        item_ui_->GetBoundsInScreen().CenterPoint());

    ui::MouseEvent event(ui::EventType::kMouseEntered, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(), 0, 0);
    item_ui_->OnMouseEntered(event);
  }

  void SimulateNotHoveringOverItemUI() {
    gfx::Rect container_bounds = item_ui_->GetBoundsInScreen();
    gfx::Point point_outside_container =
        container_bounds.bottom_right() + gfx::Vector2d(1, 1);
    fake_screen_.set_cursor_screen_point(point_outside_container);

    ui::MouseEvent event(ui::EventType::kMouseExited, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(), 0, 0);
    item_ui_->OnMouseExited(event);
  }

  void SimulateItemUIClicked() {
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(item_ui_).NotifyClick(event);
  }

  void SimulateHeaderClicked() {
    item_ui_->OnHeaderClicked(/*activate_original_media=*/false);
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

  MediaItemUIDetailedView* GetView() { return item_ui()->view_for_testing(); }

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<MediaItemUIView> item_ui_ = nullptr;
  std::unique_ptr<global_media_controls::test::MockMediaItemUIObserver>
      observer_;
  std::unique_ptr<media_message_center::test::MockMediaNotificationItem> item_;

  // Set of actions currently enabled.
  base::flat_set<MediaSessionAction> actions_;

  display::test::TestScreen fake_screen_;
};

TEST_F(MediaItemUIViewTest, SendsMetadataUpdates) {
  EXPECT_CALL(observer(), OnMediaItemUIMetadataChanged());
  SimulateMetadataChanged();
}

TEST_F(MediaItemUIViewTest, SendsDestroyedUpdates) {
  auto container = std::make_unique<MediaItemUIView>(
      kOtherTestNotificationId, notification_item(), nullptr, nullptr,
      media_message_center::MediaColorTheme(),
      MediaDisplayPage::kQuickSettingsMediaView);
  global_media_controls::test::MockMediaItemUIObserver observer;
  container->AddObserver(&observer);

  // When the container is destroyed, it should notify the observer.
  EXPECT_CALL(observer, OnMediaItemUIDestroyed(kOtherTestNotificationId));
  container.reset();
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(MediaItemUIViewTest, SendsClicks) {
  // When the container is clicked directly, it should notify its observers.
  EXPECT_CALL(observer(),
              OnMediaItemUIClicked(kTestNotificationId,
                                   /*activate_original_media=*/false));
  SimulateItemUIClicked();
  testing::Mock::VerifyAndClearExpectations(&observer());

  // It should also notify its observers when the header is clicked.
  EXPECT_CALL(observer(),
              OnMediaItemUIClicked(kTestNotificationId,
                                   /*activate_original_media=*/false));
  SimulateHeaderClicked();
  testing::Mock::VerifyAndClearExpectations(&observer());
}

TEST_F(MediaItemUIViewTest, MetadataTest) {
  auto container_view = std::make_unique<MediaItemUIView>(
      kOtherTestNotificationId, notification_item(), nullptr, nullptr,
      media_message_center::MediaColorTheme(),
      MediaDisplayPage::kQuickSettingsMediaView);
  views::test::TestViewMetadata(container_view.get());
}

TEST_F(MediaItemUIViewTest, UpdateView) {
  auto footer_view = std::make_unique<NiceMock<test::MockMediaItemUIFooter>>();
  auto* footer_ptr = footer_view.get();
  item_ui()->UpdateFooterView(std::move(footer_view));
  EXPECT_EQ(footer_ptr, item_ui()->footer_view_for_testing());

  auto device_selector_view =
      std::make_unique<NiceMock<test::MockMediaItemUIDeviceSelector>>();
  auto* device_selector_ptr = device_selector_view.get();
  item_ui()->UpdateDeviceSelector(std::move(device_selector_view));
  EXPECT_EQ(device_selector_ptr, item_ui()->device_selector_view_for_testing());
}

}  // namespace global_media_controls
