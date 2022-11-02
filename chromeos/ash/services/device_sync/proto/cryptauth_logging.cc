// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chromeos/ash/services/device_sync/proto/cryptauth_logging.h"

#include "base/base64url.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"

namespace cryptauthv2 {

namespace {

std::string Encode(const std::string& str) {
  std::string encoded_string;
  base::Base64UrlEncode(str, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_string);

  return encoded_string;
}

}  // namespace

std::string TruncateStringForLogs(const std::string& str) {
  if (str.length() <= 10)
    return str;

  return str.substr(0, 5) + "..." + str.substr(str.length() - 5, str.length());
}

std::string TargetServiceToString(TargetService service) {
  switch (service) {
    case TargetService::TARGET_SERVICE_UNSPECIFIED:
      return "[Unspecified]";
    case TargetService::ENROLLMENT:
      return "[Enrollment]";
    case TargetService::DEVICE_SYNC:
      return "[DeviceSync]";
    default:
      return "[Unknown TargetService value " + base::NumberToString(service) +
             "]";
  }
}

std::ostream& operator<<(std::ostream& stream, const TargetService& service) {
  stream << TargetServiceToString(service);
  return stream;
}

std::string InvocationReasonToString(ClientMetadata::InvocationReason reason) {
  switch (reason) {
    case ClientMetadata::INVOCATION_REASON_UNSPECIFIED:
      return "[Unspecified]";
    case ClientMetadata::INITIALIZATION:
      return "[Initialization]";
    case ClientMetadata::PERIODIC:
      return "[Periodic]";
    case ClientMetadata::SLOW_PERIODIC:
      return "[Slow periodic]";
    case ClientMetadata::FAST_PERIODIC:
      return "[Fast periodic]";
    case ClientMetadata::EXPIRATION:
      return "[Expiration]";
    case ClientMetadata::FAILURE_RECOVERY:
      return "[Failure recovery]";
    case ClientMetadata::NEW_ACCOUNT:
      return "[New account]";
    case ClientMetadata::CHANGED_ACCOUNT:
      return "[Changed account]";
    case ClientMetadata::FEATURE_TOGGLED:
      return "[Feature toggled]";
    case ClientMetadata::SERVER_INITIATED:
      return "[Server initiated]";
    case ClientMetadata::ADDRESS_CHANGE:
      return "[Address change]";
    case ClientMetadata::SOFTWARE_UPDATE:
      return "[Software update]";
    case ClientMetadata::MANUAL:
      return "[Manual]";
    case ClientMetadata::CUSTOM_KEY_INVALIDATION:
      return "[Custom key invalidation]";
    case ClientMetadata::PROXIMITY_PERIODIC:
      return "[Proximity periodic]";
    default:
      return "[Unknown InvocationReason value " + base::NumberToString(reason) +
             "]";
  }
}

std::ostream& operator<<(std::ostream& stream,
                         const ClientMetadata::InvocationReason& reason) {
  stream << InvocationReasonToString(reason);
  return stream;
}

std::string ConnectivityStatusToString(ConnectivityStatus status) {
  switch (status) {
    case ConnectivityStatus::UNKNOWN_CONNECTIVITY:
      return "[Unknown connectivity]";
    case ConnectivityStatus::OFFLINE:
      return "[Offline]";
    case ConnectivityStatus::ONLINE:
      return "[Online]";
    default:
      return "[Unknown ConnectivityStatus value " +
             base::NumberToString(status) + "]";
  }
}

std::ostream& operator<<(std::ostream& stream,
                         const ConnectivityStatus& status) {
  stream << ConnectivityStatusToString(status);
  return stream;
}

base::Value PolicyReferenceToReadableDictionary(const PolicyReference& policy) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("Name", policy.name());
  dict.SetIntKey("Version", policy.version());
  return dict;
}

std::ostream& operator<<(std::ostream& stream, const PolicyReference& policy) {
  stream << PolicyReferenceToReadableDictionary(policy);
  return stream;
}

base::Value InvokeNextToReadableDictionary(const InvokeNext& invoke_next) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("Target service",
                    TargetServiceToString(invoke_next.service()));
  dict.SetStringKey("Key name", invoke_next.key_name());
  return dict;
}

std::ostream& operator<<(std::ostream& stream, const InvokeNext& invoke_next) {
  stream << InvokeNextToReadableDictionary(invoke_next);
  return stream;
}

base::Value ClientDirectiveToReadableDictionary(
    const ClientDirective& directive) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("Policy reference", PolicyReferenceToReadableDictionary(
                                      directive.policy_reference()));

  {
    std::u16string checkin_delay;
    bool success = base::TimeDurationFormatWithSeconds(
        base::Milliseconds(directive.checkin_delay_millis()),
        base::DurationFormatWidth::DURATION_WIDTH_NARROW, &checkin_delay);
    if (success) {
      dict.SetStringKey("Next enrollment", checkin_delay);
    }
    checkin_delay = base::UTF8ToUTF16(
        "[Error formatting time " +
        base::NumberToString(directive.checkin_delay_millis()) + "ms]");
  }

  dict.SetIntKey("Immediate failure retry attempts",
                 directive.retry_attempts());

  {
    std::u16string retry_period;
    bool success = base::TimeDurationFormatWithSeconds(
        base::Milliseconds(directive.retry_period_millis()),
        base::DurationFormatWidth::DURATION_WIDTH_NARROW, &retry_period);
    if (!success) {
      retry_period = base::UTF8ToUTF16(
          "[Error formatting time " +
          base::NumberToString(directive.retry_period_millis()) + "ms]");
    }
    dict.SetStringKey("Failure retry delay", retry_period);
  }

  dict.SetStringKey(
      "Directive creation time",
      base::TimeFormatShortDateAndTimeWithTimeZone(
          base::Time::FromJavaTime(directive.create_time_millis())));

  base::Value invoke_next_list(base::Value::Type::LIST);
  for (const auto& invoke_next : directive.invoke_next()) {
    invoke_next_list.Append(InvokeNextToReadableDictionary(invoke_next));
  }
  dict.SetKey("Invoke next list", std::move(invoke_next_list));

  return dict;
}

std::ostream& operator<<(std::ostream& stream,
                         const ClientDirective& directive) {
  stream << ClientDirectiveToReadableDictionary(directive);
  return stream;
}

base::Value DeviceMetadataPacketToReadableDictionary(
    const DeviceMetadataPacket& packet) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("Instance ID", packet.device_id());
  dict.SetStringKey(
      "Encrypted metadata",
      (packet.encrypted_metadata().empty()
           ? "[Empty]"
           : TruncateStringForLogs(Encode(packet.encrypted_metadata()))));
  dict.SetBoolKey("Needs group private key?", packet.need_group_private_key());
  dict.SetStringKey("DeviceSync:BetterTogether device public key",
                    TruncateStringForLogs(Encode(packet.device_public_key())));
  dict.SetStringKey("Device name", packet.device_name());
  return dict;
}

std::ostream& operator<<(std::ostream& stream,
                         const DeviceMetadataPacket& packet) {
  stream << DeviceMetadataPacketToReadableDictionary(packet);
  return stream;
}

base::Value EncryptedGroupPrivateKeyToReadableDictionary(
    const EncryptedGroupPrivateKey& key) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("Recipient Instance ID", key.recipient_device_id());
  dict.SetStringKey("Sender Instance ID", key.sender_device_id());
  dict.SetStringKey(
      "Encrypted group private key",
      (key.encrypted_private_key().empty()
           ? "[Empty]"
           : TruncateStringForLogs(Encode(key.encrypted_private_key()))));
  dict.SetStringKey("Group public key hash",
                    base::NumberToString(key.group_public_key_hash()));
  return dict;
}

std::ostream& operator<<(std::ostream& stream,
                         const EncryptedGroupPrivateKey& key) {
  stream << EncryptedGroupPrivateKeyToReadableDictionary(key);
  return stream;
}

base::Value SyncMetadataResponseToReadableDictionary(
    const SyncMetadataResponse& response) {
  base::Value dict(base::Value::Type::DICTIONARY);

  base::Value metadata_list(base::Value::Type::LIST);
  for (const auto& metadata : response.encrypted_metadata()) {
    metadata_list.Append(DeviceMetadataPacketToReadableDictionary(metadata));
  }
  dict.SetKey("Device metadata packets", std::move(metadata_list));

  dict.SetStringKey("Group public key",
                    TruncateStringForLogs(Encode(response.group_public_key())));
  if (response.has_encrypted_group_private_key()) {
    dict.SetKey("Encrypted group private key",
                EncryptedGroupPrivateKeyToReadableDictionary(
                    response.encrypted_group_private_key()));
  } else {
    dict.SetStringKey("Encrypted group private key", "[Not sent]");
  }
  dict.SetStringKey("Freshness token",
                    TruncateStringForLogs(Encode(response.freshness_token())));
  if (response.has_client_directive()) {
    dict.SetKey("Client directive", ClientDirectiveToReadableDictionary(
                                        response.client_directive()));
  } else {
    dict.SetStringKey("Client directive", "[Not sent]");
  }

  return dict;
}

std::ostream& operator<<(std::ostream& stream,
                         const SyncMetadataResponse& response) {
  stream << SyncMetadataResponseToReadableDictionary(response);
  return stream;
}

base::Value FeatureStatusToReadableDictionary(
    const DeviceFeatureStatus::FeatureStatus& status) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("Feature type", status.feature_type());
  dict.SetBoolKey("Enabled?", status.enabled());
  dict.SetStringKey(
      "Last modified time (BatchGet* only)",
      base::TimeFormatShortDateAndTimeWithTimeZone(
          base::Time::FromJavaTime(status.last_modified_time_millis())));
  dict.SetBoolKey("Enable exclusively (BatchSet* only)?",
                  status.enable_exclusively());
  return dict;
}

std::ostream& operator<<(std::ostream& stream,
                         const DeviceFeatureStatus::FeatureStatus& status) {
  stream << FeatureStatusToReadableDictionary(status);
  return stream;
}

base::Value DeviceFeatureStatusToReadableDictionary(
    const DeviceFeatureStatus& status) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("Instance ID", status.device_id());

  base::Value feature_status_list(base::Value::Type::LIST);
  for (const auto& feature_status : status.feature_statuses()) {
    feature_status_list.Append(
        FeatureStatusToReadableDictionary(feature_status));
  }
  dict.SetKey("Feature statuses", std::move(feature_status_list));

  return dict;
}

std::ostream& operator<<(std::ostream& stream,
                         const DeviceFeatureStatus& status) {
  stream << DeviceFeatureStatusToReadableDictionary(status);
  return stream;
}

base::Value BatchGetFeatureStatusesResponseToReadableDictionary(
    const BatchGetFeatureStatusesResponse& response) {
  base::Value dict(base::Value::Type::DICTIONARY);

  base::Value device_statuses_list(base::Value::Type::LIST);
  for (const auto& device_statuses : response.device_feature_statuses()) {
    device_statuses_list.Append(
        DeviceFeatureStatusToReadableDictionary(device_statuses));
  }
  dict.SetKey("Device feature statuses list", std::move(device_statuses_list));

  return dict;
}

std::ostream& operator<<(std::ostream& stream,
                         const BatchGetFeatureStatusesResponse& response) {
  stream << BatchGetFeatureStatusesResponseToReadableDictionary(response);
  return stream;
}

base::Value DeviceActivityStatusToReadableDictionary(
    const DeviceActivityStatus& status) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("Instance ID", status.device_id());
  dict.SetStringKey(
      "Last activity time",
      base::TimeFormatShortDateAndTimeWithTimeZone(
          base::Time::FromTimeT(status.last_activity_time_sec())));
  dict.SetStringKey("Connectivity status",
                    ConnectivityStatusToString(status.connectivity_status()));
  dict.SetStringKey(
      "Last update time",
      base::TimeFormatShortDateAndTimeWithTimeZone(
          base::Time::FromTimeT(status.last_update_time().seconds()) +
          base::Nanoseconds(status.last_update_time().nanos())));

  return dict;
}

std::ostream& operator<<(std::ostream& stream,
                         const DeviceActivityStatus& status) {
  stream << DeviceActivityStatusToReadableDictionary(status);
  return stream;
}

base::Value GetDevicesActivityStatusResponseToReadableDictionary(
    const GetDevicesActivityStatusResponse& response) {
  base::Value dict(base::Value::Type::DICTIONARY);

  base::Value status_list(base::Value::Type::LIST);
  for (const auto& status : response.device_activity_statuses()) {
    status_list.Append(DeviceActivityStatusToReadableDictionary(status));
  }
  dict.SetKey("Device activity statuses", std::move(status_list));

  return dict;
}

std::ostream& operator<<(std::ostream& stream,
                         const GetDevicesActivityStatusResponse& response) {
  stream << GetDevicesActivityStatusResponseToReadableDictionary(response);
  return stream;
}

base::Value BeaconSeedToReadableDictionary(const BeaconSeed& seed) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("Data", TruncateStringForLogs(Encode(seed.data())));
  dict.SetStringKey("Start time",
                    base::TimeFormatShortDateAndTimeWithTimeZone(
                        base::Time::FromJavaTime(seed.start_time_millis())));
  dict.SetStringKey("End time",
                    base::TimeFormatShortDateAndTimeWithTimeZone(
                        base::Time::FromJavaTime(seed.end_time_millis())));
  return dict;
}

std::ostream& operator<<(std::ostream& stream, const BeaconSeed& seed) {
  stream << BeaconSeedToReadableDictionary(seed);
  return stream;
}

base::Value BetterTogetherDeviceMetadataToReadableDictionary(
    const BetterTogetherDeviceMetadata& metadata) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("Public key",
                    TruncateStringForLogs(Encode(metadata.public_key())));
  dict.SetStringKey("PII-free device name", metadata.no_pii_device_name());
  dict.SetStringKey("Bluetooth MAC address",
                    metadata.bluetooth_public_address());

  base::Value beacon_seed_list(base::Value::Type::LIST);
  for (const auto& seed : metadata.beacon_seeds()) {
    beacon_seed_list.Append(BeaconSeedToReadableDictionary(seed));
  }
  dict.SetKey("Beacon seeds", std::move(beacon_seed_list));

  return dict;
}

std::ostream& operator<<(std::ostream& stream,
                         const BetterTogetherDeviceMetadata& metadata) {
  stream << BetterTogetherDeviceMetadataToReadableDictionary(metadata);
  return stream;
}

}  // namespace cryptauthv2
