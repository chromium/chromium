// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/secure_channel_impl.h"

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "components/legion/attestation_handler.h"
#include "components/legion/legion_common.h"
#include "components/legion/oak_session.h"
#include "components/legion/transport.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"
#include "third_party/oak/chromium/proto/session/session.test.h"

namespace legion {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

using oak::session::v1::AttestRequest;
using oak::session::v1::AttestResponse;
using oak::session::v1::EqualsSessionRequest;
using oak::session::v1::HandshakeRequest;
using oak::session::v1::HandshakeResponse;
using oak::session::v1::SessionRequest;

class MockTransport : public Transport {
 public:
  MOCK_METHOD(void,
              Send,
              (const oak::session::v1::SessionRequest& request,
               ResponseCallback callback),
              (override));
};

class MockOakSession : public OakSession {
 public:
  MOCK_METHOD(std::optional<oak::session::v1::HandshakeRequest>,
              GetHandshakeMessage,
              (),
              (override));
  MOCK_METHOD(bool,
              ProcessHandshakeResponse,
              (const oak::session::v1::HandshakeResponse& response),
              (override));
  MOCK_METHOD(std::optional<oak::session::v1::EncryptedMessage>,
              Encrypt,
              (const Request& data),
              (override));
  MOCK_METHOD(std::optional<Request>,
              Decrypt,
              (const oak::session::v1::EncryptedMessage& data),
              (override));
};

class MockAttestationHandler : public AttestationHandler {
 public:
  MOCK_METHOD(std::optional<oak::session::v1::AttestRequest>,
              GetAttestationRequest,
              (),
              (override));
  MOCK_METHOD(bool,
              VerifyAttestationResponse,
              (const oak::session::v1::AttestResponse& evidence),
              (override));
};

class SecureChannelImplTest : public ::testing::Test {
 protected:
  SecureChannelImplTest() {
    auto transport = std::make_unique<StrictMock<MockTransport>>();
    transport_ = transport.get();
    auto oak_session = std::make_unique<StrictMock<MockOakSession>>();
    oak_session_ = oak_session.get();
    auto attestation_handler =
        std::make_unique<StrictMock<MockAttestationHandler>>();
    attestation_handler_ = attestation_handler.get();

    secure_channel_ = std::make_unique<SecureChannelImpl>(
        std::move(transport), std::move(oak_session),
        std::move(attestation_handler));
  }

  void TearDown() override {
    testing::Mock::VerifyAndClearExpectations(transport_);
    testing::Mock::VerifyAndClearExpectations(oak_session_);
    testing::Mock::VerifyAndClearExpectations(attestation_handler_);
  }

  void SetUpHandshakeAndAttestation();

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<SecureChannelImpl> secure_channel_;

  raw_ptr<MockTransport> transport_;
  raw_ptr<MockOakSession> oak_session_;
  raw_ptr<MockAttestationHandler> attestation_handler_;
};

void SecureChannelImplTest::SetUpHandshakeAndAttestation() {
  oak::session::v1::SessionRequest attestation_request;
  attestation_request.mutable_attest_request();
  oak::session::v1::SessionRequest handshake_request;
  handshake_request.mutable_handshake_request();

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(attestation_request.attest_request()));
  EXPECT_CALL(*transport_, Send(EqualsSessionRequest(attestation_request), _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        oak::session::v1::SessionResponse response;
        response.mutable_attest_response();
        std::move(callback).Run(response);
      });
  EXPECT_CALL(*attestation_handler_, VerifyAttestationResponse(_))
      .WillOnce(Return(true));

  EXPECT_CALL(*oak_session_, GetHandshakeMessage())
      .WillOnce(Return(handshake_request.handshake_request()));
  EXPECT_CALL(*transport_, Send(EqualsSessionRequest(handshake_request), _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        oak::session::v1::SessionResponse response;
        response.mutable_handshake_response();
        std::move(callback).Run(response);
      });

  EXPECT_CALL(*oak_session_, ProcessHandshakeResponse(_))
      .WillOnce(Return(true));
}

// TODO: We need to implement Deserialization for Attestation and
// EncryptedMessage and fix and re-enable the tests.

// Tests the successful establishment of a secure session and sending a single
// request.
TEST_F(SecureChannelImplTest, DISABLED_WriteAndEstablishSessionSuccess) {
  Request request_data = {1, 2, 3};
  Response response_data = {4, 5, 6};
  oak::session::v1::EncryptedMessage encrypted_request;
  Request decrypted_response = {6};

  SetUpHandshakeAndAttestation();

  EXPECT_CALL(*oak_session_, Encrypt(request_data))
      .WillOnce(Return(encrypted_request));
  EXPECT_CALL(*transport_, Send(_, _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        oak::session::v1::SessionResponse response;
        *response.mutable_encrypted_message() =
            oak::session::v1::EncryptedMessage();  // Assuming an empty
                                                   // encrypted message for now
        std::move(callback).Run(response);
      });
  EXPECT_CALL(*oak_session_, Decrypt(_)).WillOnce(Return(decrypted_response));

  secure_channel_->Write(
      request_data,
      base::BindOnce(
          [](ResultCode result_code, std::optional<Response> response) {
            EXPECT_EQ(result_code, ResultCode::kSuccess);
            EXPECT_TRUE(response.has_value());
          }));
  task_environment_.RunUntilIdle();
}

// Tests that multiple requests are queued and processed sequentially after the
// session is established.
TEST_F(SecureChannelImplTest, DISABLED_WriteQueuedDuringSessionEstablishment) {
  Request request_data1 = {1};
  Request request_data2 = {2};
  Response response_data1 = {3};
  Response response_data2 = {4};
  oak::session::v1::AttestRequest attestation_request;
  oak::session::v1::AttestResponse attestation_response;
  oak::session::v1::HandshakeRequest handshake_request;
  oak::session::v1::HandshakeResponse handshake_response;
  oak::session::v1::EncryptedMessage encrypted_request1;
  oak::session::v1::EncryptedMessage encrypted_request2;
  Request decrypted_response1 = {11};
  Request decrypted_response2 = {12};
  oak::session::v1::SessionResponse session_response1;
  oak::session::v1::SessionResponse session_response2;

  SetUpHandshakeAndAttestation();

  // First Request
  EXPECT_CALL(*oak_session_, Encrypt(request_data1))
      .WillOnce(Return(encrypted_request1));
  EXPECT_CALL(*transport_, Send(_, _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(session_response1);
      });
  EXPECT_CALL(*oak_session_, Decrypt(_)).WillOnce(Return(decrypted_response1));

  // Second Request
  EXPECT_CALL(*oak_session_, Encrypt(request_data2))
      .WillOnce(Return(encrypted_request2));
  EXPECT_CALL(*transport_, Send(_, _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(session_response2);
      });
  EXPECT_CALL(*oak_session_, Decrypt(_)).WillOnce(Return(decrypted_response2));

  secure_channel_->Write(
      request_data1,
      base::BindOnce(
          [](ResultCode result_code, std::optional<Response> response) {
            EXPECT_EQ(result_code, ResultCode::kSuccess);
          }));
  secure_channel_->Write(
      request_data2,
      base::BindOnce(
          [](ResultCode result_code, std::optional<Response> response) {
            EXPECT_EQ(result_code, ResultCode::kSuccess);
          }));

  task_environment_.RunUntilIdle();
}

// Tests the case where attestation verification fails, leading to a session
// failure.
TEST_F(SecureChannelImplTest, DISABLED_AttestationFailure) {
  Request request_data = {1};
  oak::session::v1::AttestRequest attestation_request;
  oak::session::v1::AttestResponse attestation_response;
  oak::session::v1::SessionResponse session_response;

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(attestation_request));
  EXPECT_CALL(*transport_, Send(_, _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(session_response);
      });
  EXPECT_CALL(*attestation_handler_, VerifyAttestationResponse(_))
      .WillOnce(Return(false));

  secure_channel_->Write(
      request_data,
      base::BindOnce(
          [](ResultCode result_code, std::optional<Response> response) {
            EXPECT_EQ(result_code, ResultCode::kAttestationFailed);
            EXPECT_FALSE(response.has_value());
          }));

  task_environment_.RunUntilIdle();
}

// Tests a transport-level error during the handshake phase of session
// establishment.
TEST_F(SecureChannelImplTest, DISABLED_TransportErrorDuringHandshake) {
  Request request_data = {1};
  oak::session::v1::AttestRequest attestation_request;
  oak::session::v1::AttestResponse attestation_response;
  oak::session::v1::HandshakeRequest handshake_request;
  oak::session::v1::SessionResponse session_response;

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(attestation_request));
  EXPECT_CALL(*transport_, Send(_, _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(session_response);
      });
  EXPECT_CALL(*attestation_handler_, VerifyAttestationResponse(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*oak_session_, GetHandshakeMessage())
      .WillOnce(Return(handshake_request));
  EXPECT_CALL(*transport_, Send(_, _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(
            base::unexpected(Transport::TransportError::kError));
      });

  secure_channel_->Write(
      request_data,
      base::BindOnce(
          [](ResultCode result_code, std::optional<Response> response) {
            EXPECT_EQ(result_code, ResultCode::kNetworkError);
            EXPECT_FALSE(response.has_value());
          }));

  task_environment_.RunUntilIdle();
}

// Tests a failure in generating the initial attestation request.
TEST_F(SecureChannelImplTest, DISABLED_GetAttestationRequestFails) {
  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(std::nullopt));

  secure_channel_->Write(
      {1}, base::BindOnce([](ResultCode result_code,
                             std::optional<Response> response) {
        EXPECT_EQ(result_code, ResultCode::kAttestationFailed);
        EXPECT_FALSE(response.has_value());
      }));

  task_environment_.RunUntilIdle();
}

// Tests a failure in generating the handshake message.
TEST_F(SecureChannelImplTest, DISABLED_GetHandshakeMessageFails) {
  oak::session::v1::AttestRequest attestation_request;
  oak::session::v1::AttestResponse attestation_response;
  oak::session::v1::SessionResponse session_response;

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(attestation_request));
  EXPECT_CALL(*transport_, Send(_, _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(session_response);
      });
  EXPECT_CALL(*attestation_handler_, VerifyAttestationResponse(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*oak_session_, GetHandshakeMessage())
      .WillOnce(Return(std::nullopt));

  secure_channel_->Write(
      {1}, base::BindOnce([](ResultCode result_code,
                             std::optional<Response> response) {
        EXPECT_EQ(result_code, ResultCode::kHandshakeFailed);
        EXPECT_FALSE(response.has_value());
      }));

  task_environment_.RunUntilIdle();
}

// Tests a failure in processing the handshake response.
TEST_F(SecureChannelImplTest, DISABLED_ProcessHandshakeResponseFails) {
  oak::session::v1::AttestRequest attestation_request;
  oak::session::v1::AttestResponse attestation_response;
  oak::session::v1::HandshakeRequest handshake_request;
  oak::session::v1::HandshakeResponse handshake_response;
  oak::session::v1::SessionResponse session_response;

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(attestation_request));
  EXPECT_CALL(*transport_, Send(_, _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(session_response);
      });
  EXPECT_CALL(*attestation_handler_, VerifyAttestationResponse(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*oak_session_, GetHandshakeMessage())
      .WillOnce(Return(handshake_request));
  EXPECT_CALL(*transport_, Send(_, _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(session_response);
      });
  EXPECT_CALL(*oak_session_, ProcessHandshakeResponse(_))
      .WillOnce(Return(false));

  secure_channel_->Write(
      {1}, base::BindOnce([](ResultCode result_code,
                             std::optional<Response> response) {
        EXPECT_EQ(result_code, ResultCode::kHandshakeFailed);
        EXPECT_FALSE(response.has_value());
      }));

  task_environment_.RunUntilIdle();
}

// Tests a failure to encrypt a request after the session is established.
TEST_F(SecureChannelImplTest, DISABLED_EncryptRequestFails) {
  Request request_data = {1};
  oak::session::v1::AttestRequest attestation_request;
  oak::session::v1::AttestResponse attestation_response;
  oak::session::v1::HandshakeRequest handshake_request;
  oak::session::v1::HandshakeResponse handshake_response;
  oak::session::v1::SessionResponse session_response;

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(attestation_request));
  EXPECT_CALL(*transport_, Send(_, _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(session_response);
      });
  EXPECT_CALL(*attestation_handler_, VerifyAttestationResponse(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*oak_session_, GetHandshakeMessage())
      .WillOnce(Return(handshake_request));
  EXPECT_CALL(*transport_, Send(_, _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(session_response);
      });
  EXPECT_CALL(*oak_session_, ProcessHandshakeResponse(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*oak_session_, Encrypt(request_data))
      .WillOnce(Return(std::nullopt));

  secure_channel_->Write(
      request_data,
      base::BindOnce([](ResultCode result_code,
                        std::optional<Response> response) {
        EXPECT_EQ(result_code, ResultCode::kEncryptionFailed);
        EXPECT_FALSE(response.has_value());
      }));

  task_environment_.RunUntilIdle();
}

// Tests a failure to decrypt a response from the server.
TEST_F(SecureChannelImplTest, DISABLED_DecryptResponseFails) {
  Request request_data = {1};
  Response response_data = {2};
  oak::session::v1::AttestRequest attestation_request;
  oak::session::v1::AttestResponse attestation_response;
  oak::session::v1::HandshakeRequest handshake_request;
  oak::session::v1::HandshakeResponse handshake_response;
  oak::session::v1::EncryptedMessage encrypted_request;
  oak::session::v1::SessionResponse session_response;

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(attestation_request));
  EXPECT_CALL(*transport_, Send(_, _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(session_response);
      });
  EXPECT_CALL(*attestation_handler_, VerifyAttestationResponse(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*oak_session_, GetHandshakeMessage())
      .WillOnce(Return(handshake_request));
  EXPECT_CALL(*transport_, Send(_, _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(session_response);
      });
  EXPECT_CALL(*oak_session_, ProcessHandshakeResponse(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*oak_session_, Encrypt(request_data))
      .WillOnce(Return(encrypted_request));
  EXPECT_CALL(*transport_, Send(_, _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(session_response);
      });
  EXPECT_CALL(*oak_session_, Decrypt(_)).WillOnce(Return(std::nullopt));
  EXPECT_CALL(*oak_session_, Decrypt(_)).WillOnce(Return(std::nullopt));

  secure_channel_->Write(
      request_data,
      base::BindOnce([](ResultCode result_code,
                        std::optional<Response> response) {
        EXPECT_EQ(result_code, ResultCode::kDecryptionFailed);
        EXPECT_FALSE(response.has_value());
      }));

  task_environment_.RunUntilIdle();
}

// Tests that new requests are failed immediately if the channel enters a
// permanent failure state.
TEST_F(SecureChannelImplTest, DISABLED_WriteInPermanentFailureState) {
  oak::session::v1::AttestRequest attestation_request;
  oak::session::v1::SessionResponse session_response;

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(attestation_request));
  EXPECT_CALL(*transport_, Send(_, _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(
            base::unexpected(Transport::TransportError::kError));
      });

  // First write triggers the failure.
  secure_channel_->Write(
      {1}, base::BindOnce([](ResultCode result_code,
                             std::optional<Response> response) {
        EXPECT_EQ(result_code, ResultCode::kNetworkError);
      }));

  task_environment_.RunUntilIdle();

  // Second write should fail immediately.
  secure_channel_->Write(
      {2},
      base::BindOnce([](ResultCode result_code,
                        std::optional<Response> response) {
        EXPECT_EQ(result_code, ResultCode::kError);
      }));

  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace legion
