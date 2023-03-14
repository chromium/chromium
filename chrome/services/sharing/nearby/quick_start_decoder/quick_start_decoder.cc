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
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "sandbox/policy/sandbox.h"

namespace ash::quick_start {

namespace {

using CBOR = cbor::Value;
using GetAssertionStatus = mojom::GetAssertionResponse::GetAssertionStatus;

constexpr char kCredentialIdKey[] = "id";
constexpr char kEntitiyIdMapKey[] = "id";
constexpr char kBootstrapConfigurationsKey[] = "bootstrapConfigurations";
constexpr char kDeviceDetailsKey[] = "deviceDetails";
constexpr char kCryptauthDeviceIdKey[] = "cryptauthDeviceId";
constexpr uint8_t kCtapDeviceResponseSuccess = 0x00;
constexpr int kCborDecoderNoError = 0;
constexpr int kCborDecoderUnknownError = 14;
constexpr uint8_t kCtap2ErrInvalidCBOR = 0x12;
constexpr char kSecondDeviceAuthPayloadKey[] = "secondDeviceAuthPayload";
constexpr char kFidoMessageKey[] = "fidoMessage";

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
    mojo::PendingReceiver<mojom::QuickStartDecoder> receiver)
    : receiver_(this, std::move(receiver)) {}

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

mojom::BootstrapConfigurationsPtr
QuickStartDecoder::DoDecodeBootstrapConfigurations(
    const std::vector<uint8_t>& data) {
  std::string raw_message_payload(data.begin(), data.end());
  absl::optional<base::Value> message_payload_json =
      base::JSONReader::Read(raw_message_payload);
  if (!message_payload_json.has_value() || !message_payload_json->is_dict()) {
    LOG(ERROR) << "MessagePayload cannot be parsed as a JSON Dictionary.";
    return nullptr;
  }
  base::Value::Dict& message_payload = message_payload_json.value().GetDict();
  base::Value::Dict* bootstrap_configurations =
      message_payload.FindDict(kBootstrapConfigurationsKey);
  if (!bootstrap_configurations) {
    LOG(ERROR)
        << "BootstrapConfigurations cannot be found within MessagePayload.";
    return nullptr;
  }
  base::Value::Dict* device_details =
      bootstrap_configurations->FindDict(kDeviceDetailsKey);
  if (!device_details) {
    LOG(ERROR)
        << "DeviceDetails cannot be found within BootstrapConfigurations.";
    return nullptr;
  }
  std::string* cryptauth_device_id_ptr =
      device_details->FindString(kCryptauthDeviceIdKey);
  if (!cryptauth_device_id_ptr) {
    LOG(WARNING)
        << "CryptauthDeviceId for the Android Device could not be found.";
    return mojom::BootstrapConfigurations::New(/*cryptauth_device_id=*/"");
  }
  return mojom::BootstrapConfigurations::New(*cryptauth_device_id_ptr);
}

void QuickStartDecoder::DecodeBootstrapConfigurations(
    const std::vector<uint8_t>& data,
    DecodeBootstrapConfigurationsCallback callback) {
  DCHECK(sandbox::policy::Sandbox::IsProcessSandboxed());
  std::move(callback).Run(DoDecodeBootstrapConfigurations(data));
}

void QuickStartDecoder::DecodeGetAssertionResponse(
    const std::vector<uint8_t>& data,
    DecodeGetAssertionResponseCallback callback) {
  DCHECK(sandbox::policy::Sandbox::IsProcessSandboxed());
  std::move(callback).Run(DoDecodeGetAssertionResponse(data));
}

absl::optional<std::vector<uint8_t>>
QuickStartDecoder::ExtractFidoDataFromJsonResponse(
    const std::vector<uint8_t>& data) {
  std::string raw_message_payload(data.begin(), data.end());
  absl::optional<base::Value> message_payload_json =
      base::JSONReader::Read(raw_message_payload);
  if (!message_payload_json.has_value()) {
    LOG(ERROR) << "MessagePayload cannot be parsed as JSON";
    return absl::nullopt;
  }

  base::Value::Dict* message_json = message_payload_json->GetIfDict();
  if (!message_json) {
    LOG(ERROR) << "MessagePayload cannot be parsed as a JSON Dictionary.";
    return absl::nullopt;
  }

  base::Value::Dict* second_device_auth_payload =
      message_json->FindDict(kSecondDeviceAuthPayloadKey);
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

}  // namespace ash::quick_start
