// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_device_registry_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/services/device_sync/cryptauth_device_registry.h"
#include "chromeos/services/device_sync/pref_names.h"
#include "chromeos/services/device_sync/proto/cryptauth_better_together_device_metadata.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "chromeos/services/device_sync/value_string_encoding.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace device_sync {

namespace {

const char kInstanceId0[] = "instance_id_0";
const char kInstanceId1[] = "instance_id_1";

}  // namespace

class DeviceSyncCryptAuthDeviceRegistryImplTest : public testing::Test {
 protected:
  DeviceSyncCryptAuthDeviceRegistryImplTest() = default;

  ~DeviceSyncCryptAuthDeviceRegistryImplTest() override = default;

  void SetUp() override {
    CryptAuthDeviceRegistryImpl::RegisterPrefs(pref_service_.registry());
  }

  void CreateDeviceRegistry() {
    device_registry_ =
        CryptAuthDeviceRegistryImpl::Factory::Get()->BuildInstance(
            &pref_service_);
  }

  const CryptAuthDevice& GetDeviceForTest(size_t index) const {
    static const base::NoDestructor<std::vector<CryptAuthDevice>> devices([] {
      const char kDeviceName0[] = "device_name_0";
      const char kDeviceName1[] = "device_name_1";
      const char kDeviceBetterTogetherPublicKey0[] =
          "device_better_together_public_key_0";
      const char kDeviceBetterTogetherPublicKey1[] =
          "device_better_together_public_key_1";
      const base::Time kLastUpdateTime0 = base::Time::FromDoubleT(100);
      const base::Time kLastUpdateTime1 = base::Time::FromDoubleT(200);
      const std::map<multidevice::SoftwareFeature,
                     multidevice::SoftwareFeatureState>
          kFakeFeatureStates0 = {
              {multidevice::SoftwareFeature::kBetterTogetherClient,
               multidevice::SoftwareFeatureState::kEnabled}};
      const std::map<multidevice::SoftwareFeature,
                     multidevice::SoftwareFeatureState>
          kFakeFeatureStates1 = {
              {multidevice::SoftwareFeature::kBetterTogetherHost,
               multidevice::SoftwareFeatureState::kEnabled}};

      return std::vector<CryptAuthDevice>{
          CryptAuthDevice(kInstanceId0, kDeviceName0,
                          kDeviceBetterTogetherPublicKey0, kLastUpdateTime0,
                          cryptauthv2::GetBetterTogetherDeviceMetadataForTest(),
                          kFakeFeatureStates0),
          CryptAuthDevice(kInstanceId1, kDeviceName1,
                          kDeviceBetterTogetherPublicKey1, kLastUpdateTime1,
                          base::nullopt /* better_together_device_metadata */,
                          kFakeFeatureStates1)};
    }());

    EXPECT_LT(index, devices->size());
    return devices->at(index);
  }

  base::Value AsDictionary(
      const CryptAuthDeviceRegistry::InstanceIdToDeviceMap& devices) const {
    base::Value dict(base::Value::Type::DICTIONARY);
    for (const std::pair<std::string, CryptAuthDevice>& id_device_pair :
         devices) {
      dict.SetKey(util::EncodeAsString(id_device_pair.first),
                  id_device_pair.second.AsDictionary());
    }

    return dict;
  }

  void VerifyDeviceRegistry(
      const CryptAuthDeviceRegistry::InstanceIdToDeviceMap& expected_devices) {
    EXPECT_EQ(expected_devices, device_registry()->instance_id_to_device_map());

    // Verify pref.
    EXPECT_EQ(AsDictionary(expected_devices),
              *pref_service_.Get(prefs::kCryptAuthDeviceRegistry));
  }

  PrefService* pref_service() { return &pref_service_; }

  CryptAuthDeviceRegistry* device_registry() { return device_registry_.get(); }

 private:
  TestingPrefServiceSimple pref_service_;

  std::unique_ptr<CryptAuthDeviceRegistry> device_registry_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncCryptAuthDeviceRegistryImplTest);
};

TEST_F(DeviceSyncCryptAuthDeviceRegistryImplTest, AddAndGetDevices) {
  CreateDeviceRegistry();

  EXPECT_TRUE(device_registry()->AddDevice(GetDeviceForTest(0)));
  EXPECT_FALSE(device_registry()->AddDevice(GetDeviceForTest(0)));
  EXPECT_TRUE(device_registry()->AddDevice(GetDeviceForTest(1)));

  VerifyDeviceRegistry(
      {{kInstanceId0, GetDeviceForTest(0)},
       {kInstanceId1, GetDeviceForTest(1)}} /* expected_devices */);
  EXPECT_EQ(GetDeviceForTest(0), *device_registry()->GetDevice(kInstanceId0));
  EXPECT_EQ(GetDeviceForTest(1), *device_registry()->GetDevice(kInstanceId1));
}

TEST_F(DeviceSyncCryptAuthDeviceRegistryImplTest, OverwriteDevice) {
  CreateDeviceRegistry();

  EXPECT_TRUE(device_registry()->AddDevice(GetDeviceForTest(0)));
  EXPECT_EQ(GetDeviceForTest(0), *device_registry()->GetDevice(kInstanceId0));

  CryptAuthDevice device_with_same_instance_id(
      kInstanceId0, "name", "key", base::Time::FromDoubleT(5000),
      cryptauthv2::BetterTogetherDeviceMetadata(), {});
  EXPECT_TRUE(device_registry()->AddDevice(device_with_same_instance_id));
  EXPECT_EQ(device_with_same_instance_id,
            *device_registry()->GetDevice(kInstanceId0));
}

TEST_F(DeviceSyncCryptAuthDeviceRegistryImplTest, OverwriteRegistry) {
  CreateDeviceRegistry();

  CryptAuthDeviceRegistry::InstanceIdToDeviceMap old_devices = {
      {kInstanceId0, GetDeviceForTest(0)}};
  EXPECT_TRUE(device_registry()->SetRegistry(old_devices));
  VerifyDeviceRegistry(old_devices);
  EXPECT_FALSE(device_registry()->SetRegistry(old_devices));

  CryptAuthDeviceRegistry::InstanceIdToDeviceMap new_devices = {
      {kInstanceId1, GetDeviceForTest(1)}};
  EXPECT_TRUE(device_registry()->SetRegistry(new_devices));
  VerifyDeviceRegistry(new_devices);
}

TEST_F(DeviceSyncCryptAuthDeviceRegistryImplTest, DeleteDevice) {
  CreateDeviceRegistry();

  EXPECT_TRUE(device_registry()->AddDevice(GetDeviceForTest(0)));
  VerifyDeviceRegistry(
      {{kInstanceId0, GetDeviceForTest(0)}} /* expected_devices */);

  EXPECT_TRUE(device_registry()->DeleteDevice(kInstanceId0));
  EXPECT_FALSE(device_registry()->GetDevice(kInstanceId0));
  VerifyDeviceRegistry({} /* expected_devices */);
  EXPECT_FALSE(device_registry()->DeleteDevice(kInstanceId0));
}

TEST_F(DeviceSyncCryptAuthDeviceRegistryImplTest, PopulateRegistryFromPref) {
  CryptAuthDeviceRegistry::InstanceIdToDeviceMap expected_devices = {
      {kInstanceId0, GetDeviceForTest(0)}, {kInstanceId1, GetDeviceForTest(1)}};
  pref_service()->Set(prefs::kCryptAuthDeviceRegistry,
                      AsDictionary(expected_devices));

  CreateDeviceRegistry();

  VerifyDeviceRegistry(expected_devices);
}

}  // namespace device_sync

}  // namespace chromeos
