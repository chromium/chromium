// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "chrome/services/sharing/public/cpp/advertisement.h"

#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sharing {

namespace {

const char kDeviceName[] = "deviceName";
// Salt for advertisement.
const std::vector<uint8_t> kSalt(Advertisement::kSaltSize, 0);
// Key for encrypting personal info metadata.
static const std::vector<uint8_t> kEncryptedMetadataKey(
    Advertisement::kMetadataEncryptionKeyHashByteSize,
    0);
const nearby_share::mojom::ShareTargetType kDeviceType =
    nearby_share::mojom::ShareTargetType::kPhone;

}  // namespace

TEST(AdvertisementTest, CreateNewInstanceWithNullName) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceType,
                                          /* device_name=*/std::nullopt);
  EXPECT_FALSE(advertisement->device_name());
  EXPECT_EQ(kEncryptedMetadataKey, advertisement->encrypted_metadata_key());
  EXPECT_EQ(kDeviceType, advertisement->device_type());
  EXPECT_FALSE(advertisement->HasDeviceName());
  EXPECT_EQ(kSalt, advertisement->salt());
}

TEST(AdvertisementTest, CreateNewInstance) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceType, kDeviceName);
  EXPECT_EQ(kDeviceName, advertisement->device_name());
  EXPECT_EQ(kEncryptedMetadataKey, advertisement->encrypted_metadata_key());
  EXPECT_EQ(kDeviceType, advertisement->device_type());
  EXPECT_TRUE(advertisement->HasDeviceName());
  EXPECT_EQ(kSalt, advertisement->salt());
}

TEST(AdvertisementTest, CreateNewInstanceWithWrongSaltSize) {
  EXPECT_FALSE(sharing::Advertisement::NewInstance(
      /* salt= */ std::vector<uint8_t>(5, 5), kEncryptedMetadataKey,
      kDeviceType, kDeviceName));
}

TEST(AdvertisementTest, CreateNewInstanceWithWrongAccountIdentifierSize) {
  EXPECT_FALSE(sharing::Advertisement::NewInstance(
      kSalt, /* encrypted_metadata_key= */ std::vector<uint8_t>(2, 1),
      kDeviceType, kDeviceName));
}

}  // namespace sharing
