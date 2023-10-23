// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/multidevice/mojom/multidevice_mojom_traits.h"

#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/beacon_seed.h"
#include "chromeos/ash/components/multidevice/mojom/multidevice_types.mojom.h"
#include "chromeos/ash/components/multidevice/remote_device.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestBeaconSeedData[] = "data";
const int64_t kTestBeaconSeedStartTimeMillis = 1L;
const int64_t kTestBeaconSeedEndTimeMillis = 2L;

ash::multidevice::BeaconSeed CreateTestBeaconSeed() {
  return ash::multidevice::BeaconSeed(
      kTestBeaconSeedData,
      base::Time::FromMillisecondsSinceUnixEpoch(
          kTestBeaconSeedStartTimeMillis),
      base::Time::FromMillisecondsSinceUnixEpoch(kTestBeaconSeedEndTimeMillis));
}

}  // namespace

TEST(MultiDeviceMojomStructTraitsTest, BeaconSeed) {
  ash::multidevice::BeaconSeed input = CreateTestBeaconSeed();

  ash::multidevice::BeaconSeed output;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<ash::multidevice::mojom::BeaconSeed>(
          input, output));

  EXPECT_EQ(kTestBeaconSeedData, output.data());
  EXPECT_EQ(kTestBeaconSeedStartTimeMillis,
            output.start_time().InMillisecondsSinceUnixEpoch());
  EXPECT_EQ(kTestBeaconSeedEndTimeMillis,
            output.end_time().InMillisecondsSinceUnixEpoch());
}

TEST(MultiDeviceMojomStructTraitsTest, RemoteDevice) {
  std::map<ash::multidevice::SoftwareFeature,
           ash::multidevice::SoftwareFeatureState>
      software_features = std::map<ash::multidevice::SoftwareFeature,
                                   ash::multidevice::SoftwareFeatureState>();
  software_features[ash::multidevice::SoftwareFeature::kBetterTogetherClient] =
      ash::multidevice::SoftwareFeatureState::kSupported;
  software_features[ash::multidevice::SoftwareFeature::kBetterTogetherHost] =
      ash::multidevice::SoftwareFeatureState::kEnabled;

  ash::multidevice::RemoteDevice input;
  input.user_email = "userEmail";
  input.instance_id = "instanceId";
  input.name = "name";
  input.pii_free_name = "piiFreeName";
  input.public_key = "publicKey";
  input.persistent_symmetric_key = "persistentSymmetricKey";
  input.last_update_time_millis = 3L;
  input.software_features = software_features;
  input.beacon_seeds = {CreateTestBeaconSeed()};
  input.bluetooth_public_address = "01:23:45:67:89:AB";

  ash::multidevice::RemoteDevice output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              ash::multidevice::mojom::RemoteDevice>(input, output));

  EXPECT_EQ("userEmail", output.user_email);
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
            output.beacon_seeds[0].start_time().InMillisecondsSinceUnixEpoch());
  EXPECT_EQ(kTestBeaconSeedEndTimeMillis,
            output.beacon_seeds[0].end_time().InMillisecondsSinceUnixEpoch());
  EXPECT_EQ("01:23:45:67:89:AB", output.bluetooth_public_address);
}

TEST(DeviceSyncMojomEnumTraitsTest, SoftwareFeature) {
  static constexpr ash::multidevice::SoftwareFeature kTestSoftwareFeatures[] = {
      ash::multidevice::SoftwareFeature::kBetterTogetherHost,
      ash::multidevice::SoftwareFeature::kBetterTogetherClient,
      ash::multidevice::SoftwareFeature::kSmartLockHost,
      ash::multidevice::SoftwareFeature::kSmartLockClient,
      ash::multidevice::SoftwareFeature::kInstantTetheringHost,
      ash::multidevice::SoftwareFeature::kInstantTetheringClient,
      ash::multidevice::SoftwareFeature::kMessagesForWebHost,
      ash::multidevice::SoftwareFeature::kMessagesForWebClient,
      ash::multidevice::SoftwareFeature::kPhoneHubHost,
      ash::multidevice::SoftwareFeature::kPhoneHubClient,
      ash::multidevice::SoftwareFeature::kWifiSyncHost,
      ash::multidevice::SoftwareFeature::kWifiSyncClient,
      ash::multidevice::SoftwareFeature::kEcheHost,
      ash::multidevice::SoftwareFeature::kEcheClient,
      ash::multidevice::SoftwareFeature::kPhoneHubCameraRollHost,
      ash::multidevice::SoftwareFeature::kPhoneHubCameraRollClient};

  for (auto feature_in : kTestSoftwareFeatures) {
    ash::multidevice::SoftwareFeature feature_out;

    ash::multidevice::mojom::SoftwareFeature serialized_feature =
        mojo::EnumTraits<
            ash::multidevice::mojom::SoftwareFeature,
            ash::multidevice::SoftwareFeature>::ToMojom(feature_in);
    ASSERT_TRUE((mojo::EnumTraits<ash::multidevice::mojom::SoftwareFeature,
                                  ash::multidevice::SoftwareFeature>::
                     FromMojom(serialized_feature, &feature_out)));
    EXPECT_EQ(feature_in, feature_out);
  }
}
