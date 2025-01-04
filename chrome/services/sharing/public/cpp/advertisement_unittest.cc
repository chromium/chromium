// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/public/cpp/advertisement.h"

#include <stdint.h>

#include <array>
#include <memory>
#include <string>

#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sharing {

namespace {

constexpr char kDeviceName[] = "deviceName";
// Salt for advertisement.
constexpr std::array<uint8_t, Advertisement::kSaltSize> kSalt = {};
// Key for encrypting personal info metadata.
constexpr std::array<uint8_t, Advertisement::kMetadataEncryptionKeyHashByteSize>
    kEncryptedMetadataKey = {};
constexpr nearby_share::mojom::ShareTargetType kDeviceType =
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

}  // namespace sharing
