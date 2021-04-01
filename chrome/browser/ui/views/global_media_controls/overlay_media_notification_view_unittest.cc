// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/overlay_media_notification_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/global_media_controls/overlay_media_notifications_manager.h"
#include "chrome/browser/ui/global_media_controls/test_helper.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

const char kTestNotificationId[] = "testid";

}  // anonymous namespace

class MockOverlayMediaNotificationsManager final
    : public OverlayMediaNotificationsManager {
 public:
  MockOverlayMediaNotificationsManager() = default;
  ~MockOverlayMediaNotificationsManager() = default;

  MOCK_METHOD1(OnOverlayNotificationClosed, void(const std::string& id));
};

class OverlayMediaNotificationViewTest : public ChromeViewsTestBase {
 public:
  OverlayMediaNotificationViewTest() = default;
  ~OverlayMediaNotificationViewTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    feature_list_.InitAndEnableFeature(
        media::kGlobalMediaControlsOverlayControls);

    manager_ = std::make_unique<MockOverlayMediaNotificationsManager>();
    item_ = std::make_unique<MockMediaNotificationItem>();

    auto notification = std::make_unique<MediaNotificationContainerImplView>(
        kTestNotificationId, item_->GetWeakPtr(), nullptr,
        GlobalMediaControlsEntryPoint::kToolbarIcon);
    notification->PopOut();

    overlay_ = std::make_unique<OverlayMediaNotificationView>(
        kTestNotificationId, std::move(notification),
        gfx::Rect(notification->GetPreferredSize()), GetContext());

    overlay_->SetManager(manager_.get());
    overlay_->ShowNotification();
  }

  void TearDown() override {
    overlay_.reset();
    manager_.reset();
    ViewsTestBase::TearDown();
  }

  void SimulateTitleChange(const std::u16string title) {
    media_session::MediaMetadata metadata;
    metadata.source_title = u"source_title";
    metadata.title = title;
    metadata.artist = u"artist";
    GetView()->UpdateWithMediaMetadata(metadata);
  }

  void SimulateExpandStateChanged(bool expand) {
    overlay_->notification_for_testing()->OnExpanded(expand);
  }

  void SimulateMouseDrag(const gfx::Vector2d drag_distance) {
    gfx::Point start_point = GetContainer()->bounds().CenterPoint();
    gfx::Point end_point = start_point + drag_distance;

    GetContainer()->OnMousePressed(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, start_point, start_point,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    GetContainer()->OnMouseDragged(
        ui::MouseEvent(ui::ET_MOUSE_DRAGGED, end_point, end_point,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  std::u16string GetWindowTitle() {
    return overlay_->widget_delegate()->GetWindowTitle();
  }

  gfx::Size GetWindowSize() {
    return overlay_->GetWindowBoundsInScreen().size();
  }

  MediaNotificationContainerImplView* GetContainer() {
    return overlay_->notification_for_testing();
  }

  OverlayMediaNotificationView* GetOverlay() { return overlay_.get(); }

 private:
  media_message_center::MediaNotificationViewImpl* GetView() {
    return overlay_->notification_for_testing()->view_for_testing();
  }

  std::unique_ptr<MockOverlayMediaNotificationsManager> manager_;
  std::unique_ptr<OverlayMediaNotificationView> overlay_;
  std::unique_ptr<MockMediaNotificationItem> item_;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(OverlayMediaNotificationViewTest, TaskBarTitle) {
  std::u16string title1 = u"test";
  SimulateTitleChange(title1);
  EXPECT_EQ(GetWindowTitle(), title1);

  std::u16string title2 = u"title";
  SimulateTitleChange(title2);
  EXPECT_EQ(GetWindowTitle(), title2);
}

TEST_F(OverlayMediaNotificationViewTest, ResizeOnExpandStateChanged) {
  constexpr int kExpandedHeight = 150;
  constexpr int kNormalHeight = 100;

  EXPECT_EQ(kNormalHeight, GetWindowSize().height());

  SimulateExpandStateChanged(true);
  EXPECT_EQ(kExpandedHeight, GetWindowSize().height());

  SimulateExpandStateChanged(false);
  EXPECT_EQ(kNormalHeight, GetWindowSize().height());
}

TEST_F(OverlayMediaNotificationViewTest, Dragging) {
  gfx::Point start_position = GetOverlay()->GetWindowBoundsInScreen().origin();
  gfx::Vector2d drag_distance(100, 100);

  SimulateMouseDrag(drag_distance);

  EXPECT_EQ(GetOverlay()->GetWindowBoundsInScreen().origin(),
            start_position + drag_distance);
}
