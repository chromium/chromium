// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_enroller_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/multidevice/fake_secure_message_delegate.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate.h"
#include "chromeos/ash/services/device_sync/mock_cryptauth_client.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace ash {

namespace device_sync {

namespace {

const char kAccessTokenUsed[] = "access token used by CryptAuthClient";

const char kClientSessionPublicKey[] = "throw away after one use";
const char kServerSessionPublicKey[] = "disposables are not eco-friendly";

cryptauth::InvocationReason kInvocationReason =
    cryptauth::INVOCATION_REASON_MANUAL;
const int kGCMMetadataVersion = 1;
const char kSupportedEnrollmentTypeGcmV1[] = "gcmV1";
const char kResponseStatusOk[] = "ok";
const char kResponseStatusNotOk[] = "Your key was too bland.";
const char kEnrollmentSessionId[] = "0123456789876543210";
const char kFinishEnrollmentError[] = "A hungry router ate all your packets.";

const char kDeviceId[] = "2015 AD";
const cryptauth::DeviceType kDeviceType = cryptauth::CHROME;
const char kDeviceOsVersion[] = "41.0.0";

// Creates and returns the cryptauth::GcmDeviceInfo message to be uploaded.
cryptauth::GcmDeviceInfo GetDeviceInfo() {
  cryptauth::GcmDeviceInfo device_info;
  device_info.set_long_device_id(kDeviceId);
  device_info.set_device_type(kDeviceType);
  device_info.set_device_os_version(kDeviceOsVersion);
  return device_info;
}

// Creates and returns the cryptauth::SetupEnrollmentResponse message to be
// returned to the enroller with the session_. If |success| is false, then a bad
// response will be returned.
cryptauth::SetupEnrollmentResponse GetSetupEnrollmentResponse(bool success) {
  cryptauth::SetupEnrollmentResponse response;
  if (!success) {
    response.set_status(kResponseStatusNotOk);
    return response;
  }

  response.set_status(kResponseStatusOk);
  cryptauth::SetupEnrollmentInfo* info = response.add_infos();
  info->set_type(kSupportedEnrollmentTypeGcmV1);
  info->set_enrollment_session_id(kEnrollmentSessionId);
  info->set_server_ephemeral_key(kServerSessionPublicKey);
  return response;
}

// Creates and returns the cryptauth::FinishEnrollmentResponse message to be
// returned to the enroller with the session_. If |success| is false, then a bad
// response will be returned.
cryptauth::FinishEnrollmentResponse GetFinishEnrollmentResponse(bool success) {
  cryptauth::FinishEnrollmentResponse response;
  if (success) {
    response.set_status(kResponseStatusOk);
  } else {
    response.set_status(kResponseStatusNotOk);
    response.set_error_message(kFinishEnrollmentError);
  }
  return response;
}

// Callback that saves the key returned by SecureMessageDelegate::DeriveKey().
void SaveDerivedKey(std::string* value_out, const std::string& value) {
  *value_out = value;
}

// Callback that saves the results returned by
// SecureMessageDelegate::UnwrapSecureMessage().
void SaveUnwrapResults(bool* verified_out,
                       std::string* payload_out,
                       securemessage::Header* header_out,
                       bool verified,
                       const std::string& payload,
                       const securemessage::Header& header) {
  *verified_out = verified;
  *payload_out = payload;
  *header_out = header;
}

}  // namespace

class DeviceSyncCryptAuthEnrollerTest
    : public testing::Test,
      public MockCryptAuthClientFactory::Observer {
 public:
  DeviceSyncCryptAuthEnrollerTest()
      : client_factory_(std::make_unique<MockCryptAuthClientFactory>(
            MockCryptAuthClientFactory::MockType::MAKE_NICE_MOCKS)),
        secure_message_delegate_(new multidevice::FakeSecureMessageDelegate()),
        enroller_(client_factory_.get(),
                  base::WrapUnique(secure_message_delegate_.get())) {
    client_factory_->AddObserver(this);

    // This call is actually synchronous.
    secure_message_delegate_->GenerateKeyPair(
        base::BindOnce(&DeviceSyncCryptAuthEnrollerTest::OnKeyPairGenerated,
                       base::Unretained(this)));
  }

  DeviceSyncCryptAuthEnrollerTest(const DeviceSyncCryptAuthEnrollerTest&) =
      delete;
  DeviceSyncCryptAuthEnrollerTest& operator=(
      const DeviceSyncCryptAuthEnrollerTest&) = delete;

  // Starts the enroller.
  void StartEnroller(const cryptauth::GcmDeviceInfo& device_info) {
    secure_message_delegate_->set_next_public_key(kClientSessionPublicKey);
    enroller_result_.reset();
    enroller_.Enroll(
        user_public_key_, user_private_key_, device_info, kInvocationReason,
        base::BindOnce(&DeviceSyncCryptAuthEnrollerTest::OnEnrollerCompleted,
                       base::Unretained(this)));
  }

  // Verifies that |serialized_message| is a valid SecureMessage sent with the
  // FinishEnrollment API call.
  void ValidateEnrollmentMessage(const std::string& serialized_message) {
    // Derive the session symmetric key.
    std::string server_session_private_key =
        secure_message_delegate_->GetPrivateKeyForPublicKey(
            kServerSessionPublicKey);
    std::string symmetric_key;
    secure_message_delegate_->DeriveKey(
        server_session_private_key, kClientSessionPublicKey,
        base::BindOnce(&SaveDerivedKey, &symmetric_key));

    std::string inner_message;
    std::string inner_payload;
    {
      // Unwrap the outer message.
      bool verified;
      securemessage::Header header;
      multidevice::SecureMessageDelegate::UnwrapOptions unwrap_options;
      unwrap_options.encryption_scheme = securemessage::AES_256_CBC;
      unwrap_options.signature_scheme = securemessage::HMAC_SHA256;
      secure_message_delegate_->UnwrapSecureMessage(
          serialized_message, symmetric_key, unwrap_options,
          base::BindOnce(&SaveUnwrapResults, &verified, &inner_message,
                         &header));
      EXPECT_TRUE(verified);

      cryptauth::GcmMetadata metadata;
      ASSERT_TRUE(metadata.ParseFromString(header.public_metadata()));
      EXPECT_EQ(kGCMMetadataVersion, metadata.version());
      EXPECT_EQ(cryptauth::MessageType::ENROLLMENT, metadata.type());
    }

    {
      // Unwrap inner message.
      bool verified;
      securemessage::Header header;
      multidevice::SecureMessageDelegate::UnwrapOptions unwrap_options;
      unwrap_options.encryption_scheme = securemessage::NONE;
      unwrap_options.signature_scheme = securemessage::ECDSA_P256_SHA256;
      secure_message_delegate_->UnwrapSecureMessage(
          inner_message, user_public_key_, unwrap_options,
          base::BindOnce(&SaveUnwrapResults, &verified, &inner_payload,
                         &header));
      EXPECT_TRUE(verified);
      EXPECT_EQ(user_public_key_, header.verification_key_id());
    }

    // Check that the decrypted cryptauth::GcmDeviceInfo is correct.
    cryptauth::GcmDeviceInfo device_info;
    ASSERT_TRUE(device_info.ParseFromString(inner_payload));
    EXPECT_EQ(kDeviceId, device_info.long_device_id());
    EXPECT_EQ(kDeviceType, device_info.device_type());
    EXPECT_EQ(kDeviceOsVersion, device_info.device_os_version());
    EXPECT_EQ(user_public_key_, device_info.user_public_key());
    EXPECT_EQ(user_public_key_, device_info.key_handle());
    EXPECT_EQ(kEnrollmentSessionId, device_info.enrollment_session_id());
  }

 protected:
  // MockCryptAuthClientFactory::Observer:
  void OnCryptAuthClientCreated(MockCryptAuthClient* client) override {
    ON_CALL(*client, SetupEnrollment(_, _, _))
        .WillByDefault(
            Invoke(this, &DeviceSyncCryptAuthEnrollerTest::OnSetupEnrollment));

    ON_CALL(*client, FinishEnrollment(_, _, _))
        .WillByDefault(
            Invoke(this, &DeviceSyncCryptAuthEnrollerTest::OnFinishEnrollment));

    ON_CALL(*client, GetAccessTokenUsed())
        .WillByDefault(Return(kAccessTokenUsed));
  }

  void OnKeyPairGenerated(const std::string& public_key,
                          const std::string& private_key) {
    user_public_key_ = public_key;
    user_private_key_ = private_key;
  }

  void OnEnrollerCompleted(bool success) {
    EXPECT_FALSE(enroller_result_.get());
    enroller_result_ = std::make_unique<bool>(success);
  }

  void OnSetupEnrollment(const cryptauth::SetupEnrollmentRequest& request,
                         CryptAuthClient::SetupEnrollmentCallback callback,
                         CryptAuthClient::ErrorCallback error_callback) {
    // Check that SetupEnrollment is called before FinishEnrollment.
    EXPECT_FALSE(setup_request_.get());
    EXPECT_FALSE(finish_request_.get());
    EXPECT_TRUE(setup_callback_.is_null());
    EXPECT_TRUE(error_callback_.is_null());

    setup_request_ =
        std::make_unique<cryptauth::SetupEnrollmentRequest>(request);
    setup_callback_ = std::move(callback);
    error_callback_ = std::move(error_callback);
  }

  void OnFinishEnrollment(const cryptauth::FinishEnrollmentRequest& request,
                          CryptAuthClient::FinishEnrollmentCallback callback,
                          CryptAuthClient::ErrorCallback error_callback) {
    // Check that FinishEnrollment is called after SetupEnrollment.
    EXPECT_TRUE(setup_request_.get());
    EXPECT_FALSE(finish_request_.get());
    EXPECT_TRUE(finish_callback_.is_null());

    finish_request_ =
        std::make_unique<cryptauth::FinishEnrollmentRequest>(request);
    finish_callback_ = std::move(callback);
    error_callback_ = std::move(error_callback);
  }

  // The persistent user key-pair.
  std::string user_public_key_;
  std::string user_private_key_;

  // Owned by |enroller_|.
  std::unique_ptr<MockCryptAuthClientFactory> client_factory_;
  // Owned by |enroller_|.
  raw_ptr<multidevice::FakeSecureMessageDelegate, DanglingUntriaged>
      secure_message_delegate_;
  // The CryptAuthEnroller under test.
  CryptAuthEnrollerImpl enroller_;

  // Stores the result of running |enroller_|.
  std::unique_ptr<bool> enroller_result_;

  // Stored callbacks and requests for SetupEnrollment and FinishEnrollment.
  std::unique_ptr<cryptauth::SetupEnrollmentRequest> setup_request_;
  std::unique_ptr<cryptauth::FinishEnrollmentRequest> finish_request_;
  CryptAuthClient::SetupEnrollmentCallback setup_callback_;
  CryptAuthClient::FinishEnrollmentCallback finish_callback_;
  CryptAuthClient::ErrorCallback error_callback_;
};

TEST_F(DeviceSyncCryptAuthEnrollerTest, EnrollmentSucceeds) {
  StartEnroller(GetDeviceInfo());

  // Handle SetupEnrollment request.
  EXPECT_TRUE(setup_request_.get());
  EXPECT_EQ(kInvocationReason, setup_request_->invocation_reason());
  ASSERT_EQ(1, setup_request_->types_size());
  EXPECT_EQ(kSupportedEnrollmentTypeGcmV1, setup_request_->types(0));
  ASSERT_FALSE(setup_callback_.is_null());
  std::move(setup_callback_).Run(GetSetupEnrollmentResponse(true));

  // Handle FinishEnrollment request.
  EXPECT_TRUE(finish_request_.get());
  EXPECT_EQ(kEnrollmentSessionId, finish_request_->enrollment_session_id());
  EXPECT_EQ(kClientSessionPublicKey, finish_request_->device_ephemeral_key());
  ValidateEnrollmentMessage(finish_request_->enrollment_message());
  EXPECT_EQ(kInvocationReason, finish_request_->invocation_reason());

  ASSERT_FALSE(finish_callback_.is_null());
  std::move(finish_callback_).Run(GetFinishEnrollmentResponse(true));

  ASSERT_TRUE(enroller_result_.get());
  EXPECT_TRUE(*enroller_result_);
}

TEST_F(DeviceSyncCryptAuthEnrollerTest, SetupEnrollmentApiCallError) {
  StartEnroller(GetDeviceInfo());

  EXPECT_TRUE(setup_request_.get());
  ASSERT_FALSE(error_callback_.is_null());
  std::move(error_callback_).Run(NetworkRequestError::kBadRequest);

  EXPECT_TRUE(finish_callback_.is_null());
  ASSERT_TRUE(enroller_result_.get());
  EXPECT_FALSE(*enroller_result_);
}

TEST_F(DeviceSyncCryptAuthEnrollerTest, SetupEnrollmentBadStatus) {
  StartEnroller(GetDeviceInfo());

  EXPECT_TRUE(setup_request_.get());
  std::move(setup_callback_).Run(GetSetupEnrollmentResponse(false));

  EXPECT_TRUE(finish_callback_.is_null());
  ASSERT_TRUE(enroller_result_.get());
  EXPECT_FALSE(*enroller_result_);
}

TEST_F(DeviceSyncCryptAuthEnrollerTest, SetupEnrollmentNoInfosReturned) {
  StartEnroller(GetDeviceInfo());
  EXPECT_TRUE(setup_request_.get());
  cryptauth::SetupEnrollmentResponse response;
  response.set_status(kResponseStatusOk);
  std::move(setup_callback_).Run(response);

  EXPECT_TRUE(finish_callback_.is_null());
  ASSERT_TRUE(enroller_result_.get());
  EXPECT_FALSE(*enroller_result_);
}

TEST_F(DeviceSyncCryptAuthEnrollerTest, FinishEnrollmentApiCallError) {
  StartEnroller(GetDeviceInfo());
  std::move(setup_callback_).Run(GetSetupEnrollmentResponse(true));
  ASSERT_FALSE(error_callback_.is_null());
  std::move(error_callback_).Run(NetworkRequestError::kAuthenticationError);
  ASSERT_TRUE(enroller_result_.get());
  EXPECT_FALSE(*enroller_result_);
}

TEST_F(DeviceSyncCryptAuthEnrollerTest, FinishEnrollmentBadStatus) {
  StartEnroller(GetDeviceInfo());
  std::move(setup_callback_).Run(GetSetupEnrollmentResponse(true));
  ASSERT_FALSE(finish_callback_.is_null());
  std::move(finish_callback_).Run(GetFinishEnrollmentResponse(false));
  ASSERT_TRUE(enroller_result_.get());
  EXPECT_FALSE(*enroller_result_);
}

TEST_F(DeviceSyncCryptAuthEnrollerTest, ReuseEnroller) {
  StartEnroller(GetDeviceInfo());
  std::move(setup_callback_).Run(GetSetupEnrollmentResponse(true));
  std::move(finish_callback_).Run(GetFinishEnrollmentResponse(true));
  EXPECT_TRUE(*enroller_result_);

  StartEnroller(GetDeviceInfo());
  EXPECT_FALSE(*enroller_result_);
}

TEST_F(DeviceSyncCryptAuthEnrollerTest, IncompleteDeviceInfo) {
  StartEnroller(cryptauth::GcmDeviceInfo());
  ASSERT_TRUE(enroller_result_.get());
  EXPECT_FALSE(*enroller_result_);
}

}  // namespace device_sync

}  // namespace ash
