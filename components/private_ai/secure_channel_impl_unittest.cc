// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/secure_channel_impl.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/private_ai/attestation/handler.h"
#include "components/private_ai/attestation/server_evidence.h"
#include "components/private_ai/private_ai_common.h"
#include "components/private_ai/secure_session.h"
#include "components/private_ai/transport.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"
#include "third_party/oak/chromium/proto/session/session.test.h"

namespace private_ai {

namespace {

std::string BytesToString(const Request& bytes) {
  return std::string(bytes.begin(), bytes.end());
}

Request StringToBytes(const std::string& str) {
  return Request(str.begin(), str.end());
}

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
              SetResponseCallback,
              (ResponseCallback callback),
              (override));
  MOCK_METHOD(void,
              Send,
              (const oak::session::v1::SessionRequest& request),
              (override));
};

// Constants used to by FakeSecureSession.
constexpr char kEncryptedPrefix[] = "encrypted: ";
constexpr char kInvalidPublicKey[] = "invalid public key";
constexpr char kEncryptionMustFail[] = "encrypt: must fail";
constexpr char kDecryptionMustFail[] = "decrypt: must fail";

class FakeSecureSession : public SecureSession {
 public:
  FakeSecureSession() = default;
  ~FakeSecureSession() override = default;

  FakeSecureSession(const FakeSecureSession&) = default;
  FakeSecureSession& operator=(const FakeSecureSession&) = default;

  FakeSecureSession(FakeSecureSession&&) = default;
  FakeSecureSession& operator=(FakeSecureSession&&) = default;

  void GetHandshakeMessage(
      SecureSession::GetHandshakeMessageOnceCallback callback) override {
    if (should_fail_handshake_message_generation_) {
      auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
      task_runner->PostTask(FROM_HERE,
                            base::BindOnce(std::move(callback), std::nullopt));
      return;
    }
    oak::session::v1::HandshakeRequest handshake_request;

    auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(handshake_request)));
  }

  // Runs callback with `true` if response's ephemeral public key is NOT equal
  // to `kInvalidPublicKey`.
  void ProcessHandshakeResponse(
      const oak::session::v1::HandshakeResponse& response,
      SecureSession::ProcessHandshakeResponseOnceCallback callback) override {
    bool result = ProcessHandshakeResponseSync(response);

    auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(std::move(callback), result));
  }

  bool ProcessHandshakeResponseSync(
      const oak::session::v1::HandshakeResponse& response) {
    if (response.has_noise_handshake_message()) {
      const auto& message = response.noise_handshake_message();
      if (message.ephemeral_public_key() == kInvalidPublicKey) {
        return false;
      }
    }
    return true;
  }

  // Runs callback with `std::nullopt` if message is equal to
  // `kEncryptionMustFail`.
  //
  // Otherwise adds "encrypted: " prefix to the message.
  void Encrypt(const Request& message,
               SecureSession::EncryptOnceCallback callback) override {
    auto result = EncryptSync(message);

    auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
  }

  std::optional<oak::session::v1::EncryptedMessage> EncryptSync(
      const Request& message) {
    std::string message_str = BytesToString(message);
    if (message_str == kEncryptionMustFail) {
      return std::nullopt;
    }

    CHECK(!message_str.starts_with(kEncryptedPrefix));
    message_str = kEncryptedPrefix + message_str;

    oak::session::v1::EncryptedMessage encrypted_message;
    encrypted_message.set_ciphertext(message_str.data(), message_str.size());
    return encrypted_message;
  }

  // Runs callback with `std::nullopt` if message's ciphertext is equal to
  // `kDecryptionMustFail`.
  //
  // Expects that message has "encrypted: " prefix and removes that prefix.
  void Decrypt(const oak::session::v1::EncryptedMessage& message,
               SecureSession::DecryptOnceCallback callback) override {
    auto result = DecryptSync(message);

    auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
  }

  std::optional<Request> DecryptSync(
      const oak::session::v1::EncryptedMessage& message) {
    Request message_bytes(message.ciphertext().begin(),
                          message.ciphertext().end());

    std::string message_str = BytesToString(message_bytes);
    if (message_str == kDecryptionMustFail) {
      return std::nullopt;
    }

    CHECK(message_str.starts_with(kEncryptedPrefix));
    message_str = message_str.substr(strlen(kEncryptedPrefix));

    return Request(message_str.begin(), message_str.end());
  }

  void set_should_fail_handshake_message_generation(bool fail) {
    should_fail_handshake_message_generation_ = fail;
  }

 private:
  bool should_fail_handshake_message_generation_ = false;
};

class MockAttestationHandler : public AttestationHandler {
 public:
  MOCK_METHOD(std::optional<oak::session::v1::AttestRequest>,
              GetAttestationRequest,
              (),
              (override));
  MOCK_METHOD(bool,
              VerifyAttestationResponse,
              (const AttestationEvidence& evidence),
              (override));
};

class SecureChannelImplTest : public ::testing::Test {
 protected:
  SecureChannelImplTest() {
    transport_ptr_ = std::make_unique<StrictMock<MockTransport>>();
    transport_ = transport_ptr_.get();
    EXPECT_CALL(*transport_, SetResponseCallback(_))
        .WillOnce(testing::SaveArg<0>(&response_callback_));
    secure_session_ptr_ = std::make_unique<FakeSecureSession>();
    secure_session_ = secure_session_ptr_.get();
    attestation_handler_ptr_ =
        std::make_unique<StrictMock<MockAttestationHandler>>();
    attestation_handler_ = attestation_handler_ptr_.get();
  }

  void TearDown() override {
    testing::Mock::VerifyAndClearExpectations(transport_);
    testing::Mock::VerifyAndClearExpectations(attestation_handler_);
  }

  void CreateSecureChannel(SecureChannel::ResponseCallback callback) {
    secure_channel_ = std::make_unique<SecureChannelImpl>(
        std::move(callback), std::move(transport_ptr_),
        std::move(secure_session_ptr_), std::move(attestation_handler_ptr_),
        &logger_);
  }

  void SetUpAttestation();

  void SetUpHandshake();

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;

  PrivateAiLogger logger_;
  std::unique_ptr<SecureChannelImpl> secure_channel_;

  std::unique_ptr<MockTransport> transport_ptr_;
  std::unique_ptr<FakeSecureSession> secure_session_ptr_;
  std::unique_ptr<MockAttestationHandler> attestation_handler_ptr_;

  raw_ptr<MockTransport> transport_;
  raw_ptr<FakeSecureSession> secure_session_;
  raw_ptr<MockAttestationHandler> attestation_handler_;
  Transport::ResponseCallback response_callback_;
};

void SecureChannelImplTest::SetUpAttestation() {
  oak::session::v1::SessionRequest expected_attestation_request;
  expected_attestation_request.mutable_attest_request();

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(expected_attestation_request.attest_request()));
  EXPECT_CALL(*transport_,
              Send(EqualsSessionRequest(expected_attestation_request)))
      .WillOnce([&]() {
        oak::session::v1::SessionResponse response;
        response.mutable_attest_response();
        response_callback_.Run(response);
      });
  EXPECT_CALL(*attestation_handler_, VerifyAttestationResponse(_))
      .WillOnce(Return(true));
}

void SecureChannelImplTest::SetUpHandshake() {
  oak::session::v1::SessionRequest expected_handshake_request;
  expected_handshake_request.mutable_handshake_request();

  EXPECT_CALL(*transport_,
              Send(EqualsSessionRequest(expected_handshake_request)))
      .WillOnce([&]() {
        oak::session::v1::SessionResponse response;
        response.mutable_handshake_response();
        response_callback_.Run(response);
      });
}

// Tests the successful establishment of a secure connection and sending a
// single request.
TEST_F(SecureChannelImplTest, WriteAndEstablishConnectionSucceeds) {
  SetUpAttestation();
  SetUpHandshake();

  oak::session::v1::SessionRequest expected_session_request;
  {
    oak::session::v1::EncryptedMessage encrypted_request;
    encrypted_request.set_ciphertext("encrypted: secret request");
    *expected_session_request.mutable_encrypted_message() = encrypted_request;
  }

  EXPECT_CALL(*transport_, Send(EqualsSessionRequest(expected_session_request)))
      .WillOnce([&]() {
        oak::session::v1::SessionResponse response;
        {
          oak::session::v1::EncryptedMessage encrypted_response;
          encrypted_response.set_ciphertext("encrypted: secret response");
          *response.mutable_encrypted_message() = encrypted_response;
        }
        response_callback_.Run(response);
      });

  base::test::TestFuture<base::expected<Response, ErrorCode>> future;
  CreateSecureChannel(future.GetRepeatingCallback());
  EXPECT_TRUE(secure_channel_->Write(StringToBytes("secret request")));

  const auto& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(BytesToString(result.value()), "secret response");

  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetAttestationRequestLatency.Success", 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendAttestationRequestLatency.Success", 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetHandshakeMessageLatency.Success", 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendHandshakeRequestLatency.Success", 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetAttestationRequestLatency.Error", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendAttestationRequestLatency.Error", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetHandshakeMessageLatency.Error", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendHandshakeRequestLatency.Error", 0);
}

// Tests that a closed channel is reported through the response callback.
TEST_F(SecureChannelImplTest, ChannelClosedIsReported) {
  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(std::nullopt));

  base::test::TestFuture<base::expected<Response, ErrorCode>> future;
  CreateSecureChannel(future.GetRepeatingCallback());
  EXPECT_FALSE(secure_channel_->Write(StringToBytes("secret request")));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kAttestationFailed);
}

// Tests the case where attestation verification fails, leading to a session
// failure.
TEST_F(SecureChannelImplTest, AttestationErrorFailsWrite) {
  oak::session::v1::SessionRequest expected_attestation_request;
  expected_attestation_request.mutable_attest_request();
  oak::session::v1::SessionResponse attestation_session_response;
  attestation_session_response.mutable_attest_response();

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(expected_attestation_request.attest_request()));
  EXPECT_CALL(*transport_,
              Send(EqualsSessionRequest(expected_attestation_request)))
      .WillOnce(
          [&]() { response_callback_.Run(attestation_session_response); });
  EXPECT_CALL(*attestation_handler_, VerifyAttestationResponse(_))
      .WillOnce(Return(false));

  base::test::TestFuture<base::expected<Response, ErrorCode>> future;
  CreateSecureChannel(future.GetRepeatingCallback());
  EXPECT_FALSE(secure_channel_->Write(StringToBytes("secret request")));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kAttestationFailed);

  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetAttestationRequestLatency.Success", 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendAttestationRequestLatency.Error", 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetHandshakeMessageLatency.Success", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendHandshakeRequestLatency.Success", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetAttestationRequestLatency.Error", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetHandshakeMessageLatency.Error", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendHandshakeRequestLatency.Error", 0);
}

// Tests the case where attestation evidence conversion fails, leading to a
// session failure.
TEST_F(SecureChannelImplTest, AttestationEvidenceConversionFails) {
  oak::session::v1::SessionRequest expected_attestation_request;
  expected_attestation_request.mutable_attest_request();
  oak::session::v1::SessionResponse attestation_session_response;
  {
    auto* attest_response =
        attestation_session_response.mutable_attest_response();
    // Add an endorsed evidence entry that is missing endorsements, which will
    // cause the conversion to fail.
    (*attest_response->mutable_endorsed_evidence())["key"];
  }

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(expected_attestation_request.attest_request()));
  EXPECT_CALL(*transport_,
              Send(EqualsSessionRequest(expected_attestation_request)))
      .WillOnce(
          [&]() { response_callback_.Run(attestation_session_response); });

  base::test::TestFuture<base::expected<Response, ErrorCode>> future;
  CreateSecureChannel(future.GetRepeatingCallback());
  EXPECT_FALSE(secure_channel_->Write(StringToBytes("secret request")));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kAttestationFailed);

  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetAttestationRequestLatency.Success", 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendAttestationRequestLatency.Error", 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetAttestationRequestLatency.Error", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendAttestationRequestLatency.Success", 0);
}

// Tests a transport-level error during the attestation phase of session
// establishment.
TEST_F(SecureChannelImplTest, TransportErrorDuringAttestationFailsRequest) {
  oak::session::v1::SessionRequest expected_attestation_request;
  expected_attestation_request.mutable_attest_request();

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(expected_attestation_request.attest_request()));
  EXPECT_CALL(*transport_,
              Send(EqualsSessionRequest(expected_attestation_request)))
      .WillOnce([&]() {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](Transport::ResponseCallback cb) {
                  cb.Run(base::unexpected(Transport::TransportError::kError));
                },
                response_callback_));
      });

  base::test::TestFuture<base::expected<Response, ErrorCode>> future;
  CreateSecureChannel(future.GetRepeatingCallback());
  EXPECT_TRUE(secure_channel_->Write(StringToBytes("secret request")));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kAttestationFailed);

  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetAttestationRequestLatency.Success", 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetHandshakeMessageLatency.Success", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendHandshakeRequestLatency.Success", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetAttestationRequestLatency.Error", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendAttestationRequestLatency.Error", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetHandshakeMessageLatency.Error", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendHandshakeRequestLatency.Error", 0);
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
                Send(EqualsSessionRequest(expected_attestation_request)))
        .WillOnce([&]() {
          oak::session::v1::SessionResponse response;
          response.mutable_attest_response();
          response_callback_.Run(response);
        });
    EXPECT_CALL(*attestation_handler_, VerifyAttestationResponse(_))
        .WillOnce(Return(true));
  }

  // Setup transport failure during handshake.
  {
    oak::session::v1::SessionRequest expected_handshake_request;
    expected_handshake_request.mutable_handshake_request();
    EXPECT_CALL(*transport_,
                Send(EqualsSessionRequest(expected_handshake_request)))
        .WillOnce([&]() {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  [](Transport::ResponseCallback cb) {
                    cb.Run(base::unexpected(Transport::TransportError::kError));
                  },
                  response_callback_));
        });
  }

  base::test::TestFuture<base::expected<Response, ErrorCode>> future;
  CreateSecureChannel(future.GetRepeatingCallback());
  EXPECT_TRUE(secure_channel_->Write(StringToBytes("secret request")));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kHandshakeFailed);

  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetAttestationRequestLatency.Success", 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendAttestationRequestLatency.Success", 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetHandshakeMessageLatency.Error", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendHandshakeRequestLatency.Error", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetAttestationRequestLatency.Error", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetHandshakeMessageLatency.Success", 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendHandshakeRequestLatency.Success", 0);
}

// Tests a transport-level error after the session is established.
TEST_F(SecureChannelImplTest, TransportErrorAfterSessionEstablished) {
  SetUpAttestation();
  SetUpHandshake();

  oak::session::v1::SessionRequest expected_session_request;
  {
    oak::session::v1::EncryptedMessage encrypted_request;
    encrypted_request.set_ciphertext("encrypted: secret request");
    *expected_session_request.mutable_encrypted_message() = encrypted_request;
  }

  EXPECT_CALL(*transport_, Send(EqualsSessionRequest(expected_session_request)))
      .WillOnce([&]() {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](Transport::ResponseCallback cb) {
                  cb.Run(base::unexpected(Transport::TransportError::kError));
                },
                response_callback_));
      });

  base::test::TestFuture<base::expected<Response, ErrorCode>> future;
  CreateSecureChannel(future.GetRepeatingCallback());
  EXPECT_TRUE(secure_channel_->Write(StringToBytes("secret request")));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kNetworkError);

  histogram_tester_.ExpectTotalCount("PrivateAi.SecureChannel.SessionDuration",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "PrivateAi.SecureChannel.RequestsPerSession", /*sample=*/1,
      /*expected_bucket_count=*/1);
}

// Tests a failure in generating the initial attestation request.
TEST_F(SecureChannelImplTest, GetAttestationRequestFails) {
  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(std::nullopt));

  base::test::TestFuture<base::expected<Response, ErrorCode>> future;
  CreateSecureChannel(future.GetRepeatingCallback());
  EXPECT_FALSE(secure_channel_->Write(StringToBytes("secret request")));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kAttestationFailed);

  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetAttestationRequestLatency.Error", 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetAttestationRequestLatency.Success", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetHandshakeMessageLatency.Success", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendHandshakeRequestLatency.Success", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetHandshakeMessageLatency.Error", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendHandshakeRequestLatency.Error", 0);
}

// Tests that a response without an attestation response during attestation
// fails.
TEST_F(SecureChannelImplTest, AttestationResponseMissingFails) {
  oak::session::v1::SessionRequest expected_attestation_request;
  expected_attestation_request.mutable_attest_request();

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(expected_attestation_request.attest_request()));
  EXPECT_CALL(*transport_,
              Send(EqualsSessionRequest(expected_attestation_request)))
      .WillOnce([&]() {
        // Return an empty response.
        response_callback_.Run(oak::session::v1::SessionResponse());
      });

  base::test::TestFuture<base::expected<Response, ErrorCode>> future;
  CreateSecureChannel(future.GetRepeatingCallback());
  EXPECT_FALSE(secure_channel_->Write(StringToBytes("secret request")));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kAttestationFailed);
}

// Tests a failure in processing the handshake response.
TEST_F(SecureChannelImplTest, ProcessHandshakeResponseFails) {
  oak::session::v1::SessionRequest expected_attestation_request;
  expected_attestation_request.mutable_attest_request();
  oak::session::v1::SessionResponse attestation_session_response;
  attestation_session_response.mutable_attest_response();
  oak::session::v1::SessionRequest expected_handshake_request;
  expected_handshake_request.mutable_handshake_request();

  // Setting `kInvalidPublicKey` as an ephemeral public key will fail handshake
  // on a client side.
  oak::session::v1::SessionResponse handshake_session_response;
  {
    oak::session::v1::HandshakeResponse handshake_response;
    auto* noise_handshake_message =
        handshake_response.mutable_noise_handshake_message();
    noise_handshake_message->set_ephemeral_public_key(kInvalidPublicKey);

    *handshake_session_response.mutable_handshake_response() =
        handshake_response;
  }

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(expected_attestation_request.attest_request()));
  EXPECT_CALL(*transport_,
              Send(EqualsSessionRequest(expected_attestation_request)))
      .WillOnce(
          [&]() { response_callback_.Run(attestation_session_response); });
  EXPECT_CALL(*attestation_handler_, VerifyAttestationResponse(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*transport_,
              Send(EqualsSessionRequest(expected_handshake_request)))
      .WillOnce([&]() { response_callback_.Run(handshake_session_response); });

  base::test::TestFuture<base::expected<Response, ErrorCode>> future;
  CreateSecureChannel(future.GetRepeatingCallback());
  EXPECT_TRUE(secure_channel_->Write(StringToBytes("secret request")));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kHandshakeFailed);

  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetAttestationRequestLatency.Success", 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendAttestationRequestLatency.Success", 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetHandshakeMessageLatency.Success", 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendHandshakeRequestLatency.Error", 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetAttestationRequestLatency.Error", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendAttestationRequestLatency.Error", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetHandshakeMessageLatency.Error", 0);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.SendHandshakeRequestLatency.Success", 0);
}

// Tests that a response without a handshake response during handshake fails.
TEST_F(SecureChannelImplTest, HandshakeResponseMissingFails) {
  SetUpAttestation();

  oak::session::v1::SessionRequest expected_handshake_request;
  expected_handshake_request.mutable_handshake_request();
  EXPECT_CALL(*transport_,
              Send(EqualsSessionRequest(expected_handshake_request)))
      .WillOnce([&]() {
        // Return an empty response.
        response_callback_.Run(oak::session::v1::SessionResponse());
      });

  base::test::TestFuture<base::expected<Response, ErrorCode>> future;
  CreateSecureChannel(future.GetRepeatingCallback());
  EXPECT_TRUE(secure_channel_->Write(StringToBytes("secret request")));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kHandshakeFailed);
}

// Tests a failure to encrypt a request after the session is established.
TEST_F(SecureChannelImplTest, EncryptRequestFails) {
  SetUpAttestation();
  SetUpHandshake();

  base::test::TestFuture<base::expected<Response, ErrorCode>> future;
  CreateSecureChannel(future.GetRepeatingCallback());
  EXPECT_TRUE(secure_channel_->Write(StringToBytes(kEncryptionMustFail)));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kEncryptionFailed);
}

// Tests a failure to decrypt a response from the server.
TEST_F(SecureChannelImplTest, DecryptResponseFails) {
  SetUpAttestation();
  SetUpHandshake();

  oak::session::v1::SessionRequest expected_session_request;
  {
    oak::session::v1::EncryptedMessage encrypted_request;
    encrypted_request.set_ciphertext("encrypted: secret request");
    *expected_session_request.mutable_encrypted_message() = encrypted_request;
  }

  EXPECT_CALL(*transport_, Send(EqualsSessionRequest(expected_session_request)))
      .WillOnce([&]() {
        oak::session::v1::SessionResponse response;
        {
          oak::session::v1::EncryptedMessage encrypted_response;
          encrypted_response.set_ciphertext(kDecryptionMustFail);
          *response.mutable_encrypted_message() = encrypted_response;
        }
        response_callback_.Run(response);
      });

  base::test::TestFuture<base::expected<Response, ErrorCode>> future;
  CreateSecureChannel(future.GetRepeatingCallback());
  EXPECT_TRUE(secure_channel_->Write(StringToBytes("secret request")));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kDecryptionFailed);
}

// Tests receiving an empty response from the server after session
// establishment.
TEST_F(SecureChannelImplTest, EmptyResponseFailsRequest) {
  SetUpAttestation();
  SetUpHandshake();

  oak::session::v1::SessionRequest expected_session_request;
  {
    oak::session::v1::EncryptedMessage encrypted_request;
    encrypted_request.set_ciphertext("encrypted: secret request");
    *expected_session_request.mutable_encrypted_message() = encrypted_request;
  }

  EXPECT_CALL(*transport_, Send(EqualsSessionRequest(expected_session_request)))
      .WillOnce([&]() {
        // Return an empty response.
        response_callback_.Run(oak::session::v1::SessionResponse());
      });

  base::test::TestFuture<base::expected<Response, ErrorCode>> future;
  CreateSecureChannel(future.GetRepeatingCallback());
  EXPECT_TRUE(secure_channel_->Write(StringToBytes("secret request")));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kDecryptionFailed);
}

// Tests that OnHandshakeMessageReady receiving std::nullopt results in
// a handshake failure.
TEST_F(SecureChannelImplTest, GetHandshakeMessageFails) {
  SetUpAttestation();

  // Configure FakeSecureSession to return std::nullopt for GetHandshakeMessage.
  secure_session_->set_should_fail_handshake_message_generation(true);

  base::test::TestFuture<base::expected<Response, ErrorCode>> future;
  CreateSecureChannel(future.GetRepeatingCallback());
  EXPECT_TRUE(secure_channel_->Write(StringToBytes("secret request")));

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ErrorCode::kHandshakeFailed);

  histogram_tester_.ExpectTotalCount(
      "PrivateAi.SecureChannel.GetHandshakeMessageLatency.Error", 1);
}

// Tests that `Write` returns false if the channel is closed.
TEST_F(SecureChannelImplTest, WriteInClosedState) {
  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(std::nullopt));

  base::test::TestFuture<base::expected<Response, ErrorCode>> future;
  CreateSecureChannel(future.GetRepeatingCallback());

  // Write fail immediately when channel is closed.
  EXPECT_FALSE(secure_channel_->Write(StringToBytes("secret request")));
  EXPECT_FALSE(future.Get().has_value());
}

}  // namespace

}  // namespace private_ai
