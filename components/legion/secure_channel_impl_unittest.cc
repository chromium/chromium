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

namespace legion {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

class MockTransport : public Transport {
 public:
  MOCK_METHOD(void,
              Send,
              (Request request, ResponseCallback callback),
              (override));
};

class MockOakSession : public OakSession {
 public:
  MOCK_METHOD(std::optional<Request>, GetHandshakeMessage, (), (override));
  MOCK_METHOD(bool,
              ProcessHandshakeResponse,
              (const Response& response),
              (override));
  MOCK_METHOD(std::optional<Response>,
              Encrypt,
              (const Request& request),
              (override));
  MOCK_METHOD(std::optional<Request>,
              Decrypt,
              (const Response& response),
              (override));
};

class MockAttestationHandler : public AttestationHandler {
 public:
  MOCK_METHOD(std::optional<Request>, GetAttestationRequest, (), (override));
  MOCK_METHOD(bool,
              VerifyAttestationResponse,
              (const Response& response),
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

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<SecureChannelImpl> secure_channel_;

  raw_ptr<MockTransport> transport_;
  raw_ptr<MockOakSession> oak_session_;
  raw_ptr<MockAttestationHandler> attestation_handler_;
};

// Tests the successful establishment of a secure session and sending a single
// request.
TEST_F(SecureChannelImplTest, WriteAndEstablishSessionSuccess) {
  Request request_data = {1, 2, 3};
  Response response_data = {4, 5, 6};
  Request attestation_request = {1};
  Response attestation_response = {2};
  Request handshake_request = {3};
  Response handshake_response = {4};
  Response encrypted_request = {5};
  Request decrypted_response = {6};

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(attestation_request));
  EXPECT_CALL(*transport_, Send(attestation_request, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
        std::move(callback).Run(attestation_response);
      });
  EXPECT_CALL(*attestation_handler_,
              VerifyAttestationResponse(attestation_response))
      .WillOnce(Return(true));
  EXPECT_CALL(*oak_session_, GetHandshakeMessage())
      .WillOnce(Return(handshake_request));
  EXPECT_CALL(*transport_, Send(handshake_request, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
        std::move(callback).Run(handshake_response);
      });
  EXPECT_CALL(*oak_session_, ProcessHandshakeResponse(handshake_response))
      .WillOnce(Return(true));
  EXPECT_CALL(*oak_session_, Encrypt(request_data))
      .WillOnce(Return(encrypted_request));
  EXPECT_CALL(*transport_, Send(encrypted_request, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
        std::move(callback).Run(response_data);
      });
  EXPECT_CALL(*oak_session_, Decrypt(response_data))
      .WillOnce(Return(decrypted_response));

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
TEST_F(SecureChannelImplTest, WriteQueuedDuringSessionEstablishment) {
  Request request_data1 = {1};
  Request request_data2 = {2};
  Response response_data1 = {3};
  Response response_data2 = {4};
  Request attestation_request = {5};
  Response attestation_response = {6};
  Request handshake_request = {7};
  Response handshake_response = {8};
  Response encrypted_request1 = {9};
  Response encrypted_request2 = {10};
  Request decrypted_response1 = {11};
  Request decrypted_response2 = {12};

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(attestation_request));
  EXPECT_CALL(*transport_, Send(attestation_request, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
        std::move(callback).Run(attestation_response);
      });
  EXPECT_CALL(*attestation_handler_,
              VerifyAttestationResponse(attestation_response))
      .WillOnce(Return(true));
  EXPECT_CALL(*oak_session_, GetHandshakeMessage())
      .WillOnce(Return(handshake_request));
  EXPECT_CALL(*transport_, Send(handshake_request, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
        std::move(callback).Run(handshake_response);
      });
  EXPECT_CALL(*oak_session_, ProcessHandshakeResponse(handshake_response))
      .WillOnce(Return(true));

  // First Request
  EXPECT_CALL(*oak_session_, Encrypt(request_data1))
      .WillOnce(Return(encrypted_request1));
  EXPECT_CALL(*transport_, Send(encrypted_request1, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
        std::move(callback).Run(response_data1);
      });
  EXPECT_CALL(*oak_session_, Decrypt(response_data1))
      .WillOnce(Return(decrypted_response1));

  // Second Request
  EXPECT_CALL(*oak_session_, Encrypt(request_data2))
      .WillOnce(Return(encrypted_request2));
  EXPECT_CALL(*transport_, Send(encrypted_request2, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
        std::move(callback).Run(response_data2);
      });
  EXPECT_CALL(*oak_session_, Decrypt(response_data2))
      .WillOnce(Return(decrypted_response2));

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
TEST_F(SecureChannelImplTest, AttestationFailure) {
  Request request_data = {1};
  Request attestation_request = {2};
  Response attestation_response = {3};

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(attestation_request));
  EXPECT_CALL(*transport_, Send(attestation_request, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
        std::move(callback).Run(attestation_response);
      });
  EXPECT_CALL(*attestation_handler_,
              VerifyAttestationResponse(attestation_response))
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
TEST_F(SecureChannelImplTest, TransportErrorDuringHandshake) {
  Request request_data = {1};
  Request attestation_request = {2};
  Response attestation_response = {3};
  Request handshake_request = {4};

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(attestation_request));
  EXPECT_CALL(*transport_, Send(attestation_request, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
        std::move(callback).Run(attestation_response);
      });
  EXPECT_CALL(*attestation_handler_,
              VerifyAttestationResponse(attestation_response))
      .WillOnce(Return(true));
  EXPECT_CALL(*oak_session_, GetHandshakeMessage())
      .WillOnce(Return(handshake_request));
  EXPECT_CALL(*transport_, Send(handshake_request, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
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
TEST_F(SecureChannelImplTest, GetAttestationRequestFails) {
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
TEST_F(SecureChannelImplTest, GetHandshakeMessageFails) {
  Request attestation_request = {1};
  Response attestation_response = {2};

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(attestation_request));
  EXPECT_CALL(*transport_, Send(attestation_request, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
        std::move(callback).Run(attestation_response);
      });
  EXPECT_CALL(*attestation_handler_,
              VerifyAttestationResponse(attestation_response))
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
TEST_F(SecureChannelImplTest, ProcessHandshakeResponseFails) {
  Request attestation_request = {1};
  Response attestation_response = {2};
  Request handshake_request = {3};
  Response handshake_response = {4};

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(attestation_request));
  EXPECT_CALL(*transport_, Send(attestation_request, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
        std::move(callback).Run(attestation_response);
      });
  EXPECT_CALL(*attestation_handler_,
              VerifyAttestationResponse(attestation_response))
      .WillOnce(Return(true));
  EXPECT_CALL(*oak_session_, GetHandshakeMessage())
      .WillOnce(Return(handshake_request));
  EXPECT_CALL(*transport_, Send(handshake_request, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
        std::move(callback).Run(handshake_response);
      });
  EXPECT_CALL(*oak_session_, ProcessHandshakeResponse(handshake_response))
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
TEST_F(SecureChannelImplTest, EncryptRequestFails) {
  Request request_data = {1};
  Request attestation_request = {2};
  Response attestation_response = {3};
  Request handshake_request = {4};
  Response handshake_response = {5};

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(attestation_request));
  EXPECT_CALL(*transport_, Send(attestation_request, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
        std::move(callback).Run(attestation_response);
      });
  EXPECT_CALL(*attestation_handler_,
              VerifyAttestationResponse(attestation_response))
      .WillOnce(Return(true));
  EXPECT_CALL(*oak_session_, GetHandshakeMessage())
      .WillOnce(Return(handshake_request));
  EXPECT_CALL(*transport_, Send(handshake_request, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
        std::move(callback).Run(handshake_response);
      });
  EXPECT_CALL(*oak_session_, ProcessHandshakeResponse(handshake_response))
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
TEST_F(SecureChannelImplTest, DecryptResponseFails) {
  Request request_data = {1};
  Response response_data = {2};
  Request attestation_request = {3};
  Response attestation_response = {4};
  Request handshake_request = {5};
  Response handshake_response = {6};
  Response encrypted_request = {7};

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(attestation_request));
  EXPECT_CALL(*transport_, Send(attestation_request, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
        std::move(callback).Run(attestation_response);
      });
  EXPECT_CALL(*attestation_handler_,
              VerifyAttestationResponse(attestation_response))
      .WillOnce(Return(true));
  EXPECT_CALL(*oak_session_, GetHandshakeMessage())
      .WillOnce(Return(handshake_request));
  EXPECT_CALL(*transport_, Send(handshake_request, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
        std::move(callback).Run(handshake_response);
      });
  EXPECT_CALL(*oak_session_, ProcessHandshakeResponse(handshake_response))
      .WillOnce(Return(true));
  EXPECT_CALL(*oak_session_, Encrypt(request_data))
      .WillOnce(Return(encrypted_request));
  EXPECT_CALL(*transport_, Send(encrypted_request, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
        std::move(callback).Run(response_data);
      });
  EXPECT_CALL(*oak_session_, Decrypt(response_data))
      .WillOnce(Return(std::nullopt));

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
TEST_F(SecureChannelImplTest, WriteInPermanentFailureState) {
  Request attestation_request = {1};

  EXPECT_CALL(*attestation_handler_, GetAttestationRequest())
      .WillOnce(Return(attestation_request));
  EXPECT_CALL(*transport_, Send(attestation_request, _))
      .WillOnce([&](Request, Transport::ResponseCallback callback) {
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
