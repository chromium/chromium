// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_list_view.h"

#include <memory>
#include <string>

#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl_view.h"
#include "chrome/test/views/chrome_views_test_base.h"

namespace {

// Test IDs for notifications.
const char kTestNotificationId1[] = "testid1";
const char kTestNotificationId2[] = "testid2";
const char kTestNotificationId3[] = "testid3";

}  // anonymous namespace

class MediaNotificationListViewTest : public ChromeViewsTestBase {
 public:
  MediaNotificationListViewTest() = default;
  ~MediaNotificationListViewTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = CreateTestWidget();

    list_view_ =
        widget_->SetContentsView(std::make_unique<MediaNotificationListView>());

    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  void ShowNotification(const std::string& id) {
    list_view_->ShowNotification(
        id, std::make_unique<MediaNotificationContainerImplView>(id, nullptr,
                                                                 nullptr));
  }

  void HideNotification(const std::string& id) {
    list_view_->HideNotification(id);
  }

  MediaNotificationListView* list_view() { return list_view_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  MediaNotificationListView* list_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationListViewTest);
};

TEST_F(MediaNotificationListViewTest, NoSeparatorForOneNotification) {
  // Show a single notification.
  ShowNotification(kTestNotificationId1);

  // There should be just one notification.
  EXPECT_EQ(1u, list_view()->notifications_for_testing().size());

  // Since there's only one, there should be no separator line.
  EXPECT_EQ(nullptr, list_view()
                         ->notifications_for_testing()
                         .at(kTestNotificationId1)
                         ->border());
}

TEST_F(MediaNotificationListViewTest, SeparatorBetweenNotifications) {
  // Show two notifications.
  ShowNotification(kTestNotificationId1);
  ShowNotification(kTestNotificationId2);

  // There should be two notifications.
  EXPECT_EQ(2u, list_view()->notifications_for_testing().size());

  // There should be a separator between them. Since the separators are
  // top-sided, the bottom notification should have one.
  EXPECT_EQ(nullptr, list_view()
                         ->notifications_for_testing()
                         .at(kTestNotificationId1)
                         ->border());
  EXPECT_NE(nullptr, list_view()
                         ->notifications_for_testing()
                         .at(kTestNotificationId2)
                         ->border());
}

TEST_F(MediaNotificationListViewTest, SeparatorRemovedWhenNotificationRemoved) {
  // Show three notifications.
  ShowNotification(kTestNotificationId1);
  ShowNotification(kTestNotificationId2);
  ShowNotification(kTestNotificationId3);

  // There should be three notifications.
  EXPECT_EQ(3u, list_view()->notifications_for_testing().size());

  // There should be separators.
  EXPECT_EQ(nullptr, list_view()
                         ->notifications_for_testing()
                         .at(kTestNotificationId1)
                         ->border());
  EXPECT_NE(nullptr, list_view()
                         ->notifications_for_testing()
                         .at(kTestNotificationId2)
                         ->border());
  EXPECT_NE(nullptr, list_view()
                         ->notifications_for_testing()
                         .at(kTestNotificationId3)
                         ->border());

  // Remove the topmost notification.
  HideNotification(kTestNotificationId1);

  // There should be two notifications.
  EXPECT_EQ(2u, list_view()->notifications_for_testing().size());

  // The new top notification should have lost its top separator.
  EXPECT_EQ(nullptr, list_view()
                         ->notifications_for_testing()
                         .at(kTestNotificationId2)
                         ->border());
  EXPECT_NE(nullptr, list_view()
                         ->notifications_for_testing()
                         .at(kTestNotificationId3)
                         ->border());
}
