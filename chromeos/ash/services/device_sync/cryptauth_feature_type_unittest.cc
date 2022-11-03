// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_feature_type.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace device_sync {

namespace {

// The base64url encoded, SHA-256 8-byte hash of "BETTER_TOGETHER_HOST" and
// "EASY_UNLOCK_HOST".
const char kBetterTogetherHostEncodedHash[] = "vA7vs_-9Ayo";
const char kEasyUnlockHostEncodedHash[] = "n_ekjyVWntQ";

}  // namespace

TEST(DeviceSyncCryptAuthFeatureTypeTest, ToAndFromString) {
  for (CryptAuthFeatureType feature_type : GetAllCryptAuthFeatureTypes()) {
    EXPECT_EQ(feature_type, CryptAuthFeatureTypeFromString(
                                CryptAuthFeatureTypeToString(feature_type)));
  }
}

TEST(DeviceSyncCryptAuthFeatureTypeTest, ToAndFromHash) {
  for (CryptAuthFeatureType feature_type : GetAllCryptAuthFeatureTypes()) {
    EXPECT_EQ(feature_type, CryptAuthFeatureTypeFromGcmHash(
                                CryptAuthFeatureTypeToGcmHash(feature_type)));
  }
}

TEST(DeviceSyncCryptAuthFeatureTypeTest, ToAndFromSoftwareFeature) {
  // SoftwareFeatures map onto the "enabled" feature types.
  for (CryptAuthFeatureType feature_type : GetEnabledCryptAuthFeatureTypes()) {
    EXPECT_EQ(feature_type,
              CryptAuthFeatureTypeFromSoftwareFeature(
                  CryptAuthFeatureTypeToSoftwareFeature(feature_type)));
  }
}

TEST(DeviceSyncCryptAuthFeatureTypeTest, ToHash) {
  EXPECT_EQ(kBetterTogetherHostEncodedHash,
            CryptAuthFeatureTypeToGcmHash(
                CryptAuthFeatureType::kBetterTogetherHostEnabled));
  EXPECT_EQ(kEasyUnlockHostEncodedHash,
            CryptAuthFeatureTypeToGcmHash(
                CryptAuthFeatureType::kEasyUnlockHostEnabled));
}

}  // namespace device_sync

}  // namespace ash
