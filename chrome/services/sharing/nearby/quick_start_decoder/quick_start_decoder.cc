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
constexpr char kDeviceDetailsKey[] = "deviceDetails";
constexpr char kCryptauthDeviceIdKey[] = "cryptauthDeviceId";
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

std::pair<int, absl::optional<cbor::Value>> CborDecodeGetAssertionResponse(
    base::span<const uint8_t> response) {
  cbor::Reader::DecoderError error;
  cbor::Reader::Config config;

  config.error_code_out = &error;
  absl::optional<cbor::Value> cbor = cbor::Reader::Read(response, config);
  if (!cbor) {
    int converted_decode_error = static_cast<int>(error);
    LOG(ERROR) << "Error CBOR decoding the response bytes: "
               << cbor::Reader::ErrorCodeToString(error);
    return std::make_pair(converted_decode_error, absl::nullopt);
  }
  return std::make_pair(kCborDecoderNoError, std::move(cbor));
}
}  // namespace

QuickStartDecoder::QuickStartDecoder(
    mojo::PendingReceiver<mojom::QuickStartDecoder> receiver,
    base::OnceClosure on_disconnect)
    : receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(std::move(on_disconnect));
}

QuickStartDecoder::~QuickStartDecoder() = default;

void QuickStartDecoder::DoDecodeGetAssertionResponse(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeGetAssertionResponseCallback callback) {
  if (!data.has_value()) {
    LOG(ERROR) << "No response bytes received.";
    std::move(callback).Run(nullptr,
                            mojom::QuickStartDecoderError::kEmptyMessage);
    return;
  }

  absl::optional<std::vector<uint8_t>> parsed_response_bytes =
      ExtractFidoDataFromJsonResponse(data.value());
  if (!parsed_response_bytes.has_value()) {
    LOG(ERROR) << "Failed to extract Fido data from JSON response.";
    std::move(callback).Run(nullptr,
                            mojom::QuickStartDecoderError::kUnableToReadAsJSON);
    return;
  }

  std::vector<unsigned char>& response_bytes = parsed_response_bytes.value();
  if (response_bytes.size() < kExpectedResponseSize) {
    LOG(ERROR) << "GetAssertionResponse requires a status code byte and "
                  "response bytes. Data in size: "
               << response_bytes.size();
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
  }
  uint8_t ctap_status = response_bytes[0];
  base::span<const uint8_t> cbor_bytes(response_bytes);
  cbor_bytes = cbor_bytes.subspan(1);
  if (ctap_status != kCtapDeviceResponseSuccess) {
    LOG(ERROR) << "Ctap Device Response Status Code is not Success(0x00). Got: "
               << ctap_status;
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
  }
  std::pair<int, absl::optional<cbor::Value>> decoded_values =
      CborDecodeGetAssertionResponse(cbor_bytes);
  if (decoded_values.first != kCborDecoderNoError) {
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
  }
  if (!decoded_values.second || !decoded_values.second->is_map()) {
    LOG(ERROR) << "The CBOR decoded response values needs to be a valid CBOR "
                  "Value Map.";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
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
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
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
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
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
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
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
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
  }

  std::move(callback).Run(mojom::FidoAssertionResponse::New(
                              email, credential_id, auth_data, signature),
                          absl::nullopt);
}

void QuickStartDecoder::DoDecodeBootstrapConfigurations(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeBootstrapConfigurationsCallback callback) {
  if (!data.has_value()) {
    LOG(ERROR) << "No response bytes received.";
    std::move(callback).Run(nullptr,
                            mojom::QuickStartDecoderError::kEmptyMessage);
    return;
  }

  QuickStartMessage::ReadResult read_result = QuickStartMessage::ReadMessage(
      data.value(), QuickStartMessageType::kBootstrapConfigurations);

  if (!read_result.has_value()) {
    LOG(ERROR) << "Bootstrap Configurations decoder failed";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  }

  base::Value::Dict* device_details =
      read_result.value()->GetPayload()->FindDict(kDeviceDetailsKey);
  if (!device_details) {
    LOG(ERROR)
        << "DeviceDetails cannot be found within BootstrapConfigurations.";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
  }
  std::string* cryptauth_device_id_ptr =
      device_details->FindString(kCryptauthDeviceIdKey);
  if (!cryptauth_device_id_ptr) {
    LOG(WARNING)
        << "CryptauthDeviceId for the Android Device could not be found.";
    std::move(callback).Run(
        mojom::BootstrapConfigurations::New(/*cryptauth_device_id=*/""),
        absl::nullopt);
    return;
  }
  std::move(callback).Run(
      mojom::BootstrapConfigurations::New(*cryptauth_device_id_ptr),
      absl::nullopt);
}

void QuickStartDecoder::DecodeBootstrapConfigurations(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeBootstrapConfigurationsCallback callback) {
  DoDecodeBootstrapConfigurations(data, std::move(callback));
}

void QuickStartDecoder::DecodeWifiCredentialsResponse(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeWifiCredentialsResponseCallback callback) {
  DoDecodeWifiCredentialsResponse(data, std::move(callback));
}

void QuickStartDecoder::DecodeUserVerificationRequested(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeUserVerificationRequestedCallback callback) {
  if (!data.has_value()) {
    LOG(ERROR) << "No response bytes received.";
    std::move(callback).Run(nullptr,
                            mojom::QuickStartDecoderError::kEmptyMessage);
    return;
  }

  QuickStartMessage::ReadResult read_result = QuickStartMessage::ReadMessage(
      data.value(), QuickStartMessageType::kQuickStartPayload);
  if (!read_result.has_value()) {
    LOG(ERROR)
        << "Failed to read UserVerificationRequested as QuickStartMessage";
    std::move(callback).Run(nullptr,
                            mojom::QuickStartDecoderError::kUnableToReadAsJSON);
    return;
  }

  absl::optional<bool> is_awaiting_user_verification =
      read_result.value()->GetPayload()->FindBool(kAwaitingUserVerificationKey);
  if (!is_awaiting_user_verification.has_value()) {
    LOG(ERROR) << "UserVerificationRequested message does not include "
                  "await_user_verification";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
  }

  std::move(callback).Run(mojom::UserVerificationRequested::New(
                              is_awaiting_user_verification.value()),
                          absl::nullopt);
}

void QuickStartDecoder::DecodeUserVerificationResult(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeUserVerificationResultCallback callback) {
  if (!data.has_value()) {
    LOG(ERROR) << "No response bytes received.";
    std::move(callback).Run(nullptr,
                            mojom::QuickStartDecoderError::kEmptyMessage);
    return;
  }

  QuickStartMessage::ReadResult read_result = QuickStartMessage::ReadMessage(
      data.value(), QuickStartMessageType::kQuickStartPayload);

  if (!read_result.has_value()) {
    LOG(ERROR) << "Failed to read UserVerificationResult as QuickStartMessage";
    std::move(callback).Run(nullptr,
                            mojom::QuickStartDecoderError::kUnableToReadAsJSON);
    return;
  }

  absl::optional<int> user_verification_result_code =
      read_result.value()->GetPayload()->FindInt(kUserVerificationResultKey);

  if (!user_verification_result_code.has_value()) {
    LOG(ERROR) << "User Verification Result was not include in verification "
                  "result message";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
  }

  mojom::UserVerificationResult user_verification_result =
      static_cast<mojom::UserVerificationResult>(
          user_verification_result_code.value());

  if (!mojom::IsKnownEnumValue(user_verification_result)) {
    LOG(ERROR) << "User Verification Result is an unknown status code";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
  }

  absl::optional<bool> is_first_user_verification =
      read_result.value()->GetPayload()->FindBool(kIsFirstUserVerificationKey);
  if (!is_first_user_verification.has_value()) {
    LOG(ERROR) << "Message does not contain key is_first_user_verification";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
  }

  std::move(callback).Run(
      mojom::UserVerificationResponse::New(user_verification_result,
                                           is_first_user_verification.value()),
      absl::nullopt);
}

void QuickStartDecoder::DoDecodeWifiCredentialsResponse(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeWifiCredentialsResponseCallback callback) {
  if (!data.has_value()) {
    LOG(ERROR) << "No response bytes received.";
    std::move(callback).Run(nullptr,
                            mojom::QuickStartDecoderError::kEmptyMessage);
    return;
  }

  QuickStartMessage::ReadResult read_result = QuickStartMessage::ReadMessage(
      data.value(), QuickStartMessageType::kQuickStartPayload);

  if (!read_result.has_value()) {
    LOG(ERROR) << "Message cannot be parsed as a JSON Dictionary.";
    std::move(callback).Run(nullptr,
                            mojom::QuickStartDecoderError::kUnableToReadAsJSON);
    quick_start_metrics::RecordWifiTransferResult(
        /*succeeded=*/false, /*failure_reason=*/quick_start_metrics::
            WifiTransferResultFailureReason::kUnableToReadAsJSON);
    return;
  }

  base::Value::Dict* wifi_network_information =
      read_result.value()->GetPayload()->FindDict(kWifiNetworkInformationKey);
  if (!wifi_network_information) {
    LOG(ERROR) << "Wifi Network information not present in payload";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    quick_start_metrics::RecordWifiTransferResult(
        /*succeeded=*/false, /*failure_reason=*/quick_start_metrics::
            WifiTransferResultFailureReason::kWifiNetworkInformationNotFound);
    return;
  }

  std::string* ssid = wifi_network_information->FindString(kWifiNetworkSsidKey);
  if (!ssid) {
    LOG(ERROR) << "SSID cannot be found within WifiCredentialsResponse.";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    quick_start_metrics::RecordWifiTransferResult(
        /*succeeded=*/false, /*failure_reason=*/quick_start_metrics::
            WifiTransferResultFailureReason::kSsidNotFound);
    return;
  }

  if (ssid->length() == 0) {
    LOG(ERROR) << "SSID has a length of 0.";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    quick_start_metrics::RecordWifiTransferResult(
        /*succeeded=*/false, /*failure_reason=*/quick_start_metrics::
            WifiTransferResultFailureReason::kEmptySsid);
    return;
  }

  std::string* security_type_string =
      wifi_network_information->FindString(kWifiNetworkSecurityTypeKey);
  if (!security_type_string) {
    LOG(ERROR)
        << "Security Type cannot be found within WifiCredentialsResponse";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    quick_start_metrics::RecordWifiTransferResult(
        /*succeeded=*/false, /*failure_reason=*/quick_start_metrics::
            WifiTransferResultFailureReason::kSecurityTypeNotFound);
    return;
  }

  absl::optional<mojom::WifiSecurityType> maybe_security_type =
      WifiSecurityTypeFromString(*security_type_string);

  if (!maybe_security_type.has_value()) {
    LOG(ERROR) << "Security type was not a valid value.";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    quick_start_metrics::RecordWifiTransferResult(
        /*succeeded=*/false, /*failure_reason=*/quick_start_metrics::
            WifiTransferResultFailureReason::kInvalidSecurityType);
    return;
  }

  mojom::WifiSecurityType security_type = maybe_security_type.value();

  // Password may not be included in payload for passwordless, open networks.
  absl::optional<std::string> password = absl::nullopt;
  std::string* password_ptr =
      wifi_network_information->FindString(kWifiNetworkPasswordKey);

  if (password_ptr && security_type == mojom::WifiSecurityType::kOpen) {
    LOG(ERROR) << "Password is found but network security type is open.";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    quick_start_metrics::RecordWifiTransferResult(
        /*succeeded=*/false, /*failure_reason=*/quick_start_metrics::
            WifiTransferResultFailureReason::kPasswordFoundAndOpenNetwork);
    return;
  }

  if (!password_ptr && security_type != mojom::WifiSecurityType::kOpen) {
    LOG(ERROR) << "Password cannot be found within WifiCredentialsResponse but "
                  "network is not open. wifi_security_type: "
               << security_type;
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    quick_start_metrics::RecordWifiTransferResult(
        /*succeeded=*/false, /*failure_reason=*/quick_start_metrics::
            WifiTransferResultFailureReason::
                kPasswordNotFoundAndNotOpenNetwork);
    return;
  }

  if (password_ptr) {
    password = *password_ptr;
  }

  absl::optional<bool> is_hidden =
      wifi_network_information->FindBool(kWifiNetworkIsHiddenKey);
  if (!is_hidden.has_value()) {
    LOG(ERROR)
        << "Wifi Hide Status cannot be found within WifiCredentialsResponse";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    quick_start_metrics::RecordWifiTransferResult(
        /*succeeded=*/false, /*failure_reason=*/quick_start_metrics::
            WifiTransferResultFailureReason::kWifiHideStatusNotFound);
    return;
  }

  std::move(callback).Run(
      mojom::WifiCredentials::New(*ssid, security_type, is_hidden.value(),
                                  password),
      absl::nullopt);
}

void QuickStartDecoder::DecodeGetAssertionResponse(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeGetAssertionResponseCallback callback) {
  DoDecodeGetAssertionResponse(std::move(data), std::move(callback));
}

absl::optional<std::vector<uint8_t>>
QuickStartDecoder::ExtractFidoDataFromJsonResponse(
    const std::vector<uint8_t>& data) {
  QuickStartMessage::ReadResult read_result =
      ash::quick_start::QuickStartMessage::ReadMessage(
          data, QuickStartMessageType::kSecondDeviceAuthPayload);

  if (!read_result.has_value()) {
    LOG(ERROR) << "MessagePayload cannot be parsed as a JSON Dictionary.";
    return absl::nullopt;
  }

  base::Value::Dict* second_device_auth_payload =
      read_result.value()->GetPayload();
  if (!second_device_auth_payload) {
    LOG(ERROR) << "secondDeviceAuthPayload cannot be found within Message.";
    return absl::nullopt;
  }

  std::string* fido_message =
      second_device_auth_payload->FindString(kFidoMessageKey);
  if (!fido_message) {
    LOG(ERROR) << "fidoMessage cannot be found within secondDeviceAuthPayload.";
    return absl::nullopt;
  }

  std::string base64_decoded_fido_message;

  if (!base::Base64Decode(*fido_message, &base64_decoded_fido_message,
                          base::Base64DecodePolicy::kForgiving)) {
    LOG(ERROR) << "Failed to decode fidoMessage as a Base64 String";
    return absl::nullopt;
  }

  return std::vector<uint8_t>(base64_decoded_fido_message.begin(),
                              base64_decoded_fido_message.end());
}

void QuickStartDecoder::DecodeNotifySourceOfUpdateResponse(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeNotifySourceOfUpdateResponseCallback callback) {
  std::move(callback).Run(DoDecodeNotifySourceOfUpdateResponse(data));
}

absl::optional<bool> QuickStartDecoder::DoDecodeNotifySourceOfUpdateResponse(
    const absl::optional<std::vector<uint8_t>>& data) {
  if (!data.has_value()) {
    LOG(ERROR) << "No response bytes received.";
    return absl::nullopt;
  }

  QuickStartMessage::ReadResult read_result = QuickStartMessage::ReadMessage(
      data.value(), QuickStartMessageType::kQuickStartPayload);

  if (!read_result.has_value()) {
    LOG(ERROR) << "Notify Source of Update message cannot be parsed as a JSON "
                  "Dictionary.";
    return absl::nullopt;
  }

  return read_result.value()->GetPayload()->FindBool(
      kNotifySourceOfUpdateAckKey);
}

}  // namespace ash::quick_start
