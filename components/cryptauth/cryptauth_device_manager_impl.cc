// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/cryptauth_device_manager_impl.h"

#include <stddef.h>
#include <stdexcept>
#include <utility>

#include <memory>

#include "base/base64url.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "components/cryptauth/cryptauth_client.h"
#include "components/cryptauth/pref_names.h"
#include "components/cryptauth/proto/enum_util.h"
#include "components/cryptauth/software_feature_state.h"
#include "components/cryptauth/sync_scheduler_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace cryptauth {

namespace {

// The normal period between successful syncs, in hours.
const int kRefreshPeriodHours = 24;

// A more aggressive period between sync attempts to recover when the last
// sync attempt fails, in minutes. This is a base time that increases for each
// subsequent failure.
const int kDeviceSyncBaseRecoveryPeriodMinutes = 10;

// The bound on the amount to jitter the period between syncs.
const double kDeviceSyncMaxJitterRatio = 0.2;

// Keys for ExternalDeviceInfo dictionaries that are stored in the user's prefs.
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

// Keys for ExternalDeviceInfo's BeaconSeed.
const char kExternalDeviceKeyBeaconSeedData[] = "beacon_seed_data";
const char kExternalDeviceKeyBeaconSeedStartMs[] = "beacon_seed_start_ms";
const char kExternalDeviceKeyBeaconSeedEndMs[] = "beacon_seed_end_ms";

// Keys specific to the dictionary which stores ExternalDeviceInfo info.
const char kDictionaryKeySoftwareFeatures[] = "software_features";

// Converts BeaconSeed protos to a list value that can be stored in user prefs.
std::unique_ptr<base::ListValue> BeaconSeedsToListValue(
    const google::protobuf::RepeatedPtrField<BeaconSeed>& seeds) {
  std::unique_ptr<base::ListValue> list(new base::ListValue());

  for (int i = 0; i < seeds.size(); i++) {
    BeaconSeed seed = seeds.Get(i);

    if (!seed.has_data() || !seed.has_start_time_millis() ||
        !seed.has_end_time_millis()) {
      PA_LOG(WARNING) << "Unable to serialize BeaconSeed due to missing data; "
                      << "skipping.";
      continue;
    }

    std::unique_ptr<base::DictionaryValue> beacon_seed_value(
        new base::DictionaryValue());

    // Note that the |BeaconSeed|s' data is stored in Base64Url encoding because
    // dictionary values must be valid UTF8 strings.
    std::string seed_data_b64;
    base::Base64UrlEncode(seed.data(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &seed_data_b64);
    beacon_seed_value->SetString(kExternalDeviceKeyBeaconSeedData,
                                 seed_data_b64);

    // Set the timestamps as string representations of their numeric value
    // since there is no notion of a base::LongValue.
    beacon_seed_value->SetString(kExternalDeviceKeyBeaconSeedStartMs,
                                 std::to_string(seed.start_time_millis()));
    beacon_seed_value->SetString(kExternalDeviceKeyBeaconSeedEndMs,
                                 std::to_string(seed.end_time_millis()));

    list->Append(std::move(beacon_seed_value));
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

// Converts supported and enabled SoftwareFeature protos to a single dictionary
// value that can be stored in user prefs.
std::unique_ptr<base::DictionaryValue>
SupportedAndEnabledSoftwareFeaturesToDictionaryValue(
    const google::protobuf::RepeatedPtrField<std::string>&
        supported_software_features,
    const google::protobuf::RepeatedPtrField<std::string>&
        enabled_software_features,
    bool legacy_unlock_key,
    bool legacy_mobile_hotspot_supported) {
  std::unique_ptr<base::DictionaryValue> dictionary =
      std::make_unique<base::DictionaryValue>();

  for (const auto& supported_software_feature : supported_software_features) {
    dictionary->SetInteger(supported_software_feature,
                           static_cast<int>(SoftwareFeatureState::kSupported));
  }

  for (const auto& enabled_software_feature : enabled_software_features) {
    std::string software_feature_key = enabled_software_feature;
    cryptauth::SoftwareFeature software_feature =
        SoftwareFeatureStringToEnum(software_feature_key);

    int software_feature_state;
    if (!dictionary->GetInteger(software_feature_key,
                                &software_feature_state) ||
        static_cast<SoftwareFeatureState>(software_feature_state) !=
            SoftwareFeatureState::kSupported) {
      PA_LOG(ERROR) << "A feature is marked as enabled but not as supported: "
                    << software_feature_key;
      RecordDeviceSyncSoftwareFeaturesResult(false /* success */,
                                             software_feature);

      continue;
    } else {
      RecordDeviceSyncSoftwareFeaturesResult(true /* success */,
                                             software_feature);
    }

    dictionary->SetInteger(software_feature_key,
                           static_cast<int>(SoftwareFeatureState::kEnabled));
  }

  // If software features for EASY_UNLOCK_HOST or MAGIC_TETHER_HOST have not
  // been set, check to see if the deprecated corresponding booleans are
  // enabled. This can happen if the CryptAuth server is not yet serving
  // software features, and only serving the deprecated booleans.
  int software_feature_state;
  std::string software_feature_key;
  software_feature_key =
      SoftwareFeatureEnumToString(cryptauth::SoftwareFeature::EASY_UNLOCK_HOST);
  if (legacy_unlock_key &&
      !dictionary->GetInteger(software_feature_key, &software_feature_state)) {
    dictionary->SetInteger(software_feature_key,
                           static_cast<int>(SoftwareFeatureState::kEnabled));
  }
  software_feature_key = SoftwareFeatureEnumToString(
      cryptauth::SoftwareFeature::MAGIC_TETHER_HOST);
  if (legacy_mobile_hotspot_supported &&
      !dictionary->GetInteger(software_feature_key, &software_feature_state)) {
    dictionary->SetInteger(software_feature_key,
                           static_cast<int>(SoftwareFeatureState::kSupported));
  }

  return dictionary;
}

// Converts an unlock key proto to a dictionary that can be stored in user
// prefs.
std::unique_ptr<base::DictionaryValue> UnlockKeyToDictionary(
    const ExternalDeviceInfo& device) {
  // The device public key is a required value.
  if (!device.has_public_key())
    return nullptr;

  std::unique_ptr<base::DictionaryValue> dictionary(
      new base::DictionaryValue());

  // Note that the device public key, name, and Bluetooth addresses are stored
  // in Base64Url form because dictionary values must be valid UTF8 strings.

  std::string public_key_b64;
  base::Base64UrlEncode(device.public_key(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &public_key_b64);
  dictionary->SetString(kExternalDeviceKeyPublicKey, public_key_b64);

  if (device.has_friendly_device_name()) {
    std::string device_name_b64;
    base::Base64UrlEncode(device.friendly_device_name(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &device_name_b64);
    dictionary->SetString(kExternalDeviceKeyDeviceName, device_name_b64);
  }

  if (device.has_bluetooth_address()) {
    std::string bluetooth_address_b64;
    base::Base64UrlEncode(device.bluetooth_address(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &bluetooth_address_b64);
    dictionary->SetString(kExternalDeviceKeyBluetoothAddress,
                          bluetooth_address_b64);
  }

  if (device.has_unlockable()) {
    dictionary->SetBoolean(kExternalDeviceKeyUnlockable, device.unlockable());
  }

  if (device.has_last_update_time_millis()) {
    dictionary->SetString(kExternalDeviceKeyLastUpdateTimeMillis,
                          std::to_string(device.last_update_time_millis()));
  }

  if (device.has_device_type() && DeviceType_IsValid(device.device_type())) {
    dictionary->SetInteger(kExternalDeviceKeyDeviceType, device.device_type());
  }

  dictionary->Set(kExternalDeviceKeyBeaconSeeds,
                  BeaconSeedsToListValue(device.beacon_seeds()));

  if (device.has_arc_plus_plus()) {
    dictionary->SetBoolean(kExternalDeviceKeyArcPlusPlus,
                           device.arc_plus_plus());
  }

  if (device.has_pixel_phone()) {
    dictionary->SetBoolean(kExternalDeviceKeyPixelPhone, device.pixel_phone());
  }

  // In the case that the CryptAuth server is not yet serving SoftwareFeatures,
  // but only the deprecated booleans, |unlock_key| and
  // |mobile_hotspot_supported|, pass in the legacy values in order to correctly
  // populate the SoftwareFeatures.
  bool legacy_unlock_key = device.has_unlock_key() && device.unlock_key();
  bool legacy_mobile_hotspot_supported =
      device.has_mobile_hotspot_supported() &&
      device.mobile_hotspot_supported();
  dictionary->Set(kDictionaryKeySoftwareFeatures,
                  SupportedAndEnabledSoftwareFeaturesToDictionaryValue(
                      device.supported_software_features(),
                      device.enabled_software_features(), legacy_unlock_key,
                      legacy_mobile_hotspot_supported));

  return dictionary;
}

void AddBeaconSeedsToExternalDevice(const base::ListValue& beacon_seeds,
                                    ExternalDeviceInfo* external_device) {
  for (size_t i = 0; i < beacon_seeds.GetSize(); i++) {
    const base::DictionaryValue* seed_dictionary = nullptr;
    if (!beacon_seeds.GetDictionary(i, &seed_dictionary)) {
      PA_LOG(WARNING) << "Unable to retrieve BeaconSeed dictionary; "
                      << "skipping.";
      continue;
    }

    std::string seed_data_b64, start_time_millis_str, end_time_millis_str;
    if (!seed_dictionary->GetString(kExternalDeviceKeyBeaconSeedData,
                                    &seed_data_b64) ||
        !seed_dictionary->GetString(kExternalDeviceKeyBeaconSeedStartMs,
                                    &start_time_millis_str) ||
        !seed_dictionary->GetString(kExternalDeviceKeyBeaconSeedEndMs,
                                    &end_time_millis_str)) {
      PA_LOG(WARNING) << "Unable to deserialize BeaconSeed due to missing "
                      << "data; skipping.";
      continue;
    }

    // Seed data is returned as raw data, not in Base64 encoding.
    std::string seed_data;
    if (!base::Base64UrlDecode(seed_data_b64,
                               base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                               &seed_data)) {
      PA_LOG(WARNING) << "Decoding seed data failed.";
      continue;
    }

    int64_t start_time_millis, end_time_millis;
    if (!base::StringToInt64(start_time_millis_str, &start_time_millis) ||
        !base::StringToInt64(end_time_millis_str, &end_time_millis)) {
      PA_LOG(WARNING) << "Unable to convert stored timestamp to int64_t: "
                      << start_time_millis_str << " or " << end_time_millis_str;
      continue;
    }

    BeaconSeed* seed = external_device->add_beacon_seeds();
    seed->set_data(seed_data);
    seed->set_start_time_millis(start_time_millis);
    seed->set_end_time_millis(end_time_millis);
  }
}

void AddSoftwareFeaturesToExternalDevice(
    const base::DictionaryValue& software_features_dictionary,
    ExternalDeviceInfo* external_device,
    bool old_unlock_key_value_from_prefs,
    bool old_mobile_hotspot_supported_from_prefs) {
  for (const auto& it : software_features_dictionary.DictItems()) {
    std::string software_feature = it.first;
    if (SoftwareFeatureStringToEnum(software_feature) ==
        SoftwareFeature::UNKNOWN_FEATURE) {
      // SoftwareFeatures were previously stored in prefs as ints. Now,
      // SoftwareFeatures are stored as full string values, e.g.,
      // "betterTogetherHost". If |it.first| is not recognized by
      // SoftwareFeatureStringToEnum(), that means it is in the old int
      // representation of the SoftwareFeature. Convert it to its full string
      // representation using SoftwareFeatureEnumToString();
      int software_feature_int = std::atoi(software_feature.c_str());
      software_feature = SoftwareFeatureEnumToString(
          static_cast<SoftwareFeature>(software_feature_int));
    }

    int software_feature_state;
    if (!it.second.GetAsInteger(&software_feature_state)) {
      PA_LOG(WARNING) << "Unable to retrieve SoftwareFeature; skipping.";
      continue;
    }

    switch (static_cast<SoftwareFeatureState>(software_feature_state)) {
      case SoftwareFeatureState::kEnabled:
        external_device->add_enabled_software_features(software_feature);
        FALLTHROUGH;
      case SoftwareFeatureState::kSupported:
        external_device->add_supported_software_features(software_feature);
        break;
      default:
        break;
    }
  }

  // ExternalDeviceInfos's |unlock_key| and |mobile_hotspot_supported| fields
  // are deprecated, but it may be the case that after an update to Chrome, the
  // prefs reflect the old style of using these deprecated fields, instead of
  // software features. To work around this, these pref values are migrated to
  // software features locally.
  if (old_unlock_key_value_from_prefs) {
    if (!base::ContainsValue(
            external_device->supported_software_features(),
            SoftwareFeatureEnumToString(SoftwareFeature::EASY_UNLOCK_HOST))) {
      external_device->add_supported_software_features(
          SoftwareFeatureEnumToString(SoftwareFeature::EASY_UNLOCK_HOST));
    }
    if (!base::ContainsValue(
            external_device->enabled_software_features(),
            SoftwareFeatureEnumToString(SoftwareFeature::EASY_UNLOCK_HOST))) {
      external_device->add_enabled_software_features(
          SoftwareFeatureEnumToString(SoftwareFeature::EASY_UNLOCK_HOST));
    }
  }
  if (old_mobile_hotspot_supported_from_prefs) {
    if (!base::ContainsValue(
            external_device->supported_software_features(),
            SoftwareFeatureEnumToString(SoftwareFeature::MAGIC_TETHER_HOST))) {
      external_device->add_supported_software_features(
          SoftwareFeatureEnumToString(SoftwareFeature::MAGIC_TETHER_HOST));
    }
  }
}

// Converts an unlock key dictionary stored in user prefs to an
// ExternalDeviceInfo proto. Returns true if the dictionary is valid, and the
// parsed proto is written to |external_device|.
bool DictionaryToUnlockKey(const base::DictionaryValue& dictionary,
                           ExternalDeviceInfo* external_device) {
  std::string public_key_b64;
  if (!dictionary.GetString(kExternalDeviceKeyPublicKey, &public_key_b64)) {
    // The public key is a required field, so if it is absent, there is no
    // valid data to return.
    return false;
  }

  std::string public_key;
  if (!base::Base64UrlDecode(public_key_b64,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &public_key)) {
    // The public key is stored as a Base64Url, so if it cannot be decoded,
    // there is no valid data to return.
    return false;
  }
  external_device->set_public_key(public_key);

  std::string device_name_b64;
  if (dictionary.GetString(kExternalDeviceKeyDeviceName, &device_name_b64)) {
    std::string device_name;
    if (base::Base64UrlDecode(device_name_b64,
                              base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                              &device_name)) {
      external_device->set_friendly_device_name(device_name);
    }
  }

  std::string bluetooth_address_b64;
  if (dictionary.GetString(kExternalDeviceKeyBluetoothAddress,
                           &bluetooth_address_b64)) {
    std::string bluetooth_address;
    if (base::Base64UrlDecode(bluetooth_address_b64,
                              base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                              &bluetooth_address)) {
      external_device->set_bluetooth_address(bluetooth_address);
    }
  }

  // TODO(crbug.com/848477): Migrate |unlockable| into
  // |supported_software_features|.
  bool unlockable;
  if (dictionary.GetBoolean(kExternalDeviceKeyUnlockable, &unlockable))
    external_device->set_unlockable(unlockable);

  std::string last_update_time_millis_str;
  if (dictionary.GetString(kExternalDeviceKeyLastUpdateTimeMillis,
                           &last_update_time_millis_str)) {
    int64_t last_update_time_millis;
    if (base::StringToInt64(last_update_time_millis_str,
                            &last_update_time_millis)) {
      external_device->set_last_update_time_millis(last_update_time_millis);
    } else {
      PA_LOG(WARNING) << "Unable to convert stored update time to int64_t: "
                      << last_update_time_millis_str;
    }
  }

  int device_type;
  if (dictionary.GetInteger(kExternalDeviceKeyDeviceType, &device_type) &&
      DeviceType_IsValid(device_type)) {
    external_device->set_device_type(static_cast<DeviceType>(device_type));
  }

  const base::ListValue* beacon_seeds;
  if (dictionary.GetList(kExternalDeviceKeyBeaconSeeds, &beacon_seeds))
    AddBeaconSeedsToExternalDevice(*beacon_seeds, external_device);

  bool arc_plus_plus;
  if (dictionary.GetBoolean(kExternalDeviceKeyArcPlusPlus, &arc_plus_plus))
    external_device->set_arc_plus_plus(arc_plus_plus);

  bool pixel_phone;
  if (dictionary.GetBoolean(kExternalDeviceKeyPixelPhone, &pixel_phone))
    external_device->set_pixel_phone(pixel_phone);

  bool unlock_key = false;
  dictionary.GetBoolean(kExternalDeviceKeyUnlockKey, &unlock_key);
  bool mobile_hotspot_supported = false;
  dictionary.GetBoolean(kExternalDeviceKeyMobileHotspotSupported,
                        &mobile_hotspot_supported);

  const base::DictionaryValue* software_features_dictionary;
  if (dictionary.GetDictionary(kDictionaryKeySoftwareFeatures,
                               &software_features_dictionary)) {
    AddSoftwareFeaturesToExternalDevice(*software_features_dictionary,
                                        external_device, unlock_key,
                                        mobile_hotspot_supported);
  }

  return true;
}

std::unique_ptr<SyncSchedulerImpl> CreateSyncScheduler(
    SyncScheduler::Delegate* delegate) {
  return std::make_unique<SyncSchedulerImpl>(
      delegate, base::TimeDelta::FromHours(kRefreshPeriodHours),
      base::TimeDelta::FromMinutes(kDeviceSyncBaseRecoveryPeriodMinutes),
      kDeviceSyncMaxJitterRatio, "CryptAuth DeviceSync");
}

}  // namespace

// static
CryptAuthDeviceManagerImpl::Factory*
    CryptAuthDeviceManagerImpl::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<CryptAuthDeviceManager>
CryptAuthDeviceManagerImpl::Factory::NewInstance(
    base::Clock* clock,
    CryptAuthClientFactory* cryptauth_client_factory,
    CryptAuthGCMManager* gcm_manager,
    PrefService* pref_service) {
  if (!factory_instance_)
    factory_instance_ = new Factory();

  return factory_instance_->BuildInstance(clock, cryptauth_client_factory,
                                          gcm_manager, pref_service);
}

// static
void CryptAuthDeviceManagerImpl::Factory::SetInstanceForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

CryptAuthDeviceManagerImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthDeviceManager>
CryptAuthDeviceManagerImpl::Factory::BuildInstance(
    base::Clock* clock,
    CryptAuthClientFactory* cryptauth_client_factory,
    CryptAuthGCMManager* gcm_manager,
    PrefService* pref_service) {
  return base::WrapUnique(new CryptAuthDeviceManagerImpl(
      clock, cryptauth_client_factory, gcm_manager, pref_service));
}

CryptAuthDeviceManagerImpl::CryptAuthDeviceManagerImpl(
    base::Clock* clock,
    CryptAuthClientFactory* cryptauth_client_factory,
    CryptAuthGCMManager* gcm_manager,
    PrefService* pref_service)
    : clock_(clock),
      cryptauth_client_factory_(cryptauth_client_factory),
      gcm_manager_(gcm_manager),
      pref_service_(pref_service),
      scheduler_(CreateSyncScheduler(this)),
      weak_ptr_factory_(this) {
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
    InvocationReason invocation_reason) {
  pref_service_->SetInteger(prefs::kCryptAuthDeviceSyncReason,
                            invocation_reason);
  scheduler_->ForceSync();
}

base::Time CryptAuthDeviceManagerImpl::GetLastSyncTime() const {
  return base::Time::FromDoubleT(
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

std::vector<ExternalDeviceInfo> CryptAuthDeviceManagerImpl::GetSyncedDevices()
    const {
  return synced_devices_;
}

std::vector<ExternalDeviceInfo> CryptAuthDeviceManagerImpl::GetUnlockKeys()
    const {
  std::vector<ExternalDeviceInfo> unlock_keys;
  for (const auto& device : synced_devices_) {
    if (base::ContainsValue(
            device.enabled_software_features(),
            SoftwareFeatureEnumToString(SoftwareFeature::EASY_UNLOCK_HOST))) {
      unlock_keys.push_back(device);
    }
  }
  return unlock_keys;
}

std::vector<ExternalDeviceInfo> CryptAuthDeviceManagerImpl::GetPixelUnlockKeys()
    const {
  std::vector<ExternalDeviceInfo> unlock_keys;
  for (const auto& device : synced_devices_) {
    if (base::ContainsValue(
            device.enabled_software_features(),
            SoftwareFeatureEnumToString(SoftwareFeature::EASY_UNLOCK_HOST)) &&
        device.pixel_phone()) {
      unlock_keys.push_back(device);
    }
  }
  return unlock_keys;
}

std::vector<ExternalDeviceInfo> CryptAuthDeviceManagerImpl::GetTetherHosts()
    const {
  std::vector<ExternalDeviceInfo> tether_hosts;
  for (const auto& device : synced_devices_) {
    if (base::ContainsValue(
            device.supported_software_features(),
            SoftwareFeatureEnumToString(SoftwareFeature::MAGIC_TETHER_HOST))) {
      tether_hosts.push_back(device);
    }
  }
  return tether_hosts;
}

std::vector<ExternalDeviceInfo>
CryptAuthDeviceManagerImpl::GetPixelTetherHosts() const {
  std::vector<ExternalDeviceInfo> tether_hosts;
  for (const auto& device : synced_devices_) {
    if (base::ContainsValue(
            device.supported_software_features(),
            SoftwareFeatureEnumToString(SoftwareFeature::MAGIC_TETHER_HOST)) &&
        device.pixel_phone())
      tether_hosts.push_back(device);
  }
  return tether_hosts;
}

void CryptAuthDeviceManagerImpl::OnGetMyDevicesSuccess(
    const GetMyDevicesResponse& response) {
  // Update the synced devices stored in the user's prefs.
  std::unique_ptr<base::ListValue> devices_as_list(new base::ListValue());

  if (!response.devices().empty())
    PA_LOG(INFO) << "Devices were successfully synced.";

  for (const auto& device : response.devices()) {
    std::unique_ptr<base::DictionaryValue> device_dictionary =
        UnlockKeyToDictionary(device);

    const std::string& device_name = device.has_friendly_device_name()
                                         ? device.friendly_device_name()
                                         : "[unknown]";
    PA_LOG(INFO) << "Synced device '" << device_name
                 << "': " << *device_dictionary;

    devices_as_list->Append(std::move(device_dictionary));
  }

  bool unlock_keys_changed = !devices_as_list->Equals(
      pref_service_->GetList(prefs::kCryptAuthDeviceSyncUnlockKeys));
  {
    ListPrefUpdate update(pref_service_, prefs::kCryptAuthDeviceSyncUnlockKeys);
    update.Get()->Swap(devices_as_list.get());
  }
  UpdateUnlockKeysFromPrefs();

  // Reset metadata used for scheduling syncing.
  pref_service_->SetBoolean(prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure,
                            false);
  pref_service_->SetDouble(prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds,
                           clock_->Now().ToDoubleT());
  pref_service_->SetInteger(prefs::kCryptAuthDeviceSyncReason,
                            INVOCATION_REASON_UNKNOWN);

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
}

void CryptAuthDeviceManagerImpl::OnResyncMessage() {
  ForceSyncNow(INVOCATION_REASON_SERVER_INITIATED);
}

void CryptAuthDeviceManagerImpl::UpdateUnlockKeysFromPrefs() {
  const base::ListValue* unlock_key_list =
      pref_service_->GetList(prefs::kCryptAuthDeviceSyncUnlockKeys);
  synced_devices_.clear();
  for (size_t i = 0; i < unlock_key_list->GetSize(); ++i) {
    const base::DictionaryValue* unlock_key_dictionary;
    if (unlock_key_list->GetDictionary(i, &unlock_key_dictionary)) {
      ExternalDeviceInfo unlock_key;
      if (DictionaryToUnlockKey(*unlock_key_dictionary, &unlock_key)) {
        synced_devices_.push_back(unlock_key);
      } else {
        PA_LOG(ERROR) << "Unable to deserialize unlock key dictionary "
                      << "(index=" << i << "):\n"
                      << *unlock_key_dictionary;
      }
    } else {
      PA_LOG(ERROR) << "Can not get dictionary in list of unlock keys "
                    << "(index=" << i << "):\n"
                    << *unlock_key_list;
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

  InvocationReason invocation_reason = INVOCATION_REASON_UNKNOWN;

  int reason_stored_in_prefs =
      pref_service_->GetInteger(prefs::kCryptAuthDeviceSyncReason);

  // If the sync attempt is not forced, it is acceptable for CryptAuth to return
  // a cached copy of the user's devices, rather taking a database hit for the
  // freshest data.
  bool is_sync_speculative =
      reason_stored_in_prefs != INVOCATION_REASON_UNKNOWN;

  if (InvocationReason_IsValid(reason_stored_in_prefs) &&
      reason_stored_in_prefs != INVOCATION_REASON_UNKNOWN) {
    invocation_reason = static_cast<InvocationReason>(reason_stored_in_prefs);
  } else if (GetLastSyncTime().is_null()) {
    invocation_reason = INVOCATION_REASON_INITIALIZATION;
  } else if (IsRecoveringFromFailure()) {
    invocation_reason = INVOCATION_REASON_FAILURE_RECOVERY;
  } else {
    invocation_reason = INVOCATION_REASON_PERIODIC;
  }

  GetMyDevicesRequest request;
  request.set_invocation_reason(invocation_reason);
  request.set_allow_stale_read(is_sync_speculative);
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
          "devides that do not re-enrolled for more than X days (currently 45) "
          "are automatically removed from the server."
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
      base::Bind(&CryptAuthDeviceManagerImpl::OnGetMyDevicesSuccess,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&CryptAuthDeviceManagerImpl::OnGetMyDevicesFailure,
                 weak_ptr_factory_.GetWeakPtr()),
      partial_traffic_annotation);
}

}  // namespace cryptauth
