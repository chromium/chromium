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

absl::optional<
    std::map<multidevice::SoftwareFeature, multidevice::SoftwareFeatureState>>
FeatureStatesFromDictionary(const base::Value* dict) {
  if (!dict || !dict->is_dict())
    return absl::nullopt;

  std::map<multidevice::SoftwareFeature, multidevice::SoftwareFeatureState>
      feature_states;
  for (const auto feature_state_pair : dict->DictItems()) {
    int feature;
    if (!base::StringToInt(feature_state_pair.first, &feature) ||
        !feature_state_pair.second.is_int()) {
      return absl::nullopt;
    }

    feature_states[static_cast<multidevice::SoftwareFeature>(feature)] =
        static_cast<multidevice::SoftwareFeatureState>(
            feature_state_pair.second.GetInt());
  }

  return feature_states;
}

base::Value FeatureStatesToDictionary(
    const std::map<multidevice::SoftwareFeature,
                   multidevice::SoftwareFeatureState>& feature_states) {
  base::Value dict(base::Value::Type::DICTIONARY);
  for (const auto& feature_state_pair : feature_states) {
    dict.SetIntKey(
        base::NumberToString(static_cast<int>(feature_state_pair.first)),
        static_cast<int>(feature_state_pair.second));
  }

  return dict;
}

base::Value FeatureStatesToReadableDictionary(
    const std::map<multidevice::SoftwareFeature,
                   multidevice::SoftwareFeatureState>& feature_states) {
  base::Value dict(base::Value::Type::DICTIONARY);
  for (const auto& feature_state_pair : feature_states) {
    std::stringstream feature_ss;
    feature_ss << feature_state_pair.first;
    std::stringstream feature_state_ss;
    feature_state_ss << feature_state_pair.second;
    dict.SetStringKey(feature_ss.str(), feature_state_ss.str());
  }

  return dict;
}

}  // namespace

// static
absl::optional<CryptAuthDevice> CryptAuthDevice::FromDictionary(
    const base::Value& dict) {
  if (!dict.is_dict())
    return absl::nullopt;

  absl::optional<std::string> instance_id =
      util::DecodeFromValueString(dict.FindKey(kInstanceIdDictKey));
  if (!instance_id || instance_id->empty())
    return absl::nullopt;

  absl::optional<std::string> device_name =
      util::DecodeFromValueString(dict.FindKey(kDeviceNameDictKey));
  if (!device_name || device_name->empty())
    return absl::nullopt;

  absl::optional<std::string> device_better_together_public_key =
      util::DecodeFromValueString(
          dict.FindKey(kDeviceBetterTogetherPublicKeyDictKey));
  if (!device_better_together_public_key ||
      device_better_together_public_key->empty()) {
    return absl::nullopt;
  }

  absl::optional<base::Time> last_update_time =
      ::base::ValueToTime(dict.FindKey(kLastUpdateTimeDictKey));
  if (!last_update_time)
    return absl::nullopt;

  absl::optional<cryptauthv2::BetterTogetherDeviceMetadata>
      better_together_device_metadata;
  const base::Value* metadata_value =
      dict.FindKey(kBetterTogetherDeviceMetadataDictKey);
  if (metadata_value) {
    better_together_device_metadata = util::DecodeProtoMessageFromValueString<
        cryptauthv2::BetterTogetherDeviceMetadata>(metadata_value);
    if (!better_together_device_metadata)
      return absl::nullopt;
  }

  absl::optional<
      std::map<multidevice::SoftwareFeature, multidevice::SoftwareFeatureState>>
      feature_states =
          FeatureStatesFromDictionary(dict.FindDictKey(kFeatureStatesDictKey));
  if (!feature_states)
    return absl::nullopt;

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
    const absl::optional<cryptauthv2::BetterTogetherDeviceMetadata>&
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

base::Value CryptAuthDevice::AsDictionary() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(kInstanceIdDictKey, util::EncodeAsValueString(instance_id_));
  dict.SetKey(kDeviceNameDictKey, util::EncodeAsValueString(device_name));
  dict.SetKey(kDeviceBetterTogetherPublicKeyDictKey,
              util::EncodeAsValueString(device_better_together_public_key));
  dict.SetKey(kLastUpdateTimeDictKey, ::base::TimeToValue(last_update_time));
  dict.SetKey(kFeatureStatesDictKey, FeatureStatesToDictionary(feature_states));
  if (better_together_device_metadata) {
    dict.SetKey(kBetterTogetherDeviceMetadataDictKey,
                util::EncodeProtoMessageAsValueString(
                    &better_together_device_metadata.value()));
  }

  return dict;
}

base::Value CryptAuthDevice::AsReadableDictionary() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("Instance ID", instance_id_);
  dict.SetStringKey("Device name", device_name);
  dict.SetStringKey("DeviceSync:BetterTogether device public key",
                    cryptauthv2::TruncateStringForLogs(util::EncodeAsString(
                        device_better_together_public_key)));
  dict.SetKey("Feature states",
              FeatureStatesToReadableDictionary(feature_states));
  dict.SetKey(
      "BetterTogether device metadata",
      (better_together_device_metadata
           ? cryptauthv2::BetterTogetherDeviceMetadataToReadableDictionary(
                 *better_together_device_metadata)
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
