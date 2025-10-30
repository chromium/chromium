// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/secure_channel_impl.h"

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/legion/attestation_handler.h"
#include "components/legion/legion_common.h"
#include "components/legion/secure_session.h"
#include "components/legion/transport.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"
#include "third_party/oak/chromium/proto/session/session.test.h"

namespace legion {

namespace {

using ::testing::_;
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

class MockSecureSession : public SecureSession {
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
    auto secure_session = std::make_unique<StrictMock<MockSecureSession>>();
    secure_session_ = secure_session.get();
    auto attestation_handler =
        std::make_unique<StrictMock<MockAttestationHandler>>();
    attestation_handler_ = attestation_handler.get();

    secure_channel_ = std::make_unique<SecureChannelImpl>(
        std::move(transport), std::move(secure_session),
        std::move(attestation_handler));
  }

  void TearDown() override {
    testing::Mock::VerifyAndClearExpectations(transport_);
    testing::Mock::VerifyAndClearExpectations(secure_session_);
    testing::Mock::VerifyAndClearExpectations(attestation_handler_);
  }

  void SetUpHandshakeAndAttestation();

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<SecureChannelImpl> secure_channel_;

  raw_ptr<MockTransport> transport_;
  raw_ptr<MockSecureSession> secure_session_;
  raw_ptr<MockAttestationHandler> attestation_handler_;
};

void SecureChannelImplTest::SetUpHandshakeAndAttestation() {
  oak::session::v1::SessionRequest expected_attestation_request;
  expected_attestation_request.mutable_attest_request();
  oak::session::v1::SessionRequest expected_handshake_request;
  expected_handshake_request.mutable_handshake_request();

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(expected_attestation_request.attest_request()));
  EXPECT_CALL(*transport_,
              Send(EqualsSessionRequest(expected_attestation_request), _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        oak::session::v1::SessionResponse response;
        response.mutable_attest_response();
        std::move(callback).Run(response);
      });
  EXPECT_CALL(*attestation_handler_, VerifyAttestationResponse(_))
      .WillOnce(Return(true));

  EXPECT_CALL(*secure_session_, GetHandshakeMessage())
      .WillOnce(Return(expected_handshake_request.handshake_request()));
  EXPECT_CALL(*transport_,
              Send(EqualsSessionRequest(expected_handshake_request), _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        oak::session::v1::SessionResponse response;
        response.mutable_handshake_response();
        std::move(callback).Run(response);
      });

  EXPECT_CALL(*secure_session_, ProcessHandshakeResponse(_))
      .WillOnce(Return(true));
}

// Tests the successful establishment of a secure session and sending a single
// request.
TEST_F(SecureChannelImplTest, WriteAndEstablishSessionSucceeds) {
  Request request_data = {1, 2, 3};
  oak::session::v1::EncryptedMessage encrypted_request;
  encrypted_request.set_ciphertext("encrypted_request");
  Request decrypted_response = {6};
  oak::session::v1::EncryptedMessage encrypted_response;
  encrypted_response.set_ciphertext("encrypted_response");

  SetUpHandshakeAndAttestation();

  EXPECT_CALL(*secure_session_, Encrypt(request_data))
      .WillOnce(Return(encrypted_request));

  oak::session::v1::SessionRequest expected_session_request;
  *expected_session_request.mutable_encrypted_message() = encrypted_request;

  EXPECT_CALL(*transport_, Send(EqualsSessionRequest(expected_session_request), _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        oak::session::v1::SessionResponse response;
        *response.mutable_encrypted_message() = encrypted_response;
        std::move(callback).Run(response);
      });
  EXPECT_CALL(*secure_session_, Decrypt(testing::Property(
                                   &oak::session::v1::EncryptedMessage::ciphertext,
                                   "encrypted_response")))
      .WillOnce(Return(decrypted_response));

  base::test::TestFuture<ResultCode, std::optional<Response>> future;
  secure_channel_->Write(request_data, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), ResultCode::kSuccess);
  ASSERT_TRUE(future.Get<1>().has_value());
  EXPECT_EQ(future.Get<1>().value(), decrypted_response);
}

// Tests that multiple requests are queued and processed sequentially after the
// session is established.
TEST_F(SecureChannelImplTest, WritesQueuedDuringSessionEstablishment) {
  Request request_data1 = {1};
  Request request_data2 = {2};
  oak::session::v1::EncryptedMessage encrypted_request1;
  encrypted_request1.set_ciphertext("encrypted_request1");
  oak::session::v1::EncryptedMessage encrypted_request2;
  encrypted_request2.set_ciphertext("encrypted_request2");
  Request decrypted_response1 = {11};
  Request decrypted_response2 = {12};
  oak::session::v1::EncryptedMessage encrypted_response1;
  encrypted_response1.set_ciphertext("encrypted_response1");
  oak::session::v1::EncryptedMessage encrypted_response2;
  encrypted_response2.set_ciphertext("encrypted_response2");

  SetUpHandshakeAndAttestation();

  // First Request
  EXPECT_CALL(*secure_session_, Encrypt(request_data1))
      .WillOnce(Return(encrypted_request1));

  oak::session::v1::SessionRequest expected_session_request1;
  *expected_session_request1.mutable_encrypted_message() = encrypted_request1;

  EXPECT_CALL(*transport_, Send(EqualsSessionRequest(expected_session_request1), _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        oak::session::v1::SessionResponse response;
        *response.mutable_encrypted_message() = encrypted_response1;
        std::move(callback).Run(response);
      });
  EXPECT_CALL(*secure_session_, Decrypt(testing::Property(
                                   &oak::session::v1::EncryptedMessage::ciphertext,
                                   "encrypted_response1")))
      .WillOnce(Return(decrypted_response1));

  // Second Request
  EXPECT_CALL(*secure_session_, Encrypt(request_data2))
      .WillOnce(Return(encrypted_request2));

  oak::session::v1::SessionRequest expected_session_request2;
  *expected_session_request2.mutable_encrypted_message() = encrypted_request2;

  EXPECT_CALL(*transport_, Send(EqualsSessionRequest(expected_session_request2), _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        oak::session::v1::SessionResponse response;
        *response.mutable_encrypted_message() = encrypted_response2;
        std::move(callback).Run(response);
      });
  EXPECT_CALL(*secure_session_, Decrypt(testing::Property(
                                   &oak::session::v1::EncryptedMessage::ciphertext,
                                   "encrypted_response2")))
      .WillOnce(Return(decrypted_response2));

  base::test::TestFuture<ResultCode, std::optional<Response>> future1;
  secure_channel_->Write(request_data1, future1.GetCallback());
  base::test::TestFuture<ResultCode, std::optional<Response>> future2;
  secure_channel_->Write(request_data2, future2.GetCallback());

  EXPECT_EQ(future1.Get<0>(), ResultCode::kSuccess);
  ASSERT_TRUE(future1.Get<1>().has_value());
  EXPECT_EQ(future1.Get<1>().value(), decrypted_response1);

  EXPECT_EQ(future2.Get<0>(), ResultCode::kSuccess);
  ASSERT_TRUE(future2.Get<1>().has_value());
  EXPECT_EQ(future2.Get<1>().value(), decrypted_response2);
}

// Tests the case where attestation verification fails, leading to a session
// failure.
TEST_F(SecureChannelImplTest, AttestationErrorFailsWrite) {
  Request request_data = {1};
  oak::session::v1::SessionRequest expected_attestation_request;
  expected_attestation_request.mutable_attest_request();
  oak::session::v1::SessionResponse attestation_session_response;
  attestation_session_response.mutable_attest_response();

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(expected_attestation_request.attest_request()));
  EXPECT_CALL(*transport_,
              Send(EqualsSessionRequest(expected_attestation_request), _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(attestation_session_response);
      });
  EXPECT_CALL(*attestation_handler_, VerifyAttestationResponse(_))
      .WillOnce(Return(false));

  base::test::TestFuture<ResultCode, std::optional<Response>> future;
  secure_channel_->Write(request_data, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), ResultCode::kAttestationFailed);
  EXPECT_FALSE(future.Get<1>().has_value());
}

// Tests a transport-level error during the attestation phase of session
// establishment.
TEST_F(SecureChannelImplTest, TransportErrorDuringAttestationFailsRequest) {
  Request request_data = {1};
  oak::session::v1::SessionRequest expected_attestation_request;
  expected_attestation_request.mutable_attest_request();

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(expected_attestation_request.attest_request()));
  EXPECT_CALL(*transport_,
              Send(EqualsSessionRequest(expected_attestation_request), _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(
            base::unexpected(Transport::TransportError::kError));
      });

  base::test::TestFuture<ResultCode, std::optional<Response>> future;
  secure_channel_->Write(request_data, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), ResultCode::kAttestationFailed);
  EXPECT_FALSE(future.Get<1>().has_value());
}

// Tests a transport-level error during the handshake phase of session
// establishment.
TEST_F(SecureChannelImplTest, TransportErrorDuringHandshakeFailsRequest) {
  // Setup successful attestation.
  {
    oak::session::v1::SessionRequest expected_attestation_request;
    expected_attestation_request.mutable_attest_request();

    EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
        .WillOnce(Return(expected_attestation_request.attest_request()));
    EXPECT_CALL(*transport_,
                Send(EqualsSessionRequest(expected_attestation_request), _))
        .WillOnce([&](const oak::session::v1::SessionRequest&,
                      Transport::ResponseCallback callback) {
          oak::session::v1::SessionResponse response;
          response.mutable_attest_response();
          std::move(callback).Run(response);
        });
    EXPECT_CALL(*attestation_handler_, VerifyAttestationResponse(_))
        .WillOnce(Return(true));
  }

  // Setup transport failure during handshake.
  {
    oak::session::v1::SessionRequest expected_handshake_request;
    expected_handshake_request.mutable_handshake_request();
    EXPECT_CALL(*secure_session_, GetHandshakeMessage())
        .WillOnce(Return(expected_handshake_request.handshake_request()));
    EXPECT_CALL(*transport_,
                Send(EqualsSessionRequest(expected_handshake_request), _))
        .WillOnce([&](const oak::session::v1::SessionRequest&,
                      Transport::ResponseCallback callback) {
          std::move(callback).Run(
              base::unexpected(Transport::TransportError::kError));
        });
  }

  Request request_data = {1};
  base::test::TestFuture<ResultCode, std::optional<Response>> future;
  secure_channel_->Write(request_data, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), ResultCode::kHandshakeFailed);
  EXPECT_FALSE(future.Get<1>().has_value());
}

// Tests a transport-level error after the session is established.
TEST_F(SecureChannelImplTest, TransportErrorAfterSessionEstablished) {
  Request request_data = {1};
  oak::session::v1::EncryptedMessage encrypted_request;
  encrypted_request.set_ciphertext("encrypted_request");

  SetUpHandshakeAndAttestation();

  EXPECT_CALL(*secure_session_, Encrypt(request_data))
      .WillOnce(Return(encrypted_request));

  oak::session::v1::SessionRequest expected_session_request;
  *expected_session_request.mutable_encrypted_message() = encrypted_request;

  EXPECT_CALL(*transport_, Send(EqualsSessionRequest(expected_session_request), _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(
            base::unexpected(Transport::TransportError::kError));
      });

  base::test::TestFuture<ResultCode, std::optional<Response>> future;
  secure_channel_->Write(request_data, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), ResultCode::kNetworkError);
  EXPECT_FALSE(future.Get<1>().has_value());
}

// Tests a failure in generating the initial attestation request.
TEST_F(SecureChannelImplTest, GetAttestationRequestFails) {
  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(std::nullopt));

  base::test::TestFuture<ResultCode, std::optional<Response>> future;
  secure_channel_->Write({1}, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), ResultCode::kAttestationFailed);
  EXPECT_FALSE(future.Get<1>().has_value());
}

// Tests a failure in generating the handshake message.
TEST_F(SecureChannelImplTest, GetHandshakeMessageFails) {
  oak::session::v1::SessionRequest expected_attestation_request;
  expected_attestation_request.mutable_attest_request();
  oak::session::v1::SessionResponse attestation_session_response;
  attestation_session_response.mutable_attest_response();

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(expected_attestation_request.attest_request()));
  EXPECT_CALL(*transport_,
              Send(EqualsSessionRequest(expected_attestation_request), _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(attestation_session_response);
      });
  EXPECT_CALL(*attestation_handler_, VerifyAttestationResponse(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*secure_session_, GetHandshakeMessage())
      .WillOnce(Return(std::nullopt));

  base::test::TestFuture<ResultCode, std::optional<Response>> future;
  secure_channel_->Write({1}, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), ResultCode::kHandshakeFailed);
  EXPECT_FALSE(future.Get<1>().has_value());
}

// Tests a failure in processing the handshake response.
TEST_F(SecureChannelImplTest, ProcessHandshakeResponseFails) {
  oak::session::v1::SessionRequest expected_attestation_request;
  expected_attestation_request.mutable_attest_request();
  oak::session::v1::SessionResponse attestation_session_response;
  attestation_session_response.mutable_attest_response();
  oak::session::v1::SessionRequest expected_handshake_request;
  expected_handshake_request.mutable_handshake_request();
  oak::session::v1::SessionResponse handshake_session_response;
  handshake_session_response.mutable_handshake_response();

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(expected_attestation_request.attest_request()));
  EXPECT_CALL(*transport_,
              Send(EqualsSessionRequest(expected_attestation_request), _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(attestation_session_response);
      });
  EXPECT_CALL(*attestation_handler_, VerifyAttestationResponse(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*secure_session_, GetHandshakeMessage())
      .WillOnce(Return(expected_handshake_request.handshake_request()));
  EXPECT_CALL(*transport_,
              Send(EqualsSessionRequest(expected_handshake_request), _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(handshake_session_response);
      });
  EXPECT_CALL(*secure_session_, ProcessHandshakeResponse(_))
      .WillOnce(Return(false));

  base::test::TestFuture<ResultCode, std::optional<Response>> future;
  secure_channel_->Write({1}, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), ResultCode::kHandshakeFailed);
  EXPECT_FALSE(future.Get<1>().has_value());
}

// Tests a failure to encrypt a request after the session is established.
TEST_F(SecureChannelImplTest, EncryptRequestFails) {
  Request request_data = {1};

  SetUpHandshakeAndAttestation();
  EXPECT_CALL(*secure_session_, Encrypt(request_data))
      .WillOnce(Return(std::nullopt));

  base::test::TestFuture<ResultCode, std::optional<Response>> future;
  secure_channel_->Write(request_data, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), ResultCode::kEncryptionFailed);
  EXPECT_FALSE(future.Get<1>().has_value());
}

// Tests a failure to decrypt a response from the server.
TEST_F(SecureChannelImplTest, DecryptResponseFails) {
  Request request_data = {1};
  oak::session::v1::EncryptedMessage encrypted_request;
  encrypted_request.set_ciphertext("encrypted_request");

  SetUpHandshakeAndAttestation();
  EXPECT_CALL(*secure_session_, Encrypt(request_data))
      .WillOnce(Return(encrypted_request));

  oak::session::v1::SessionRequest expected_session_request;
  *expected_session_request.mutable_encrypted_message() = encrypted_request;

  EXPECT_CALL(*transport_, Send(EqualsSessionRequest(expected_session_request), _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        oak::session::v1::SessionResponse response;
        response.mutable_encrypted_message();
        std::move(callback).Run(response);
      });
  EXPECT_CALL(*secure_session_, Decrypt(_)).WillOnce(Return(std::nullopt));

  base::test::TestFuture<ResultCode, std::optional<Response>> future;
  secure_channel_->Write(request_data, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), ResultCode::kDecryptionFailed);
  EXPECT_FALSE(future.Get<1>().has_value());
}

// Tests receiving an empty response from the server after session
// establishment.
TEST_F(SecureChannelImplTest, EmptyResponseFailsRequest) {
  Request request_data = {1};
  oak::session::v1::EncryptedMessage encrypted_request;
  encrypted_request.set_ciphertext("encrypted_request");

  SetUpHandshakeAndAttestation();
  EXPECT_CALL(*secure_session_, Encrypt(request_data))
      .WillOnce(Return(encrypted_request));

  oak::session::v1::SessionRequest expected_session_request;
  *expected_session_request.mutable_encrypted_message() = encrypted_request;

  EXPECT_CALL(*transport_, Send(EqualsSessionRequest(expected_session_request), _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        // Return an empty response.
        std::move(callback).Run(oak::session::v1::SessionResponse());
      });

  base::test::TestFuture<ResultCode, std::optional<Response>> future;
  secure_channel_->Write(request_data, future.GetCallback());

  EXPECT_EQ(future.Get<0>(), ResultCode::kNetworkError);
  EXPECT_FALSE(future.Get<1>().has_value());
}

// Tests that new requests are failed immediately if the channel enters a
// permanent failure state.
TEST_F(SecureChannelImplTest, WriteInPermanentFailureState) {
  oak::session::v1::SessionRequest expected_attestation_request;
  expected_attestation_request.mutable_attest_request();

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(expected_attestation_request.attest_request()));
  EXPECT_CALL(*transport_,
              Send(EqualsSessionRequest(expected_attestation_request), _))
      .WillOnce([&](const oak::session::v1::SessionRequest&,
                    Transport::ResponseCallback callback) {
        std::move(callback).Run(
            base::unexpected(Transport::TransportError::kError));
      });

  // First write triggers the failure.
  base::test::TestFuture<ResultCode, std::optional<Response>> future1;
  secure_channel_->Write({1}, future1.GetCallback());

  // Second write should fail immediately.
  base::test::TestFuture<ResultCode, std::optional<Response>> future2;
  secure_channel_->Write({2}, future2.GetCallback());

  EXPECT_EQ(future1.Get<0>(), ResultCode::kAttestationFailed);
  EXPECT_FALSE(future1.Get<1>().has_value());
  EXPECT_EQ(future2.Get<0>(), ResultCode::kError);
  EXPECT_FALSE(future2.Get<1>().has_value());
}

}  // namespace

}  // namespace legion
