// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_device.h"

#include <sstream>

#include "base/i18n/time_formatting.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_logging.h"
#include "chromeos/ash/services/device_sync/value_string_encoding.h"

namespace ash {

namespace device_sync {

namespace {

// Strings used as DictionaryValue keys.
const char kInstanceIdDictKey[] = "instance_id";
const char kDeviceNameDictKey[] = "device_name";
const char kLastUpdateTimeDictKey[] = "last_update_time";
const char kDeviceBetterTogetherPublicKeyDictKey[] =
    "device_better_together_public_key";
const char kBetterTogetherDeviceMetadataDictKey[] =
    "better_together_device_metadata";
const char kFeatureStatesDictKey[] = "feature_states";

std::optional<
    std::map<multidevice::SoftwareFeature, multidevice::SoftwareFeatureState>>
FeatureStatesFromDictionary(const base::Value::Dict* dict) {
  if (!dict) {
    return std::nullopt;
  }

  std::map<multidevice::SoftwareFeature, multidevice::SoftwareFeatureState>
      feature_states;
  for (const auto feature_state_pair : *dict) {
    int feature;
    if (!base::StringToInt(feature_state_pair.first, &feature) ||
        !feature_state_pair.second.is_int()) {
      return std::nullopt;
    }

    feature_states[static_cast<multidevice::SoftwareFeature>(feature)] =
        static_cast<multidevice::SoftwareFeatureState>(
            feature_state_pair.second.GetInt());
  }

  return feature_states;
}

base::Value::Dict FeatureStatesToDictionary(
    const std::map<multidevice::SoftwareFeature,
                   multidevice::SoftwareFeatureState>& feature_states) {
  base::Value::Dict dict;
  for (const auto& feature_state_pair : feature_states) {
    dict.Set(base::NumberToString(static_cast<int>(feature_state_pair.first)),
             static_cast<int>(feature_state_pair.second));
  }

  return dict;
}

base::Value::Dict FeatureStatesToReadableDictionary(
    const std::map<multidevice::SoftwareFeature,
                   multidevice::SoftwareFeatureState>& feature_states) {
  base::Value::Dict dict;
  for (const auto& feature_state_pair : feature_states) {
    std::stringstream feature_ss;
    feature_ss << feature_state_pair.first;
    std::stringstream feature_state_ss;
    feature_state_ss << feature_state_pair.second;
    dict.Set(feature_ss.str(), feature_state_ss.str());
  }

  return dict;
}

}  // namespace

// static
std::optional<CryptAuthDevice> CryptAuthDevice::FromDictionary(
    const base::Value::Dict& dict) {
  std::optional<std::string> instance_id =
      util::DecodeFromValueString(dict.Find(kInstanceIdDictKey));
  if (!instance_id || instance_id->empty())
    return std::nullopt;

  std::optional<std::string> device_name =
      util::DecodeFromValueString(dict.Find(kDeviceNameDictKey));
  if (!device_name || device_name->empty())
    return std::nullopt;

  std::optional<std::string> device_better_together_public_key =
      util::DecodeFromValueString(
          dict.Find(kDeviceBetterTogetherPublicKeyDictKey));
  if (!device_better_together_public_key ||
      device_better_together_public_key->empty()) {
    return std::nullopt;
  }

  std::optional<base::Time> last_update_time =
      ::base::ValueToTime(dict.Find(kLastUpdateTimeDictKey));
  if (!last_update_time)
    return std::nullopt;

  std::optional<cryptauthv2::BetterTogetherDeviceMetadata>
      better_together_device_metadata;
  const base::Value* metadata_value =
      dict.Find(kBetterTogetherDeviceMetadataDictKey);
  if (metadata_value) {
    better_together_device_metadata = util::DecodeProtoMessageFromValueString<
        cryptauthv2::BetterTogetherDeviceMetadata>(metadata_value);
    if (!better_together_device_metadata)
      return std::nullopt;
  }

  std::optional<
      std::map<multidevice::SoftwareFeature, multidevice::SoftwareFeatureState>>
      feature_states =
          FeatureStatesFromDictionary(dict.FindDict(kFeatureStatesDictKey));
  if (!feature_states)
    return std::nullopt;

  return CryptAuthDevice(*instance_id, *device_name,
                         *device_better_together_public_key, *last_update_time,
                         better_together_device_metadata, *feature_states);
}

CryptAuthDevice::CryptAuthDevice(const std::string& instance_id)
    : instance_id_(instance_id) {
  DCHECK(!instance_id.empty());
}

CryptAuthDevice::CryptAuthDevice(
    const std::string& instance_id,
    const std::string& device_name,
    const std::string& device_better_together_public_key,
    const base::Time& last_update_time,
    const std::optional<cryptauthv2::BetterTogetherDeviceMetadata>&
        better_together_device_metadata,
    const std::map<multidevice::SoftwareFeature,
                   multidevice::SoftwareFeatureState>& feature_states)
    : device_name(device_name),
      device_better_together_public_key(device_better_together_public_key),
      last_update_time(last_update_time),
      better_together_device_metadata(better_together_device_metadata),
      feature_states(feature_states),
      instance_id_(instance_id) {
  DCHECK(!instance_id.empty());
}

CryptAuthDevice::CryptAuthDevice(const CryptAuthDevice&) = default;

CryptAuthDevice::~CryptAuthDevice() = default;

base::Value::Dict CryptAuthDevice::AsDictionary() const {
  base::Value::Dict dict;
  dict.Set(kInstanceIdDictKey, util::EncodeAsValueString(instance_id_));
  dict.Set(kDeviceNameDictKey, util::EncodeAsValueString(device_name));
  dict.Set(kDeviceBetterTogetherPublicKeyDictKey,
           util::EncodeAsValueString(device_better_together_public_key));
  dict.Set(kLastUpdateTimeDictKey, ::base::TimeToValue(last_update_time));
  dict.Set(kFeatureStatesDictKey, FeatureStatesToDictionary(feature_states));
  if (better_together_device_metadata) {
    dict.Set(kBetterTogetherDeviceMetadataDictKey,
             util::EncodeProtoMessageAsValueString(
                 &better_together_device_metadata.value()));
  }

  return dict;
}

base::Value::Dict CryptAuthDevice::AsReadableDictionary() const {
  base::Value::Dict dict;
  dict.Set("Instance ID", instance_id_);
  dict.Set("Device name", device_name);
  dict.Set("DeviceSync:BetterTogether device public key",
           cryptauthv2::TruncateStringForLogs(
               util::EncodeAsString(device_better_together_public_key)));
  dict.Set("Feature states", FeatureStatesToReadableDictionary(feature_states));
  dict.Set(
      "BetterTogether device metadata",
      (better_together_device_metadata
           ? base::Value(
                 cryptauthv2::BetterTogetherDeviceMetadataToReadableDictionary(
                     *better_together_device_metadata))
           : base::Value("[No decrypted metadata]")));
  return dict;
}

bool CryptAuthDevice::operator==(const CryptAuthDevice& other) const {
  bool does_metadata_match =
      (!better_together_device_metadata &&
       !other.better_together_device_metadata) ||
      (better_together_device_metadata.has_value() &&
       other.better_together_device_metadata.has_value() &&
       better_together_device_metadata->SerializeAsString() ==
           other.better_together_device_metadata->SerializeAsString());

  return does_metadata_match && instance_id_ == other.instance_id_ &&
         device_name == other.device_name &&
         device_better_together_public_key ==
             other.device_better_together_public_key &&
         last_update_time == other.last_update_time &&
         feature_states == other.feature_states;
}

bool CryptAuthDevice::operator!=(const CryptAuthDevice& other) const {
  return !(*this == other);
}

std::ostream& operator<<(std::ostream& stream, const CryptAuthDevice& device) {
  stream << device.AsReadableDictionary();
  return stream;
}

}  // namespace device_sync

}  // namespace ash
