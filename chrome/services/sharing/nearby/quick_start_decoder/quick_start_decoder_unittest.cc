// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quick_start_decoder.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chromeos/ash/components/quick_start/quick_start_message.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-forward.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_start {

using QuickStartMessage = ash::quick_start::QuickStartMessage;

namespace {

constexpr char kCredentialIdKey[] = "id";
constexpr char kEntitiyIdMapKey[] = "id";
constexpr char kDeviceDetailsKey[] = "deviceDetails";
constexpr char kCryptauthDeviceIdKey[] = "cryptauthDeviceId";
constexpr char kExampleCryptauthDeviceId[] = "helloworld";
constexpr char kFidoMessageKey[] = "fidoMessage";
constexpr uint8_t kSuccess = 0x00;
constexpr uint8_t kCtap2ErrInvalidCBOR = 0x12;
constexpr int kCborDecoderErrorInvalidUtf8 = 6;
constexpr int kCborDecoderNoError = 0;
constexpr int kCborDecoderUnknownError = 14;

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

constexpr int kUserVerifiedStatusCode = 0;

const std::vector<uint8_t> kValidCredentialId = {0x01, 0x02, 0x03};
const std::vector<uint8_t> kValidAuthData = {0x02, 0x03, 0x04};
const std::vector<uint8_t> kValidSignature = {0x03, 0x04, 0x05};

using GetAssertionStatus = mojom::GetAssertionResponse::GetAssertionStatus;

std::vector<uint8_t> BuildEncodedResponseData(
    std::vector<uint8_t> credential_id,
    std::vector<uint8_t> auth_data,
    std::vector<uint8_t> signature,
    std::vector<uint8_t> user_id,
    uint8_t status) {
  cbor::Value::MapValue cbor_map;
  cbor::Value::MapValue credential_map;
  credential_map[cbor::Value(kCredentialIdKey)] = cbor::Value(credential_id);
  cbor_map[cbor::Value(1)] = cbor::Value(credential_map);
  cbor_map[cbor::Value(2)] = cbor::Value(auth_data);
  cbor_map[cbor::Value(3)] = cbor::Value(signature);
  cbor::Value::MapValue user_map;
  user_map[cbor::Value(kEntitiyIdMapKey)] = cbor::Value(user_id);
  cbor_map[cbor::Value(4)] = cbor::Value(user_map);
  absl::optional<std::vector<uint8_t>> cbor_bytes =
      cbor::Writer::Write(cbor::Value(std::move(cbor_map)));
  DCHECK(cbor_bytes);
  std::vector<uint8_t> response_bytes = std::move(*cbor_bytes);
  // Add the status byte to the beginning of this now fully encoded cbor bytes
  // vector.
  response_bytes.insert(response_bytes.begin(), status);
  return response_bytes;
}

}  // namespace

class QuickStartDecoderTest : public testing::Test {
 public:
  QuickStartDecoderTest() {
    QuickStartMessage::DisableSandboxCheckForTesting();
    decoder_ = std::make_unique<QuickStartDecoder>(
        remote_.BindNewPipeAndPassReceiver(), base::DoNothing());
  }

  mojom::GetAssertionResponsePtr DoDecodeGetAssertionResponse(
      const std::vector<uint8_t>& data) {
    return decoder_->DoDecodeGetAssertionResponse(data);
  }

  void DoDecodeBootstrapConfigurations(
      const std::vector<uint8_t>& data,
      QuickStartDecoder::DecodeBootstrapConfigurationsCallback callback) {
    return decoder_->DoDecodeBootstrapConfigurations(data, std::move(callback));
  }

  void DoDecodeWifiCredentialsResponse(
      QuickStartMessage* message,
      QuickStartDecoder::DecodeWifiCredentialsResponseCallback callback) {
    return decoder_->DoDecodeWifiCredentialsResponse(
        ConvertMessageToBytes(message), std::move(callback));
  }

  absl::optional<bool> DoDecodeNotifySourceOfUpdateResponse(
      QuickStartMessage* message) {
    return decoder_->DoDecodeNotifySourceOfUpdateResponse(
        ConvertMessageToBytes(message));
  }

  absl::optional<std::vector<uint8_t>> ExtractFidoDataFromJsonResponse(
      const std::vector<uint8_t>& data) {
    return decoder_->ExtractFidoDataFromJsonResponse(data);
  }

  QuickStartDecoder* decoder() const { return decoder_.get(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::Remote<mojom::QuickStartDecoder> remote_;
  std::unique_ptr<QuickStartDecoder> decoder_;

  std::vector<uint8_t> ConvertMessageToBytes(QuickStartMessage* message) {
    std::string json;
    base::JSONWriter::Write(*message->GenerateEncodedMessage(), &json);

    std::vector<uint8_t> payload(json.begin(), json.end());

    return payload;
  }

  std::vector<uint8_t> BuildSecondDeviceAuthPayload(std::vector<uint8_t> data) {
    // Package FIDO GetAssertion command bytes into MessagePayload.
    QuickStartMessage message(QuickStartMessageType::kSecondDeviceAuthPayload);

    message.GetPayload()->Set(kFidoMessageKey, base::Base64Encode(data));
    return ConvertMessageToBytes(&message);
  }
};

TEST_F(QuickStartDecoderTest, ConvertCtapDeviceResponseCodeTest_InRange) {
  std::vector<uint8_t> credential_id = {0x01, 0x02, 0x03};
  std::vector<uint8_t> auth_data = {};
  std::vector<uint8_t> signature = {};
  std::vector<uint8_t> user_id = {};
  // kCtap2ErrActionTimeout
  uint8_t status_code = 0x3A;
  std::vector<uint8_t> data = BuildEncodedResponseData(
      credential_id, auth_data, signature, user_id, status_code);
  std::vector<uint8_t> message = BuildSecondDeviceAuthPayload(data);
  mojom::GetAssertionResponsePtr response =
      DoDecodeGetAssertionResponse(std::move(message));
  EXPECT_EQ(response->ctap_device_response_code, status_code);
  EXPECT_EQ(response->status, GetAssertionStatus::kCtapResponseError);
  EXPECT_TRUE(response->credential_id.empty());
}

TEST_F(QuickStartDecoderTest, ConvertCtapDeviceRespnoseCodeTest_OutOfRange) {
  std::vector<uint8_t> auth_data = {};
  std::vector<uint8_t> signature = {};
  std::vector<uint8_t> user_id = {};
  // Unmapped error byte
  uint8_t status_code = 0x07;
  std::vector<uint8_t> data = BuildEncodedResponseData(
      kValidCredentialId, auth_data, signature, user_id, status_code);
  std::vector<uint8_t> message = BuildSecondDeviceAuthPayload(data);
  mojom::GetAssertionResponsePtr response =
      DoDecodeGetAssertionResponse(std::move(message));
  EXPECT_EQ(response->ctap_device_response_code, status_code);
  EXPECT_EQ(response->status, GetAssertionStatus::kCtapResponseError);
  EXPECT_TRUE(response->credential_id.empty());
}

TEST_F(QuickStartDecoderTest, CborDecodeGetAssertionResponse_DecoderError) {
  // UTF-8 validation should not stop at the first NUL character in the string.
  // That is, a string with an invalid byte sequence should fail UTF-8
  // validation even if the invalid character is located after one or more NUL
  // characters. Here, 0xA6 is an unexpected continuation byte.

  // Include 0x00 as first byte for kSuccess CtapDeviceResponse status.
  std::vector<uint8_t> data = {0x00, 0x63, 0x00, 0x00, 0xA6};
  std::vector<uint8_t> message = BuildSecondDeviceAuthPayload(data);
  int expected = kCborDecoderErrorInvalidUtf8;
  mojom::GetAssertionResponsePtr response =
      DoDecodeGetAssertionResponse(std::move(message));
  EXPECT_EQ(response->cbor_decoder_error, expected);
  EXPECT_EQ(response->status, GetAssertionStatus::kCborDecoderError);
  EXPECT_TRUE(response->credential_id.empty());
}

TEST_F(QuickStartDecoderTest, DecodeGetAssertionResponse_ResponseIsNotJson) {
  std::vector<uint8_t> data;
  uint8_t expected_device_response_code = kCtap2ErrInvalidCBOR;
  int expected_decoder_error = kCborDecoderUnknownError;
  mojom::GetAssertionResponsePtr response =
      DoDecodeGetAssertionResponse(std::move(data));
  EXPECT_EQ(response->ctap_device_response_code, expected_device_response_code);
  EXPECT_EQ(response->cbor_decoder_error, expected_decoder_error);
  EXPECT_EQ(response->status, GetAssertionStatus::kMessagePayloadParseError);
  EXPECT_TRUE(response->credential_id.empty());
}

TEST_F(QuickStartDecoderTest, DecodeGetAssertionResponse_EmptyResponse) {
  std::vector<uint8_t> data{};
  uint8_t expected_device_response_code = kCtap2ErrInvalidCBOR;
  int expected_decoder_error = kCborDecoderUnknownError;
  std::vector<uint8_t> message = BuildSecondDeviceAuthPayload(data);
  mojom::GetAssertionResponsePtr response =
      DoDecodeGetAssertionResponse(std::move(message));
  EXPECT_EQ(response->ctap_device_response_code, expected_device_response_code);
  EXPECT_EQ(response->cbor_decoder_error, expected_decoder_error);
  EXPECT_EQ(response->status, GetAssertionStatus::kCtapResponseError);
  EXPECT_TRUE(response->credential_id.empty());
}

TEST_F(QuickStartDecoderTest, DecodeGetAssertionResponse_OnlyStatusCode) {
  std::vector<uint8_t> data{0x00};
  uint8_t expected_device_response_code = kCtap2ErrInvalidCBOR;
  int expected_decoder_error = kCborDecoderUnknownError;
  std::vector<uint8_t> message = BuildSecondDeviceAuthPayload(data);
  mojom::GetAssertionResponsePtr response =
      DoDecodeGetAssertionResponse(std::move(message));
  EXPECT_EQ(response->ctap_device_response_code, expected_device_response_code);
  EXPECT_EQ(response->cbor_decoder_error, expected_decoder_error);
  EXPECT_EQ(response->status, GetAssertionStatus::kCtapResponseError);
  EXPECT_TRUE(response->credential_id.empty());
}

TEST_F(QuickStartDecoderTest, DecodeGetAssertionResponse_Valid) {
  std::string expected_credential_id(kValidCredentialId.begin(),
                                     kValidCredentialId.end());
  std::string email = "testcase@google.com";
  std::vector<uint8_t> user_id(email.begin(), email.end());
  // kSuccess
  uint8_t status = kSuccess;
  std::vector<uint8_t> data = BuildEncodedResponseData(
      kValidCredentialId, kValidAuthData, kValidSignature, user_id, status);
  std::vector<uint8_t> message = BuildSecondDeviceAuthPayload(data);
  mojom::GetAssertionResponsePtr response =
      DoDecodeGetAssertionResponse(std::move(message));
  EXPECT_EQ(response->ctap_device_response_code, kSuccess);
  EXPECT_EQ(response->cbor_decoder_error, kCborDecoderNoError);
  EXPECT_EQ(response->status, GetAssertionStatus::kSuccess);
  EXPECT_EQ(response->credential_id, expected_credential_id);
  EXPECT_EQ(response->email, email);
  EXPECT_EQ(response->auth_data, kValidAuthData);
  EXPECT_EQ(response->signature, kValidSignature);
}

TEST_F(QuickStartDecoderTest, DecodeGetAssertionResponse_ValidEmptyValues) {
  std::vector<uint8_t> credential_id = {};
  std::string expected_credential_id(credential_id.begin(),
                                     credential_id.end());
  std::string email = "";
  std::vector<uint8_t> user_id(email.begin(), email.end());
  // kSuccess
  uint8_t status = kSuccess;
  std::vector<uint8_t> data = BuildEncodedResponseData(
      credential_id, kValidAuthData, kValidSignature, user_id, status);
  std::vector<uint8_t> message = BuildSecondDeviceAuthPayload(data);
  mojom::GetAssertionResponsePtr response =
      DoDecodeGetAssertionResponse(std::move(message));
  EXPECT_EQ(response->ctap_device_response_code, kSuccess);
  EXPECT_EQ(response->cbor_decoder_error, kCborDecoderNoError);
  EXPECT_EQ(response->status, GetAssertionStatus::kSuccess);
  EXPECT_EQ(response->credential_id, expected_credential_id);
  EXPECT_EQ(response->email, email);
  EXPECT_EQ(response->auth_data, kValidAuthData);
  EXPECT_EQ(response->signature, kValidSignature);
}

TEST_F(QuickStartDecoderTest,
       DecodeBootstrapConfigurations_EmptyMessagePayload) {
  QuickStartMessage message(QuickStartMessageType::kBootstrapConfigurations);

  base::test::TestFuture<
      ::ash::quick_start::mojom::BootstrapConfigurationsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeBootstrapConfigurations(ConvertMessageToBytes(&message),
                                  future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest,
       DecodeBootstrapConfigurations_EmptyBootstrapConfigurations) {
  QuickStartMessage message(QuickStartMessageType::kBootstrapConfigurations);

  base::test::TestFuture<
      ::ash::quick_start::mojom::BootstrapConfigurationsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeBootstrapConfigurations(ConvertMessageToBytes(&message),
                                  future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest,
       DecodeBootstrapConfigurations_EmptyDeviceDetails) {
  base::Value::Dict device_details;

  QuickStartMessage message(QuickStartMessageType::kBootstrapConfigurations);
  message.GetPayload()->Set(kDeviceDetailsKey, std::move(device_details));

  base::test::TestFuture<
      ::ash::quick_start::mojom::BootstrapConfigurationsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeBootstrapConfigurations(ConvertMessageToBytes(&message),
                                  future.GetCallback());

  EXPECT_FALSE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<0>()->cryptauth_device_id, "");
  EXPECT_EQ(future.Get<1>(), absl::nullopt);
}

TEST_F(QuickStartDecoderTest,
       DecodeBootstrapConfigurations_EmptyCryptauthDeviceId) {
  base::Value::Dict device_details;
  device_details.Set(kCryptauthDeviceIdKey, "");

  QuickStartMessage message(QuickStartMessageType::kBootstrapConfigurations);
  message.GetPayload()->Set(kDeviceDetailsKey, std::move(device_details));

  base::test::TestFuture<
      ::ash::quick_start::mojom::BootstrapConfigurationsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeBootstrapConfigurations(ConvertMessageToBytes(&message),
                                  future.GetCallback());

  EXPECT_FALSE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<0>()->cryptauth_device_id, "");
  EXPECT_EQ(future.Get<1>(), absl::nullopt);
}

TEST_F(QuickStartDecoderTest,
       DecodeBootstrapConfigurations_ValidBootstrapConfigurations) {
  base::Value::Dict device_details;
  device_details.Set(kCryptauthDeviceIdKey, kExampleCryptauthDeviceId);

  QuickStartMessage message(QuickStartMessageType::kBootstrapConfigurations);
  message.GetPayload()->Set(kDeviceDetailsKey, std::move(device_details));

  base::test::TestFuture<
      ::ash::quick_start::mojom::BootstrapConfigurationsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeBootstrapConfigurations(ConvertMessageToBytes(&message),
                                  future.GetCallback());
  EXPECT_FALSE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<0>()->cryptauth_device_id, kExampleCryptauthDeviceId);
  EXPECT_EQ(future.Get<1>(), absl::nullopt);
}

TEST_F(QuickStartDecoderTest, ExtractFidoDataFromValidJsonResponse) {
  // Build a FIDO Message
  std::string expected_credential_id(kValidCredentialId.begin(),
                                     kValidCredentialId.end());
  std::string email = "testcase@google.com";
  std::vector<uint8_t> user_id(email.begin(), email.end());
  // kSuccess
  uint8_t status = kSuccess;
  std::vector<uint8_t> data = BuildEncodedResponseData(
      kValidCredentialId, kValidAuthData, kValidSignature, user_id, status);

  std::vector<uint8_t> payload = BuildSecondDeviceAuthPayload(data);

  absl::optional<std::vector<uint8_t>> result =
      ExtractFidoDataFromJsonResponse(payload);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), data);
}

TEST_F(QuickStartDecoderTest,
       ExtractFidoDataFromJsonResponseFailsIfFidoDataMissingFromPayload) {
  QuickStartMessage message(QuickStartMessageType::kSecondDeviceAuthPayload);

  absl::optional<std::vector<uint8_t>> result =
      ExtractFidoDataFromJsonResponse(ConvertMessageToBytes(&message));
  EXPECT_FALSE(result.has_value());
}

TEST_F(QuickStartDecoderTest,
       ExtractFidoDataFromJsonResponseFailsIfSecondDeviceAuthPayloadMissing) {
  base::Value::Dict message_payload;

  std::string json_serialized_payload;
  base::JSONWriter::Write(message_payload, &json_serialized_payload);
  std::vector<uint8_t> response_bytes(json_serialized_payload.begin(),
                                      json_serialized_payload.end());

  absl::optional<std::vector<uint8_t>> result =
      ExtractFidoDataFromJsonResponse(response_bytes);
  EXPECT_FALSE(result.has_value());
}

TEST_F(QuickStartDecoderTest,
       ExtractFidoDataFromJsonResponseFailsIfPayloadIsNotJsonDictionary) {
  std::string message_payload = "This is a JSON string";

  std::string json_serialized_payload;
  base::JSONWriter::Write(message_payload, &json_serialized_payload);
  std::vector<uint8_t> response_bytes(json_serialized_payload.begin(),
                                      json_serialized_payload.end());

  absl::optional<std::vector<uint8_t>> result =
      ExtractFidoDataFromJsonResponse(response_bytes);
  EXPECT_FALSE(result.has_value());
}

TEST_F(QuickStartDecoderTest,
       ExtractFidoDataFromJsonResponseFailsIfResponseIsNotJson) {
  // This is just a random payload that is not a valid JSON.
  std::vector<uint8_t> random_payload = {0x01, 0x02, 0x03};

  absl::optional<std::vector<uint8_t>> result =
      ExtractFidoDataFromJsonResponse(random_payload);
  EXPECT_FALSE(result.has_value());
}

TEST_F(QuickStartDecoderTest, ExtractWifiInformationPassesOnValidResponse) {
  base::Value::Dict wifi_information;
  wifi_information.Set(kWifiNetworkSsidKey, "ssid");
  wifi_information.Set(kWifiNetworkPasswordKey, "password");
  wifi_information.Set(kWifiNetworkSecurityTypeKey, "PSK");
  wifi_information.Set(kWifiNetworkIsHiddenKey, true);

  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kWifiNetworkInformationKey,
                            std::move(wifi_information));

  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeWifiCredentialsResponse(&message, future.GetCallback());

  ASSERT_FALSE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<0>()->ssid, "ssid");
  EXPECT_EQ(future.Get<0>()->password, "password");
  EXPECT_EQ(future.Get<0>()->security_type, mojom::WifiSecurityType::kPSK);
  EXPECT_TRUE(future.Get<0>()->is_hidden);
  EXPECT_EQ(future.Get<1>(), absl::nullopt);
}

TEST_F(QuickStartDecoderTest,
       ExtractWifiInformationPassesWhenMissingPasswordAndOpenNetwork) {
  base::Value::Dict wifi_information;
  wifi_information.Set(kWifiNetworkSsidKey, "ssid");
  wifi_information.Set(kWifiNetworkSecurityTypeKey, "Open");
  wifi_information.Set(kWifiNetworkIsHiddenKey, true);

  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kWifiNetworkInformationKey,
                            std::move(wifi_information));

  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeWifiCredentialsResponse(&message, future.GetCallback());

  ASSERT_FALSE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<0>()->password, absl::nullopt);
}

TEST_F(QuickStartDecoderTest,
       ExtractWifiInformationFailsWhenPasswordFoundAndOpenNetwork) {
  base::Value::Dict wifi_information;
  wifi_information.Set(kWifiNetworkSsidKey, "ssid");
  wifi_information.Set(kWifiNetworkPasswordKey, "password");
  wifi_information.Set(kWifiNetworkSecurityTypeKey, "Open");
  wifi_information.Set(kWifiNetworkIsHiddenKey, true);

  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kWifiNetworkInformationKey,
                            std::move(wifi_information));

  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeWifiCredentialsResponse(&message, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest,
       ExtractWifiInformationFailsWhenMissingPasswordAndNotOpenNetwork_PSK) {
  base::Value::Dict wifi_information;
  wifi_information.Set(kWifiNetworkSsidKey, "ssid");
  wifi_information.Set(kWifiNetworkSecurityTypeKey, "PSK");
  wifi_information.Set(kWifiNetworkIsHiddenKey, true);

  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kWifiNetworkInformationKey,
                            std::move(wifi_information));

  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeWifiCredentialsResponse(&message, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest,
       ExtractWifiInformationFailsWhenMissingPasswordAndNotOpenNetwork_WEP) {
  base::Value::Dict wifi_information;
  wifi_information.Set(kWifiNetworkSsidKey, "ssid");
  wifi_information.Set(kWifiNetworkSecurityTypeKey, "WEP");
  wifi_information.Set(kWifiNetworkIsHiddenKey, true);

  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kWifiNetworkInformationKey,
                            std::move(wifi_information));

  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeWifiCredentialsResponse(&message, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest,
       ExtractWifiInformationFailsWhenMissingPasswordAndNotOpenNetwork_EAP) {
  base::Value::Dict wifi_information;
  wifi_information.Set(kWifiNetworkSsidKey, "ssid");
  wifi_information.Set(kWifiNetworkSecurityTypeKey, "EAP");
  wifi_information.Set(kWifiNetworkIsHiddenKey, true);

  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kWifiNetworkInformationKey,
                            std::move(wifi_information));

  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeWifiCredentialsResponse(&message, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest,
       ExtractWifiInformationFailsWhenMissingPasswordAndNotOpenNetwork_OWE) {
  base::Value::Dict wifi_information;
  wifi_information.Set(kWifiNetworkSsidKey, "ssid");
  wifi_information.Set(kWifiNetworkSecurityTypeKey, "OWE");
  wifi_information.Set(kWifiNetworkIsHiddenKey, true);

  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kWifiNetworkInformationKey,
                            std::move(wifi_information));

  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeWifiCredentialsResponse(&message, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest,
       ExtractWifiInformationFailsWhenMissingPasswordAndNotOpenNetwork_SAE) {
  base::Value::Dict wifi_information;
  wifi_information.Set(kWifiNetworkSsidKey, "ssid");
  wifi_information.Set(kWifiNetworkSecurityTypeKey, "SAE");
  wifi_information.Set(kWifiNetworkIsHiddenKey, true);

  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kWifiNetworkInformationKey,
                            std::move(wifi_information));

  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeWifiCredentialsResponse(&message, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest, ExtractWifiInformationFailsIfSSIDLengthIsZero) {
  base::Value::Dict wifi_information;
  wifi_information.Set(kWifiNetworkSsidKey, "");
  wifi_information.Set(kWifiNetworkPasswordKey, "password");
  wifi_information.Set(kWifiNetworkSecurityTypeKey, "PSK");
  wifi_information.Set(kWifiNetworkIsHiddenKey, true);

  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kWifiNetworkInformationKey,
                            std::move(wifi_information));

  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeWifiCredentialsResponse(&message, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest, ExtractWifiInformationFailsWhenMissingSSID) {
  base::Value::Dict wifi_information;
  wifi_information.Set(kWifiNetworkPasswordKey, "password");
  wifi_information.Set(kWifiNetworkSecurityTypeKey, "PSK");
  wifi_information.Set(kWifiNetworkIsHiddenKey, true);

  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kWifiNetworkInformationKey,
                            std::move(wifi_information));

  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeWifiCredentialsResponse(&message, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest,
       ExtractWifiInformationFailsWhenMissingSecurityType) {
  base::Value::Dict wifi_information;
  wifi_information.Set(kWifiNetworkSsidKey, "ssid");
  wifi_information.Set(kWifiNetworkPasswordKey, "password");
  wifi_information.Set(kWifiNetworkIsHiddenKey, true);

  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kWifiNetworkInformationKey,
                            std::move(wifi_information));

  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeWifiCredentialsResponse(&message, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest,
       ExtractWifiInformationFailsOnInvalidSecurityType) {
  base::Value::Dict wifi_information;
  wifi_information.Set(kWifiNetworkSsidKey, "ssid");
  wifi_information.Set(kWifiNetworkPasswordKey, "password");
  wifi_information.Set(kWifiNetworkSecurityTypeKey, "invalid");
  wifi_information.Set(kWifiNetworkIsHiddenKey, true);

  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kWifiNetworkInformationKey,
                            std::move(wifi_information));

  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeWifiCredentialsResponse(&message, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest,
       ExtractWifiInformationFailsWhenMissingHiddenStatus) {
  base::Value::Dict wifi_information;
  wifi_information.Set(kWifiNetworkSsidKey, "ssid");
  wifi_information.Set(kWifiNetworkPasswordKey, "password");
  wifi_information.Set(kWifiNetworkSecurityTypeKey, "PSK");

  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kWifiNetworkInformationKey,
                            std::move(wifi_information));

  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeWifiCredentialsResponse(&message, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest,
       ExtractWifiInformationFailsWhenMissingWifiInformation) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);

  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DoDecodeWifiCredentialsResponse(&message, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest, DecodeNotifySourceOfUpdateResponseSuccess) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kNotifySourceOfUpdateAckKey, true);

  EXPECT_TRUE(DoDecodeNotifySourceOfUpdateResponse(&message).has_value());
  EXPECT_TRUE(DoDecodeNotifySourceOfUpdateResponse(&message).value());

  message.GetPayload()->Set(kNotifySourceOfUpdateAckKey, false);

  EXPECT_TRUE(DoDecodeNotifySourceOfUpdateResponse(&message).has_value());
  EXPECT_FALSE(DoDecodeNotifySourceOfUpdateResponse(&message).value());
}

TEST_F(QuickStartDecoderTest,
       DecodeNotifySourceOfUpdateResponseFailsWhenMissingValue) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);

  EXPECT_FALSE(DoDecodeNotifySourceOfUpdateResponse(&message).has_value());
}

TEST_F(QuickStartDecoderTest, DecodeUserVerificationResultSucceeds) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kUserVerificationResultKey,
                            kUserVerifiedStatusCode);
  message.GetPayload()->Set(kIsFirstUserVerificationKey, true);

  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationResponsePtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  decoder()->DecodeUserVerificationResult(ConvertMessageToBytes(&message),
                                          future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  ASSERT_FALSE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<0>().get()->result,
            mojom::UserVerificationResult::kUserVerified);
  EXPECT_TRUE(future.Get<0>().get()->is_first_user_verification);
  EXPECT_EQ(future.Get<1>(), absl::nullopt);
}

TEST_F(QuickStartDecoderTest,
       DecodeUserVerificationResultFailsIfMessageIsNotJson) {
  std::vector<uint8_t> message;
  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationResponsePtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  decoder()->DecodeUserVerificationResult(message, future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kUnableToReadAsJSON);
}

TEST_F(QuickStartDecoderTest,
       DecodeUserVerificationResultFailsIfMissingStatusCode) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kIsFirstUserVerificationKey, true);

  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationResponsePtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  decoder()->DecodeUserVerificationResult(ConvertMessageToBytes(&message),
                                          future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest,
       DecodeUserVerificationResultFailsIfMissingIsFirstUserVerification) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kUserVerificationResultKey,
                            kUserVerifiedStatusCode);

  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationResponsePtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  decoder()->DecodeUserVerificationResult(ConvertMessageToBytes(&message),
                                          future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest,
       DecodeUserVerificationResultFailsIfStatusCodeIsInvalid) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kUserVerificationResultKey, 5);

  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationResponsePtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  decoder()->DecodeUserVerificationResult(ConvertMessageToBytes(&message),
                                          future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest, DecodeUserVerificationRequestSucceeds) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kAwaitingUserVerificationKey, true);

  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationRequestedPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  decoder()->DecodeUserVerificationRequested(ConvertMessageToBytes(&message),
                                             future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  ASSERT_FALSE(future.Get<0>().is_null());
  EXPECT_TRUE(future.Get<0>().get()->is_awaiting_user_verification);
  EXPECT_EQ(future.Get<1>(), absl::nullopt);
}

TEST_F(QuickStartDecoderTest, DecodeUserVerificationRequestFailsIfKeyMissing) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);

  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationRequestedPtr,
      absl::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  decoder()->DecodeUserVerificationRequested(ConvertMessageToBytes(&message),
                                             future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

}  // namespace ash::quick_start
