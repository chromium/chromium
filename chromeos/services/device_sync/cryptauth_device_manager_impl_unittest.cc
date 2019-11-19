// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_device_manager_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/base64url.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/services/device_sync/fake_cryptauth_gcm_manager.h"
#include "chromeos/services/device_sync/mock_cryptauth_client.h"
#include "chromeos/services/device_sync/mock_sync_scheduler.h"
#include "chromeos/services/device_sync/network_request_error.h"
#include "chromeos/services/device_sync/pref_names.h"
#include "chromeos/services/device_sync/proto/enum_util.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

namespace chromeos {

namespace device_sync {

namespace {

// The initial "Now" time for testing.
const double kInitialTimeNowSeconds = 20000000;

// A later "Now" time for testing.
const double kLaterTimeNowSeconds = kInitialTimeNowSeconds + 30;

// The timestamp of a last successful sync in seconds.
const double kLastSyncTimeSeconds = kInitialTimeNowSeconds - (60 * 60 * 5);

// Unlock key fields originally stored in the user prefs.
const char kStoredPublicKey[] = "storedPublicKey";
const char kStoredDeviceName[] = "Pixel 2";
const char kStoredBluetoothAddress[] = "12:34:56:78:90:AB";
const bool kStoredUnlockable = false;

// cryptauth::ExternalDeviceInfo fields for the synced unlock key.
const char kPublicKey1[] = "GOOG";
const char kDeviceName1[] = "Pixel XL";
const char kNoPiiDeviceName1[] = "marlin";
const char kBluetoothAddress1[] = "aa:bb:cc:ee:dd:ff";
const bool kUnlockable1 = false;
const char kBeaconSeed1Data[] = "beaconSeed1Data";
const int64_t kBeaconSeed1StartTime = 123456;
const int64_t kBeaconSeed1EndTime = 123457;
const char kBeaconSeed2Data[] = "beaconSeed2Data";
const int64_t kBeaconSeed2StartTime = 234567;
const int64_t kBeaconSeed2EndTime = 234568;
const bool kArcPlusPlus1 = true;
const bool kPixelPhone1 = true;

// cryptauth::ExternalDeviceInfo fields for a non-synced unlockable device.
const char kPublicKey2[] = "CROS";
const char kDeviceName2[] = "Pixelbook";
const char kNoPiiDeviceName2[] = "eve-signed-mpkeys";
const bool kUnlockable2 = true;
const char kBeaconSeed3Data[] = "beaconSeed3Data";
const int64_t kBeaconSeed3StartTime = 123456;
const int64_t kBeaconSeed3EndTime = 123457;
const char kBeaconSeed4Data[] = "beaconSeed4Data";
const int64_t kBeaconSeed4StartTime = 234567;
const int64_t kBeaconSeed4EndTime = 234568;
const bool kArcPlusPlus2 = false;
const bool kPixelPhone2 = false;

// Validates that |devices| is equal to |expected_devices|.
void ExpectSyncedDevicesAreEqual(
    const std::vector<cryptauth::ExternalDeviceInfo>& expected_devices,
    const std::vector<cryptauth::ExternalDeviceInfo>& devices) {
  ASSERT_EQ(expected_devices.size(), devices.size());
  for (size_t i = 0; i < devices.size(); ++i) {
    SCOPED_TRACE(
        base::StringPrintf("Compare protos at index=%d", static_cast<int>(i)));
    const auto& expected_device = expected_devices[i];
    const auto& device = devices.at(i);
    EXPECT_TRUE(expected_device.has_public_key());
    EXPECT_TRUE(device.has_public_key());
    EXPECT_EQ(expected_device.public_key(), device.public_key());

    EXPECT_EQ(expected_device.has_friendly_device_name(),
              device.has_friendly_device_name());
    EXPECT_EQ(expected_device.friendly_device_name(),
              device.friendly_device_name());

    EXPECT_EQ(expected_device.has_no_pii_device_name(),
              device.has_no_pii_device_name());
    EXPECT_EQ(expected_device.no_pii_device_name(),
              device.no_pii_device_name());

    EXPECT_EQ(expected_device.has_bluetooth_address(),
              device.has_bluetooth_address());
    EXPECT_EQ(expected_device.bluetooth_address(), device.bluetooth_address());

    EXPECT_EQ(expected_device.has_unlock_key(), device.has_unlock_key());
    EXPECT_EQ(expected_device.unlock_key(), device.unlock_key());

    EXPECT_EQ(expected_device.has_unlockable(), device.has_unlockable());
    EXPECT_EQ(expected_device.unlockable(), device.unlockable());

    EXPECT_EQ(expected_device.has_last_update_time_millis(),
              device.has_last_update_time_millis());
    EXPECT_EQ(expected_device.last_update_time_millis(),
              device.last_update_time_millis());

    EXPECT_EQ(expected_device.has_mobile_hotspot_supported(),
              device.has_mobile_hotspot_supported());
    EXPECT_EQ(expected_device.mobile_hotspot_supported(),
              device.mobile_hotspot_supported());

    EXPECT_EQ(expected_device.has_device_type(), device.has_device_type());
    EXPECT_EQ(expected_device.device_type(), device.device_type());

    ASSERT_EQ(expected_device.beacon_seeds_size(), device.beacon_seeds_size());
    for (int i = 0; i < expected_device.beacon_seeds_size(); i++) {
      const cryptauth::BeaconSeed expected_seed =
          expected_device.beacon_seeds(i);
      const cryptauth::BeaconSeed seed = device.beacon_seeds(i);
      EXPECT_TRUE(expected_seed.has_data());
      EXPECT_TRUE(seed.has_data());
      EXPECT_EQ(expected_seed.data(), seed.data());

      EXPECT_TRUE(expected_seed.has_start_time_millis());
      EXPECT_TRUE(seed.has_start_time_millis());
      EXPECT_EQ(expected_seed.start_time_millis(), seed.start_time_millis());

      EXPECT_TRUE(expected_seed.has_end_time_millis());
      EXPECT_TRUE(seed.has_end_time_millis());
      EXPECT_EQ(expected_seed.end_time_millis(), seed.end_time_millis());
    }

    EXPECT_EQ(expected_device.has_arc_plus_plus(), device.has_arc_plus_plus());
    EXPECT_EQ(expected_device.arc_plus_plus(), device.arc_plus_plus());

    EXPECT_EQ(expected_device.has_pixel_phone(), device.has_pixel_phone());
    EXPECT_EQ(expected_device.pixel_phone(), device.pixel_phone());

    EXPECT_EQ(expected_device.supported_software_features_size(),
              device.supported_software_features_size());
    for (const auto& software_feature :
         expected_device.supported_software_features()) {
      EXPECT_TRUE(base::Contains(device.supported_software_features(),
                                 software_feature));
    }

    EXPECT_EQ(expected_device.enabled_software_features_size(),
              device.enabled_software_features_size());
    for (const auto& software_feature :
         expected_device.enabled_software_features()) {
      EXPECT_TRUE(
          base::Contains(device.enabled_software_features(), software_feature));
    }
  }
}

// Validates that |devices| and the corresponding preferences stored by
// |pref_service| are equal to |expected_devices|.
void ExpectSyncedDevicesAndPrefAreEqual(
    const std::vector<cryptauth::ExternalDeviceInfo>& expected_devices,
    const std::vector<cryptauth::ExternalDeviceInfo>& devices,
    const PrefService& pref_service) {
  ExpectSyncedDevicesAreEqual(expected_devices, devices);

  const base::ListValue* synced_devices_pref =
      pref_service.GetList(prefs::kCryptAuthDeviceSyncUnlockKeys);
  ASSERT_EQ(expected_devices.size(), synced_devices_pref->GetSize());
  for (size_t i = 0; i < synced_devices_pref->GetSize(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Compare pref dictionary at index=%d",
                                    static_cast<int>(i)));
    const base::DictionaryValue* device_dictionary;
    EXPECT_TRUE(synced_devices_pref->GetDictionary(i, &device_dictionary));

    const auto& expected_device = expected_devices[i];

    std::string public_key_b64, public_key;
    EXPECT_TRUE(device_dictionary->GetString("public_key", &public_key_b64));
    EXPECT_TRUE(base::Base64UrlDecode(
        public_key_b64, base::Base64UrlDecodePolicy::REQUIRE_PADDING,
        &public_key));
    EXPECT_TRUE(expected_device.has_public_key());
    EXPECT_EQ(expected_device.public_key(), public_key);

    std::string device_name_b64, device_name;
    if (device_dictionary->GetString("device_name", &device_name_b64)) {
      EXPECT_TRUE(base::Base64UrlDecode(
          device_name_b64, base::Base64UrlDecodePolicy::REQUIRE_PADDING,
          &device_name));
      EXPECT_TRUE(expected_device.has_friendly_device_name());
      EXPECT_EQ(expected_device.friendly_device_name(), device_name);
    } else {
      EXPECT_FALSE(expected_device.has_friendly_device_name());
    }

    std::string no_pii_device_name_b64, no_pii_device_name;
    if (device_dictionary->GetString("no_pii_device_name",
                                     &no_pii_device_name_b64)) {
      EXPECT_TRUE(base::Base64UrlDecode(
          no_pii_device_name_b64, base::Base64UrlDecodePolicy::REQUIRE_PADDING,
          &no_pii_device_name));
      EXPECT_TRUE(expected_device.has_no_pii_device_name());
      EXPECT_EQ(expected_device.no_pii_device_name(), no_pii_device_name);
    } else {
      EXPECT_FALSE(expected_device.has_no_pii_device_name());
    }

    std::string bluetooth_address_b64, bluetooth_address;
    if (device_dictionary->GetString("bluetooth_address",
                                     &bluetooth_address_b64)) {
      EXPECT_TRUE(base::Base64UrlDecode(
          bluetooth_address_b64, base::Base64UrlDecodePolicy::REQUIRE_PADDING,
          &bluetooth_address));
      EXPECT_TRUE(expected_device.has_bluetooth_address());
      EXPECT_EQ(expected_device.bluetooth_address(), bluetooth_address);
    } else {
      EXPECT_FALSE(expected_device.has_bluetooth_address());
    }

    bool unlock_key;
    if (device_dictionary->GetBoolean("unlock_key", &unlock_key)) {
      EXPECT_TRUE(expected_device.has_unlock_key());
      EXPECT_EQ(expected_device.unlock_key(), unlock_key);
    } else {
      EXPECT_FALSE(expected_device.has_unlock_key());
    }

    bool unlockable;
    if (device_dictionary->GetBoolean("unlockable", &unlockable)) {
      EXPECT_TRUE(expected_device.has_unlockable());
      EXPECT_EQ(expected_device.unlockable(), unlockable);
    } else {
      EXPECT_FALSE(expected_device.has_unlockable());
    }

    std::string last_update_time_millis_str;
    if (device_dictionary->GetString("last_update_time_millis",
                                     &last_update_time_millis_str)) {
      int64_t last_update_time_millis;
      EXPECT_TRUE(base::StringToInt64(last_update_time_millis_str,
                                      &last_update_time_millis));
      EXPECT_TRUE(expected_device.has_last_update_time_millis());
      EXPECT_EQ(expected_device.last_update_time_millis(),
                last_update_time_millis);
    } else {
      EXPECT_FALSE(expected_device.has_last_update_time_millis());
    }

    bool mobile_hotspot_supported;
    if (device_dictionary->GetBoolean("mobile_hotspot_supported",
                                      &mobile_hotspot_supported)) {
      EXPECT_TRUE(expected_device.has_mobile_hotspot_supported());
      EXPECT_EQ(expected_device.mobile_hotspot_supported(),
                mobile_hotspot_supported);
    } else {
      EXPECT_FALSE(expected_device.has_mobile_hotspot_supported());
    }

    int device_type;
    if (device_dictionary->GetInteger("device_type", &device_type)) {
      EXPECT_TRUE(expected_device.has_device_type());
      EXPECT_EQ(DeviceTypeStringToEnum(expected_device.device_type()),
                device_type);
    } else {
      EXPECT_FALSE(expected_device.has_device_type());
    }

    const base::ListValue* beacon_seeds_from_prefs;
    if (device_dictionary->GetList("beacon_seeds", &beacon_seeds_from_prefs)) {
      ASSERT_EQ(static_cast<size_t>(expected_device.beacon_seeds_size()),
                beacon_seeds_from_prefs->GetSize());
      for (size_t i = 0; i < beacon_seeds_from_prefs->GetSize(); i++) {
        const base::DictionaryValue* seed;
        ASSERT_TRUE(beacon_seeds_from_prefs->GetDictionary(i, &seed));

        std::string data_b64, start_ms, end_ms;
        EXPECT_TRUE(seed->GetString("beacon_seed_data", &data_b64));
        EXPECT_TRUE(seed->GetString("beacon_seed_start_ms", &start_ms));
        EXPECT_TRUE(seed->GetString("beacon_seed_end_ms", &end_ms));

        const cryptauth::BeaconSeed& expected_seed =
            expected_device.beacon_seeds((int)i);

        std::string data;
        EXPECT_TRUE(base::Base64UrlDecode(
            data_b64, base::Base64UrlDecodePolicy::REQUIRE_PADDING, &data));
        EXPECT_TRUE(expected_seed.has_data());
        EXPECT_EQ(expected_seed.data(), data);

        EXPECT_TRUE(expected_seed.has_start_time_millis());
        EXPECT_EQ(expected_seed.start_time_millis(), std::stol(start_ms));

        EXPECT_TRUE(expected_seed.has_end_time_millis());
        EXPECT_EQ(expected_seed.end_time_millis(), std::stol(end_ms));
      }
    } else {
      EXPECT_FALSE(expected_device.beacon_seeds_size());
    }

    bool arc_plus_plus;
    if (device_dictionary->GetBoolean("arc_plus_plus", &arc_plus_plus)) {
      EXPECT_TRUE(expected_device.has_arc_plus_plus());
      EXPECT_EQ(expected_device.arc_plus_plus(), arc_plus_plus);
    } else {
      EXPECT_FALSE(expected_device.has_arc_plus_plus());
    }

    bool pixel_phone;
    if (device_dictionary->GetBoolean("pixel_phone", &pixel_phone)) {
      EXPECT_TRUE(expected_device.has_pixel_phone());
      EXPECT_EQ(expected_device.pixel_phone(), pixel_phone);
    } else {
      EXPECT_FALSE(expected_device.has_pixel_phone());
    }

    const base::DictionaryValue* software_features_from_prefs;
    if (device_dictionary->GetDictionary("software_features",
                                         &software_features_from_prefs)) {
      std::vector<cryptauth::SoftwareFeature> supported_software_features;
      std::vector<cryptauth::SoftwareFeature> enabled_software_features;

      for (const auto& it : software_features_from_prefs->DictItems()) {
        int software_feature_state;
        ASSERT_TRUE(it.second.GetAsInteger(&software_feature_state));

        cryptauth::SoftwareFeature software_feature =
            SoftwareFeatureStringToEnum(it.first);
        switch (static_cast<multidevice::SoftwareFeatureState>(
            software_feature_state)) {
          case multidevice::SoftwareFeatureState::kEnabled:
            enabled_software_features.push_back(software_feature);
            FALLTHROUGH;
          case multidevice::SoftwareFeatureState::kSupported:
            supported_software_features.push_back(software_feature);
            break;
          default:
            break;
        }
      }

      ASSERT_EQ(static_cast<size_t>(
                    expected_device.supported_software_features_size()),
                supported_software_features.size());
      ASSERT_EQ(
          static_cast<size_t>(expected_device.enabled_software_features_size()),
          enabled_software_features.size());
      for (auto supported_software_feature :
           expected_device.supported_software_features()) {
        EXPECT_TRUE(base::Contains(
            supported_software_features,
            SoftwareFeatureStringToEnum(supported_software_feature)));
      }
      for (auto enabled_software_feature :
           expected_device.enabled_software_features()) {
        EXPECT_TRUE(base::Contains(
            enabled_software_features,
            SoftwareFeatureStringToEnum(enabled_software_feature)));
      }
    } else {
      EXPECT_FALSE(expected_device.supported_software_features_size());
      EXPECT_FALSE(expected_device.enabled_software_features_size());
    }
  }
}

// Harness for testing CryptAuthDeviceManager.
class TestCryptAuthDeviceManager : public CryptAuthDeviceManagerImpl {
 public:
  TestCryptAuthDeviceManager(base::Clock* clock,
                             CryptAuthClientFactory* client_factory,
                             CryptAuthGCMManager* gcm_manager,
                             PrefService* pref_service)
      : CryptAuthDeviceManagerImpl(clock,
                                   client_factory,
                                   gcm_manager,
                                   pref_service),
        scoped_sync_scheduler_(new NiceMock<MockSyncScheduler>()),
        weak_sync_scheduler_factory_(scoped_sync_scheduler_) {
    SetSyncSchedulerForTest(base::WrapUnique(scoped_sync_scheduler_));
  }

  ~TestCryptAuthDeviceManager() override {}

  base::WeakPtr<MockSyncScheduler> GetSyncScheduler() {
    return weak_sync_scheduler_factory_.GetWeakPtr();
  }

 private:
  // Ownership is passed to |CryptAuthDeviceManager| super class when
  // SetSyncSchedulerForTest() is called.
  MockSyncScheduler* scoped_sync_scheduler_;

  // Stores the pointer of |scoped_sync_scheduler_| after ownership is passed to
  // the super class.
  // This should be safe because the life-time this SyncScheduler will always be
  // within the life of the TestCryptAuthDeviceManager object.
  base::WeakPtrFactory<MockSyncScheduler> weak_sync_scheduler_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestCryptAuthDeviceManager);
};

}  // namespace

class DeviceSyncCryptAuthDeviceManagerImplTest
    : public testing::Test,
      public CryptAuthDeviceManager::Observer,
      public MockCryptAuthClientFactory::Observer {
 protected:
  DeviceSyncCryptAuthDeviceManagerImplTest()
      : client_factory_(std::make_unique<MockCryptAuthClientFactory>(
            MockCryptAuthClientFactory::MockType::MAKE_STRICT_MOCKS)),
        gcm_manager_("existing gcm registration id") {
    client_factory_->AddObserver(this);

    cryptauth::ExternalDeviceInfo unlock_key;
    unlock_key.set_public_key(kPublicKey1);
    unlock_key.set_friendly_device_name(kDeviceName1);
    unlock_key.set_no_pii_device_name(kNoPiiDeviceName1);
    unlock_key.set_bluetooth_address(kBluetoothAddress1);
    unlock_key.set_unlockable(kUnlockable1);
    cryptauth::BeaconSeed* seed1 = unlock_key.add_beacon_seeds();
    seed1->set_data(kBeaconSeed1Data);
    seed1->set_start_time_millis(kBeaconSeed1StartTime);
    seed1->set_end_time_millis(kBeaconSeed1EndTime);
    cryptauth::BeaconSeed* seed2 = unlock_key.add_beacon_seeds();
    seed2->set_data(kBeaconSeed2Data);
    seed2->set_start_time_millis(kBeaconSeed2StartTime);
    seed2->set_end_time_millis(kBeaconSeed2EndTime);
    unlock_key.set_arc_plus_plus(kArcPlusPlus1);
    unlock_key.set_pixel_phone(kPixelPhone1);
    unlock_key.add_supported_software_features(SoftwareFeatureEnumToString(
        cryptauth::SoftwareFeature::EASY_UNLOCK_HOST));
    unlock_key.add_enabled_software_features(SoftwareFeatureEnumToString(
        cryptauth::SoftwareFeature::EASY_UNLOCK_HOST));
    unlock_key.add_supported_software_features(SoftwareFeatureEnumToString(
        cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST));
    unlock_key.add_supported_software_features(SoftwareFeatureEnumToString(
        cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT));
    unlock_key.add_enabled_software_features(SoftwareFeatureEnumToString(
        cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST));
    devices_in_response_.push_back(unlock_key);

    cryptauth::ExternalDeviceInfo unlockable_device;
    unlockable_device.set_public_key(kPublicKey2);
    unlockable_device.set_friendly_device_name(kDeviceName2);
    unlockable_device.set_no_pii_device_name(kNoPiiDeviceName2);
    unlockable_device.set_unlockable(kUnlockable2);
    cryptauth::BeaconSeed* seed3 = unlockable_device.add_beacon_seeds();
    seed3->set_data(kBeaconSeed3Data);
    seed3->set_start_time_millis(kBeaconSeed3StartTime);
    seed3->set_end_time_millis(kBeaconSeed3EndTime);
    cryptauth::BeaconSeed* seed4 = unlockable_device.add_beacon_seeds();
    seed4->set_data(kBeaconSeed4Data);
    seed4->set_start_time_millis(kBeaconSeed4StartTime);
    seed4->set_end_time_millis(kBeaconSeed4EndTime);
    unlockable_device.set_arc_plus_plus(kArcPlusPlus2);
    unlockable_device.set_pixel_phone(kPixelPhone2);
    unlockable_device.add_supported_software_features(
        SoftwareFeatureEnumToString(
            cryptauth::SoftwareFeature::MAGIC_TETHER_HOST));
    unlockable_device.add_supported_software_features(
        SoftwareFeatureEnumToString(
            cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT));
    unlockable_device.add_enabled_software_features(SoftwareFeatureEnumToString(
        cryptauth::SoftwareFeature::MAGIC_TETHER_HOST));
    devices_in_response_.push_back(unlockable_device);
  }

  ~DeviceSyncCryptAuthDeviceManagerImplTest() {
    client_factory_->RemoveObserver(this);
  }

  // testing::Test:
  void SetUp() override {
    clock_.SetNow(base::Time::FromDoubleT(kInitialTimeNowSeconds));

    CryptAuthDeviceManager::RegisterPrefs(pref_service_.registry());
    pref_service_.SetUserPref(
        prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure,
        std::make_unique<base::Value>(false));
    pref_service_.SetUserPref(
        prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds,
        std::make_unique<base::Value>(kLastSyncTimeSeconds));
    pref_service_.SetUserPref(
        prefs::kCryptAuthDeviceSyncReason,
        std::make_unique<base::Value>(cryptauth::INVOCATION_REASON_UNKNOWN));

    std::unique_ptr<base::DictionaryValue> device_dictionary(
        new base::DictionaryValue());

    std::string public_key_b64, device_name_b64, bluetooth_address_b64;
    base::Base64UrlEncode(kStoredPublicKey,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &public_key_b64);
    base::Base64UrlEncode(kStoredDeviceName,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &device_name_b64);
    base::Base64UrlEncode(kStoredBluetoothAddress,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &bluetooth_address_b64);

    device_dictionary->SetString("public_key", public_key_b64);
    device_dictionary->SetString("device_name", device_name_b64);
    device_dictionary->SetString("bluetooth_address", bluetooth_address_b64);
    device_dictionary->SetBoolean("unlockable", kStoredUnlockable);
    device_dictionary->Set("beacon_seeds", std::make_unique<base::ListValue>());
    device_dictionary->Set("software_features",
                           std::make_unique<base::DictionaryValue>());

    {
      ListPrefUpdate update(&pref_service_,
                            prefs::kCryptAuthDeviceSyncUnlockKeys);
      update.Get()->Append(std::move(device_dictionary));
    }

    device_manager_.reset(new TestCryptAuthDeviceManager(
        &clock_, client_factory_.get(), &gcm_manager_, &pref_service_));
    device_manager_->AddObserver(this);

    get_my_devices_response_.add_devices()->CopyFrom(devices_in_response_[0]);
    get_my_devices_response_.add_devices()->CopyFrom(devices_in_response_[1]);

    ON_CALL(*sync_scheduler(), GetStrategy())
        .WillByDefault(Return(SyncScheduler::Strategy::PERIODIC_REFRESH));
  }

  void TearDown() override { device_manager_->RemoveObserver(this); }

  // CryptAuthDeviceManager::Observer:
  void OnSyncStarted() override { OnSyncStartedProxy(); }

  void OnSyncFinished(CryptAuthDeviceManager::SyncResult sync_result,
                      CryptAuthDeviceManager::DeviceChangeResult
                          device_change_result) override {
    OnSyncFinishedProxy(sync_result, device_change_result);
  }

  MOCK_METHOD0(OnSyncStartedProxy, void());
  MOCK_METHOD2(OnSyncFinishedProxy,
               void(CryptAuthDeviceManager::SyncResult,
                    CryptAuthDeviceManager::DeviceChangeResult));

  // Simulates firing the SyncScheduler to trigger a device sync attempt.
  void FireSchedulerForSync(
      cryptauth::InvocationReason expected_invocation_reason) {
    SyncScheduler::Delegate* delegate =
        static_cast<SyncScheduler::Delegate*>(device_manager_.get());

    std::unique_ptr<SyncScheduler::SyncRequest> sync_request =
        std::make_unique<SyncScheduler::SyncRequest>(
            device_manager_->GetSyncScheduler());
    EXPECT_CALL(*this, OnSyncStartedProxy());
    delegate->OnSyncRequested(std::move(sync_request));

    EXPECT_EQ(expected_invocation_reason,
              get_my_devices_request_.invocation_reason());

    // The allow_stale_read flag is set if the sync was not forced.
    bool allow_stale_read =
        expected_invocation_reason !=
            cryptauth::INVOCATION_REASON_FEATURE_TOGGLED &&
        expected_invocation_reason !=
            cryptauth::INVOCATION_REASON_SERVER_INITIATED &&
        expected_invocation_reason != cryptauth::INVOCATION_REASON_MANUAL;
    EXPECT_EQ(allow_stale_read, get_my_devices_request_.allow_stale_read());
  }

  // MockCryptAuthClientFactory::Observer:
  void OnCryptAuthClientCreated(MockCryptAuthClient* client) override {
    EXPECT_CALL(*client, GetMyDevices(_, _, _, _))
        .WillOnce(DoAll(SaveArg<0>(&get_my_devices_request_),
                        SaveArg<1>(&success_callback_),
                        SaveArg<2>(&error_callback_)));
  }

  MockSyncScheduler* sync_scheduler() {
    return device_manager_->GetSyncScheduler().get();
  }

  base::SimpleTestClock clock_;

  std::unique_ptr<MockCryptAuthClientFactory> client_factory_;

  TestingPrefServiceSimple pref_service_;

  FakeCryptAuthGCMManager gcm_manager_;

  std::unique_ptr<TestCryptAuthDeviceManager> device_manager_;

  std::vector<cryptauth::ExternalDeviceInfo> devices_in_response_;

  cryptauth::GetMyDevicesResponse get_my_devices_response_;

  cryptauth::GetMyDevicesRequest get_my_devices_request_;

  CryptAuthClient::GetMyDevicesCallback success_callback_;

  CryptAuthClient::ErrorCallback error_callback_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncCryptAuthDeviceManagerImplTest);
};

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest, RegisterPrefs) {
  TestingPrefServiceSimple pref_service;
  CryptAuthDeviceManager::RegisterPrefs(pref_service.registry());
  EXPECT_TRUE(pref_service.FindPreference(
      prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds));
  EXPECT_TRUE(pref_service.FindPreference(
      prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure));
  EXPECT_TRUE(pref_service.FindPreference(prefs::kCryptAuthDeviceSyncReason));
  EXPECT_TRUE(
      pref_service.FindPreference(prefs::kCryptAuthDeviceSyncUnlockKeys));
}

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest, GetSyncState) {
  device_manager_->Start();

  ON_CALL(*sync_scheduler(), GetStrategy())
      .WillByDefault(Return(SyncScheduler::Strategy::PERIODIC_REFRESH));
  EXPECT_FALSE(device_manager_->IsRecoveringFromFailure());

  ON_CALL(*sync_scheduler(), GetStrategy())
      .WillByDefault(Return(SyncScheduler::Strategy::AGGRESSIVE_RECOVERY));
  EXPECT_TRUE(device_manager_->IsRecoveringFromFailure());

  base::TimeDelta time_to_next_sync = base::TimeDelta::FromMinutes(60);
  ON_CALL(*sync_scheduler(), GetTimeToNextSync())
      .WillByDefault(Return(time_to_next_sync));
  EXPECT_EQ(time_to_next_sync, device_manager_->GetTimeToNextAttempt());

  ON_CALL(*sync_scheduler(), GetSyncState())
      .WillByDefault(Return(SyncScheduler::SyncState::SYNC_IN_PROGRESS));
  EXPECT_TRUE(device_manager_->IsSyncInProgress());

  ON_CALL(*sync_scheduler(), GetSyncState())
      .WillByDefault(Return(SyncScheduler::SyncState::WAITING_FOR_REFRESH));
  EXPECT_FALSE(device_manager_->IsSyncInProgress());
}

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest, InitWithDefaultPrefs) {
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::FromDoubleT(kInitialTimeNowSeconds));
  base::TimeDelta elapsed_time = clock.Now() - base::Time::FromDoubleT(0);

  TestingPrefServiceSimple pref_service;
  CryptAuthDeviceManager::RegisterPrefs(pref_service.registry());

  TestCryptAuthDeviceManager device_manager(&clock, client_factory_.get(),
                                            &gcm_manager_, &pref_service);

  EXPECT_CALL(
      *(device_manager.GetSyncScheduler()),
      Start(elapsed_time, SyncScheduler::Strategy::AGGRESSIVE_RECOVERY));
  device_manager.Start();
  EXPECT_TRUE(device_manager.GetLastSyncTime().is_null());
  EXPECT_EQ(0u, device_manager.GetSyncedDevices().size());
}

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest, InitWithExistingPrefs) {
  EXPECT_CALL(
      *sync_scheduler(),
      Start(clock_.Now() - base::Time::FromDoubleT(kLastSyncTimeSeconds),
            SyncScheduler::Strategy::PERIODIC_REFRESH));

  device_manager_->Start();
  EXPECT_EQ(base::Time::FromDoubleT(kLastSyncTimeSeconds),
            device_manager_->GetLastSyncTime());

  auto synced_devices = device_manager_->GetSyncedDevices();
  ASSERT_EQ(1u, synced_devices.size());
  EXPECT_EQ(kStoredPublicKey, synced_devices[0].public_key());
  EXPECT_EQ(kStoredDeviceName, synced_devices[0].friendly_device_name());
  EXPECT_EQ(kStoredBluetoothAddress, synced_devices[0].bluetooth_address());
  EXPECT_EQ(kStoredUnlockable, synced_devices[0].unlockable());
}

// cryptauth::ExternalDeviceInfos's |unlock_key| and |mobile_hotspot_supported|
// fields are deprecated, but it may be the case that after an update to Chrome,
// the prefs reflect the old style of using these deprecated fields, instead of
// software features. This test ensures the CryptAuthDeviceManager considers
// these deprecated booleans, and populates the correct software features.
TEST_F(
    DeviceSyncCryptAuthDeviceManagerImplTest,
    InitWithExistingPrefs_MigrateDeprecateBooleansFromPrefsToSoftwareFeature) {
  ListPrefUpdate update_clear(&pref_service_,
                              prefs::kCryptAuthDeviceSyncUnlockKeys);
  update_clear.Get()->Clear();

  // Simulate a deprecated device being persisted to prefs.
  auto device_dictionary = std::make_unique<base::DictionaryValue>();
  std::string public_key_b64;
  base::Base64UrlEncode(kStoredPublicKey,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &public_key_b64);
  device_dictionary->SetString("public_key", public_key_b64);
  device_dictionary->SetBoolean("unlock_key", true);
  device_dictionary->SetBoolean("mobile_hotspot_supported", true);
  device_dictionary->Set("software_features",
                         std::make_unique<base::DictionaryValue>());

  ListPrefUpdate update(&pref_service_, prefs::kCryptAuthDeviceSyncUnlockKeys);
  update.Get()->Append(std::move(device_dictionary));

  device_manager_.reset(new TestCryptAuthDeviceManager(
      &clock_, client_factory_.get(), &gcm_manager_, &pref_service_));
  device_manager_->Start();

  // Ensure that the deprecated booleans are not exposed in the final
  // cryptauth::ExternalDeviceInfo, but rather in the correct software features.
  auto synced_devices = device_manager_->GetSyncedDevices();
  ASSERT_EQ(1u, synced_devices.size());
  EXPECT_EQ(kStoredPublicKey, synced_devices[0].public_key());
  EXPECT_FALSE(synced_devices[0].unlock_key());
  EXPECT_FALSE(synced_devices[0].mobile_hotspot_supported());

  EXPECT_EQ(2, synced_devices[0].supported_software_features().size());
  EXPECT_TRUE(
      base::Contains(synced_devices[0].supported_software_features(),
                     SoftwareFeatureEnumToString(
                         cryptauth::SoftwareFeature::EASY_UNLOCK_HOST)));
  EXPECT_TRUE(
      base::Contains(synced_devices[0].supported_software_features(),
                     SoftwareFeatureEnumToString(
                         cryptauth::SoftwareFeature::MAGIC_TETHER_HOST)));
  EXPECT_EQ(1, synced_devices[0].enabled_software_features().size());
  EXPECT_TRUE(
      base::Contains(synced_devices[0].enabled_software_features(),
                     SoftwareFeatureEnumToString(
                         cryptauth::SoftwareFeature::EASY_UNLOCK_HOST)));
}

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest, SyncSucceedsForFirstTime) {
  pref_service_.ClearPref(prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds);
  device_manager_->Start();

  FireSchedulerForSync(cryptauth::INVOCATION_REASON_INITIALIZATION);
  ASSERT_FALSE(success_callback_.is_null());

  clock_.SetNow(base::Time::FromDoubleT(kLaterTimeNowSeconds));
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));

  success_callback_.Run(get_my_devices_response_);
  EXPECT_EQ(clock_.Now(), device_manager_->GetLastSyncTime());

  ExpectSyncedDevicesAndPrefAreEqual(
      devices_in_response_, device_manager_->GetSyncedDevices(), pref_service_);
}

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest, ForceSync) {
  device_manager_->Start();

  EXPECT_CALL(*sync_scheduler(), ForceSync());
  device_manager_->ForceSyncNow(cryptauth::INVOCATION_REASON_MANUAL);

  FireSchedulerForSync(cryptauth::INVOCATION_REASON_MANUAL);

  clock_.SetNow(base::Time::FromDoubleT(kLaterTimeNowSeconds));
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(get_my_devices_response_);
  EXPECT_EQ(clock_.Now(), device_manager_->GetLastSyncTime());

  ExpectSyncedDevicesAndPrefAreEqual(
      devices_in_response_, device_manager_->GetSyncedDevices(), pref_service_);
}

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest, ForceSyncFailsThenSucceeds) {
  device_manager_->Start();
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure));
  base::Time old_sync_time = device_manager_->GetLastSyncTime();

  // The first force sync fails.
  EXPECT_CALL(*sync_scheduler(), ForceSync());
  device_manager_->ForceSyncNow(cryptauth::INVOCATION_REASON_MANUAL);
  FireSchedulerForSync(cryptauth::INVOCATION_REASON_MANUAL);
  clock_.SetNow(base::Time::FromDoubleT(kLaterTimeNowSeconds));
  EXPECT_CALL(*this,
              OnSyncFinishedProxy(
                  CryptAuthDeviceManager::SyncResult::FAILURE,
                  CryptAuthDeviceManager::DeviceChangeResult::UNCHANGED));
  error_callback_.Run(NetworkRequestError::kEndpointNotFound);
  EXPECT_EQ(old_sync_time, device_manager_->GetLastSyncTime());
  EXPECT_TRUE(pref_service_.GetBoolean(
      prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure));
  EXPECT_EQ(static_cast<int>(cryptauth::INVOCATION_REASON_MANUAL),
            pref_service_.GetInteger(prefs::kCryptAuthDeviceSyncReason));

  // The second recovery sync succeeds.
  ON_CALL(*sync_scheduler(), GetStrategy())
      .WillByDefault(Return(SyncScheduler::Strategy::AGGRESSIVE_RECOVERY));
  FireSchedulerForSync(cryptauth::INVOCATION_REASON_MANUAL);
  clock_.SetNow(base::Time::FromDoubleT(kLaterTimeNowSeconds + 30));
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(get_my_devices_response_);
  EXPECT_EQ(clock_.Now(), device_manager_->GetLastSyncTime());

  ExpectSyncedDevicesAndPrefAreEqual(
      devices_in_response_, device_manager_->GetSyncedDevices(), pref_service_);

  EXPECT_FLOAT_EQ(
      clock_.Now().ToDoubleT(),
      pref_service_.GetDouble(prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds));
  EXPECT_EQ(static_cast<int>(cryptauth::INVOCATION_REASON_UNKNOWN),
            pref_service_.GetInteger(prefs::kCryptAuthDeviceSyncReason));
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure));
}

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest,
       PeriodicSyncFailsThenSucceeds) {
  device_manager_->Start();
  base::Time old_sync_time = device_manager_->GetLastSyncTime();

  // The first periodic sync fails.
  FireSchedulerForSync(cryptauth::INVOCATION_REASON_PERIODIC);
  clock_.SetNow(base::Time::FromDoubleT(kLaterTimeNowSeconds));
  EXPECT_CALL(*this,
              OnSyncFinishedProxy(
                  CryptAuthDeviceManager::SyncResult::FAILURE,
                  CryptAuthDeviceManager::DeviceChangeResult::UNCHANGED));
  error_callback_.Run(NetworkRequestError::kAuthenticationError);
  EXPECT_EQ(old_sync_time, device_manager_->GetLastSyncTime());
  EXPECT_TRUE(pref_service_.GetBoolean(
      prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure));

  // The second recovery sync succeeds.
  ON_CALL(*sync_scheduler(), GetStrategy())
      .WillByDefault(Return(SyncScheduler::Strategy::AGGRESSIVE_RECOVERY));
  FireSchedulerForSync(cryptauth::INVOCATION_REASON_FAILURE_RECOVERY);
  clock_.SetNow(base::Time::FromDoubleT(kLaterTimeNowSeconds + 30));
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(get_my_devices_response_);
  EXPECT_EQ(clock_.Now(), device_manager_->GetLastSyncTime());

  ExpectSyncedDevicesAndPrefAreEqual(
      devices_in_response_, device_manager_->GetSyncedDevices(), pref_service_);

  EXPECT_FLOAT_EQ(
      clock_.Now().ToDoubleT(),
      pref_service_.GetDouble(prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds));
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure));
}

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest, SyncSameDevice) {
  device_manager_->Start();
  auto original_devices = device_manager_->GetSyncedDevices();

  // Sync new devices.
  FireSchedulerForSync(cryptauth::INVOCATION_REASON_PERIODIC);
  ASSERT_FALSE(success_callback_.is_null());
  EXPECT_CALL(*this,
              OnSyncFinishedProxy(
                  CryptAuthDeviceManager::SyncResult::SUCCESS,
                  CryptAuthDeviceManager::DeviceChangeResult::UNCHANGED));

  // Sync the same device.
  cryptauth::ExternalDeviceInfo synced_device;
  synced_device.set_public_key(kStoredPublicKey);
  synced_device.set_friendly_device_name(kStoredDeviceName);
  synced_device.set_bluetooth_address(kStoredBluetoothAddress);
  synced_device.set_unlockable(kStoredUnlockable);
  cryptauth::GetMyDevicesResponse get_my_devices_response;
  get_my_devices_response.add_devices()->CopyFrom(synced_device);
  success_callback_.Run(get_my_devices_response);

  // Check that devices are still the same after sync.
  ExpectSyncedDevicesAndPrefAreEqual(
      original_devices, device_manager_->GetSyncedDevices(), pref_service_);
}

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest, SyncEmptyDeviceList) {
  cryptauth::GetMyDevicesResponse empty_response;

  device_manager_->Start();
  EXPECT_EQ(1u, device_manager_->GetSyncedDevices().size());

  FireSchedulerForSync(cryptauth::INVOCATION_REASON_PERIODIC);
  ASSERT_FALSE(success_callback_.is_null());
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(empty_response);

  ExpectSyncedDevicesAndPrefAreEqual(
      std::vector<cryptauth::ExternalDeviceInfo>(),
      device_manager_->GetSyncedDevices(), pref_service_);
}

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest, SyncThreeDevices) {
  cryptauth::GetMyDevicesResponse response(get_my_devices_response_);
  cryptauth::ExternalDeviceInfo synced_device2;
  synced_device2.set_public_key("new public key");
  synced_device2.set_friendly_device_name("new device name");
  synced_device2.set_bluetooth_address("aa:bb:cc:dd:ee:ff");
  synced_device2.add_supported_software_features(SoftwareFeatureEnumToString(
      cryptauth::SoftwareFeature::EASY_UNLOCK_HOST));
  synced_device2.add_enabled_software_features(SoftwareFeatureEnumToString(
      cryptauth::SoftwareFeature::EASY_UNLOCK_HOST));

  response.add_devices()->CopyFrom(synced_device2);

  std::vector<cryptauth::ExternalDeviceInfo> expected_devices;
  expected_devices.push_back(devices_in_response_[0]);
  expected_devices.push_back(devices_in_response_[1]);
  expected_devices.push_back(synced_device2);

  device_manager_->Start();
  EXPECT_EQ(1u, device_manager_->GetSyncedDevices().size());
  EXPECT_EQ(
      1u,
      pref_service_.GetList(prefs::kCryptAuthDeviceSyncUnlockKeys)->GetSize());

  FireSchedulerForSync(cryptauth::INVOCATION_REASON_PERIODIC);
  ASSERT_FALSE(success_callback_.is_null());
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(response);

  ExpectSyncedDevicesAndPrefAreEqual(
      expected_devices, device_manager_->GetSyncedDevices(), pref_service_);
}

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest, SyncOnGCMPushMessage) {
  device_manager_->Start();

  EXPECT_CALL(*sync_scheduler(), ForceSync());
  gcm_manager_.PushResyncMessage(base::nullopt /* session_id */,
                                 base::nullopt /* feature_type */);

  FireSchedulerForSync(cryptauth::INVOCATION_REASON_SERVER_INITIATED);

  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(get_my_devices_response_);

  ExpectSyncedDevicesAndPrefAreEqual(
      devices_in_response_, device_manager_->GetSyncedDevices(), pref_service_);
}

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest, SyncDeviceWithNoContents) {
  device_manager_->Start();

  EXPECT_CALL(*sync_scheduler(), ForceSync());
  gcm_manager_.PushResyncMessage(base::nullopt /* session_id */,
                                 base::nullopt /* feature_type */);

  FireSchedulerForSync(cryptauth::INVOCATION_REASON_SERVER_INITIATED);

  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(get_my_devices_response_);

  ExpectSyncedDevicesAndPrefAreEqual(
      devices_in_response_, device_manager_->GetSyncedDevices(), pref_service_);
}

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest,
       SyncFullyDetailedExternalDeviceInfos) {
  // First, use a device with only a public key (a public key is the only
  // required field). This ensures devices work properly when they do not have
  // all fields filled out.
  cryptauth::ExternalDeviceInfo device_with_only_public_key;
  device_with_only_public_key.set_public_key("publicKey1");
  device_with_only_public_key.add_supported_software_features(
      SoftwareFeatureEnumToString(
          cryptauth::SoftwareFeature::EASY_UNLOCK_HOST));
  device_with_only_public_key.add_enabled_software_features(
      SoftwareFeatureEnumToString(
          cryptauth::SoftwareFeature::EASY_UNLOCK_HOST));

  // Second, use a device with all fields filled out. This ensures that all
  // device details are properly saved.
  cryptauth::ExternalDeviceInfo device_with_all_fields;
  device_with_all_fields.set_public_key("publicKey2");
  device_with_all_fields.set_friendly_device_name("deviceName");
  device_with_all_fields.set_bluetooth_address("aa:bb:cc:dd:ee:ff");
  device_with_all_fields.set_unlockable(true);
  device_with_all_fields.set_last_update_time_millis(123456789L);
  device_with_all_fields.set_device_type(
      DeviceTypeEnumToString(cryptauth::DeviceType::ANDROID));

  cryptauth::BeaconSeed seed1;
  seed1.set_data(kBeaconSeed1Data);
  seed1.set_start_time_millis(kBeaconSeed1StartTime);
  seed1.set_end_time_millis(kBeaconSeed1EndTime);
  device_with_all_fields.add_beacon_seeds()->CopyFrom(seed1);

  cryptauth::BeaconSeed seed2;
  seed2.set_data(kBeaconSeed2Data);
  seed2.set_start_time_millis(kBeaconSeed2StartTime);
  seed2.set_end_time_millis(kBeaconSeed2EndTime);
  device_with_all_fields.add_beacon_seeds()->CopyFrom(seed2);

  device_with_all_fields.set_arc_plus_plus(true);
  device_with_all_fields.set_pixel_phone(true);

  device_with_all_fields.add_supported_software_features(
      SoftwareFeatureEnumToString(
          cryptauth::SoftwareFeature::EASY_UNLOCK_HOST));
  device_with_all_fields.add_supported_software_features(
      SoftwareFeatureEnumToString(
          cryptauth::SoftwareFeature::MAGIC_TETHER_HOST));

  device_with_all_fields.add_enabled_software_features(
      SoftwareFeatureEnumToString(
          cryptauth::SoftwareFeature::EASY_UNLOCK_HOST));

  std::vector<cryptauth::ExternalDeviceInfo> expected_devices;
  expected_devices.push_back(device_with_only_public_key);
  expected_devices.push_back(device_with_all_fields);

  device_manager_->Start();
  FireSchedulerForSync(cryptauth::INVOCATION_REASON_PERIODIC);
  ASSERT_FALSE(success_callback_.is_null());
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));

  cryptauth::GetMyDevicesResponse response;
  response.add_devices()->CopyFrom(device_with_only_public_key);
  response.add_devices()->CopyFrom(device_with_all_fields);
  success_callback_.Run(response);

  ExpectSyncedDevicesAndPrefAreEqual(
      expected_devices, device_manager_->GetSyncedDevices(), pref_service_);
}

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest, SubsetsOfSyncedDevices) {
  device_manager_->Start();

  FireSchedulerForSync(cryptauth::INVOCATION_REASON_PERIODIC);
  ASSERT_FALSE(success_callback_.is_null());
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(get_my_devices_response_);

  // All synced devices.
  ExpectSyncedDevicesAndPrefAreEqual(
      devices_in_response_, device_manager_->GetSyncedDevices(), pref_service_);

  // Only unlock keys.
  ExpectSyncedDevicesAreEqual(
      std::vector<cryptauth::ExternalDeviceInfo>(1, devices_in_response_[0]),
      device_manager_->GetUnlockKeys());

  // Only tether hosts.
  ExpectSyncedDevicesAreEqual(
      std::vector<cryptauth::ExternalDeviceInfo>(1, devices_in_response_[1]),
      device_manager_->GetTetherHosts());
}

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest,
       TestDeprecatedBooleansArePersistedOnlyAsSoftwareFeatures) {
  device_manager_->Start();

  cryptauth::ExternalDeviceInfo device;
  device.set_public_key("public key");
  device.set_friendly_device_name("deprecated device");
  device.set_unlock_key(true);
  device.set_mobile_hotspot_supported(true);

  devices_in_response_.push_back(device);
  get_my_devices_response_.add_devices()->CopyFrom(device);

  FireSchedulerForSync(cryptauth::INVOCATION_REASON_PERIODIC);
  ASSERT_FALSE(success_callback_.is_null());
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(get_my_devices_response_);

  cryptauth::ExternalDeviceInfo synced_device =
      device_manager_->GetSyncedDevices()[2];

  EXPECT_FALSE(synced_device.unlock_key());
  EXPECT_FALSE(synced_device.mobile_hotspot_supported());

  EXPECT_TRUE(
      base::Contains(synced_device.supported_software_features(),
                     SoftwareFeatureEnumToString(
                         cryptauth::SoftwareFeature::EASY_UNLOCK_HOST)));
  EXPECT_TRUE(
      base::Contains(synced_device.enabled_software_features(),
                     SoftwareFeatureEnumToString(
                         cryptauth::SoftwareFeature::EASY_UNLOCK_HOST)));
  EXPECT_TRUE(
      base::Contains(synced_device.supported_software_features(),
                     SoftwareFeatureEnumToString(
                         cryptauth::SoftwareFeature::MAGIC_TETHER_HOST)));
  EXPECT_FALSE(
      base::Contains(synced_device.enabled_software_features(),
                     SoftwareFeatureEnumToString(
                         cryptauth::SoftwareFeature::MAGIC_TETHER_HOST)));
}

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest,
       TestIgnoreDeprecatedBooleansIfSoftwareFeaturesArePresent) {
  device_manager_->Start();

  cryptauth::ExternalDeviceInfo device;
  device.set_public_key("public key");
  device.set_friendly_device_name("deprecated device");
  device.set_unlock_key(false);
  device.set_mobile_hotspot_supported(false);

  device.add_supported_software_features(SoftwareFeatureEnumToString(
      cryptauth::SoftwareFeature::EASY_UNLOCK_HOST));
  device.add_enabled_software_features(SoftwareFeatureEnumToString(
      cryptauth::SoftwareFeature::EASY_UNLOCK_HOST));
  device.add_supported_software_features(SoftwareFeatureEnumToString(
      cryptauth::SoftwareFeature::MAGIC_TETHER_HOST));

  devices_in_response_.push_back(device);
  get_my_devices_response_.add_devices()->CopyFrom(device);

  FireSchedulerForSync(cryptauth::INVOCATION_REASON_PERIODIC);
  ASSERT_FALSE(success_callback_.is_null());
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(get_my_devices_response_);

  cryptauth::ExternalDeviceInfo synced_device =
      device_manager_->GetSyncedDevices()[2];

  EXPECT_FALSE(synced_device.unlock_key());
  EXPECT_FALSE(synced_device.mobile_hotspot_supported());

  EXPECT_TRUE(
      base::Contains(synced_device.supported_software_features(),
                     SoftwareFeatureEnumToString(
                         cryptauth::SoftwareFeature::EASY_UNLOCK_HOST)));
  EXPECT_TRUE(
      base::Contains(synced_device.enabled_software_features(),
                     SoftwareFeatureEnumToString(
                         cryptauth::SoftwareFeature::EASY_UNLOCK_HOST)));
  EXPECT_TRUE(
      base::Contains(synced_device.supported_software_features(),
                     SoftwareFeatureEnumToString(
                         cryptauth::SoftwareFeature::MAGIC_TETHER_HOST)));
  EXPECT_FALSE(
      base::Contains(synced_device.enabled_software_features(),
                     SoftwareFeatureEnumToString(
                         cryptauth::SoftwareFeature::MAGIC_TETHER_HOST)));
}

// Regression test for crbug.com/888031.
TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest,
       TestMigrateFromIntToStringSoftwareFeaturePrefRepresentation) {
  device_manager_->Start();

  cryptauth::ExternalDeviceInfo device;
  device.set_public_key("public key");
  device.set_friendly_device_name("deprecated device");

  // Simulate how older client versions persisted SoftwareFeatures as ints.
  device.add_supported_software_features(
      std::to_string(cryptauth::SoftwareFeature::EASY_UNLOCK_HOST));
  device.add_enabled_software_features(
      std::to_string(cryptauth::SoftwareFeature::EASY_UNLOCK_HOST));
  device.add_supported_software_features(
      std::to_string(cryptauth::SoftwareFeature::MAGIC_TETHER_HOST));

  devices_in_response_.push_back(device);
  get_my_devices_response_.add_devices()->CopyFrom(device);

  FireSchedulerForSync(cryptauth::INVOCATION_REASON_PERIODIC);
  ASSERT_FALSE(success_callback_.is_null());
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(get_my_devices_response_);

  cryptauth::ExternalDeviceInfo synced_device =
      device_manager_->GetSyncedDevices()[2];

  // CryptAuthDeviceManager should recognize that the SoftwareFeature prefs had
  // been stored as refs, and convert them to their full string representations.
  EXPECT_TRUE(
      base::Contains(synced_device.supported_software_features(),
                     SoftwareFeatureEnumToString(
                         cryptauth::SoftwareFeature::EASY_UNLOCK_HOST)));
  EXPECT_TRUE(
      base::Contains(synced_device.enabled_software_features(),
                     SoftwareFeatureEnumToString(
                         cryptauth::SoftwareFeature::EASY_UNLOCK_HOST)));
  EXPECT_TRUE(
      base::Contains(synced_device.supported_software_features(),
                     SoftwareFeatureEnumToString(
                         cryptauth::SoftwareFeature::MAGIC_TETHER_HOST)));
  EXPECT_FALSE(
      base::Contains(synced_device.enabled_software_features(),
                     SoftwareFeatureEnumToString(
                         cryptauth::SoftwareFeature::MAGIC_TETHER_HOST)));
}

TEST_F(DeviceSyncCryptAuthDeviceManagerImplTest,
       MetricsForEnabledAndNotSupported) {
  cryptauth::ExternalDeviceInfo enabled_not_supported_device;
  enabled_not_supported_device.set_public_key("new public key");
  enabled_not_supported_device.add_supported_software_features(
      SoftwareFeatureEnumToString(
          cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST));
  enabled_not_supported_device.add_enabled_software_features(
      SoftwareFeatureEnumToString(
          cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST));

  // EASY_UNLOCK_HOST is a special case; it is allowed to not be marked as
  // supported, but still be set as enabled.
  enabled_not_supported_device.add_enabled_software_features(
      SoftwareFeatureEnumToString(
          cryptauth::SoftwareFeature::EASY_UNLOCK_HOST));

  // These will fail because they are not set as supported.
  enabled_not_supported_device.add_enabled_software_features(
      SoftwareFeatureEnumToString(
          cryptauth::SoftwareFeature::MAGIC_TETHER_HOST));
  enabled_not_supported_device.add_enabled_software_features(
      "MyUnknownFeature");

  cryptauth::GetMyDevicesResponse response;
  response.add_devices()->CopyFrom(enabled_not_supported_device);

  device_manager_->Start();
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "CryptAuth.DeviceSyncSoftwareFeaturesResult", 0);
  histogram_tester.ExpectTotalCount(
      "CryptAuth.DeviceSyncSoftwareFeaturesResult.Failures", 0);

  EXPECT_EQ(1u, device_manager_->GetSyncedDevices().size());
  FireSchedulerForSync(cryptauth::INVOCATION_REASON_PERIODIC);
  ASSERT_FALSE(success_callback_.is_null());
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(response);

  histogram_tester.ExpectTotalCount(
      "CryptAuth.DeviceSyncSoftwareFeaturesResult", 4);
  histogram_tester.ExpectBucketCount<bool>(
      "CryptAuth.DeviceSyncSoftwareFeaturesResult", true, 2);
  histogram_tester.ExpectBucketCount<bool>(
      "CryptAuth.DeviceSyncSoftwareFeaturesResult", false, 2);

  histogram_tester.ExpectTotalCount(
      "CryptAuth.DeviceSyncSoftwareFeaturesResult.Failures", 2);
  histogram_tester.ExpectBucketCount<cryptauth::SoftwareFeature>(
      "CryptAuth.DeviceSyncSoftwareFeaturesResult.Failures",
      cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST, 0);
  histogram_tester.ExpectBucketCount<cryptauth::SoftwareFeature>(
      "CryptAuth.DeviceSyncSoftwareFeaturesResult.Failures",
      cryptauth::SoftwareFeature::EASY_UNLOCK_HOST, 0);
  histogram_tester.ExpectBucketCount<cryptauth::SoftwareFeature>(
      "CryptAuth.DeviceSyncSoftwareFeaturesResult.Failures",
      cryptauth::SoftwareFeature::MAGIC_TETHER_HOST, 1);
  histogram_tester.ExpectBucketCount<cryptauth::SoftwareFeature>(
      "CryptAuth.DeviceSyncSoftwareFeaturesResult.Failures",
      cryptauth::SoftwareFeature::UNKNOWN_FEATURE, 1);
}

}  // namespace device_sync

}  // namespace chromeos
