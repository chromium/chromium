// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_list_view.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "components/media_message_center/mock_media_notification_item.h"
#include "ui/views/test/views_test_base.h"

using testing::NiceMock;

namespace global_media_controls {

namespace {

// Test IDs for items.
const char kTestItemId1[] = "testid1";
const char kTestItemId2[] = "testid2";
const char kTestItemId3[] = "testid3";

}  // anonymous namespace

class MediaItemUIListViewTest : public views::ViewsTestBase {
 public:
  MediaItemUIListViewTest() = default;
  MediaItemUIListViewTest(const MediaItemUIListViewTest&) = delete;
  MediaItemUIListViewTest& operator=(const MediaItemUIListViewTest&) = delete;
  ~MediaItemUIListViewTest() override = default;

  // views::ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    widget_ = CreateTestWidget();

    list_view_ =
        widget_->SetContentsView(std::make_unique<MediaItemUIListView>());

    item_ = std::make_unique<
        NiceMock<media_message_center::test::MockMediaNotificationItem>>();
    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    views::ViewsTestBase::TearDown();
  }

  void ShowItem(const std::string& id) {
    list_view_->ShowItem(id, std::make_unique<MediaItemUIView>(
                                 id, item_->GetWeakPtr(), nullptr, nullptr));
  }

  void HideItem(const std::string& id) { list_view_->HideItem(id); }

  MediaItemUIListView* list_view() { return list_view_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<MediaItemUIListView> list_view_ = nullptr;
  std::unique_ptr<media_message_center::test::MockMediaNotificationItem> item_;
};

TEST_F(MediaItemUIListViewTest, NoSeparatorForOneItem) {
  // Show a single item.
  ShowItem(kTestItemId1);

  // There should be just one item.
  EXPECT_EQ(1u, list_view()->items_for_testing().size());

  // Since there's only one, there should be no separator line.
  EXPECT_EQ(nullptr,
            list_view()->items_for_testing().at(kTestItemId1)->GetBorder());
}

TEST_F(MediaItemUIListViewTest, SeparatorBetweenItems) {
  // Show two items.
  ShowItem(kTestItemId1);
  ShowItem(kTestItemId2);

  // There should be two items.
  EXPECT_EQ(2u, list_view()->items_for_testing().size());

  // There should be a separator between them. Since the separators are
  // top-sided, the bottom item should have one.
  EXPECT_EQ(nullptr,
            list_view()->items_for_testing().at(kTestItemId1)->GetBorder());
  EXPECT_NE(nullptr,
            list_view()->items_for_testing().at(kTestItemId2)->GetBorder());
}

TEST_F(MediaItemUIListViewTest, SeparatorRemovedWhenItemRemoved) {
  // Show three items.
  ShowItem(kTestItemId1);
  ShowItem(kTestItemId2);
  ShowItem(kTestItemId3);

  // There should be three items.
  EXPECT_EQ(3u, list_view()->items_for_testing().size());

  // There should be separators.
  EXPECT_EQ(nullptr,
            list_view()->items_for_testing().at(kTestItemId1)->GetBorder());
  EXPECT_NE(nullptr,
            list_view()->items_for_testing().at(kTestItemId2)->GetBorder());
  EXPECT_NE(nullptr,
            list_view()->items_for_testing().at(kTestItemId3)->GetBorder());

  // Remove the topmost item.
  HideItem(kTestItemId1);

  // There should be two items.
  EXPECT_EQ(2u, list_view()->items_for_testing().size());

  // The new top item should have lost its top separator.
  EXPECT_EQ(nullptr,
            list_view()->items_for_testing().at(kTestItemId2)->GetBorder());
  EXPECT_NE(nullptr,
            list_view()->items_for_testing().at(kTestItemId3)->GetBorder());
}

}  // namespace global_media_controls
