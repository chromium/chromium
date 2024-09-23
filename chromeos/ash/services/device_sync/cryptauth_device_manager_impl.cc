// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_device_manager_impl.h"

#include <stddef.h>

#include <memory>
#include <stdexcept>
#include <utility>

#include "base/base64url.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/services/device_sync/cryptauth_client.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "chromeos/ash/services/device_sync/proto/enum_util.h"
#include "chromeos/ash/services/device_sync/sync_scheduler_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ash::device_sync {

namespace {

// The normal period between successful syncs, in hours.
const int kRefreshPeriodHours = 24;

// A more aggressive period between sync attempts to recover when the last
// sync attempt fails, in minutes. This is a base time that increases for each
// subsequent failure.
const int kDeviceSyncBaseRecoveryPeriodMinutes = 10;

// The bound on the amount to jitter the period between syncs.
const double kDeviceSyncMaxJitterRatio = 0.2;

// Keys for cryptauth::ExternalDeviceInfo dictionaries that are stored in the
// user's prefs.
const char kExternalDeviceKeyPublicKey[] = "public_key";
const char kExternalDeviceKeyDeviceName[] = "device_name";
const char kExternalDeviceKeyBluetoothAddress[] = "bluetooth_address";
const char kExternalDeviceKeyUnlockKey[] = "unlock_key";
const char kExternalDeviceKeyUnlockable[] = "unlockable";
const char kExternalDeviceKeyLastUpdateTimeMillis[] = "last_update_time_millis";
const char kExternalDeviceKeyMobileHotspotSupported[] =
    "mobile_hotspot_supported";
const char kExternalDeviceKeyDeviceType[] = "device_type";
const char kExternalDeviceKeyBeaconSeeds[] = "beacon_seeds";
const char kExternalDeviceKeyArcPlusPlus[] = "arc_plus_plus";
const char kExternalDeviceKeyPixelPhone[] = "pixel_phone";
const char kExternalDeviceKeyNoPiiDeviceName[] = "no_pii_device_name";

// Keys for cryptauth::ExternalDeviceInfo's BeaconSeed.
const char kExternalDeviceKeyBeaconSeedData[] = "beacon_seed_data";
const char kExternalDeviceKeyBeaconSeedStartMs[] = "beacon_seed_start_ms";
const char kExternalDeviceKeyBeaconSeedEndMs[] = "beacon_seed_end_ms";

// Keys specific to the dictionary which stores cryptauth::ExternalDeviceInfo
// info.
const char kDictionaryKeySoftwareFeatures[] = "software_features";

// Converts BeaconSeed protos to a list value that can be stored in user prefs.
base::Value::List BeaconSeedsToListValue(
    const google::protobuf::RepeatedPtrField<cryptauth::BeaconSeed>& seeds) {
  base::Value::List list;

  for (int i = 0; i < seeds.size(); i++) {
    cryptauth::BeaconSeed seed = seeds.Get(i);

    if (!seed.has_data() || !seed.has_start_time_millis() ||
        !seed.has_end_time_millis()) {
      PA_LOG(WARNING) << "Unable to serialize BeaconSeed due to missing data; "
                      << "skipping.";
      continue;
    }

    // Note that the |BeaconSeed|s' data is stored in Base64Url encoding because
    // dictionary values must be valid UTF8 strings.
    std::string seed_data_b64;
    base::Base64UrlEncode(seed.data(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &seed_data_b64);
    auto beacon_seed_value =
        base::Value::Dict()
            .Set(kExternalDeviceKeyBeaconSeedData, seed_data_b64)
            // Set the timestamps as string representations of their numeric
            // value since there is no notion of a base::LongValue.
            .Set(kExternalDeviceKeyBeaconSeedStartMs,
                 base::NumberToString(seed.start_time_millis()))
            .Set(kExternalDeviceKeyBeaconSeedEndMs,
                 base::NumberToString(seed.end_time_millis()));

    list.Append(std::move(beacon_seed_value));
  }

  return list;
}

void RecordDeviceSyncSoftwareFeaturesResult(
    bool success,
    cryptauth::SoftwareFeature software_feature) {
  UMA_HISTOGRAM_BOOLEAN("CryptAuth.DeviceSyncSoftwareFeaturesResult", success);
  if (!success) {
    UMA_HISTOGRAM_ENUMERATION(
        "CryptAuth.DeviceSyncSoftwareFeaturesResult.Failures", software_feature,
        cryptauth::SoftwareFeature_ARRAYSIZE);
  }
}

void RecordDeviceSyncResult(bool success) {
  UMA_HISTOGRAM_BOOLEAN("CryptAuth.DeviceSync.Result", success);
}

// Converts supported and enabled SoftwareFeature protos to a single dictionary
// value that can be stored in user prefs.
base::Value::Dict SupportedAndEnabledSoftwareFeaturesToDictionaryValue(
    const google::protobuf::RepeatedPtrField<std::string>&
        supported_software_features,
    const google::protobuf::RepeatedPtrField<std::string>&
        enabled_software_features,
    bool legacy_unlock_key,
    bool legacy_mobile_hotspot_supported) {
  base::Value::Dict dictionary;

  for (const auto& supported_software_feature : supported_software_features) {
    dictionary.Set(
        supported_software_feature,
        static_cast<int>(multidevice::SoftwareFeatureState::kSupported));
  }

  for (const auto& enabled_software_feature : enabled_software_features) {
    std::string software_feature_key = enabled_software_feature;
    cryptauth::SoftwareFeature software_feature =
        SoftwareFeatureStringToEnum(software_feature_key);

    std::optional<int> software_feature_state =
        dictionary.FindInt(software_feature_key);
    bool software_feature_success_result = true;
    if (!software_feature_state ||
        static_cast<multidevice::SoftwareFeatureState>(
            *software_feature_state) !=
            multidevice::SoftwareFeatureState::kSupported) {
      if (software_feature == cryptauth::SoftwareFeature::EASY_UNLOCK_HOST) {
        // Allow this known special-case for legacy purposes; fall-through to
        // logic which marks this device as enabled.
        PA_LOG(VERBOSE) << "Encountered legacy case: feature EASY_UNLOCK_HOST "
                           "is marked as supported but not enabled. Setting as "
                           "enabled.";
      } else {
        PA_LOG(ERROR) << "A feature is marked as enabled but not as supported: "
                      << software_feature_key << ". Not setting as enabled.";
        software_feature_success_result = false;
      }
    }

    RecordDeviceSyncSoftwareFeaturesResult(
        software_feature_success_result /* success */, software_feature);

    dictionary.Set(
        software_feature_key,
        static_cast<int>(multidevice::SoftwareFeatureState::kEnabled));
  }

  // If software features for EASY_UNLOCK_HOST or MAGIC_TETHER_HOST have not
  // been set, check to see if the deprecated corresponding booleans are
  // enabled. This can happen if the CryptAuth server is not yet serving
  // software features, and only serving the deprecated booleans.
  std::string software_feature_key =
      SoftwareFeatureEnumToString(cryptauth::SoftwareFeature::EASY_UNLOCK_HOST);
  if (legacy_unlock_key && !dictionary.FindInt(software_feature_key)) {
    dictionary.Set(
        software_feature_key,
        static_cast<int>(multidevice::SoftwareFeatureState::kEnabled));
  }
  software_feature_key = SoftwareFeatureEnumToString(
      cryptauth::SoftwareFeature::MAGIC_TETHER_HOST);
  if (legacy_mobile_hotspot_supported &&
      !dictionary.FindInt(software_feature_key)) {
    dictionary.Set(
        software_feature_key,
        static_cast<int>(multidevice::SoftwareFeatureState::kSupported));
  }

  return dictionary;
}

// Converts an unlock key proto to a dictionary that can be stored in user
// prefs.
base::Value::Dict UnlockKeyToDictionary(
    const cryptauth::ExternalDeviceInfo& device) {
  base::Value::Dict dictionary;

  // The device public key is a required value.
  if (!device.has_public_key())
    return dictionary;

  // Note that the device public key, name, and Bluetooth addresses are stored
  // in Base64Url form because dictionary values must be valid UTF8 strings.

  std::string public_key_b64;
  base::Base64UrlEncode(device.public_key(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &public_key_b64);
  dictionary.Set(kExternalDeviceKeyPublicKey, public_key_b64);

  if (device.has_friendly_device_name()) {
    std::string device_name_b64;
    base::Base64UrlEncode(device.friendly_device_name(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &device_name_b64);
    dictionary.Set(kExternalDeviceKeyDeviceName, device_name_b64);
  }

  if (device.has_bluetooth_address()) {
    std::string bluetooth_address_b64;
    base::Base64UrlEncode(device.bluetooth_address(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &bluetooth_address_b64);
    dictionary.Set(kExternalDeviceKeyBluetoothAddress, bluetooth_address_b64);
  }

  if (device.has_unlockable()) {
    dictionary.Set(kExternalDeviceKeyUnlockable, device.unlockable());
  }

  if (device.has_last_update_time_millis()) {
    dictionary.Set(kExternalDeviceKeyLastUpdateTimeMillis,
                   base::NumberToString(device.last_update_time_millis()));
  }

  if (device.has_device_type() &&
      cryptauth::DeviceType_IsValid(
          DeviceTypeStringToEnum(device.device_type()))) {
    dictionary.Set(kExternalDeviceKeyDeviceType,
                   DeviceTypeStringToEnum(device.device_type()));
  }

  dictionary.Set(kExternalDeviceKeyBeaconSeeds,
                 BeaconSeedsToListValue(device.beacon_seeds()));

  if (device.has_arc_plus_plus()) {
    dictionary.Set(kExternalDeviceKeyArcPlusPlus, device.arc_plus_plus());
  }

  if (device.has_pixel_phone()) {
    dictionary.Set(kExternalDeviceKeyPixelPhone, device.pixel_phone());
  }

  if (device.has_no_pii_device_name()) {
    std::string no_pii_device_name_b64;
    base::Base64UrlEncode(device.no_pii_device_name(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &no_pii_device_name_b64);
    dictionary.Set(kExternalDeviceKeyNoPiiDeviceName, no_pii_device_name_b64);
  }

  // In the case that the CryptAuth server is not yet serving SoftwareFeatures,
  // but only the deprecated booleans, |unlock_key| and
  // |mobile_hotspot_supported|, pass in the legacy values in order to correctly
  // populate the SoftwareFeatures.
  bool legacy_unlock_key = device.has_unlock_key() && device.unlock_key();
  bool legacy_mobile_hotspot_supported =
      device.has_mobile_hotspot_supported() &&
      device.mobile_hotspot_supported();
  dictionary.Set(kDictionaryKeySoftwareFeatures,
                 SupportedAndEnabledSoftwareFeaturesToDictionaryValue(
                     device.supported_software_features(),
                     device.enabled_software_features(), legacy_unlock_key,
                     legacy_mobile_hotspot_supported));

  return dictionary;
}

void AddBeaconSeedsToExternalDevice(
    const base::Value::List& beacon_seeds,
    cryptauth::ExternalDeviceInfo* external_device) {
  for (const base::Value& seed_dictionary_value : beacon_seeds) {
    if (!seed_dictionary_value.is_dict()) {
      PA_LOG(WARNING) << "Unable to retrieve BeaconSeed dictionary; "
                      << "skipping.";
      continue;
    }

    const base::Value::Dict& seed_dictionary = seed_dictionary_value.GetDict();
    const std::string* seed_data_b64 =
        seed_dictionary.FindString(kExternalDeviceKeyBeaconSeedData);
    const std::string* start_time_millis_str =
        seed_dictionary.FindString(kExternalDeviceKeyBeaconSeedStartMs);
    const std::string* end_time_millis_str =
        seed_dictionary.FindString(kExternalDeviceKeyBeaconSeedEndMs);

    if (!seed_data_b64 || !start_time_millis_str || !end_time_millis_str) {
      PA_LOG(WARNING) << "Unable to deserialize BeaconSeed due to missing "
                      << "data; skipping.";
      continue;
    }

    // Seed data is returned as raw data, not in Base64 encoding.
    std::string seed_data;
    if (!base::Base64UrlDecode(*seed_data_b64,
                               base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                               &seed_data)) {
      PA_LOG(WARNING) << "Decoding seed data failed.";
      continue;
    }

    int64_t start_time_millis, end_time_millis;
    if (!base::StringToInt64(*start_time_millis_str, &start_time_millis) ||
        !base::StringToInt64(*end_time_millis_str, &end_time_millis)) {
      PA_LOG(WARNING) << "Unable to convert stored timestamp to int64_t: "
                      << start_time_millis_str << " or " << end_time_millis_str;
      continue;
    }

    cryptauth::BeaconSeed* seed = external_device->add_beacon_seeds();
    seed->set_data(seed_data);
    seed->set_start_time_millis(start_time_millis);
    seed->set_end_time_millis(end_time_millis);
  }
}

void AddSoftwareFeaturesToExternalDevice(
    const base::Value::Dict& software_features_dictionary,
    cryptauth::ExternalDeviceInfo* external_device,
    bool old_unlock_key_value_from_prefs,
    bool old_mobile_hotspot_supported_from_prefs) {
  for (const auto it : software_features_dictionary) {
    std::string software_feature = it.first;
    if (SoftwareFeatureStringToEnum(software_feature) ==
        cryptauth::SoftwareFeature::UNKNOWN_FEATURE) {
      // SoftwareFeatures were previously stored in prefs as ints. Now,
      // SoftwareFeatures are stored as full string values, e.g.,
      // "betterTogetherHost". If |it.first| is not recognized by
      // SoftwareFeatureStringToEnum(), that means it is in the old int
      // representation of the SoftwareFeature. Convert it to its full string
      // representation using SoftwareFeatureEnumToString();
      int software_feature_int = std::atoi(software_feature.c_str());
      software_feature = SoftwareFeatureEnumToString(
          static_cast<cryptauth::SoftwareFeature>(software_feature_int));
    }

    if (!it.second.is_int()) {
      PA_LOG(WARNING) << "Unable to retrieve SoftwareFeature; skipping.";
      continue;
    }

    switch (
        static_cast<multidevice::SoftwareFeatureState>(it.second.GetInt())) {
      case multidevice::SoftwareFeatureState::kEnabled:
        external_device->add_enabled_software_features(software_feature);
        [[fallthrough]];
      case multidevice::SoftwareFeatureState::kSupported:
        external_device->add_supported_software_features(software_feature);
        break;
      default:
        break;
    }
  }

  // cryptauth::ExternalDeviceInfos's |unlock_key| and
  // |mobile_hotspot_supported| fields are deprecated, but it may be the case
  // that after an update to Chrome, the prefs reflect the old style of using
  // these deprecated fields, instead of software features. To work around this,
  // these pref values are migrated to software features locally.
  if (old_unlock_key_value_from_prefs) {
    if (!base::Contains(external_device->supported_software_features(),
                        SoftwareFeatureEnumToString(
                            cryptauth::SoftwareFeature::EASY_UNLOCK_HOST))) {
      external_device->add_supported_software_features(
          SoftwareFeatureEnumToString(
              cryptauth::SoftwareFeature::EASY_UNLOCK_HOST));
    }
    if (!base::Contains(external_device->enabled_software_features(),
                        SoftwareFeatureEnumToString(
                            cryptauth::SoftwareFeature::EASY_UNLOCK_HOST))) {
      external_device->add_enabled_software_features(
          SoftwareFeatureEnumToString(
              cryptauth::SoftwareFeature::EASY_UNLOCK_HOST));
    }
  }
  if (old_mobile_hotspot_supported_from_prefs) {
    if (!base::Contains(external_device->supported_software_features(),
                        SoftwareFeatureEnumToString(
                            cryptauth::SoftwareFeature::MAGIC_TETHER_HOST))) {
      external_device->add_supported_software_features(
          SoftwareFeatureEnumToString(
              cryptauth::SoftwareFeature::MAGIC_TETHER_HOST));
    }
  }
}

// Converts an unlock key dictionary stored in user prefs to an
// cryptauth::ExternalDeviceInfo proto. Returns true if the dictionary is valid,
// and the parsed proto is written to |external_device|.
bool DictionaryToUnlockKey(const base::Value::Dict& dictionary,
                           cryptauth::ExternalDeviceInfo* external_device) {
  const std::string* public_key_b64 =
      dictionary.FindString(kExternalDeviceKeyPublicKey);
  if (!public_key_b64) {
    // The public key is a required field, so if it is absent, there is no
    // valid data to return.
    return false;
  }

  std::string public_key;
  if (!base::Base64UrlDecode(*public_key_b64,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &public_key)) {
    // The public key is stored as a Base64Url, so if it cannot be decoded,
    // there is no valid data to return.
    return false;
  }
  external_device->set_public_key(public_key);

  const std::string* device_name_b64 =
      dictionary.FindString(kExternalDeviceKeyDeviceName);
  if (device_name_b64) {
    std::string device_name;
    if (base::Base64UrlDecode(*device_name_b64,
                              base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                              &device_name)) {
      external_device->set_friendly_device_name(device_name);
    }
  }

  const std::string* bluetooth_address_b64 =
      dictionary.FindString(kExternalDeviceKeyBluetoothAddress);
  if (bluetooth_address_b64) {
    std::string bluetooth_address;
    if (base::Base64UrlDecode(*bluetooth_address_b64,
                              base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                              &bluetooth_address)) {
      external_device->set_bluetooth_address(bluetooth_address);
    }
  }

  // TODO(crbug.com/40578817): Migrate |unlockable| into
  // |supported_software_features|.
  std::optional<bool> unlockable =
      dictionary.FindBool(kExternalDeviceKeyUnlockable);
  if (unlockable.has_value())
    external_device->set_unlockable(unlockable.value());

  const std::string* last_update_time_millis_str =
      dictionary.FindString(kExternalDeviceKeyLastUpdateTimeMillis);
  if (last_update_time_millis_str) {
    int64_t last_update_time_millis;
    if (base::StringToInt64(*last_update_time_millis_str,
                            &last_update_time_millis)) {
      external_device->set_last_update_time_millis(last_update_time_millis);
    } else {
      PA_LOG(WARNING) << "Unable to convert stored update time to int64_t: "
                      << *last_update_time_millis_str;
    }
  }

  std::optional<int> device_type =
      dictionary.FindInt(kExternalDeviceKeyDeviceType);
  if (device_type.has_value() &&
      cryptauth::DeviceType_IsValid(device_type.value())) {
    external_device->set_device_type(DeviceTypeEnumToString(
        static_cast<cryptauth::DeviceType>(device_type.value())));
  }

  const base::Value::List* beacon_seeds =
      dictionary.FindList(kExternalDeviceKeyBeaconSeeds);
  if (beacon_seeds)
    AddBeaconSeedsToExternalDevice(*beacon_seeds, external_device);

  std::optional<bool> arc_plus_plus =
      dictionary.FindBool(kExternalDeviceKeyArcPlusPlus);
  if (arc_plus_plus.has_value())
    external_device->set_arc_plus_plus(arc_plus_plus.value());

  std::optional<bool> pixel_phone =
      dictionary.FindBool(kExternalDeviceKeyPixelPhone);
  if (pixel_phone.has_value())
    external_device->set_pixel_phone(pixel_phone.value());

  const std::string* no_pii_device_name_b64 =
      dictionary.FindString(kExternalDeviceKeyNoPiiDeviceName);
  if (no_pii_device_name_b64) {
    std::string no_pii_device_name;
    if (base::Base64UrlDecode(*no_pii_device_name_b64,
                              base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                              &no_pii_device_name)) {
      external_device->set_no_pii_device_name(no_pii_device_name);
    }
  }

  std::optional<bool> unlock_key =
      dictionary.FindBool(kExternalDeviceKeyUnlockKey);
  std::optional<bool> mobile_hotspot_supported =
      dictionary.FindBool(kExternalDeviceKeyMobileHotspotSupported);

  const base::Value::Dict* software_features_dictionary =
      dictionary.FindDict(kDictionaryKeySoftwareFeatures);
  if (software_features_dictionary) {
    AddSoftwareFeaturesToExternalDevice(
        *software_features_dictionary, external_device,
        unlock_key.value_or(false), mobile_hotspot_supported.value_or(false));
  }

  return true;
}

std::unique_ptr<SyncSchedulerImpl> CreateSyncScheduler(
    SyncScheduler::Delegate* delegate) {
  return std::make_unique<SyncSchedulerImpl>(
      delegate, base::Hours(kRefreshPeriodHours),
      base::Minutes(kDeviceSyncBaseRecoveryPeriodMinutes),
      kDeviceSyncMaxJitterRatio, "CryptAuth DeviceSync");
}

}  // namespace

// static
CryptAuthDeviceManagerImpl::Factory*
    CryptAuthDeviceManagerImpl::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<CryptAuthDeviceManager>
CryptAuthDeviceManagerImpl::Factory::Create(
    base::Clock* clock,
    CryptAuthClientFactory* cryptauth_client_factory,
    CryptAuthGCMManager* gcm_manager,
    PrefService* pref_service) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(clock, cryptauth_client_factory,
                                             gcm_manager, pref_service);
  }

  return base::WrapUnique(new CryptAuthDeviceManagerImpl(
      clock, cryptauth_client_factory, gcm_manager, pref_service));
}

// static
void CryptAuthDeviceManagerImpl::Factory::SetFactoryForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

CryptAuthDeviceManagerImpl::Factory::~Factory() = default;

CryptAuthDeviceManagerImpl::CryptAuthDeviceManagerImpl(
    base::Clock* clock,
    CryptAuthClientFactory* cryptauth_client_factory,
    CryptAuthGCMManager* gcm_manager,
    PrefService* pref_service)
    : clock_(clock),
      cryptauth_client_factory_(cryptauth_client_factory),
      gcm_manager_(gcm_manager),
      pref_service_(pref_service),
      scheduler_(CreateSyncScheduler(this)) {
  UpdateUnlockKeysFromPrefs();
}

CryptAuthDeviceManagerImpl::~CryptAuthDeviceManagerImpl() {
  if (gcm_manager_)
    gcm_manager_->RemoveObserver(this);
}

void CryptAuthDeviceManagerImpl::SetSyncSchedulerForTest(
    std::unique_ptr<SyncScheduler> sync_scheduler) {
  scheduler_ = std::move(sync_scheduler);
}

void CryptAuthDeviceManagerImpl::Start() {
  gcm_manager_->AddObserver(this);

  base::Time last_successful_sync = GetLastSyncTime();
  base::TimeDelta elapsed_time_since_last_sync =
      clock_->Now() - last_successful_sync;

  bool is_recovering_from_failure =
      pref_service_->GetBoolean(
          prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure) ||
      last_successful_sync.is_null();

  scheduler_->Start(elapsed_time_since_last_sync,
                    is_recovering_from_failure
                        ? SyncScheduler::Strategy::AGGRESSIVE_RECOVERY
                        : SyncScheduler::Strategy::PERIODIC_REFRESH);
}

void CryptAuthDeviceManagerImpl::ForceSyncNow(
    cryptauth::InvocationReason invocation_reason) {
  pref_service_->SetInteger(prefs::kCryptAuthDeviceSyncReason,
                            invocation_reason);
  scheduler_->ForceSync();
}

base::Time CryptAuthDeviceManagerImpl::GetLastSyncTime() const {
  return base::Time::FromSecondsSinceUnixEpoch(
      pref_service_->GetDouble(prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds));
}

base::TimeDelta CryptAuthDeviceManagerImpl::GetTimeToNextAttempt() const {
  return scheduler_->GetTimeToNextSync();
}

bool CryptAuthDeviceManagerImpl::IsSyncInProgress() const {
  return scheduler_->GetSyncState() ==
         SyncScheduler::SyncState::SYNC_IN_PROGRESS;
}

bool CryptAuthDeviceManagerImpl::IsRecoveringFromFailure() const {
  return scheduler_->GetStrategy() ==
         SyncScheduler::Strategy::AGGRESSIVE_RECOVERY;
}

std::vector<cryptauth::ExternalDeviceInfo>
CryptAuthDeviceManagerImpl::GetSyncedDevices() const {
  return synced_devices_;
}

std::vector<cryptauth::ExternalDeviceInfo>
CryptAuthDeviceManagerImpl::GetUnlockKeys() const {
  std::vector<cryptauth::ExternalDeviceInfo> unlock_keys;
  for (const auto& device : synced_devices_) {
    if (base::Contains(device.enabled_software_features(),
                       SoftwareFeatureEnumToString(
                           cryptauth::SoftwareFeature::EASY_UNLOCK_HOST))) {
      unlock_keys.push_back(device);
    }
  }
  return unlock_keys;
}

std::vector<cryptauth::ExternalDeviceInfo>
CryptAuthDeviceManagerImpl::GetPixelUnlockKeys() const {
  std::vector<cryptauth::ExternalDeviceInfo> unlock_keys;
  for (const auto& device : synced_devices_) {
    if (base::Contains(device.enabled_software_features(),
                       SoftwareFeatureEnumToString(
                           cryptauth::SoftwareFeature::EASY_UNLOCK_HOST)) &&
        device.pixel_phone()) {
      unlock_keys.push_back(device);
    }
  }
  return unlock_keys;
}

std::vector<cryptauth::ExternalDeviceInfo>
CryptAuthDeviceManagerImpl::GetTetherHosts() const {
  std::vector<cryptauth::ExternalDeviceInfo> tether_hosts;
  for (const auto& device : synced_devices_) {
    if (base::Contains(device.supported_software_features(),
                       SoftwareFeatureEnumToString(
                           cryptauth::SoftwareFeature::MAGIC_TETHER_HOST))) {
      tether_hosts.push_back(device);
    }
  }
  return tether_hosts;
}

std::vector<cryptauth::ExternalDeviceInfo>
CryptAuthDeviceManagerImpl::GetPixelTetherHosts() const {
  std::vector<cryptauth::ExternalDeviceInfo> tether_hosts;
  for (const auto& device : synced_devices_) {
    if (base::Contains(device.supported_software_features(),
                       SoftwareFeatureEnumToString(
                           cryptauth::SoftwareFeature::MAGIC_TETHER_HOST)) &&
        device.pixel_phone())
      tether_hosts.push_back(device);
  }
  return tether_hosts;
}

void CryptAuthDeviceManagerImpl::OnGetMyDevicesSuccess(
    const cryptauth::GetMyDevicesResponse& response) {
  // Update the synced devices stored in the user's prefs.
  base::Value::List devices_as_list;

  if (!response.devices().empty()) {
    PA_LOG(VERBOSE) << "Devices were successfully synced.";
    RecordDeviceSyncResult(true /* success */);
  } else {
    RecordDeviceSyncResult(false /* success */);
  }

  for (const auto& device : response.devices()) {
    base::Value::Dict device_dictionary = UnlockKeyToDictionary(device);

    const std::string& device_name = device.has_friendly_device_name()
                                         ? device.friendly_device_name()
                                         : "[unknown]";
    PA_LOG(INFO) << "Synced device '" << device_name
                 << "': " << device_dictionary;

    devices_as_list.Append(std::move(device_dictionary));
  }

  bool unlock_keys_changed =
      devices_as_list !=
      pref_service_->GetList(prefs::kCryptAuthDeviceSyncUnlockKeys);
  {
    ScopedListPrefUpdate update(pref_service_,
                                prefs::kCryptAuthDeviceSyncUnlockKeys);
    *update = std::move(devices_as_list);
  }
  UpdateUnlockKeysFromPrefs();

  // Reset metadata used for scheduling syncing.
  pref_service_->SetBoolean(prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure,
                            false);
  pref_service_->SetDouble(prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds,
                           clock_->Now().InSecondsFSinceUnixEpoch());
  pref_service_->SetInteger(prefs::kCryptAuthDeviceSyncReason,
                            cryptauth::INVOCATION_REASON_UNKNOWN);

  sync_request_->OnDidComplete(true);
  cryptauth_client_.reset();
  sync_request_.reset();
  NotifySyncFinished(SyncResult::SUCCESS, unlock_keys_changed
                                              ? DeviceChangeResult::CHANGED
                                              : DeviceChangeResult::UNCHANGED);
}

void CryptAuthDeviceManagerImpl::OnGetMyDevicesFailure(
    NetworkRequestError error) {
  PA_LOG(ERROR) << "GetMyDevices API failed: " << error;
  pref_service_->SetBoolean(prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure,
                            true);
  sync_request_->OnDidComplete(false);
  cryptauth_client_.reset();
  sync_request_.reset();
  NotifySyncFinished(SyncResult::FAILURE, DeviceChangeResult::UNCHANGED);
  RecordDeviceSyncResult(false /* success */);
}

void CryptAuthDeviceManagerImpl::OnResyncMessage(
    const std::optional<std::string>& session_id,
    const std::optional<CryptAuthFeatureType>& feature_type) {
  ForceSyncNow(cryptauth::INVOCATION_REASON_SERVER_INITIATED);
}

void CryptAuthDeviceManagerImpl::UpdateUnlockKeysFromPrefs() {
  const base::Value::List& unlock_key_list =
      pref_service_->GetList(prefs::kCryptAuthDeviceSyncUnlockKeys);
  synced_devices_.clear();
  for (const auto& it : unlock_key_list) {
    if (it.is_dict()) {
      cryptauth::ExternalDeviceInfo unlock_key;
      if (DictionaryToUnlockKey(it.GetDict(), &unlock_key)) {
        synced_devices_.push_back(unlock_key);
      } else {
        PA_LOG(ERROR) << "Unable to deserialize unlock key dictionary:\n" << it;
      }
    } else {
      PA_LOG(ERROR) << "Can not get dictionary in list of unlock keys:\n"
                    << unlock_key_list;
    }
  }
}

void CryptAuthDeviceManagerImpl::OnSyncRequested(
    std::unique_ptr<SyncScheduler::SyncRequest> sync_request) {
  // If a sync is already in progress, there is no need to start a new one.
  if (sync_request_) {
    sync_request->Cancel();
    return;
  }

  NotifySyncStarted();

  sync_request_ = std::move(sync_request);
  cryptauth_client_ = cryptauth_client_factory_->CreateInstance();

  cryptauth::InvocationReason invocation_reason =
      cryptauth::INVOCATION_REASON_UNKNOWN;

  int reason_stored_in_prefs =
      pref_service_->GetInteger(prefs::kCryptAuthDeviceSyncReason);

  if (cryptauth::InvocationReason_IsValid(reason_stored_in_prefs) &&
      reason_stored_in_prefs != cryptauth::INVOCATION_REASON_UNKNOWN) {
    invocation_reason =
        static_cast<cryptauth::InvocationReason>(reason_stored_in_prefs);
  } else if (GetLastSyncTime().is_null()) {
    invocation_reason = cryptauth::INVOCATION_REASON_INITIALIZATION;
  } else if (IsRecoveringFromFailure()) {
    invocation_reason = cryptauth::INVOCATION_REASON_FAILURE_RECOVERY;
  } else {
    invocation_reason = cryptauth::INVOCATION_REASON_PERIODIC;
  }

  // Syncs due to toggled features, server-initiated requests, and manual
  // "forced" udpates require that fresh data is requested. For all other sync
  // requests, stale reads are allowed. Note that stale reads are allowed in
  // other cases because they are less taxing on the server.
  bool allow_stale_read =
      invocation_reason != cryptauth::INVOCATION_REASON_FEATURE_TOGGLED &&
      invocation_reason != cryptauth::INVOCATION_REASON_SERVER_INITIATED &&
      invocation_reason != cryptauth::INVOCATION_REASON_MANUAL;

  cryptauth::GetMyDevicesRequest request;
  request.set_invocation_reason(invocation_reason);
  request.set_allow_stale_read(allow_stale_read);
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation("cryptauth_get_my_devices",
                                                 "oauth2_api_call_flow", R"(
      semantics {
        sender: "CryptAuth Device Manager"
        description:
          "Gets a list of the devices registered (for the same user) on "
          "CryptAuth."
        trigger:
          "Once every day, or by API request. Periodic calls happen because "
          "devices that are not re-enrolled for more than X days (currently "
          "45) are automatically removed from the server."
        data: "OAuth 2.0 token."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        setting:
          "This feature cannot be disabled in settings. However, this request "
          "is made only for signed-in users."
        chrome_policy {
          SigninAllowed {
            SigninAllowed: false
          }
        }
      })");
  cryptauth_client_->GetMyDevices(
      request,
      base::BindOnce(&CryptAuthDeviceManagerImpl::OnGetMyDevicesSuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&CryptAuthDeviceManagerImpl::OnGetMyDevicesFailure,
                     weak_ptr_factory_.GetWeakPtr()),
      partial_traffic_annotation);
}

}  // namespace ash::device_sync
