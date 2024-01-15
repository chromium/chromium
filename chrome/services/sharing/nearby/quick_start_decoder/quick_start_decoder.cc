// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quick_start_decoder.h"

#include <optional>

#include "base/base64.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/ash/components/quick_start/quick_start_message.h"
#include "chromeos/ash/components/quick_start/quick_start_message_type.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-forward.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "quick_start_conversions.h"

namespace ash::quick_start {

namespace {

using CBOR = cbor::Value;

constexpr int kExpectedResponseSize = 2;
constexpr char kCredentialIdKey[] = "id";
constexpr char kEntitiyIdMapKey[] = "id";
constexpr char kBootstrapConfigurationsKey[] = "bootstrapConfigurations";
constexpr char kDeviceDetailsKey[] = "deviceDetails";
constexpr char kSecondDeviceAuthPayloadKey[] = "secondDeviceAuthPayload";
constexpr char kIsTransferUnicornKey[] = "isTransferUnicorn";
constexpr char kBootstrapAccountsKey[] = "bootstrapAccounts";
constexpr char kNameKey[] = "name";
constexpr char kInstanceIdKey[] = "instanceId";
constexpr uint8_t kCtapDeviceResponseSuccess = 0x00;
constexpr int kCborDecoderNoError = 0;
constexpr char kFidoMessageKey[] = "fidoMessage";

// Key in Wifi Information response containing information about the wifi
// network as a JSON Dictionary.
constexpr char kWifiNetworkInformationKey[] = "wifi_network";

// Key in wifi_network dictionary containing the SSID of the wifi network.
constexpr char kWifiNetworkSsidKey[] = "wifi_ssid";

// Key in wifi_network dictionary containing the password of the wifi network.
constexpr char kWifiNetworkPasswordKey[] = "wifi_pre_shared_key";

// Key in wifi_network dictionary containing the security type of the wifi
// network.
constexpr char kWifiNetworkSecurityTypeKey[] = "wifi_security_type";

// Key in wifi_network dictionary containing if the wifi network is hidden.
constexpr char kWifiNetworkIsHiddenKey[] = "wifi_hidden_ssid";

// Key in Notify Source of Update response containing bool acknowledging the
// message.
constexpr char kNotifySourceOfUpdateAckKey[] = "forced_update_acknowledged";

// Key in UserVerificationResult containing the result
constexpr char kUserVerificationResultKey[] = "user_verification_result";

// Key in UserVerificationResult indicating if this is the first user
// verification
constexpr char kIsFirstUserVerificationKey[] = "is_first_user_verification";

// Key in UserVerificationRequested indicating if user verification was
// requested
constexpr char kAwaitingUserVerificationKey[] = "await_user_verification";

// Key in UserVerificationMethod. Value is an enum indicating which user
// verification method the source device intends to use. Always expected to be 0
// = SOURCE_LSKF_VERIFICATION.
constexpr char kUserVerificationMethodKey[] = "user_verification_method";

// This value indicates that user verification will take place on the source
// device using a lock screen prompt. This is the only supported method on
// ChromeOS. Value defined here:
// http://google3/java/com/google/android/gmscore/integ/client/smartdevice/src/com/google/android/gms/smartdevice/d2d/UserVerificationMethod.java;l=15;rcl=557316806
constexpr int kUserVerificationMethodSourceLockScreenPrompt = 0;

std::pair<int, std::optional<cbor::Value>> CborDecodeGetAssertionResponse(
    base::span<const uint8_t> response) {
  cbor::Reader::DecoderError error;
  cbor::Reader::Config config;

  config.error_code_out = &error;
  std::optional<cbor::Value> cbor = cbor::Reader::Read(response, config);
  if (!cbor) {
    int converted_decode_error = static_cast<int>(error);
    LOG(ERROR) << "Error CBOR decoding the response bytes: "
               << cbor::Reader::ErrorCodeToString(error);
    return std::make_pair(converted_decode_error, std::nullopt);
  }
  return std::make_pair(kCborDecoderNoError, std::move(cbor));
}

std::string FindInstanceIdInBootstrapConfigurations(
    const base::Value::Dict& payload) {
  const base::Value::Dict* bootstrap_configurations =
      payload.FindDict(kBootstrapConfigurationsKey);
  CHECK(bootstrap_configurations);

  const base::Value::Dict* device_details =
      bootstrap_configurations->FindDict(kDeviceDetailsKey);
  if (!device_details) {
    LOG(WARNING) << "DeviceDetails not found within BootstrapConfigurations.";
    return "";
  }

  const std::string* instance_id_ptr =
      device_details->FindString(kInstanceIdKey);
  return instance_id_ptr ? *instance_id_ptr : "";
}

std::string FindEmailInBootstrapConfigurations(
    const base::Value::Dict& payload) {
  const base::Value::Dict* bootstrap_configurations =
      payload.FindDict(kBootstrapConfigurationsKey);
  CHECK(bootstrap_configurations);

  const base::Value::List* accounts =
      bootstrap_configurations->FindList(kBootstrapAccountsKey);
  if (!accounts) {
    LOG(WARNING)
        << "BootstrapAccounts not found within BootstrapConfigurations.";
    return "";
  }

  if (accounts->empty()) {
    LOG(WARNING) << "Empty accounts list received from source device.";
    return "";
  }

  const base::Value::Dict* first_account = accounts->front().GetIfDict();
  if (!first_account) {
    LOG(WARNING) << "Invalid value for account received from source device.";
    return "";
  }

  const std::string* email_ptr = first_account->FindString(kNameKey);
  if (!email_ptr) {
    LOG(WARNING) << "Email missing from account received from source device.";
    return "";
  }

  return *email_ptr;
}

bool FindIsSupervisedAccountInBootstrapConfigurations(
    const base::Value::Dict& payload) {
  const base::Value::Dict* second_device_auth_payload =
      payload.FindDict(kSecondDeviceAuthPayloadKey);
  if (!second_device_auth_payload) {
    LOG(WARNING) << "SecondDeviceAuthPayload not found in "
                    "BootstrapConfigurations message.";
    return false;
  }

  std::optional<bool> is_supervised_account_optional =
      second_device_auth_payload->FindBool(kIsTransferUnicornKey);
  if (!is_supervised_account_optional.has_value()) {
    LOG(WARNING) << "Supervised account boolean not found in "
                    "BootstrapConfigurations message.";
    return false;
  }

  return is_supervised_account_optional.value();
}

}  // namespace

QuickStartDecoder::QuickStartDecoder(
    mojo::PendingReceiver<mojom::QuickStartDecoder> receiver,
    base::OnceClosure on_disconnect)
    : receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(std::move(on_disconnect));
}

QuickStartDecoder::~QuickStartDecoder() = default;

void QuickStartDecoder::DecodeQuickStartMessage(
    const std::optional<std::vector<uint8_t>>& data,
    DecodeQuickStartMessageCallback callback) {
  if (!data.has_value()) {
    LOG(ERROR) << "No response bytes received.";
    std::move(callback).Run(nullptr,
                            mojom::QuickStartDecoderError::kEmptyMessage);
    return;
  }

  auto result = DoDecodeQuickStartMessage(data.value());
  if (result.has_value()) {
    std::move(callback).Run(std::move(result.value()), std::nullopt);
  } else {
    std::move(callback).Run(nullptr, result.error());
  }
}

base::expected<mojom::QuickStartMessagePtr, mojom::QuickStartDecoderError>
QuickStartDecoder::DoDecodeQuickStartMessage(const std::vector<uint8_t>& data) {
  QuickStartMessage::ReadResult read_result =
      QuickStartMessage::ReadMessage(data);
  if (!read_result.has_value()) {
    switch (read_result.error()) {
      case QuickStartMessage::ReadError::INVALID_JSON:
        return base::unexpected(
            mojom::QuickStartDecoderError::kUnableToReadAsJSON);
      case QuickStartMessage::ReadError::MISSING_MESSAGE_PAYLOAD:
        return base::unexpected(mojom::QuickStartDecoderError::kUnknownPayload);
      case QuickStartMessage::ReadError::BASE64_DESERIALIZATION_FAILURE:
        return base::unexpected(
            mojom::QuickStartDecoderError::kUnableToReadAsBase64);
      case QuickStartMessage::ReadError::UNEXPECTED_MESSAGE_TYPE:
        return base::unexpected(
            mojom::QuickStartDecoderError::kUnexpectedMessageType);
    }
  }

  base::Value::Dict* payload = read_result.value()->GetPayload();
  QuickStartMessageType type = read_result.value()->get_type();
  switch (type) {
    case QuickStartMessageType::kSecondDeviceAuthPayload:
      return DecodeSecondDeviceAuthPayload(*payload);
    case QuickStartMessageType::kBootstrapOptions:
      NOTIMPLEMENTED();
      break;
    case QuickStartMessageType::kBootstrapState:
      NOTIMPLEMENTED();
      break;
    case QuickStartMessageType::kBootstrapConfigurations:
      return DecodeBootstrapConfigurations(*payload);
    case QuickStartMessageType::kQuickStartPayload:
      return DecodeQuickStartPayload(*payload);
  }
  return base::unexpected(mojom::QuickStartDecoderError::kEmptyMessage);
}

base::expected<mojom::QuickStartMessagePtr, mojom::QuickStartDecoderError>
QuickStartDecoder::DecodeSecondDeviceAuthPayload(
    const base::Value::Dict& payload) {
  const std::string* fido_message = payload.FindString(kFidoMessageKey);
  if (!fido_message) {
    LOG(ERROR) << "fidoMessage cannot be found within secondDeviceAuthPayload.";
    return base::unexpected(
        mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }

  std::string base64_decoded_fido_message;

  if (!base::Base64Decode(*fido_message, &base64_decoded_fido_message,
                          base::Base64DecodePolicy::kForgiving)) {
    LOG(ERROR) << "Failed to decode fidoMessage as a Base64 String";
    return base::unexpected(
        mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }

  auto response_bytes = std::vector<unsigned char>(
      base64_decoded_fido_message.begin(), base64_decoded_fido_message.end());

  if (response_bytes.size() < kExpectedResponseSize) {
    LOG(ERROR) << "GetAssertionResponse requires a status code byte and "
                  "response bytes. Data in size: "
               << response_bytes.size();
    return base::unexpected(
        mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }
  uint8_t ctap_status = response_bytes[0];
  base::span<const uint8_t> cbor_bytes(response_bytes);
  cbor_bytes = cbor_bytes.subspan(1);
  if (ctap_status != kCtapDeviceResponseSuccess) {
    LOG(ERROR) << "Ctap Device Response Status Code is not Success(0x00). Got: "
               << ctap_status;
    return base::unexpected(
        mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }
  std::pair<int, std::optional<cbor::Value>> decoded_values =
      CborDecodeGetAssertionResponse(cbor_bytes);
  if (decoded_values.first != kCborDecoderNoError) {
    return base::unexpected(
        mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }
  if (!decoded_values.second || !decoded_values.second->is_map()) {
    LOG(ERROR) << "The CBOR decoded response values needs to be a valid CBOR "
                  "Value Map.";
    return base::unexpected(
        mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }

  const cbor::Value::MapValue& response_map = decoded_values.second->GetMap();
  // According to FIDO CTAP2 GetAssertionResponse, credential is stored at CBOR
  // index 0x01.
  auto credential_value_it = response_map.find(CBOR(0x01));
  std::string credential_id;
  if (credential_value_it != response_map.end() &&
      credential_value_it->second.is_map()) {
    const cbor::Value::MapValue& credential_value_map =
        credential_value_it->second.GetMap();
    auto cid = credential_value_map.find(cbor::Value(kCredentialIdKey));
    if (cid != credential_value_map.end() && cid->second.is_bytestring()) {
      credential_id = std::string(cid->second.GetBytestringAsString());
    }
  }

  if (credential_id.empty()) {
    LOG(ERROR) << "credential_id is empty in FIDO Message";
    return base::unexpected(
        mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }

  // According to FIDO CTAP2 GetAssertionResponse, authData is stored at CBOR
  // index 0x02.
  auto auth_data_value_it = response_map.find(CBOR(0x02));
  std::vector<uint8_t> auth_data;
  if (auth_data_value_it != response_map.end() &&
      auth_data_value_it->second.is_bytestring()) {
    auth_data = auth_data_value_it->second.GetBytestring();
  }

  if (auth_data.empty()) {
    LOG(ERROR) << "auth_data is empty in FIDO Message";
    return base::unexpected(
        mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }

  // According to FIDO CTAP2 GetAssertionResponse, signature is stored at CBOR
  // index 0x03.
  auto signature_value_it = response_map.find(CBOR(0x03));
  std::vector<uint8_t> signature;
  if (signature_value_it != response_map.end() &&
      signature_value_it->second.is_bytestring()) {
    signature = signature_value_it->second.GetBytestring();
  }

  if (signature.empty()) {
    LOG(ERROR) << "signature is empty in FIDO Message";
    return base::unexpected(
        mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }

  // According to FIDO CTAP2 GetAssertionResponse, user is stored at CBOR index
  // 0x04.
  auto user_value_it = response_map.find(CBOR(0x04));
  std::string email;
  if (user_value_it != response_map.end() && user_value_it->second.is_map()) {
    const cbor::Value::MapValue& user_value_map =
        user_value_it->second.GetMap();
    auto uid = user_value_map.find(cbor::Value(kEntitiyIdMapKey));
    if (uid != user_value_map.end() && uid->second.is_bytestring()) {
      email = std::string(uid->second.GetBytestringAsString());
    }
  }

  if (email.empty()) {
    LOG(ERROR) << "email is empty in FIDO Message";
    return base::unexpected(
        mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }

  return mojom::QuickStartMessage::NewFidoAssertionResponse(
      mojom::FidoAssertionResponse::New(email, credential_id, auth_data,
                                        signature));
}

base::expected<mojom::QuickStartMessagePtr, mojom::QuickStartDecoderError>
QuickStartDecoder::DecodeQuickStartPayload(const base::Value::Dict& payload) {
  // user verification requested
  std::optional<bool> is_awaiting_user_verification;
  if ((is_awaiting_user_verification =
           payload.FindBool(kAwaitingUserVerificationKey))) {
    return mojom::QuickStartMessage::NewUserVerificationRequested(
        mojom::UserVerificationRequested::New(
            is_awaiting_user_verification.value()));
  }

  // user verification method
  std::optional<int> user_verification_method;
  if ((user_verification_method =
           payload.FindInt(kUserVerificationMethodKey))) {
    return mojom::QuickStartMessage::NewUserVerificationMethod(
        mojom::UserVerificationMethod::New(
            /*use_source_lock_screen_prompt=*/user_verification_method
                .value() == kUserVerificationMethodSourceLockScreenPrompt));
  }

  // user verification response
  std::optional<int> user_verification_result_code;
  if ((user_verification_result_code =
           payload.FindInt(kUserVerificationResultKey))) {
    mojom::UserVerificationResult user_verification_result =
        static_cast<mojom::UserVerificationResult>(
            user_verification_result_code.value());

    if (!mojom::IsKnownEnumValue(user_verification_result)) {
      LOG(ERROR) << "User Verification Result is an unknown status code";
      return base::unexpected(
          mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    }

    std::optional<bool> is_first_user_verification =
        payload.FindBool(kIsFirstUserVerificationKey);
    if (!is_first_user_verification.has_value()) {
      LOG(ERROR) << "Message does not contain key is_first_user_verification";
      return base::unexpected(
          mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    }
    return mojom::QuickStartMessage::NewUserVerificationResponse(
        mojom::UserVerificationResponse::New(
            user_verification_result, is_first_user_verification.value()));
  }

  // wifi credentials
  const base::Value::Dict* wifi_network_information = nullptr;
  if ((wifi_network_information =
           payload.FindDict(kWifiNetworkInformationKey))) {
    return DecodeWifiCredentials(*wifi_network_information);
  }

  // notify source of update response
  std::optional<bool> notify_source_of_update_ack_received;
  if ((notify_source_of_update_ack_received =
           payload.FindBool(kNotifySourceOfUpdateAckKey))) {
    return mojom::QuickStartMessage::NewNotifySourceOfUpdateResponse(
        mojom::NotifySourceOfUpdateResponse::New(
            notify_source_of_update_ack_received.value()));
  }

  LOG(ERROR) << "Unknown QuickStartPayload";
  return base::unexpected(mojom::QuickStartDecoderError::kUnknownPayload);
}

base::expected<mojom::QuickStartMessagePtr, mojom::QuickStartDecoderError>
QuickStartDecoder::DecodeWifiCredentials(
    const base::Value::Dict& wifi_network_information) {
  const std::string* ssid =
      wifi_network_information.FindString(kWifiNetworkSsidKey);
  if (!ssid) {
    LOG(ERROR) << "SSID cannot be found within WifiCredentialsResponse.";
    QuickStartMetrics::RecordWifiTransferResult(
        /*succeeded=*/false, /*failure_reason=*/QuickStartMetrics::
            WifiTransferResultFailureReason::kSsidNotFound);
    return base::unexpected(
        mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }

  if (ssid->length() == 0) {
    LOG(ERROR) << "SSID has a length of 0.";
    QuickStartMetrics::RecordWifiTransferResult(
        /*succeeded=*/false, /*failure_reason=*/QuickStartMetrics::
            WifiTransferResultFailureReason::kEmptySsid);
    return base::unexpected(
        mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }

  const std::string* security_type_string =
      wifi_network_information.FindString(kWifiNetworkSecurityTypeKey);
  if (!security_type_string) {
    LOG(ERROR)
        << "Security Type cannot be found within WifiCredentialsResponse";
    QuickStartMetrics::RecordWifiTransferResult(
        /*succeeded=*/false, /*failure_reason=*/QuickStartMetrics::
            WifiTransferResultFailureReason::kSecurityTypeNotFound);
    return base::unexpected(
        mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }

  std::optional<mojom::WifiSecurityType> maybe_security_type =
      WifiSecurityTypeFromString(*security_type_string);

  if (!maybe_security_type.has_value()) {
    LOG(ERROR) << "Security type was not a valid value.";
    QuickStartMetrics::RecordWifiTransferResult(
        /*succeeded=*/false, /*failure_reason=*/QuickStartMetrics::
            WifiTransferResultFailureReason::kInvalidSecurityType);
    return base::unexpected(
        mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }

  mojom::WifiSecurityType security_type = maybe_security_type.value();

  // Password may not be included in payload for passwordless, open networks.
  std::optional<std::string> password = std::nullopt;
  const std::string* password_ptr =
      wifi_network_information.FindString(kWifiNetworkPasswordKey);

  if (password_ptr && security_type == mojom::WifiSecurityType::kOpen) {
    LOG(ERROR) << "Password is found but network security type is open.";
    QuickStartMetrics::RecordWifiTransferResult(
        /*succeeded=*/false, /*failure_reason=*/QuickStartMetrics::
            WifiTransferResultFailureReason::kPasswordFoundAndOpenNetwork);
    return base::unexpected(
        mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }

  if (!password_ptr && security_type != mojom::WifiSecurityType::kOpen) {
    LOG(ERROR) << "Password cannot be found within WifiCredentialsResponse but "
                  "network is not open. wifi_security_type: "
               << security_type;
    QuickStartMetrics::RecordWifiTransferResult(
        /*succeeded=*/false,
        /*failure_reason=*/QuickStartMetrics::WifiTransferResultFailureReason::
            kPasswordNotFoundAndNotOpenNetwork);
    return base::unexpected(
        mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }

  if (password_ptr) {
    password = *password_ptr;
  }

  std::optional<bool> is_hidden =
      wifi_network_information.FindBool(kWifiNetworkIsHiddenKey);
  if (!is_hidden.has_value()) {
    LOG(ERROR)
        << "Wifi Hide Status cannot be found within WifiCredentialsResponse";
    QuickStartMetrics::RecordWifiTransferResult(
        /*succeeded=*/false, /*failure_reason=*/QuickStartMetrics::
            WifiTransferResultFailureReason::kWifiHideStatusNotFound);
    return base::unexpected(
        mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }

  return mojom::QuickStartMessage::NewWifiCredentials(
      mojom::WifiCredentials::New(*ssid, security_type, is_hidden.value(),
                                  password));
}

base::expected<mojom::QuickStartMessagePtr, mojom::QuickStartDecoderError>
QuickStartDecoder::DecodeBootstrapConfigurations(
    const base::Value::Dict& payload) {
  return mojom::QuickStartMessage::NewBootstrapConfigurations(
      mojom::BootstrapConfigurations::New(
          FindInstanceIdInBootstrapConfigurations(payload),
          FindIsSupervisedAccountInBootstrapConfigurations(payload),
          FindEmailInBootstrapConfigurations(payload)));
}

}  // namespace ash::quick_start
