// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/multidevice/mojom/multidevice_mojom_traits.h"

#include "base/time/time.h"
#include "chromeos/components/multidevice/beacon_seed.h"
#include "chromeos/components/multidevice/mojom/multidevice_types.mojom.h"
#include "chromeos/components/multidevice/remote_device.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestBeaconSeedData[] = "data";
const int64_t kTestBeaconSeedStartTimeMillis = 1L;
const int64_t kTestBeaconSeedEndTimeMillis = 2L;

chromeos::multidevice::BeaconSeed CreateTestBeaconSeed() {
  return chromeos::multidevice::BeaconSeed(
      kTestBeaconSeedData,
      base::Time::FromJavaTime(kTestBeaconSeedStartTimeMillis),
      base::Time::FromJavaTime(kTestBeaconSeedEndTimeMillis));
}

}  // namespace

TEST(MultiDeviceMojomStructTraitsTest, BeaconSeed) {
  chromeos::multidevice::BeaconSeed input = CreateTestBeaconSeed();

  chromeos::multidevice::BeaconSeed output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              chromeos::multidevice::mojom::BeaconSeed>(&input, &output));

  EXPECT_EQ(kTestBeaconSeedData, output.data());
  EXPECT_EQ(kTestBeaconSeedStartTimeMillis, output.start_time().ToJavaTime());
  EXPECT_EQ(kTestBeaconSeedEndTimeMillis, output.end_time().ToJavaTime());
}

TEST(MultiDeviceMojomStructTraitsTest, RemoteDevice) {
  std::map<chromeos::multidevice::SoftwareFeature,
           chromeos::multidevice::SoftwareFeatureState>
      software_features =
          std::map<chromeos::multidevice::SoftwareFeature,
                   chromeos::multidevice::SoftwareFeatureState>();
  software_features
      [chromeos::multidevice::SoftwareFeature::kBetterTogetherClient] =
          chromeos::multidevice::SoftwareFeatureState::kSupported;
  software_features
      [chromeos::multidevice::SoftwareFeature::kBetterTogetherHost] =
          chromeos::multidevice::SoftwareFeatureState::kEnabled;

  chromeos::multidevice::RemoteDevice input;
  input.user_id = "userId";
  input.instance_id = "instanceId";
  input.name = "name";
  input.pii_free_name = "piiFreeName";
  input.public_key = "publicKey";
  input.persistent_symmetric_key = "persistentSymmetricKey";
  input.last_update_time_millis = 3L;
  input.software_features = software_features;
  input.beacon_seeds = {CreateTestBeaconSeed()};

  chromeos::multidevice::RemoteDevice output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              chromeos::multidevice::mojom::RemoteDevice>(&input, &output));

  EXPECT_EQ("userId", output.user_id);
  EXPECT_EQ("instanceId", output.instance_id);
  EXPECT_EQ("name", output.name);
  EXPECT_EQ("piiFreeName", output.pii_free_name);
  EXPECT_EQ("publicKey", output.public_key);
  EXPECT_EQ("persistentSymmetricKey", output.persistent_symmetric_key);
  EXPECT_EQ(3L, output.last_update_time_millis);
  EXPECT_EQ(software_features, output.software_features);
  ASSERT_EQ(1u, output.beacon_seeds.size());
  EXPECT_EQ(kTestBeaconSeedData, output.beacon_seeds[0].data());
  EXPECT_EQ(kTestBeaconSeedStartTimeMillis,
            output.beacon_seeds[0].start_time().ToJavaTime());
  EXPECT_EQ(kTestBeaconSeedEndTimeMillis,
            output.beacon_seeds[0].end_time().ToJavaTime());
}

TEST(DeviceSyncMojomEnumTraitsTest, SoftwareFeature) {
  static constexpr chromeos::multidevice::SoftwareFeature
      kTestSoftwareFeatures[] = {
          chromeos::multidevice::SoftwareFeature::kBetterTogetherHost,
          chromeos::multidevice::SoftwareFeature::kBetterTogetherClient,
          chromeos::multidevice::SoftwareFeature::kSmartLockHost,
          chromeos::multidevice::SoftwareFeature::kSmartLockClient,
          chromeos::multidevice::SoftwareFeature::kInstantTetheringHost,
          chromeos::multidevice::SoftwareFeature::kInstantTetheringClient,
          chromeos::multidevice::SoftwareFeature::kMessagesForWebHost,
          chromeos::multidevice::SoftwareFeature::kMessagesForWebClient};

  for (auto feature_in : kTestSoftwareFeatures) {
    chromeos::multidevice::SoftwareFeature feature_out;

    chromeos::multidevice::mojom::SoftwareFeature serialized_feature =
        mojo::EnumTraits<
            chromeos::multidevice::mojom::SoftwareFeature,
            chromeos::multidevice::SoftwareFeature>::ToMojom(feature_in);
    ASSERT_TRUE((mojo::EnumTraits<chromeos::multidevice::mojom::SoftwareFeature,
                                  chromeos::multidevice::SoftwareFeature>::
                     FromMojom(serialized_feature, &feature_out)));
    EXPECT_EQ(feature_in, feature_out);
  }
}
