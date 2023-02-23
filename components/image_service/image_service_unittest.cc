// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_service/image_service.h"
#include <memory>

#include "build/build_config.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace image_service {

class ImageServiceTest : public testing::Test {
 public:
  ImageServiceTest() = default;

  void SetUp() override {
    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    image_service_ =
        std::make_unique<ImageService>(nullptr, test_sync_service_.get());
  }

  ImageServiceTest(const ImageServiceTest&) = delete;
  ImageServiceTest& operator=(const ImageServiceTest&) = delete;

 protected:
  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  std::unique_ptr<ImageService> image_service_;
};

TEST_F(ImageServiceTest, HasPermissionToFetchImage) {
  test_sync_service_->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  test_sync_service_->FireStateChanged();

  EXPECT_FALSE(
      image_service_->HasPermissionToFetchImage(mojom::ClientId::Journeys));
  EXPECT_FALSE(image_service_->HasPermissionToFetchImage(
      mojom::ClientId::JourneysSidePanel));
  EXPECT_FALSE(
      image_service_->HasPermissionToFetchImage(mojom::ClientId::NtpRealbox));
  EXPECT_FALSE(
      image_service_->HasPermissionToFetchImage(mojom::ClientId::NtpQuests));
  EXPECT_FALSE(
      image_service_->HasPermissionToFetchImage(mojom::ClientId::Bookmarks));

  test_sync_service_->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet(
          syncer::UserSelectableType::kHistory));
  test_sync_service_->FireStateChanged();

  EXPECT_TRUE(
      image_service_->HasPermissionToFetchImage(mojom::ClientId::Journeys));
  EXPECT_TRUE(image_service_->HasPermissionToFetchImage(
      mojom::ClientId::JourneysSidePanel));
  EXPECT_FALSE(
      image_service_->HasPermissionToFetchImage(mojom::ClientId::NtpRealbox));
  EXPECT_TRUE(
      image_service_->HasPermissionToFetchImage(mojom::ClientId::NtpQuests));
  EXPECT_FALSE(
      image_service_->HasPermissionToFetchImage(mojom::ClientId::Bookmarks));
}

}  // namespace image_service
