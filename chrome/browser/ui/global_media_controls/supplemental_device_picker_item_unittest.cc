// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/supplemental_device_picker_item.h"

#include "base/unguessable_token.h"
#include "components/global_media_controls/public/test/mock_media_item_manager.h"
#include "components/media_message_center/mock_media_notification_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

using testing::_;
using testing::Mock;
using testing::WithArg;

class SupplementalDevicePickerItemTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    item_.SetView(&view_);
  }

 protected:
  global_media_controls::test::MockMediaItemManager item_manager_;
  base::UnguessableToken source_id_{base::UnguessableToken::Create()};
  media_message_center::test::MockMediaNotificationView view_;
  SupplementalDevicePickerItem item_{&item_manager_, source_id_};
};

TEST_F(SupplementalDevicePickerItemTest, GetInfo) {
  EXPECT_EQ(media_message_center::SourceType::kPresentationRequest,
            item_.GetSourceType());
  EXPECT_FALSE(item_.RequestMediaRemoting());
  EXPECT_EQ(source_id_, item_.GetSourceId());
}

TEST_F(SupplementalDevicePickerItemTest, Dismiss) {
  EXPECT_CALL(item_manager_, HideItem(item_.id()));
  item_.Dismiss();
  // HideItem() gets called by `item_`'s dtor as well, so we check that it's
  // been called before we call the dtor.
  Mock::VerifyAndClearExpectations(&item_manager_);
}

TEST_F(SupplementalDevicePickerItemTest, UpdateViewWithMetadata) {
  media_session::MediaMetadata metadata;
  metadata.title = u"Title";
  EXPECT_CALL(view_, UpdateWithMediaMetadata(metadata));
  item_.UpdateViewWithMetadata(metadata);
}

TEST_F(SupplementalDevicePickerItemTest, UpdateViewWithArtworkImage) {
  gfx::ImageSkia image = gfx::test::CreateImageSkia(100, 100);
  EXPECT_CALL(view_, UpdateWithMediaArtwork(_));
  item_.UpdateViewWithArtworkImage(image);
}

TEST_F(SupplementalDevicePickerItemTest, UpdateViewWithFaviconImage) {
  gfx::ImageSkia image = gfx::test::CreateImageSkia(100, 100);
  EXPECT_CALL(view_, UpdateWithFavicon(_));
  item_.UpdateViewWithFaviconImage(image);
}
