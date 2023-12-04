// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/camera_roll_item.h"

#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
namespace phonehub {

namespace {

const gfx::Image CreateTestImage() {
  gfx::ImageSkia image_skia = gfx::test::CreateImageSkia(/*size=*/1);
  image_skia.MakeThreadSafe();
  return gfx::Image(image_skia);
}

}  // namespace

class CameraRollItemTest : public testing::Test {
 protected:
  CameraRollItemTest() = default;
  CameraRollItemTest(const CameraRollItemTest&) = delete;
  CameraRollItemTest& operator=(const CameraRollItemTest&) = delete;
  ~CameraRollItemTest() override = default;
};

TEST_F(CameraRollItemTest, ItemsMatch) {
  proto::CameraRollItemMetadata metadata_1;
  metadata_1.set_key("key1");
  metadata_1.set_mime_type("image/png");
  metadata_1.set_last_modified_millis(123456789L);
  metadata_1.set_file_size_bytes(123456789L);
  metadata_1.set_file_name("FakeImage.png");

  proto::CameraRollItemMetadata metadata_2;
  metadata_2.set_key("key1");
  metadata_2.set_mime_type("image/png");
  metadata_2.set_last_modified_millis(123456789L);
  metadata_2.set_file_size_bytes(123456789L);
  metadata_2.set_file_name("FakeImage.png");

  CameraRollItem item_1(metadata_1, CreateTestImage());
  CameraRollItem item_2(metadata_2, CreateTestImage());

  EXPECT_TRUE(item_1 == item_2);
  EXPECT_FALSE(item_1 != item_2);
}

TEST_F(CameraRollItemTest, ItemsDoNotMatch) {
  proto::CameraRollItemMetadata metadata_1;
  metadata_1.set_key("key1");
  metadata_1.set_mime_type("image/png");
  metadata_1.set_last_modified_millis(123456789L);
  metadata_1.set_file_size_bytes(123456789L);
  metadata_1.set_file_name("FakeImage.png");

  proto::CameraRollItemMetadata metadata_2;
  metadata_2.set_key("key2");
  metadata_2.set_mime_type("video/mp4");
  metadata_2.set_last_modified_millis(987654321L);
  metadata_2.set_file_size_bytes(987654321L);
  metadata_2.set_file_name("FakeVideo.mp4");

  CameraRollItem item_1(metadata_1, CreateTestImage());
  CameraRollItem item_2(metadata_2, CreateTestImage());

  EXPECT_FALSE(item_1 == item_2);
  EXPECT_TRUE(item_1 != item_2);
}

}  // namespace phonehub
}  // namespace ash
