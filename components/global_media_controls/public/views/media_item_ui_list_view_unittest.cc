// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_list_view.h"

#include "base/test/scoped_feature_list.h"
#include "components/global_media_controls/public/views/media_item_ui_detailed_view.h"
#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "components/media_message_center/mock_media_notification_item.h"
#include "media/base/media_switches.h"
#include "ui/views/test/views_test_base.h"

using testing::NiceMock;

namespace global_media_controls {

namespace {

// Test IDs for items.
const char kTestItemId1[] = "testid1";
const char kTestItemId2[] = "testid2";
const char kTestItemId3[] = "testid3";

}  // anonymous namespace

class MediaItemUIListViewTest : public views::ViewsTestBase,
                                public testing::WithParamInterface<bool> {
 public:
  MediaItemUIListViewTest() = default;
  MediaItemUIListViewTest(const MediaItemUIListViewTest&) = delete;
  MediaItemUIListViewTest& operator=(const MediaItemUIListViewTest&) = delete;
  ~MediaItemUIListViewTest() override = default;

  // views::ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();

#if !BUILDFLAG(IS_CHROMEOS)
    feature_list_.InitWithFeatureState(media::kGlobalMediaControlsUpdatedUI,
                                       UseUpdatedUI());
#endif

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

  bool UseUpdatedUI() {
#if BUILDFLAG(IS_CHROMEOS)
    return true;
#else
    return GetParam();
#endif
  }

  void ShowItem(const std::string& id) {
    if (UseUpdatedUI()) {
#if BUILDFLAG(IS_CHROMEOS)
      list_view_->ShowItem(
          id, std::make_unique<MediaItemUIView>(
                  id, item_->GetWeakPtr(), nullptr, nullptr, std::nullopt,
                  media_message_center::MediaColorTheme(),
                  MediaDisplayPage::kQuickSettingsMediaView));
#else
      list_view_->ShowUpdatedItem(
          id, std::make_unique<MediaItemUIUpdatedView>(
                  id, item_->GetWeakPtr(),
                  media_message_center::MediaColorTheme(), nullptr, nullptr));
#endif
    } else {
      list_view_->ShowItem(id, std::make_unique<MediaItemUIView>(
                                   id, item_->GetWeakPtr(), nullptr, nullptr));
    }
  }

  void HideItem(const std::string& id) {
#if !BUILDFLAG(IS_CHROMEOS)
    if (UseUpdatedUI()) {
      list_view_->HideUpdatedItem(id);
      return;
    }
#endif
    list_view_->HideItem(id);
  }

  int GetItemsSize() {
#if !BUILDFLAG(IS_CHROMEOS)
    if (UseUpdatedUI()) {
      return list_view()->updated_items_for_testing().size();
    }
#endif
    return list_view()->items_for_testing().size();
  }

  MediaItemUIListView* list_view() { return list_view_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<MediaItemUIListView> list_view_ = nullptr;
  std::unique_ptr<media_message_center::test::MockMediaNotificationItem> item_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(GlobalMediaControlsUpdatedUI,
                         MediaItemUIListViewTest,
                         testing::Bool());

TEST_P(MediaItemUIListViewTest, NoSeparatorForOneItem) {
  // Show a single item.
  ShowItem(kTestItemId1);
  EXPECT_EQ(1, GetItemsSize());

  if (!UseUpdatedUI()) {
    // Since there's only one, there should be no separator line.
    EXPECT_EQ(nullptr, list_view()->GetItem(kTestItemId1)->GetBorder());
  }
}

TEST_P(MediaItemUIListViewTest, SeparatorBetweenItems) {
  // Show two items.
  ShowItem(kTestItemId1);
  ShowItem(kTestItemId2);
  EXPECT_EQ(2, GetItemsSize());

  if (!UseUpdatedUI()) {
    // There should be a separator between them. Since the separators are
    // top-sided, the bottom item should have one.
    EXPECT_EQ(nullptr, list_view()->GetItem(kTestItemId1)->GetBorder());
    EXPECT_NE(nullptr, list_view()->GetItem(kTestItemId2)->GetBorder());
  }
}

TEST_P(MediaItemUIListViewTest, SeparatorRemovedWhenItemRemoved) {
  // Show three items.
  ShowItem(kTestItemId1);
  ShowItem(kTestItemId2);
  ShowItem(kTestItemId3);
  EXPECT_EQ(3, GetItemsSize());

  if (!UseUpdatedUI()) {
    // There should be separators.
    EXPECT_EQ(nullptr, list_view()->GetItem(kTestItemId1)->GetBorder());
    EXPECT_NE(nullptr, list_view()->GetItem(kTestItemId2)->GetBorder());
    EXPECT_NE(nullptr, list_view()->GetItem(kTestItemId3)->GetBorder());
  }

  // Remove the topmost item.
  HideItem(kTestItemId1);
  EXPECT_EQ(2, GetItemsSize());

  if (!UseUpdatedUI()) {
    // The new top item should have lost its top separator.
    EXPECT_EQ(nullptr, list_view()->GetItem(kTestItemId2)->GetBorder());
    EXPECT_NE(nullptr, list_view()->GetItem(kTestItemId3)->GetBorder());
  }
}

}  // namespace global_media_controls
