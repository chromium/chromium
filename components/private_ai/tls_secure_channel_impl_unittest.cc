// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/tls_secure_channel_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/mojom/oak_session.mojom.h"
#include "components/private_ai/status_code.h"
#include "components/private_ai/tls_certificate_downloader.h"
#include "components/private_ai/tls_certificate_repository.h"
#include "components/private_ai/tls_websocket_client.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {
namespace {

class FakeOakTlsSessionService : public mojom::OakTlsSession {
 public:
  FakeOakTlsSessionService() = default;
  ~FakeOakTlsSessionService() override = default;

  mojo::Remote<mojom::OakTlsSession> BindAndCreateRemote() {
    mojo::Remote<mojom::OakTlsSession> remote;
    receivers_.Add(this, remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  void StartHandshake(const std::string& expected_server_name,
                      const std::string& tca_root_cert_pem,
                      StartHandshakeCallback callback) override {
    start_tls_calls_++;
    last_expected_server_name_ = expected_server_name;
    last_tca_root_cert_pem_ = tca_root_cert_pem;

    if (start_tls_error_.has_value()) {
      std::move(callback).Run(base::unexpected(*start_tls_error_));
      return;
    }

    std::vector<uint8_t> client_hello = {0x16, 0x03, 0x03, 0x00, 0x01};
    std::move(callback).Run(
        mojom::TlsHandshake::New(std::move(client_hello), false));
  }

  void ProcessHandshakeFrame(const std::vector<uint8_t>& incoming_frame,
                             ProcessHandshakeFrameCallback callback) override {
    process_frame_calls_++;
    last_incoming_handshake_frame_ = incoming_frame;

    if (next_process_error_.has_value()) {
      std::move(callback).Run(base::unexpected(*next_process_error_));
      return;
    }

    std::vector<uint8_t> outgoing_frame = {0x14, 0x03, 0x03, 0x00, 0x01};
    std::move(callback).Run(mojom::TlsHandshake::New(std::move(outgoing_frame),
                                                     is_ready_on_process_));
  }

  void Encrypt(const std::vector<uint8_t>& input,
               EncryptCallback callback) override {
    std::vector<uint8_t> encrypted = input;
    encrypted.insert(encrypted.begin(), 0xAA);  // prefix with dummy marker
    std::move(callback).Run(encrypted);
  }

  void Decrypt(const std::vector<uint8_t>& input,
               DecryptCallback callback) override {
    if (input.empty() || input[0] != 0xAA) {
      std::move(callback).Run(std::nullopt, false);
      return;
    }
    std::vector<uint8_t> decrypted(input.begin() + 1, input.end());
    std::move(callback).Run(decrypted, false);
  }

  void DisconnectAll() { receivers_.Clear(); }

  int start_tls_calls_ = 0;
  int process_frame_calls_ = 0;
  std::string last_expected_server_name_;
  std::string last_tca_root_cert_pem_;
  std::vector<uint8_t> last_incoming_handshake_frame_;

  std::optional<mojom::TlsHandshakeError> start_tls_error_;
  std::optional<mojom::TlsHandshakeError> next_process_error_;
  bool is_ready_on_process_ = false;

 private:
  mojo::ReceiverSet<mojom::OakTlsSession> receivers_;
};

class FakeTlsCertificateRepository : public TlsCertificateRepository {
 public:
  FakeTlsCertificateRepository()
      : TlsCertificateRepository(std::make_unique<TlsCertificateDownloader>(
                                     GURL(),
                                     GURL(),
                                     network::NetworkContextGetter()),
                                 "tca",
                                 "oakct.transparentrelease.goog",
                                 network::NetworkContextGetter()) {}
  ~FakeTlsCertificateRepository() override = default;

  void GetCertificates(CertificatesCallback callback) override {
    get_count_++;
    std::move(callback).Run(certs_);
  }

  void ForceRefresh(CertificatesCallback callback) override {
    refresh_count_++;
    std::move(callback).Run(refreshed_certs_);
  }

  void SetCerts(TlsCertificates certs) { certs_ = std::move(certs); }
  void SetRefreshedCerts(TlsCertificates certs) {
    refreshed_certs_ = std::move(certs);
  }

  int get_count() const { return get_count_; }
  int refresh_count() const { return refresh_count_; }

 private:
  std::optional<TlsCertificates> certs_;
  std::optional<TlsCertificates> refreshed_certs_;
  int get_count_ = 0;
  int refresh_count_ = 0;
};

class FakeTlsWebSocketClient : public TlsWebSocketClient {
 public:
  explicit FakeTlsWebSocketClient(PrivateAiLogger* logger)
      : TlsWebSocketClient(GURL("wss://test.example.com"),
                           network::NetworkContextGetter(),
                           logger) {}
  ~FakeTlsWebSocketClient() override = default;

  void SetFrameCallback(FrameCallback callback) override {
    frame_callback_ = std::move(callback);
  }

  void SendFrame(const std::vector<uint8_t>& frame_bytes) override {
    sent_frames_.push_back(frame_bytes);
  }

  void SimulateIncomingFrame(const std::vector<uint8_t>& frame_bytes) {
    if (frame_callback_) {
      frame_callback_.Run(base::ok(frame_bytes));
    }
  }

  void SimulateTransportError(Error error) {
    if (frame_callback_) {
      frame_callback_.Run(base::unexpected(error));
    }
  }

  const std::vector<std::vector<uint8_t>>& sent_frames() const {
    return sent_frames_;
  }

 private:
  FrameCallback frame_callback_;
  std::vector<std::vector<uint8_t>> sent_frames_;
};

class TlsSecureChannelImplTest : public testing::Test {
 public:
  void SetUp() override {
    certs_.tca_root_pem = "fake_tca_pem";
    cert_repo_.SetCerts(certs_);
    cert_repo_.SetRefreshedCerts(certs_);
  }

 protected:
  std::unique_ptr<TlsSecureChannelImpl> CreateChannel(
      base::OnceClosure on_established = base::DoNothing(),
      TlsSecureChannelImpl::ResponseCallback response_callback =
          base::DoNothing(),
      const std::string& expected_server_name = "test.server.goog") {
    return std::make_unique<TlsSecureChannelImpl>(
        std::move(on_established), std::move(response_callback),
        GetTransportFactory(), GetOakSessionFactory(), &cert_repo_, &logger_,
        expected_server_name);
  }

  void WaitForClientHello(size_t transport_index = 0) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return created_transports_.size() > transport_index &&
             created_transports_[transport_index]->sent_frames().size() >= 1u;
    }));
  }

  FakeTlsWebSocketClient* transport(size_t index = 0) {
    CHECK_LT(index, created_transports_.size());
    return created_transports_[index];
  }

  TlsSecureChannelImpl::TransportFactory GetTransportFactory() {
    return base::BindRepeating(&TlsSecureChannelImplTest::CreateFakeTransport,
                               base::Unretained(this));
  }

  TlsSecureChannelImpl::OakSessionFactory GetOakSessionFactory() {
    return base::BindRepeating(&FakeOakTlsSessionService::BindAndCreateRemote,
                               base::Unretained(&fake_oak_service_));
  }

  std::unique_ptr<TlsWebSocketClient> CreateFakeTransport() {
    auto transport = std::make_unique<FakeTlsWebSocketClient>(&logger_);
    created_transports_.push_back(transport.get());
    return transport;
  }

  base::test::TaskEnvironment task_environment_;
  PrivateAiLogger logger_;
  FakeOakTlsSessionService fake_oak_service_;
  FakeTlsCertificateRepository cert_repo_;
  TlsCertificates certs_;
  std::vector<FakeTlsWebSocketClient*> created_transports_;
};

TEST_F(TlsSecureChannelImplTest, SuccessfulHandshakeAndDataTransfer) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> established_future;
  base::test::TestFuture<base::expected<Response, StatusCode>> response_future;

  fake_oak_service_.is_ready_on_process_ = true;

  auto channel = CreateChannel(established_future.GetCallback(),
                               response_future.GetRepeatingCallback());

  WaitForClientHello();

  EXPECT_EQ(cert_repo_.get_count(), 1);
  EXPECT_EQ(fake_oak_service_.start_tls_calls_, 1);
  EXPECT_EQ(fake_oak_service_.last_expected_server_name_, "test.server.goog");

  // Simulate server sending back ServerHello.
  std::vector<uint8_t> server_hello = {0x16, 0x03, 0x03, 0x00, 0x02};
  transport()->SimulateIncomingFrame(server_hello);
  EXPECT_TRUE(established_future.Wait());

  EXPECT_EQ(fake_oak_service_.process_frame_calls_, 1);
  EXPECT_EQ(fake_oak_service_.last_incoming_handshake_frame_, server_hello);

  // Application Data Transfer: Send request
  Request request = {0x01, 0x02, 0x03};
  EXPECT_TRUE(channel->Write(request));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return transport()->sent_frames().size() == 3u; }));

  // Sent frame should be encrypted (starts with 0xAA prefix)
  // ClientHello + Outgoing Finished + Encrypted app data
  EXPECT_EQ(transport()->sent_frames()[2][0], 0xAA);

  // Simulate receiving encrypted response from server
  std::vector<uint8_t> encrypted_response = {0xAA, 0x10, 0x20};
  transport()->SimulateIncomingFrame(encrypted_response);

  auto result = response_future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, Response({0x10, 0x20}));

  histogram_tester.ExpectTotalCount("PrivateAi.Tls.HandshakeLatency.Success",
                                    1);
  histogram_tester.ExpectBucketCount("PrivateAi.Tls.HandshakeStatus",
                                     TlsHandshakeStatus::kSuccess, 1);

  channel.reset();
  histogram_tester.ExpectTotalCount("PrivateAi.SecureChannel.SessionDuration2",
                                    1);
  histogram_tester.ExpectUniqueSample(
      "PrivateAi.SecureChannel.RequestsPerSession", 1, 1);
}

TEST_F(TlsSecureChannelImplTest, CertMismatchTriggersRefreshAndRetry) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> established_future;
  base::test::TestFuture<base::expected<Response, StatusCode>> response_future;

  // First process returns cert verification failed
  fake_oak_service_.next_process_error_ =
      mojom::TlsHandshakeError::kCertVerificationFailed;

  auto channel = CreateChannel(established_future.GetCallback(),
                               response_future.GetRepeatingCallback());

  WaitForClientHello();
  EXPECT_EQ(cert_repo_.get_count(), 1);

  // Simulate ServerHello triggering cert failure
  transport()->SimulateIncomingFrame({0x16, 0x03, 0x03});
  WaitForClientHello(/*transport_index=*/1);

  // Channel should have invoked ForceRefresh on cert repo
  EXPECT_EQ(cert_repo_.refresh_count(), 1);
  // Handshake should have been restarted with a brand new transport & session
  EXPECT_EQ(fake_oak_service_.start_tls_calls_, 2);

  // Second attempt succeeds
  fake_oak_service_.next_process_error_ = std::nullopt;
  fake_oak_service_.is_ready_on_process_ = true;

  transport(1)->SimulateIncomingFrame({0x16, 0x03, 0x03});
  EXPECT_TRUE(established_future.Wait());

  // One HandshakeStatus sample per attempt.
  histogram_tester.ExpectBucketCount(
      "PrivateAi.Tls.HandshakeStatus",
      TlsHandshakeStatus::kCertVerificationFailed, 1);
  histogram_tester.ExpectBucketCount("PrivateAi.Tls.HandshakeStatus",
                                     TlsHandshakeStatus::kSuccess, 1);
  histogram_tester.ExpectTotalCount("PrivateAi.Tls.HandshakeStatus", 2);
}

TEST_F(TlsSecureChannelImplTest, CertMismatchRetryFailsClosesChannel) {
  base::test::TestFuture<void> established_future;
  base::test::TestFuture<base::expected<Response, StatusCode>> response_future;

  // Both attempts fail cert verification
  fake_oak_service_.next_process_error_ =
      mojom::TlsHandshakeError::kCertVerificationFailed;

  auto channel = CreateChannel(established_future.GetCallback(),
                               response_future.GetRepeatingCallback());

  WaitForClientHello();

  // First failure triggers refresh
  transport()->SimulateIncomingFrame({0x16, 0x03, 0x03});
  WaitForClientHello(/*transport_index=*/1);

  EXPECT_EQ(cert_repo_.refresh_count(), 1);

  // Second failure aborts
  transport(1)->SimulateIncomingFrame({0x16, 0x03, 0x03});

  EXPECT_EQ(response_future.Take().error(), StatusCode::kAttestationFailed);
  EXPECT_FALSE(established_future.IsReady());
}

TEST_F(TlsSecureChannelImplTest,
       WriteMultipleRequestsWithoutWaitingForResponse) {
  base::test::TestFuture<base::expected<Response, StatusCode>> response_future;

  fake_oak_service_.is_ready_on_process_ = true;

  std::unique_ptr<TlsSecureChannelImpl> channel;
  bool established = false;
  channel = CreateChannel(base::BindLambdaForTesting([&]() {
                            established = true;
                            // A write inside on_established should be ordered
                            // after pre-handshake queued requests.
                            EXPECT_TRUE(channel->Write({0x05}));
                          }),
                          response_future.GetRepeatingCallback());

  // Queue two back-to-back requests before handshake completes (simulating
  // AnonymousTokenRequest followed immediately by an inference request).
  Request token_request = {0x01, 0x02};
  Request inference_request = {0x03, 0x04};
  EXPECT_TRUE(channel->Write(token_request));
  EXPECT_TRUE(channel->Write(inference_request));

  WaitForClientHello();

  // Complete handshake; both pre-handshake requests plus the request written
  // inside on_established should be encrypted and sent in order without waiting
  // for a server response.
  transport()->SimulateIncomingFrame({0x16, 0x03, 0x03, 0x00, 0x02});
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return transport()->sent_frames().size() == 5u; }));

  EXPECT_TRUE(established);
  // ClientHello + Outgoing Finished + 3 encrypted requests = 5 frames.
  EXPECT_EQ(transport()->sent_frames()[2],
            std::vector<uint8_t>({0xAA, 0x01, 0x02}));
  EXPECT_EQ(transport()->sent_frames()[3],
            std::vector<uint8_t>({0xAA, 0x03, 0x04}));
  EXPECT_EQ(transport()->sent_frames()[4], std::vector<uint8_t>({0xAA, 0x05}));

  // Also verify back-to-back writes after establishment do not block on a
  // response.
  EXPECT_TRUE(channel->Write({0x06}));
  EXPECT_TRUE(channel->Write({0x07}));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return transport()->sent_frames().size() == 7u; }));

  EXPECT_EQ(transport()->sent_frames()[5], std::vector<uint8_t>({0xAA, 0x06}));
  EXPECT_EQ(transport()->sent_frames()[6], std::vector<uint8_t>({0xAA, 0x07}));

  // Simulate server sending a response only for the inference request.
  transport()->SimulateIncomingFrame({0xAA, 0x99, 0x88});

  auto result = response_future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, Response({0x99, 0x88}));
}

TEST_F(TlsSecureChannelImplTest, PostHandshakeControlFrameIgnored) {
  base::test::TestFuture<void> established_future;
  base::test::TestFuture<base::expected<Response, StatusCode>> response_future;

  fake_oak_service_.is_ready_on_process_ = true;

  auto channel = CreateChannel(established_future.GetCallback(),
                               response_future.GetRepeatingCallback());

  WaitForClientHello();

  transport()->SimulateIncomingFrame({0x16, 0x03, 0x03, 0x00, 0x02});
  EXPECT_TRUE(established_future.Wait());

  // 1. Empty raw WebSocket frame should be ignored without failing decryption.
  transport()->SimulateIncomingFrame({});

  // 2. Post-handshake TLS control record (e.g., NewSessionTicket) that decrypts
  // to an empty plaintext payload ({0xAA} -> {}) should not invoke callback.
  transport()->SimulateIncomingFrame({0xAA});

  // 3. Subsequent application response still arrives normally (and since Mojo
  // preserves FIFO order, receiving 0x42 first proves the two preceding empty
  // frames were ignored without firing response_future).
  transport()->SimulateIncomingFrame({0xAA, 0x42});
  auto result = response_future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, Response({0x42}));
}

TEST_F(TlsSecureChannelImplTest,
       SocketClosedInEstablishedMapsToConnectionClosedByServer) {
  base::test::TestFuture<void> established_future;
  base::test::TestFuture<base::expected<Response, StatusCode>> response_future;

  fake_oak_service_.is_ready_on_process_ = true;

  auto channel = CreateChannel(established_future.GetCallback(),
                               response_future.GetRepeatingCallback());

  WaitForClientHello();

  transport()->SimulateIncomingFrame({0x16, 0x03, 0x03, 0x00, 0x02});
  EXPECT_TRUE(established_future.Wait());

  // When the server rejects an AnonymousTokenRequest by closing the stream,
  // kSocketClosed in State::kEstablished should map to
  // StatusCode::kConnectionClosedByServer.
  transport()->SimulateTransportError(TlsWebSocketClient::Error::kSocketClosed);

  auto result = response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StatusCode::kConnectionClosedByServer);
}

TEST_F(TlsSecureChannelImplTest,
       ConnectionFailureDuringHandshakeMapsToNetworkError) {
  base::test::TestFuture<void> established_future;
  base::test::TestFuture<base::expected<Response, StatusCode>> response_future;

  auto channel = CreateChannel(established_future.GetCallback(),
                               response_future.GetRepeatingCallback());

  WaitForClientHello();

  // The WebSocket could not be opened (e.g. offline) while the ClientHello
  // was pending: that is a network error, not a handshake failure.
  transport()->SimulateTransportError(
      TlsWebSocketClient::Error::kConnectionFailed);

  auto result = response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StatusCode::kNetworkError);
}

TEST_F(TlsSecureChannelImplTest, HandshakeStatusRecordedOncePerAttempt) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> established_future;
  base::test::TestFuture<base::expected<Response, StatusCode>> response_future;

  auto channel = CreateChannel(established_future.GetCallback(),
                               response_future.GetRepeatingCallback());

  WaitForClientHello();

  // The server flight arrives in two WebSocket frames; only the second one
  // completes the handshake.
  transport()->SimulateIncomingFrame({0x16, 0x03, 0x03, 0x00, 0x01});
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_oak_service_.process_frame_calls_ == 1; }));
  fake_oak_service_.is_ready_on_process_ = true;
  transport()->SimulateIncomingFrame({0x16, 0x03, 0x03, 0x00, 0x02});
  EXPECT_TRUE(established_future.Wait());

  histogram_tester.ExpectUniqueSample("PrivateAi.Tls.HandshakeStatus",
                                      TlsHandshakeStatus::kSuccess, 1);
}

TEST_F(TlsSecureChannelImplTest, OakSessionDisconnectFailsChannel) {
  base::test::TestFuture<void> established_future;
  base::test::TestFuture<base::expected<Response, StatusCode>> response_future;

  auto channel = CreateChannel(established_future.GetCallback(),
                               response_future.GetRepeatingCallback());

  WaitForClientHello();

  fake_oak_service_.DisconnectAll();

  auto result = response_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StatusCode::kHandshakeFailed);
  EXPECT_FALSE(channel->Write({0x01}));
}

TEST_F(TlsSecureChannelImplTest,
       BackToBackHandshakeAndApplicationFramesRoutedInOrder) {
  base::test::TestFuture<void> established_future;
  base::test::TestFuture<base::expected<Response, StatusCode>> response_future;

  fake_oak_service_.is_ready_on_process_ = true;

  auto channel = CreateChannel(established_future.GetCallback(),
                               response_future.GetRepeatingCallback());

  WaitForClientHello();

  // Deliver the handshake completion frame and an encrypted application frame
  // synchronously back-to-back before the async ProcessTlsHandshakeFrame Mojo
  // reply can run.
  transport()->SimulateIncomingFrame({0x16, 0x03, 0x03, 0x00, 0x02});
  transport()->SimulateIncomingFrame({0xAA, 0x77, 0x88});

  EXPECT_TRUE(established_future.Wait());
  auto result = response_future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, Response({0x77, 0x88}));
  EXPECT_EQ(fake_oak_service_.process_frame_calls_, 1);
}

}  // namespace
}  // namespace private_ai
