// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quick_start_decoder.h"

#include "base/base64.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/ash/components/quick_start/quick_start_message.h"
#include "chromeos/ash/components/quick_start/quick_start_message_type.h"
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
using GetAssertionStatus = mojom::GetAssertionResponse::GetAssertionStatus;

constexpr char kCredentialIdKey[] = "id";
constexpr char kEntitiyIdMapKey[] = "id";
constexpr char kDeviceDetailsKey[] = "deviceDetails";
constexpr char kCryptauthDeviceIdKey[] = "cryptauthDeviceId";
constexpr uint8_t kCtapDeviceResponseSuccess = 0x00;
constexpr int kCborDecoderNoError = 0;
constexpr int kCborDecoderUnknownError = 14;
constexpr uint8_t kCtap2ErrInvalidCBOR = 0x12;
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

mojom::GetAssertionResponsePtr ParseGetAssertionResponse(
    cbor::Value decoded_response) {
  const cbor::Value::MapValue& response_map = decoded_response.GetMap();
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

  // According to FIDO CTAP2 GetAssertionResponse, authData is stored at CBOR
  // index 0x02.
  auto auth_data_value_it = response_map.find(CBOR(0x02));
  std::vector<uint8_t> auth_data;
  if (auth_data_value_it != response_map.end() &&
      auth_data_value_it->second.is_bytestring()) {
    auth_data = auth_data_value_it->second.GetBytestring();
  }

  // According to FIDO CTAP2 GetAssertionResponse, signature is stored at CBOR
  // index 0x03.
  auto signature_value_it = response_map.find(CBOR(0x03));
  std::vector<uint8_t> signature;
  if (signature_value_it != response_map.end() &&
      signature_value_it->second.is_bytestring()) {
    signature = signature_value_it->second.GetBytestring();
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

  return mojom::GetAssertionResponse::New(
      /*status=*/GetAssertionStatus::kSuccess,
      /*ctap_device_response_code=*/kCtapDeviceResponseSuccess,
      /*cbor_decoder_error=*/kCborDecoderNoError, email, credential_id,
      auth_data, signature);
}

mojom::GetAssertionResponsePtr BuildGetAssertionResponseError(
    GetAssertionStatus status,
    uint8_t ctap_device_response_code,
    int cbor_decoder_error) {
  return mojom::GetAssertionResponse::New(status, ctap_device_response_code,
                                          cbor_decoder_error,
                                          /*email=*/"", /*credential_id=*/"",
                                          /*auth_data=*/std::vector<uint8_t>{},
                                          /*signature=*/std::vector<uint8_t>{});
}

}  // namespace

QuickStartDecoder::QuickStartDecoder(
    mojo::PendingReceiver<mojom::QuickStartDecoder> receiver,
    base::OnceClosure on_disconnect)
    : receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(std::move(on_disconnect));
}

QuickStartDecoder::~QuickStartDecoder() = default;

mojom::GetAssertionResponsePtr QuickStartDecoder::DoDecodeGetAssertionResponse(
    const std::vector<uint8_t>& data) {
  absl::optional<std::vector<uint8_t>> parsed_response_bytes =
      ExtractFidoDataFromJsonResponse(data);
  if (!parsed_response_bytes.has_value()) {
    LOG(ERROR) << "Failed to extract Fido data from JSON response.";
    return BuildGetAssertionResponseError(
        GetAssertionStatus::kMessagePayloadParseError, kCtap2ErrInvalidCBOR,
        kCborDecoderUnknownError);
  }

  std::vector<unsigned char>& response_bytes = parsed_response_bytes.value();
  if (response_bytes.size() < 2) {
    LOG(ERROR) << "GetAssertionResponse requires a status code byte and "
                  "response bytes. Data in size: "
               << response_bytes.size();
    return BuildGetAssertionResponseError(
        GetAssertionStatus::kCtapResponseError, kCtap2ErrInvalidCBOR,
        kCborDecoderUnknownError);
  }
  uint8_t ctap_status = response_bytes[0];
  base::span<const uint8_t> cbor_bytes(response_bytes);
  cbor_bytes = cbor_bytes.subspan(1);
  if (ctap_status != kCtapDeviceResponseSuccess) {
    LOG(ERROR) << "Ctap Device Response Status Code is not Success(0x00). Got: "
               << ctap_status;
    return BuildGetAssertionResponseError(
        GetAssertionStatus::kCtapResponseError, ctap_status,
        kCborDecoderUnknownError);
  }
  std::pair<int, absl::optional<cbor::Value>> decoded_values =
      CborDecodeGetAssertionResponse(cbor_bytes);
  if (decoded_values.first != kCborDecoderNoError) {
    return BuildGetAssertionResponseError(GetAssertionStatus::kCborDecoderError,
                                          ctap_status, decoded_values.first);
  }
  if (!decoded_values.second || !decoded_values.second->is_map()) {
    LOG(ERROR) << "The CBOR decoded response values needs to be a valid CBOR "
                  "Value Map.";
    return BuildGetAssertionResponseError(GetAssertionStatus::kUnknownError,
                                          ctap_status, decoded_values.first);
  }
  return ParseGetAssertionResponse(std::move(decoded_values.second.value()));
}

void QuickStartDecoder::DoDecodeBootstrapConfigurations(
    const std::vector<uint8_t>& data,
    DecodeBootstrapConfigurationsCallback callback) {
  std::unique_ptr<QuickStartMessage> message = QuickStartMessage::ReadMessage(
      data, QuickStartMessageType::kBootstrapConfigurations);

  base::Value::Dict* device_details =
      message->GetPayload()->FindDict(kDeviceDetailsKey);
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
    const std::vector<uint8_t>& data,
    DecodeBootstrapConfigurationsCallback callback) {
  DoDecodeBootstrapConfigurations(data, std::move(callback));
}

void QuickStartDecoder::DecodeWifiCredentialsResponse(
    const std::vector<uint8_t>& data,
    DecodeWifiCredentialsResponseCallback callback) {
  DoDecodeWifiCredentialsResponse(data, std::move(callback));
}

void QuickStartDecoder::DecodeUserVerificationRequested(
    const std::vector<uint8_t>& data,
    DecodeUserVerificationRequestedCallback callback) {
  std::unique_ptr<ash::quick_start::QuickStartMessage> message =
      QuickStartMessage::ReadMessage(data,
                                     QuickStartMessageType::kQuickStartPayload);
  if (!message) {
    LOG(ERROR)
        << "Failed to read UserVerificationRequested as QuickStartMessage";
    std::move(callback).Run(nullptr,
                            mojom::QuickStartDecoderError::kUnableToReadAsJSON);
    return;
  }

  absl::optional<bool> is_awaiting_user_verification =
      message->GetPayload()->FindBool(kAwaitingUserVerificationKey);
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
    const std::vector<uint8_t>& data,
    DecodeUserVerificationResultCallback callback) {
  std::unique_ptr<ash::quick_start::QuickStartMessage> message =
      QuickStartMessage::ReadMessage(data,
                                     QuickStartMessageType::kQuickStartPayload);

  if (!message) {
    LOG(ERROR) << "Failed to read UserVerificationResult as QuickStartMessage";
    std::move(callback).Run(nullptr,
                            mojom::QuickStartDecoderError::kUnableToReadAsJSON);
    return;
  }

  absl::optional<int> user_verification_result_code =
      message->GetPayload()->FindInt(kUserVerificationResultKey);

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
      message->GetPayload()->FindBool(kIsFirstUserVerificationKey);
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
    const std::vector<uint8_t>& data,
    DecodeWifiCredentialsResponseCallback callback) {
  std::unique_ptr<ash::quick_start::QuickStartMessage> message =
      QuickStartMessage::ReadMessage(data,
                                     QuickStartMessageType::kQuickStartPayload);

  if (!message) {
    LOG(ERROR) << "Message cannot be parsed as a JSON Dictionary.";
    std::move(callback).Run(nullptr,
                            mojom::QuickStartDecoderError::kUnableToReadAsJSON);
    return;
  }

  base::Value::Dict* wifi_network_information =
      message->GetPayload()->FindDict(kWifiNetworkInformationKey);
  if (!wifi_network_information) {
    LOG(ERROR) << "Wifi Network information not present in payload";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
  }

  std::string* ssid = wifi_network_information->FindString(kWifiNetworkSsidKey);
  if (!ssid) {
    LOG(ERROR) << "SSID cannot be found within WifiCredentialsResponse.";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
  }

  if (ssid->length() == 0) {
    LOG(ERROR) << "SSID has a length of 0.";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
  }

  std::string* password =
      wifi_network_information->FindString(kWifiNetworkPasswordKey);
  if (!password) {
    LOG(ERROR) << "Password cannot be found within WifiCredentialsResponse";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
  }

  std::string* security_type_string =
      wifi_network_information->FindString(kWifiNetworkSecurityTypeKey);
  if (!security_type_string) {
    LOG(ERROR)
        << "Security Type cannot be found within WifiCredentialsResponse";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
  }

  absl::optional<mojom::WifiSecurityType> security_type =
      WifiSecurityTypeFromString(*security_type_string);

  if (!security_type.has_value()) {
    {
      LOG(ERROR) << "Security type was not a valid value.";
      std::move(callback).Run(
          nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
      return;
    }
  }

  absl::optional<bool> is_hidden =
      wifi_network_information->FindBool(kWifiNetworkIsHiddenKey);
  if (!is_hidden.has_value()) {
    LOG(ERROR)
        << "Wifi Hide Status cannot be found within WifiCredentialsResponse";
    std::move(callback).Run(
        nullptr, mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
    return;
  }

  std::move(callback).Run(
      mojom::WifiCredentials::New(*ssid, security_type.value(),
                                  is_hidden.value(), *password),
      absl::nullopt);
}

void QuickStartDecoder::DecodeGetAssertionResponse(
    const std::vector<uint8_t>& data,
    DecodeGetAssertionResponseCallback callback) {
  std::move(callback).Run(DoDecodeGetAssertionResponse(data));
}

absl::optional<std::vector<uint8_t>>
QuickStartDecoder::ExtractFidoDataFromJsonResponse(
    const std::vector<uint8_t>& data) {
  std::unique_ptr<ash::quick_start::QuickStartMessage> parsed_message =
      ash::quick_start::QuickStartMessage::ReadMessage(
          data, QuickStartMessageType::kSecondDeviceAuthPayload);

  if (!parsed_message) {
    LOG(ERROR) << "MessagePayload cannot be parsed as a JSON Dictionary.";
    return absl::nullopt;
  }

  base::Value::Dict* second_device_auth_payload = parsed_message->GetPayload();
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

  return base::Base64Decode(*fido_message);
}

void QuickStartDecoder::DecodeNotifySourceOfUpdateResponse(
    const std::vector<uint8_t>& data,
    DecodeNotifySourceOfUpdateResponseCallback callback) {
  std::move(callback).Run(DoDecodeNotifySourceOfUpdateResponse(data));
}

absl::optional<bool> QuickStartDecoder::DoDecodeNotifySourceOfUpdateResponse(
    const std::vector<uint8_t>& data) {
  std::unique_ptr<ash::quick_start::QuickStartMessage> message =
      QuickStartMessage::ReadMessage(data,
                                     QuickStartMessageType::kQuickStartPayload);

  if (!message) {
    LOG(ERROR) << "Notify Source of Update message cannot be parsed as a JSON "
                  "Dictionary.";
    return absl::nullopt;
  }

  return message->GetPayload()->FindBool(kNotifySourceOfUpdateAckKey);
}

}  // namespace ash::quick_start
