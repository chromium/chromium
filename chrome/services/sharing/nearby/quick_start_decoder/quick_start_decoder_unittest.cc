// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quick_start_decoder.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chromeos/ash/components/quick_start/quick_start_message.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
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
constexpr char kBootstrapConfigurationsKey[] = "bootstrapConfigurations";
constexpr char kDeviceDetailsKey[] = "deviceDetails";
constexpr char kInstanceIdKey[] = "instanceId";
constexpr char kExampleInstanceId[] = "helloworld";
constexpr char kNameKey[] = "name";
constexpr char kExampleEmail[] = "fakeEmail";
constexpr char kBootstrapAccountsKey[] = "bootstrapAccounts";
constexpr char kIsTransferUnicornKey[] = "isTransferUnicorn";
constexpr char kSecondDeviceAuthPayloadKey[] = "secondDeviceAuthPayload";
constexpr char kFidoMessageKey[] = "fidoMessage";
constexpr uint8_t kSuccess = 0x00;

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

// Key in UserVerificationMethod containing the verification method to be used.
constexpr char kUserVerificationMethodKey[] = "user_verification_method";

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

const char kWifiTransferResultHistogramName[] = "QuickStart.WifiTransferResult";
const char kWifiTransferResultFailureReasonHistogramName[] =
    "QuickStart.WifiTransferResult.FailureReason";

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
  std::optional<std::vector<uint8_t>> cbor_bytes =
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
    decoder_ = std::make_unique<QuickStartDecoder>(
        remote_.BindNewPipeAndPassReceiver(), base::DoNothing());
  }

  void DecodeGetAssertionResponse(
      const std::optional<std::vector<uint8_t>>& data,
      base::OnceCallback<void(mojom::FidoAssertionResponsePtr,
                              std::optional<mojom::QuickStartDecoderError>)>
          callback) {
    auto decoder_callback =
        [](base::OnceCallback<void(
               mojom::FidoAssertionResponsePtr,
               std::optional<mojom::QuickStartDecoderError>)> callback,
           mojom::QuickStartMessagePtr quick_start_message,
           std::optional<mojom::QuickStartDecoderError> error) {
          if (error.has_value()) {
            std::move(callback).Run(nullptr, error);
            return;
          }
          if (!quick_start_message ||
              !quick_start_message->is_fido_assertion_response()) {
            std::move(callback).Run(
                nullptr, mojom::QuickStartDecoderError::kUnexpectedMessageType);
            return;
          }
          std::move(callback).Run(
              quick_start_message->get_fido_assertion_response().Clone(),
              std::nullopt);
        };

    decoder_->DecodeQuickStartMessage(
        data, base::BindOnce(decoder_callback, std::move(callback)));
  }

  void DecodeBootstrapConfigurations(
      const std::optional<std::vector<uint8_t>>& data,
      base::OnceCallback<void(mojom::BootstrapConfigurationsPtr,
                              std::optional<mojom::QuickStartDecoderError>)>
          callback) {
    auto decoder_callback =
        [](base::OnceCallback<void(
               mojom::BootstrapConfigurationsPtr,
               std::optional<mojom::QuickStartDecoderError>)> callback,
           mojom::QuickStartMessagePtr quick_start_message,
           std::optional<mojom::QuickStartDecoderError> error) {
          if (error.has_value()) {
            std::move(callback).Run(nullptr, error);
            return;
          }
          if (!quick_start_message ||
              !quick_start_message->is_bootstrap_configurations()) {
            std::move(callback).Run(
                nullptr, mojom::QuickStartDecoderError::kUnexpectedMessageType);
            return;
          }
          std::move(callback).Run(
              quick_start_message->get_bootstrap_configurations().Clone(),
              std::nullopt);
        };

    decoder_->DecodeQuickStartMessage(
        data, base::BindOnce(decoder_callback, std::move(callback)));
  }

  void DecodeWifiCredentialsResponse(
      const std::optional<std::vector<uint8_t>>& data,
      base::OnceCallback<void(mojom::WifiCredentialsPtr,
                              std::optional<mojom::QuickStartDecoderError>)>
          callback) {
    auto decoder_callback =
        [](base::OnceCallback<void(
               mojom::WifiCredentialsPtr,
               std::optional<mojom::QuickStartDecoderError>)> callback,
           mojom::QuickStartMessagePtr quick_start_message,
           std::optional<mojom::QuickStartDecoderError> error) {
          if (error.has_value()) {
            std::move(callback).Run(nullptr, error);
            return;
          }
          if (!quick_start_message ||
              !quick_start_message->is_wifi_credentials()) {
            std::move(callback).Run(
                nullptr, mojom::QuickStartDecoderError::kUnexpectedMessageType);
            return;
          }
          std::move(callback).Run(
              quick_start_message->get_wifi_credentials().Clone(),
              std::nullopt);
        };

    decoder_->DecodeQuickStartMessage(
        data, base::BindOnce(decoder_callback, std::move(callback)));
  }

  void DecodeUserVerificationResult(
      const std::optional<std::vector<uint8_t>>& data,
      base::OnceCallback<void(mojom::UserVerificationResponsePtr,
                              std::optional<mojom::QuickStartDecoderError>)>
          callback) {
    auto decoder_callback =
        [](base::OnceCallback<void(
               mojom::UserVerificationResponsePtr,
               std::optional<mojom::QuickStartDecoderError>)> callback,
           mojom::QuickStartMessagePtr quick_start_message,
           std::optional<mojom::QuickStartDecoderError> error) {
          if (error.has_value()) {
            std::move(callback).Run(nullptr, error);
            return;
          }
          if (!quick_start_message ||
              !quick_start_message->is_user_verification_response()) {
            std::move(callback).Run(
                nullptr, mojom::QuickStartDecoderError::kUnexpectedMessageType);
            return;
          }
          std::move(callback).Run(
              quick_start_message->get_user_verification_response().Clone(),
              std::nullopt);
        };

    decoder_->DecodeQuickStartMessage(
        data, base::BindOnce(decoder_callback, std::move(callback)));
  }

  void DecodeUserVerificationMethod(
      const std::optional<std::vector<uint8_t>>& data,
      base::OnceCallback<void(mojom::UserVerificationMethodPtr,
                              std::optional<mojom::QuickStartDecoderError>)>
          callback) {
    auto decoder_callback =
        [](base::OnceCallback<void(
               mojom::UserVerificationMethodPtr,
               std::optional<mojom::QuickStartDecoderError>)> callback,
           mojom::QuickStartMessagePtr quick_start_message,
           std::optional<mojom::QuickStartDecoderError> error) {
          if (error.has_value()) {
            std::move(callback).Run(nullptr, error);
            return;
          }
          if (!quick_start_message ||
              !quick_start_message->is_user_verification_method()) {
            std::move(callback).Run(
                nullptr, mojom::QuickStartDecoderError::kUnexpectedMessageType);
            return;
          }
          std::move(callback).Run(
              quick_start_message->get_user_verification_method().Clone(),
              std::nullopt);
        };

    decoder_->DecodeQuickStartMessage(
        data, base::BindOnce(decoder_callback, std::move(callback)));
  }

  void DecodeUserVerificationRequested(
      const std::optional<std::vector<uint8_t>>& data,
      base::OnceCallback<void(mojom::UserVerificationRequestedPtr,
                              std::optional<mojom::QuickStartDecoderError>)>
          callback) {
    auto decoder_callback =
        [](base::OnceCallback<void(
               mojom::UserVerificationRequestedPtr,
               std::optional<mojom::QuickStartDecoderError>)> callback,
           mojom::QuickStartMessagePtr quick_start_message,
           std::optional<mojom::QuickStartDecoderError> error) {
          if (error.has_value()) {
            std::move(callback).Run(nullptr, error);
            return;
          }
          if (!quick_start_message ||
              !quick_start_message->is_user_verification_requested()) {
            std::move(callback).Run(
                nullptr, mojom::QuickStartDecoderError::kUnexpectedMessageType);
            return;
          }
          std::move(callback).Run(
              quick_start_message->get_user_verification_requested().Clone(),
              std::nullopt);
        };

    decoder_->DecodeQuickStartMessage(
        data, base::BindOnce(decoder_callback, std::move(callback)));
  }
  void DecodeNotifySourceOfUpdateResponse(
      QuickStartMessage* message,
      base::OnceCallback<void(mojom::NotifySourceOfUpdateResponsePtr,
                              std::optional<mojom::QuickStartDecoderError>)>
          callback) {
    auto decoder_callback =
        [](base::OnceCallback<void(
               mojom::NotifySourceOfUpdateResponsePtr,
               std::optional<mojom::QuickStartDecoderError>)> callback,
           mojom::QuickStartMessagePtr quick_start_message,
           std::optional<mojom::QuickStartDecoderError> error) {
          if (error.has_value()) {
            std::move(callback).Run(nullptr, error);
            return;
          }
          if (!quick_start_message ||
              !quick_start_message->is_notify_source_of_update_response()) {
            std::move(callback).Run(
                nullptr, mojom::QuickStartDecoderError::kUnexpectedMessageType);
            return;
          }
          std::move(callback).Run(
              quick_start_message->get_notify_source_of_update_response()
                  .Clone(),
              std::nullopt);
        };

    decoder_->DecodeQuickStartMessage(
        ConvertMessageToBytes(message),
        base::BindOnce(decoder_callback, std::move(callback)));
  }

  QuickStartDecoder* decoder() const { return decoder_.get(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::Remote<mojom::QuickStartDecoder> remote_;
  std::unique_ptr<QuickStartDecoder> decoder_;
  base::HistogramTester histogram_tester_;

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
  base::test::TestFuture<
      ::ash::quick_start::mojom::FidoAssertionResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeGetAssertionResponse(message, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
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
  base::test::TestFuture<
      ::ash::quick_start::mojom::FidoAssertionResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeGetAssertionResponse(message, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest, CborDecodeGetAssertionResponse_DecoderError) {
  // UTF-8 validation should not stop at the first NUL character in the string.
  // That is, a string with an invalid byte sequence should fail UTF-8
  // validation even if the invalid character is located after one or more NUL
  // characters. Here, 0xA6 is an unexpected continuation byte.

  // Include 0x00 as first byte for kSuccess CtapDeviceResponse status.
  std::vector<uint8_t> data = {0x00, 0x63, 0x00, 0x00, 0xA6};
  std::vector<uint8_t> message = BuildSecondDeviceAuthPayload(data);
  base::test::TestFuture<
      ::ash::quick_start::mojom::FidoAssertionResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeGetAssertionResponse(message, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest, DecodeGetAssertionResponse_ResponseIsNotJson) {
  std::vector<uint8_t> data;
  base::test::TestFuture<
      ::ash::quick_start::mojom::FidoAssertionResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeGetAssertionResponse(data, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kUnableToReadAsJSON);
}

TEST_F(QuickStartDecoderTest, DecodeGetAssertionResponse_NullData) {
  base::test::TestFuture<
      ::ash::quick_start::mojom::FidoAssertionResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeGetAssertionResponse(std::nullopt, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(), mojom::QuickStartDecoderError::kEmptyMessage);
}

TEST_F(QuickStartDecoderTest,
       DecodeGetAssertionResponse_UnexpectedMessageType) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kAwaitingUserVerificationKey, true);

  base::test::TestFuture<
      ::ash::quick_start::mojom::FidoAssertionResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeGetAssertionResponse(ConvertMessageToBytes(&message),
                             future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kUnexpectedMessageType);
}

TEST_F(QuickStartDecoderTest, DecodeGetAssertionResponse_EmptyResponse) {
  std::vector<uint8_t> data{};
  std::vector<uint8_t> message = BuildSecondDeviceAuthPayload(data);
  base::test::TestFuture<
      ::ash::quick_start::mojom::FidoAssertionResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeGetAssertionResponse(message, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest, DecodeGetAssertionResponse_OnlyStatusCode) {
  std::vector<uint8_t> data{0x00};
  std::vector<uint8_t> message = BuildSecondDeviceAuthPayload(data);
  base::test::TestFuture<
      ::ash::quick_start::mojom::FidoAssertionResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeGetAssertionResponse(message, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
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
  base::test::TestFuture<
      ::ash::quick_start::mojom::FidoAssertionResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeGetAssertionResponse(message, future.GetCallback());
  EXPECT_FALSE(future.Get<1>().has_value());
  EXPECT_EQ(future.Get<0>()->credential_id, expected_credential_id);
  EXPECT_EQ(future.Get<0>()->email, email);
  EXPECT_EQ(future.Get<0>()->auth_data, kValidAuthData);
  EXPECT_EQ(future.Get<0>()->signature, kValidSignature);
}

TEST_F(QuickStartDecoderTest, DecodeGetAssertionResponse_InvalidEmptyValues) {
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
  base::test::TestFuture<
      ::ash::quick_start::mojom::FidoAssertionResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeGetAssertionResponse(message, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest, DecodeBootstrapConfigurations_NullPayload) {
  base::test::TestFuture<
      ::ash::quick_start::mojom::BootstrapConfigurationsPtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeBootstrapConfigurations(std::nullopt, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(), mojom::QuickStartDecoderError::kEmptyMessage);
}

TEST_F(QuickStartDecoderTest,
       DecodeBootstrapConfigurations_UnexpectedMessageType) {
  // Build a valid SecondDeviceAuthPayload
  std::string expected_credential_id(kValidCredentialId.begin(),
                                     kValidCredentialId.end());
  std::string email = "testcase@google.com";
  std::vector<uint8_t> user_id(email.begin(), email.end());
  std::vector<uint8_t> data = BuildEncodedResponseData(
      kValidCredentialId, kValidAuthData, kValidSignature, user_id, kSuccess);

  std::vector<uint8_t> payload = BuildSecondDeviceAuthPayload(data);

  base::test::TestFuture<
      ::ash::quick_start::mojom::BootstrapConfigurationsPtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  // Try to parse the SecondDeviceAuthPayload as a BootstrapConfigurations.
  DecodeBootstrapConfigurations(std::move(payload), future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kUnexpectedMessageType);
}

TEST_F(QuickStartDecoderTest,
       DecodeBootstrapConfigurations_EmptyBootstrapConfigurations) {
  QuickStartMessage message(QuickStartMessageType::kBootstrapConfigurations);
  message.GetPayload()->Set(kBootstrapConfigurationsKey, base::Value::Dict());

  base::test::TestFuture<
      ::ash::quick_start::mojom::BootstrapConfigurationsPtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeBootstrapConfigurations(ConvertMessageToBytes(&message),
                                future.GetCallback());
  EXPECT_FALSE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<0>()->instance_id, "");
  EXPECT_EQ(future.Get<1>(), std::nullopt);
}

TEST_F(QuickStartDecoderTest,
       DecodeBootstrapConfigurations_EmptyDeviceDetails) {
  base::Value::Dict bootstrap_configurations;
  bootstrap_configurations.Set(kDeviceDetailsKey, base::Value::Dict());

  QuickStartMessage message(QuickStartMessageType::kBootstrapConfigurations);
  message.GetPayload()->Set(kBootstrapConfigurationsKey,
                            std::move(bootstrap_configurations));

  base::test::TestFuture<
      ::ash::quick_start::mojom::BootstrapConfigurationsPtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeBootstrapConfigurations(ConvertMessageToBytes(&message),
                                future.GetCallback());

  EXPECT_FALSE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<0>()->instance_id, "");
  EXPECT_EQ(future.Get<1>(), std::nullopt);
}

TEST_F(QuickStartDecoderTest, DecodeBootstrapConfigurations_EmptyValues) {
  base::Value::Dict device_details;
  base::Value::Dict bootstrap_configurations;
  bootstrap_configurations.Set(kDeviceDetailsKey, std::move(device_details));

  QuickStartMessage message(QuickStartMessageType::kBootstrapConfigurations);
  message.GetPayload()->Set(kBootstrapConfigurationsKey,
                            std::move(bootstrap_configurations));

  base::test::TestFuture<
      ::ash::quick_start::mojom::BootstrapConfigurationsPtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeBootstrapConfigurations(ConvertMessageToBytes(&message),
                                future.GetCallback());

  EXPECT_FALSE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<0>()->instance_id, "");
  EXPECT_EQ(future.Get<0>()->is_supervised_account, false);
  EXPECT_EQ(future.Get<0>()->email, "");
  EXPECT_EQ(future.Get<1>(), std::nullopt);
}

TEST_F(QuickStartDecoderTest,
       DecodeBootstrapConfigurations_ValidBootstrapConfigurations) {
  base::Value::Dict device_details;
  device_details.Set(kInstanceIdKey, kExampleInstanceId);
  base::Value::Dict bootstrap_configurations;
  bootstrap_configurations.Set(kDeviceDetailsKey, std::move(device_details));

  base::Value::Dict account;
  account.Set(kNameKey, kExampleEmail);
  base::Value::List accounts_list;
  accounts_list.Append(std::move(account));
  bootstrap_configurations.Set(kBootstrapAccountsKey, std::move(accounts_list));

  QuickStartMessage message(QuickStartMessageType::kBootstrapConfigurations);
  message.GetPayload()->Set(kBootstrapConfigurationsKey,
                            std::move(bootstrap_configurations));

  base::Value::Dict second_device_auth_payload;
  bool is_supervised_account = true;
  second_device_auth_payload.Set(kIsTransferUnicornKey, is_supervised_account);
  message.GetPayload()->Set(kSecondDeviceAuthPayloadKey,
                            std::move(second_device_auth_payload));

  base::test::TestFuture<
      ::ash::quick_start::mojom::BootstrapConfigurationsPtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeBootstrapConfigurations(ConvertMessageToBytes(&message),
                                future.GetCallback());
  EXPECT_FALSE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<0>()->instance_id, kExampleInstanceId);
  EXPECT_EQ(future.Get<0>()->is_supervised_account, is_supervised_account);
  EXPECT_EQ(future.Get<0>()->email, kExampleEmail);
  EXPECT_EQ(future.Get<1>(), std::nullopt);
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

  base::test::TestFuture<
      ::ash::quick_start::mojom::FidoAssertionResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeGetAssertionResponse(payload, future.GetCallback());
  EXPECT_TRUE(future.Get<0>());
  EXPECT_FALSE(future.Get<1>().has_value());
}

TEST_F(QuickStartDecoderTest,
       ExtractFidoDataFromJsonResponseFailsIfFidoDataMissingFromPayload) {
  QuickStartMessage message(QuickStartMessageType::kSecondDeviceAuthPayload);

  std::string json_serialized_payload;
  base::JSONWriter::Write(*message.GetPayload(), &json_serialized_payload);
  std::vector<uint8_t> response_bytes(json_serialized_payload.begin(),
                                      json_serialized_payload.end());

  base::test::TestFuture<
      ::ash::quick_start::mojom::FidoAssertionResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeGetAssertionResponse(response_bytes, future.GetCallback());
  EXPECT_FALSE(future.Get<0>());
  EXPECT_TRUE(future.Get<1>().has_value());
}

TEST_F(QuickStartDecoderTest,
       ExtractFidoDataFromJsonResponseFailsIfSecondDeviceAuthPayloadMissing) {
  base::Value::Dict message_payload;

  std::string json_serialized_payload;
  base::JSONWriter::Write(message_payload, &json_serialized_payload);
  std::vector<uint8_t> response_bytes(json_serialized_payload.begin(),
                                      json_serialized_payload.end());

  base::test::TestFuture<
      ::ash::quick_start::mojom::FidoAssertionResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeGetAssertionResponse(response_bytes, future.GetCallback());
  EXPECT_FALSE(future.Get<0>());
  EXPECT_TRUE(future.Get<1>().has_value());
}

TEST_F(QuickStartDecoderTest,
       ExtractFidoDataFromJsonResponseFailsIfPayloadIsNotJsonDictionary) {
  std::string message_payload = "This is a JSON string";

  std::string json_serialized_payload;
  base::JSONWriter::Write(message_payload, &json_serialized_payload);
  std::vector<uint8_t> response_bytes(json_serialized_payload.begin(),
                                      json_serialized_payload.end());

  base::test::TestFuture<
      ::ash::quick_start::mojom::FidoAssertionResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeGetAssertionResponse(response_bytes, future.GetCallback());
  EXPECT_FALSE(future.Get<0>());
  EXPECT_TRUE(future.Get<1>().has_value());
}

TEST_F(QuickStartDecoderTest,
       ExtractFidoDataFromJsonResponseFailsIfResponseIsNotJson) {
  // This is just a random payload that is not a valid JSON.
  std::vector<uint8_t> random_payload = {0x01, 0x02, 0x03};

  base::test::TestFuture<
      ::ash::quick_start::mojom::FidoAssertionResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeGetAssertionResponse(random_payload, future.GetCallback());
  EXPECT_FALSE(future.Get<0>());
  EXPECT_TRUE(future.Get<1>().has_value());
}

TEST_F(QuickStartDecoderTest, DecodeWifiCredentialsResponse_NullData) {
  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeWifiCredentialsResponse(std::nullopt, future.GetCallback());

  ASSERT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            ash::quick_start::mojom::QuickStartDecoderError::kEmptyMessage);
}

TEST_F(QuickStartDecoderTest, DecodeWifiCredentialsResponse_BadJson) {
  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeWifiCredentialsResponse(std::vector<uint8_t>{0x01, 0x02, 0x03},
                                future.GetCallback());

  ASSERT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(
      future.Get<1>(),
      ash::quick_start::mojom::QuickStartDecoderError::kUnableToReadAsJSON);
}

TEST_F(QuickStartDecoderTest,
       DecodeWifiCredentialsResponse_UnexpectedMessageType) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kAwaitingUserVerificationKey, true);

  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeWifiCredentialsResponse(ConvertMessageToBytes(&message),
                                future.GetCallback());

  ASSERT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kUnexpectedMessageType);
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
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeWifiCredentialsResponse(ConvertMessageToBytes(&message),
                                future.GetCallback());

  ASSERT_FALSE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<0>()->ssid, "ssid");
  EXPECT_EQ(future.Get<0>()->password, "password");
  EXPECT_EQ(future.Get<0>()->security_type, mojom::WifiSecurityType::kPSK);
  EXPECT_TRUE(future.Get<0>()->is_hidden);
  EXPECT_EQ(future.Get<1>(), std::nullopt);
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
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeWifiCredentialsResponse(ConvertMessageToBytes(&message),
                                future.GetCallback());

  ASSERT_FALSE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<0>()->password, std::nullopt);
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
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeWifiCredentialsResponse(ConvertMessageToBytes(&message),
                                future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  histogram_tester_.ExpectBucketCount(
      kWifiTransferResultFailureReasonHistogramName,
      QuickStartMetrics::WifiTransferResultFailureReason::
          kPasswordFoundAndOpenNetwork,
      1);
  histogram_tester_.ExpectBucketCount(kWifiTransferResultHistogramName, false,
                                      1);
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
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeWifiCredentialsResponse(ConvertMessageToBytes(&message),
                                future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  histogram_tester_.ExpectBucketCount(
      kWifiTransferResultFailureReasonHistogramName,
      QuickStartMetrics::WifiTransferResultFailureReason::
          kPasswordNotFoundAndNotOpenNetwork,
      1);
  histogram_tester_.ExpectBucketCount(kWifiTransferResultHistogramName, false,
                                      1);
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
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeWifiCredentialsResponse(ConvertMessageToBytes(&message),
                                future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  histogram_tester_.ExpectBucketCount(
      kWifiTransferResultFailureReasonHistogramName,
      QuickStartMetrics::WifiTransferResultFailureReason::
          kPasswordNotFoundAndNotOpenNetwork,
      1);
  histogram_tester_.ExpectBucketCount(kWifiTransferResultHistogramName, false,
                                      1);
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
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeWifiCredentialsResponse(ConvertMessageToBytes(&message),
                                future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  histogram_tester_.ExpectBucketCount(
      kWifiTransferResultFailureReasonHistogramName,
      QuickStartMetrics::WifiTransferResultFailureReason::
          kPasswordNotFoundAndNotOpenNetwork,
      1);
  histogram_tester_.ExpectBucketCount(kWifiTransferResultHistogramName, false,
                                      1);
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
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeWifiCredentialsResponse(ConvertMessageToBytes(&message),
                                future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  histogram_tester_.ExpectBucketCount(
      kWifiTransferResultFailureReasonHistogramName,
      QuickStartMetrics::WifiTransferResultFailureReason::
          kPasswordNotFoundAndNotOpenNetwork,
      1);
  histogram_tester_.ExpectBucketCount(kWifiTransferResultHistogramName, false,
                                      1);
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
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeWifiCredentialsResponse(ConvertMessageToBytes(&message),
                                future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  histogram_tester_.ExpectBucketCount(
      kWifiTransferResultFailureReasonHistogramName,
      QuickStartMetrics::WifiTransferResultFailureReason::
          kPasswordNotFoundAndNotOpenNetwork,
      1);
  histogram_tester_.ExpectBucketCount(kWifiTransferResultHistogramName, false,
                                      1);
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
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeWifiCredentialsResponse(ConvertMessageToBytes(&message),
                                future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  histogram_tester_.ExpectBucketCount(
      kWifiTransferResultFailureReasonHistogramName,
      QuickStartMetrics::WifiTransferResultFailureReason::kEmptySsid, 1);
  histogram_tester_.ExpectBucketCount(kWifiTransferResultHistogramName, false,
                                      1);
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
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeWifiCredentialsResponse(ConvertMessageToBytes(&message),
                                future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  histogram_tester_.ExpectBucketCount(
      kWifiTransferResultFailureReasonHistogramName,
      QuickStartMetrics::WifiTransferResultFailureReason::kSsidNotFound, 1);
  histogram_tester_.ExpectBucketCount(kWifiTransferResultHistogramName, false,
                                      1);
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
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeWifiCredentialsResponse(ConvertMessageToBytes(&message),
                                future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  histogram_tester_.ExpectBucketCount(
      kWifiTransferResultFailureReasonHistogramName,
      QuickStartMetrics::WifiTransferResultFailureReason::kSecurityTypeNotFound,
      1);
  histogram_tester_.ExpectBucketCount(kWifiTransferResultHistogramName, false,
                                      1);
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
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeWifiCredentialsResponse(ConvertMessageToBytes(&message),
                                future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  histogram_tester_.ExpectBucketCount(
      kWifiTransferResultFailureReasonHistogramName,
      QuickStartMetrics::WifiTransferResultFailureReason::kInvalidSecurityType,
      1);
  histogram_tester_.ExpectBucketCount(kWifiTransferResultHistogramName, false,
                                      1);
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
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeWifiCredentialsResponse(ConvertMessageToBytes(&message),
                                future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  histogram_tester_.ExpectBucketCount(
      kWifiTransferResultFailureReasonHistogramName,
      QuickStartMetrics::WifiTransferResultFailureReason::
          kWifiHideStatusNotFound,
      1);
  histogram_tester_.ExpectBucketCount(kWifiTransferResultHistogramName, false,
                                      1);
}

TEST_F(QuickStartDecoderTest,
       ExtractWifiInformationFailsWhenMissingWifiInformation) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);

  base::test::TestFuture<
      ::ash::quick_start::mojom::WifiCredentialsPtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeWifiCredentialsResponse(ConvertMessageToBytes(&message),
                                future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(), mojom::QuickStartDecoderError::kUnknownPayload);

  // TODO (b/286877412): Add testing for gaia transfer result metrics once
  // finalized
}

TEST_F(QuickStartDecoderTest, DecodeNotifySourceOfUpdateResponseSuccess) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kNotifySourceOfUpdateAckKey, true);

  base::test::TestFuture<
      ::ash::quick_start::mojom::NotifySourceOfUpdateResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future1;

  DecodeNotifySourceOfUpdateResponse(&message, future1.GetCallback());
  EXPECT_TRUE(future1.Get<0>());
  EXPECT_TRUE(future1.Get<0>()->ack_received);

  message.GetPayload()->Set(kNotifySourceOfUpdateAckKey, false);

  base::test::TestFuture<
      ::ash::quick_start::mojom::NotifySourceOfUpdateResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future2;

  DecodeNotifySourceOfUpdateResponse(&message, future2.GetCallback());
  EXPECT_TRUE(future2.Get<0>());
  EXPECT_FALSE(future2.Get<0>()->ack_received);
}

TEST_F(QuickStartDecoderTest,
       DecodeNotifySourceOfUpdateResponseFailsWhenMissingValue) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);

  base::test::TestFuture<
      ::ash::quick_start::mojom::NotifySourceOfUpdateResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeNotifySourceOfUpdateResponse(&message, future.GetCallback());
  EXPECT_FALSE(future.Get<0>());
}

TEST_F(QuickStartDecoderTest, DecodeUserVerificationMethodSucceeds) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kUserVerificationMethodKey, 0);

  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationMethodPtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeUserVerificationMethod(ConvertMessageToBytes(&message),
                               future.GetCallback());

  ASSERT_FALSE(future.Get<0>().is_null());
  EXPECT_TRUE(future.Get<0>().get()->use_source_lock_screen_prompt);
  EXPECT_EQ(future.Get<1>(), std::nullopt);
}

TEST_F(QuickStartDecoderTest, DecodeUserVerificationMethod_NullData) {
  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationMethodPtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeUserVerificationMethod(std::nullopt, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(), mojom::QuickStartDecoderError::kEmptyMessage);
}

TEST_F(QuickStartDecoderTest,
       DecodeUserVerificationMethodFailsIfMessageIsNotJson) {
  std::vector<uint8_t> message;
  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationMethodPtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeUserVerificationMethod(message, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kUnableToReadAsJSON);
}

TEST_F(QuickStartDecoderTest, DecodeUserVerificationResultSucceeds) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kUserVerificationResultKey,
                            kUserVerifiedStatusCode);
  message.GetPayload()->Set(kIsFirstUserVerificationKey, true);

  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeUserVerificationResult(ConvertMessageToBytes(&message),
                               future.GetCallback());

  ASSERT_FALSE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<0>().get()->result,
            mojom::UserVerificationResult::kUserVerified);
  EXPECT_TRUE(future.Get<0>().get()->is_first_user_verification);
  EXPECT_EQ(future.Get<1>(), std::nullopt);
}

TEST_F(QuickStartDecoderTest, DecodeUserVerificationResult_NullData) {
  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeUserVerificationResult(std::nullopt, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(), mojom::QuickStartDecoderError::kEmptyMessage);
}

TEST_F(QuickStartDecoderTest,
       DecodeUserVerificationResult_UnexpectedMessageType) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kNotifySourceOfUpdateAckKey, true);

  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeUserVerificationResult(ConvertMessageToBytes(&message),
                               future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kUnexpectedMessageType);
}

TEST_F(QuickStartDecoderTest,
       DecodeUserVerificationResultFailsIfMessageIsNotJson) {
  std::vector<uint8_t> message;
  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeUserVerificationResult(message, future.GetCallback());

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
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeUserVerificationResult(ConvertMessageToBytes(&message),
                               future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(), mojom::QuickStartDecoderError::kUnknownPayload);
}

TEST_F(QuickStartDecoderTest,
       DecodeUserVerificationResultFailsIfMissingIsFirstUserVerification) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kUserVerificationResultKey,
                            kUserVerifiedStatusCode);

  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationResponsePtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeUserVerificationResult(ConvertMessageToBytes(&message),
                               future.GetCallback());

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
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeUserVerificationResult(ConvertMessageToBytes(&message),
                               future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
}

TEST_F(QuickStartDecoderTest, DecodeUserVerificationRequestSucceeds) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kAwaitingUserVerificationKey, true);

  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationRequestedPtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeUserVerificationRequested(ConvertMessageToBytes(&message),
                                  future.GetCallback());

  ASSERT_FALSE(future.Get<0>().is_null());
  EXPECT_TRUE(future.Get<0>().get()->is_awaiting_user_verification);
  EXPECT_EQ(future.Get<1>(), std::nullopt);
}

TEST_F(QuickStartDecoderTest, DecodeUserVerificationRequested_NullData) {
  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationRequestedPtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeUserVerificationRequested(std::nullopt, future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(), mojom::QuickStartDecoderError::kEmptyMessage);
}

TEST_F(QuickStartDecoderTest,
       DecodeUserVerificationRequested_UnexpectedMessageType) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set(kNotifySourceOfUpdateAckKey, true);

  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationRequestedPtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeUserVerificationRequested(ConvertMessageToBytes(&message),
                                  future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(),
            mojom::QuickStartDecoderError::kUnexpectedMessageType);
}

TEST_F(QuickStartDecoderTest, DecodeUserVerificationRequestFailsIfKeyMissing) {
  QuickStartMessage message(QuickStartMessageType::kQuickStartPayload);

  base::test::TestFuture<
      ::ash::quick_start::mojom::UserVerificationRequestedPtr,
      std::optional<::ash::quick_start::mojom::QuickStartDecoderError>>
      future;

  DecodeUserVerificationRequested(ConvertMessageToBytes(&message),
                                  future.GetCallback());

  EXPECT_TRUE(future.Get<0>().is_null());
  EXPECT_EQ(future.Get<1>(), mojom::QuickStartDecoderError::kUnknownPayload);
}

}  // namespace ash::quick_start
