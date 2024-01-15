// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "chrome/services/sharing/nearby/decoder/advertisement_decoder.h"

#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/public/cpp/advertisement.h"
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

class AdvertisementDecoderTest : public testing::Test {
 public:
  AdvertisementDecoderTest() = default;
  ~AdvertisementDecoderTest() override = default;
};

void ExpectEquals(const Advertisement& self, const Advertisement& other) {
  EXPECT_EQ(self.version(), other.version());
  EXPECT_EQ(self.HasDeviceName(), other.HasDeviceName());
  EXPECT_EQ(self.device_name(), other.device_name());
  EXPECT_EQ(self.salt(), other.salt());
  EXPECT_EQ(self.encrypted_metadata_key(), other.encrypted_metadata_key());
  EXPECT_EQ(self.device_type(), other.device_type());
}

struct DeviceTypeTestData {
  nearby_share::mojom::ShareTargetType device_type;
} kDeviceTypeTestData[] = {{nearby_share::mojom::ShareTargetType::kUnknown},
                           {nearby_share::mojom::ShareTargetType::kPhone},
                           {nearby_share::mojom::ShareTargetType::kLaptop},
                           {nearby_share::mojom::ShareTargetType::kTablet}};

class AdvertisementDecoderDeviceTypeTest
    : public AdvertisementDecoderTest,
      public testing::WithParamInterface<DeviceTypeTestData> {};

}  // namespace

TEST_P(AdvertisementDecoderDeviceTypeTest, CreateNewInstanceFromEndpointInfo) {
  std::unique_ptr<sharing::Advertisement> original =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          GetParam().device_type, kDeviceName);
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::AdvertisementDecoder::FromEndpointInfo(
          original->ToEndpointInfo());
  ExpectEquals(*original, *advertisement);
}

INSTANTIATE_TEST_SUITE_P(AdvertisementDecoderTest,
                         AdvertisementDecoderDeviceTypeTest,
                         testing::ValuesIn(kDeviceTypeTestData));

TEST(AdvertisementDecoderTest, CreateNewInstanceFromStringWithExtraLength) {
  std::unique_ptr<sharing::Advertisement> original =
      sharing::Advertisement::NewInstance(
          kSalt, kEncryptedMetadataKey, kDeviceType,
          base::StrCat({kDeviceName, "123456"}));
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::AdvertisementDecoder::FromEndpointInfo(
          original->ToEndpointInfo());
  ExpectEquals(*original, *advertisement);
}

TEST(AdvertisementDecoderTest,
     SerializeContactsOnlyAdvertisementWithoutDeviceName) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceType,
                                          /* device_name= */ std::nullopt);
  ExpectEquals(*advertisement, *sharing::AdvertisementDecoder::FromEndpointInfo(
                                   advertisement->ToEndpointInfo()));
}

TEST(AdvertisementDecoderTest,
     SerializeVisibleToEveryoneAdvertisementWithoutDeviceName) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceType,
                                          /* device_name= */ std::string());
  EXPECT_FALSE(sharing::AdvertisementDecoder::FromEndpointInfo(
      advertisement->ToEndpointInfo()));
}

TEST(AdvertisementDecoderTest, V1ContactsOnlyAdvertisementDecoding) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceType, kDeviceName);
  std::vector<uint8_t> v1EndpointInfo = {
      18, 0, 0, 0,  0,   0,   0,   0,   0,  0,   0,  0,  0,   0,
      0,  0, 0, 10, 100, 101, 118, 105, 99, 101, 78, 97, 109, 101};
  ExpectEquals(*advertisement, *sharing::AdvertisementDecoder::FromEndpointInfo(
                                   v1EndpointInfo));
}

TEST(AdvertisementDecoderTest, V1VisibleToEveryoneAdvertisementDecoding) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceType, kDeviceName);
  std::vector<uint8_t> v1EndpointInfo = {
      2, 0, 0, 0,  0,   0,   0,   0,   0,  0,   0,  0,  0,   0,
      0, 0, 0, 10, 100, 101, 118, 105, 99, 101, 78, 97, 109, 101};
  ExpectEquals(*advertisement, *sharing::AdvertisementDecoder::FromEndpointInfo(
                                   v1EndpointInfo));
}

TEST(AdvertisementDecoderTest, V1ContactsOnlyAdvertisementEncoding) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceType,
                                          /* device_name= */ std::nullopt);
  std::vector<uint8_t> v1EndpointInfo = {18, 0, 0, 0, 0, 0, 0, 0, 0,
                                         0,  0, 0, 0, 0, 0, 0, 0};
  ExpectEquals(*advertisement, *sharing::AdvertisementDecoder::FromEndpointInfo(
                                   v1EndpointInfo));
}

TEST(AdvertisementDecoderTest, V1VisibleToEveryoneAdvertisementEncoding) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceType, kDeviceName);
  std::vector<uint8_t> v1EndpointInfo = {
      2, 0, 0, 0,  0,   0,   0,   0,   0,  0,   0,  0,  0,   0,
      0, 0, 0, 10, 100, 101, 118, 105, 99, 101, 78, 97, 109, 101};
  ExpectEquals(*advertisement, *sharing::AdvertisementDecoder::FromEndpointInfo(
                                   v1EndpointInfo));
}

TEST(AdvertisementDecoderTest, InvalidDeviceNameEncoding) {
  std::vector<uint8_t> v1EndpointInfo = {
      2, 0, 0, 0,  0,   0,  0,   0,   0,  0,   0,  0,  0,   0,
      0, 0, 0, 10, 226, 40, 161, 105, 99, 101, 78, 97, 109, 101,
  };
  EXPECT_FALSE(sharing::AdvertisementDecoder::FromEndpointInfo(v1EndpointInfo));
}

}  // namespace sharing
