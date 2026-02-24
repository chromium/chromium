// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_list_view.h"

#include "components/global_media_controls/public/views/media_item_ui_detailed_view.h"
#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "components/media_message_center/mock_media_notification_item.h"
#include "ui/views/test/views_test_base.h"

using testing::NiceMock;

namespace global_media_controls {

namespace {

// Test IDs for items.
const char kTestItemId1[] = "testid1";
const char kTestItemId2[] = "testid2";

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

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

    list_view_ =
        widget_->SetContentsView(std::make_unique<MediaItemUIListView>());

    item_ = std::make_unique<
        NiceMock<media_message_center::test::MockMediaNotificationItem>>();
    widget_->Show();
  }

  void TearDown() override {
    list_view_ = nullptr;
    widget_->Close();
    views::ViewsTestBase::TearDown();
  }

  void ShowItem(const std::string& id) {
#if BUILDFLAG(IS_CHROMEOS)
    list_view_->ShowItem(id, std::make_unique<MediaItemUIView>(
                                 id, item_->GetWeakPtr(), nullptr, nullptr,
                                 media_message_center::MediaColorTheme(),
                                 MediaDisplayPage::kQuickSettingsMediaView));
#else
      list_view_->ShowUpdatedItem(
          id, std::make_unique<MediaItemUIUpdatedView>(
                  id, item_->GetWeakPtr(),
                  media_message_center::MediaColorTheme(), nullptr, nullptr));
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void HideItem(const std::string& id) {
#if BUILDFLAG(IS_CHROMEOS)
    list_view_->HideItem(id);
#else
    list_view_->HideUpdatedItem(id);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  int GetItemsSize() {
#if BUILDFLAG(IS_CHROMEOS)
    return list_view()->items_for_testing().size();
#else
    return list_view()->updated_items_for_testing().size();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  MediaItemUIListView* list_view() { return list_view_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<MediaItemUIListView> list_view_ = nullptr;
  std::unique_ptr<media_message_center::test::MockMediaNotificationItem> item_;
};

TEST_F(MediaItemUIListViewTest, ShowItems) {
  ShowItem(kTestItemId1);
  ShowItem(kTestItemId2);
  EXPECT_EQ(2, GetItemsSize());
}

}  // namespace global_media_controls
