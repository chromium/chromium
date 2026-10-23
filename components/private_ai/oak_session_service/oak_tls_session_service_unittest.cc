// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/oak_session_service/oak_tls_session_service.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/private_ai/mojom/oak_session.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/oak/src/oak_session/tls/oak_session_tls.h"
#include "third_party/oak/src/oak_session/tls/util.h"

namespace private_ai {
namespace {

class OakTlsSessionServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    service_ = std::make_unique<OakTlsSessionService>(
        remote_.BindNewPipeAndPassReceiver());
  }

  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::OakTlsSession> remote_;
  std::unique_ptr<OakTlsSessionService> service_;
};

TEST_F(OakTlsSessionServiceTest, StartWithEmptyServerName) {
  mojo::test::BadMessageObserver observer;
  remote_->StartHandshake("", "not a certificate", base::DoNothing());
  EXPECT_EQ(observer.WaitForBadMessage(), "Invalid StartHandshake parameters");
}

TEST_F(OakTlsSessionServiceTest, StartWithEmptyRootCert) {
  mojo::test::BadMessageObserver observer;
  remote_->StartHandshake("example.com", "", base::DoNothing());
  EXPECT_EQ(observer.WaitForBadMessage(), "Invalid StartHandshake parameters");
}

TEST_F(OakTlsSessionServiceTest, StartWithGarbageRootCertFails) {
  base::test::TestFuture<
      base::expected<mojom::TlsHandshakePtr, mojom::TlsHandshakeError>>
      start_future;
  remote_->StartHandshake("localhost", "not a certificate",
                          start_future.GetCallback());
  const auto& result = start_future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), mojom::TlsHandshakeError::kProtocolError);

  // A second StartHandshake after a failed first attempt is still rejected.
  mojo::test::BadMessageObserver observer;
  remote_->StartHandshake("localhost", "not a certificate", base::DoNothing());
  EXPECT_EQ(observer.WaitForBadMessage(), "Unexpected StartHandshake call");
}

TEST_F(OakTlsSessionServiceTest, ProcessFrameWithoutStart) {
  mojo::test::BadMessageObserver observer;
  remote_->ProcessHandshakeFrame({1, 2, 3}, base::DoNothing());
  EXPECT_EQ(observer.WaitForBadMessage(),
            "ProcessHandshakeFrame called without an active TLS handshake");
}

TEST_F(OakTlsSessionServiceTest, EncryptBeforeHandshake) {
  mojo::test::BadMessageObserver observer;
  remote_->Encrypt({1, 2, 3}, base::DoNothing());
  EXPECT_EQ(observer.WaitForBadMessage(),
            "Encrypt called before handshake completion");
}

TEST_F(OakTlsSessionServiceTest, DecryptBeforeHandshake) {
  mojo::test::BadMessageObserver observer;
  remote_->Decrypt({1, 2, 3}, base::DoNothing());
  EXPECT_EQ(observer.WaitForBadMessage(),
            "Decrypt called before handshake completion");
}

TEST_F(OakTlsSessionServiceTest, StartHandshakeProducesClientHello) {
  auto cert_builder = net::CertBuilder::CreateSimpleChain(1);
  std::string root_pem = cert_builder[0]->GetPEM();

  base::test::TestFuture<
      base::expected<mojom::TlsHandshakePtr, mojom::TlsHandshakeError>>
      start_future;
  remote_->StartHandshake("localhost", root_pem, start_future.GetCallback());
  auto result = start_future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result.value()->outgoing_frame.empty());
  EXPECT_FALSE(result.value()->is_ready);

  // Duplicate StartHandshake causes BadMessage.
  mojo::test::BadMessageObserver observer;
  remote_->StartHandshake("localhost", root_pem, base::DoNothing());
  EXPECT_EQ(observer.WaitForBadMessage(), "Unexpected StartHandshake call");
}

TEST_F(OakTlsSessionServiceTest, HandshakeAndCryptoSuccess) {
  auto server_id_provider_or = oak::session::tls::util::CreateSelfSigned();
  ASSERT_TRUE(server_id_provider_or.ok());
  auto server_id_or = (*server_id_provider_or)->GetIdentity();
  ASSERT_TRUE(server_id_or.ok());
  ASSERT_FALSE(server_id_or->cert_chain.empty());

  scoped_refptr<net::X509Certificate> server_cert =
      net::X509Certificate::CreateFromBytes(
          base::as_byte_span(server_id_or->cert_chain[0]));
  ASSERT_TRUE(server_cert);
  std::string root_pem;
  ASSERT_TRUE(net::X509Certificate::GetPEMEncoded(server_cert->cert_buffer(),
                                                  &root_pem));

  oak::session::tls::ServerContextConfig server_config{
      .tls_identity_provider = std::move(*server_id_provider_or),
  };
  auto server_ctx_or =
      oak::session::tls::OakSessionTlsContext::Create(std::move(server_config));
  ASSERT_TRUE(server_ctx_or.ok());
  auto server_init_or = (*server_ctx_or)->NewSession();
  ASSERT_TRUE(server_init_or.ok());
  auto server_init = std::move(*server_init_or);

  // Step 1: Client starts handshake and produces ClientHello.
  base::test::TestFuture<
      base::expected<mojom::TlsHandshakePtr, mojom::TlsHandshakeError>>
      start_future;
  remote_->StartHandshake(std::string(oak::session::tls::kDefaultServerName),
                          root_pem, start_future.GetCallback());
  auto start_res = start_future.Take();
  ASSERT_TRUE(start_res.has_value());
  EXPECT_FALSE(start_res.value()->outgoing_frame.empty());
  EXPECT_FALSE(start_res.value()->is_ready);

  // Step 2: Server processes ClientHello and produces ServerHello..Finished.
  std::string_view client_hello(
      reinterpret_cast<const char*>(start_res.value()->outgoing_frame.data()),
      start_res.value()->outgoing_frame.size());
  ASSERT_TRUE(server_init->PutTLSFrame(client_hello).ok());
  auto server_flight_or = server_init->GetTLSFrame();
  ASSERT_TRUE(server_flight_or.ok());
  EXPECT_FALSE(server_flight_or->empty());

  // Step 3: Client processes server flight, completes handshake, sends
  // Finished.
  std::vector<uint8_t> server_flight(server_flight_or->begin(),
                                     server_flight_or->end());
  base::test::TestFuture<
      base::expected<mojom::TlsHandshakePtr, mojom::TlsHandshakeError>>
      proc_future;
  remote_->ProcessHandshakeFrame(server_flight, proc_future.GetCallback());
  auto proc_res = proc_future.Take();
  ASSERT_TRUE(proc_res.has_value());
  EXPECT_FALSE(proc_res.value()->outgoing_frame.empty());
  EXPECT_TRUE(proc_res.value()->is_ready);

  // Step 4: Server processes Client Finished and opens session.
  std::string_view client_finished(
      reinterpret_cast<const char*>(proc_res.value()->outgoing_frame.data()),
      proc_res.value()->outgoing_frame.size());
  ASSERT_TRUE(server_init->PutTLSFrame(client_finished).ok());
  ASSERT_TRUE(server_init->IsReady());
  auto server_open_or = server_init->GetOpenSession();
  ASSERT_TRUE(server_open_or.ok());
  auto server_session = std::move(server_open_or->session);

  // Step 5: Test Encrypt (client -> server) and Decrypt (server -> client).
  const std::vector<uint8_t> client_plaintext = {10, 20, 30, 40};
  base::test::TestFuture<const std::optional<std::vector<uint8_t>>&> enc_future;
  remote_->Encrypt(client_plaintext, enc_future.GetCallback());
  auto encrypted = enc_future.Take();
  ASSERT_TRUE(encrypted.has_value());

  auto server_decrypted_or = server_session->Decrypt(std::string_view(
      reinterpret_cast<const char*>(encrypted->data()), encrypted->size()));
  ASSERT_TRUE(server_decrypted_or.ok());
  EXPECT_EQ(std::vector<uint8_t>(server_decrypted_or->begin(),
                                 server_decrypted_or->end()),
            client_plaintext);

  const std::string server_plaintext = "response from tls server";
  auto server_encrypted_or = server_session->Encrypt(server_plaintext);
  ASSERT_TRUE(server_encrypted_or.ok());
  std::vector<uint8_t> server_ciphertext(server_encrypted_or->begin(),
                                         server_encrypted_or->end());

  base::test::TestFuture<const std::optional<std::vector<uint8_t>>&, bool>
      dec_future;
  remote_->Decrypt(server_ciphertext, dec_future.GetCallback());
  auto [client_decrypted, peer_closed] = dec_future.Take();
  ASSERT_TRUE(client_decrypted.has_value());
  EXPECT_FALSE(peer_closed);
  EXPECT_EQ(std::string(client_decrypted->begin(), client_decrypted->end()),
            server_plaintext);

  // Partial TLS frames are buffered by BoringSSL and return an empty vector
  // until the remaining bytes complete the record.
  const std::string chunked_message = "chunked stream message";
  auto chunked_encrypted_or = server_session->Encrypt(chunked_message);
  ASSERT_TRUE(chunked_encrypted_or.ok());
  std::vector<uint8_t> chunked_ciphertext(chunked_encrypted_or->begin(),
                                          chunked_encrypted_or->end());

  const size_t split_point = chunked_ciphertext.size() / 2;
  const std::vector<uint8_t> first_chunk(
      chunked_ciphertext.begin(), chunked_ciphertext.begin() + split_point);
  const std::vector<uint8_t> second_chunk(
      chunked_ciphertext.begin() + split_point, chunked_ciphertext.end());

  base::test::TestFuture<const std::optional<std::vector<uint8_t>>&, bool>
      partial_dec_future;
  remote_->Decrypt(first_chunk, partial_dec_future.GetCallback());
  auto [partial_decrypted, partial_peer_closed] = partial_dec_future.Take();
  ASSERT_TRUE(partial_decrypted.has_value());
  EXPECT_TRUE(partial_decrypted->empty());
  EXPECT_FALSE(partial_peer_closed);

  base::test::TestFuture<const std::optional<std::vector<uint8_t>>&, bool>
      complete_dec_future;
  remote_->Decrypt(second_chunk, complete_dec_future.GetCallback());
  auto [complete_decrypted, complete_peer_closed] = complete_dec_future.Take();
  ASSERT_TRUE(complete_decrypted.has_value());
  EXPECT_FALSE(complete_peer_closed);
  EXPECT_EQ(std::string(complete_decrypted->begin(), complete_decrypted->end()),
            chunked_message);

  // Corrupted ciphertext returns std::nullopt.
  base::test::TestFuture<const std::optional<std::vector<uint8_t>>&, bool>
      corrupt_dec_future;
  remote_->Decrypt({0x16, 0x03, 0x03, 0x00, 0x05, 0xde, 0xad, 0xbe, 0xef, 0x00},
                   corrupt_dec_future.GetCallback());
  auto [corrupt_decrypted, corrupt_peer_closed] = corrupt_dec_future.Take();
  EXPECT_FALSE(corrupt_decrypted.has_value());
  EXPECT_FALSE(corrupt_peer_closed);

  // Calling Encrypt or Decrypt with empty input returns std::nullopt.
  base::test::TestFuture<const std::optional<std::vector<uint8_t>>&>
      empty_enc_future;
  remote_->Encrypt({}, empty_enc_future.GetCallback());
  EXPECT_FALSE(empty_enc_future.Take().has_value());

  base::test::TestFuture<const std::optional<std::vector<uint8_t>>&, bool>
      empty_dec_future;
  remote_->Decrypt({}, empty_dec_future.GetCallback());
  auto [empty_decrypted, empty_peer_closed] = empty_dec_future.Take();
  EXPECT_FALSE(empty_decrypted.has_value());
  EXPECT_FALSE(empty_peer_closed);

  // Calling ProcessHandshakeFrame after handshake completion is rejected
  // with BadMessage instead of crashing on a moved-from initializer.
  mojo::test::BadMessageObserver observer;
  remote_->ProcessHandshakeFrame({1, 2, 3}, base::DoNothing());
  EXPECT_EQ(observer.WaitForBadMessage(),
            "ProcessHandshakeFrame called without an active TLS handshake");
}

TEST_F(OakTlsSessionServiceTest, CertVerificationFailure) {
  // Server uses its own self-signed cert, while client trusts an unrelated CA.
  auto server_id_provider_or = oak::session::tls::util::CreateSelfSigned();
  ASSERT_TRUE(server_id_provider_or.ok());
  oak::session::tls::ServerContextConfig server_config{
      .tls_identity_provider = std::move(*server_id_provider_or),
  };
  auto server_ctx_or =
      oak::session::tls::OakSessionTlsContext::Create(std::move(server_config));
  ASSERT_TRUE(server_ctx_or.ok());
  auto server_init_or = (*server_ctx_or)->NewSession();
  ASSERT_TRUE(server_init_or.ok());
  auto server_init = std::move(*server_init_or);

  auto unrelated_ca = net::CertBuilder::CreateSimpleChain(1);
  std::string unrelated_root_pem = unrelated_ca[0]->GetPEM();

  base::test::TestFuture<
      base::expected<mojom::TlsHandshakePtr, mojom::TlsHandshakeError>>
      start_future;
  remote_->StartHandshake(std::string(oak::session::tls::kDefaultServerName),
                          unrelated_root_pem, start_future.GetCallback());
  auto start_res = start_future.Take();
  ASSERT_TRUE(start_res.has_value());

  std::string_view client_hello(
      reinterpret_cast<const char*>(start_res.value()->outgoing_frame.data()),
      start_res.value()->outgoing_frame.size());
  ASSERT_TRUE(server_init->PutTLSFrame(client_hello).ok());
  auto server_flight_or = server_init->GetTLSFrame();
  ASSERT_TRUE(server_flight_or.ok());

  std::vector<uint8_t> server_flight(server_flight_or->begin(),
                                     server_flight_or->end());
  base::test::TestFuture<
      base::expected<mojom::TlsHandshakePtr, mojom::TlsHandshakeError>>
      proc_future;
  remote_->ProcessHandshakeFrame(server_flight, proc_future.GetCallback());
  auto proc_res = proc_future.Take();
  ASSERT_FALSE(proc_res.has_value());
  EXPECT_EQ(proc_res.error(),
            mojom::TlsHandshakeError::kCertVerificationFailed);

  // Subsequent ProcessHandshakeFrame after failure triggers BadMessage.
  mojo::test::BadMessageObserver observer;
  remote_->ProcessHandshakeFrame({1, 2, 3}, base::DoNothing());
  EXPECT_EQ(observer.WaitForBadMessage(),
            "ProcessHandshakeFrame called without an active TLS handshake");
}

TEST_F(OakTlsSessionServiceTest, ProcessEmptyHandshakeFrame) {
  auto cert_builder = net::CertBuilder::CreateSimpleChain(1);
  std::string root_pem = cert_builder[0]->GetPEM();

  base::test::TestFuture<
      base::expected<mojom::TlsHandshakePtr, mojom::TlsHandshakeError>>
      start_future;
  remote_->StartHandshake("localhost", root_pem, start_future.GetCallback());
  ASSERT_TRUE(start_future.Take().has_value());

  base::test::TestFuture<
      base::expected<mojom::TlsHandshakePtr, mojom::TlsHandshakeError>>
      proc_future;
  remote_->ProcessHandshakeFrame({}, proc_future.GetCallback());
  auto proc_res = proc_future.Take();
  ASSERT_FALSE(proc_res.has_value());
  EXPECT_EQ(proc_res.error(), mojom::TlsHandshakeError::kProtocolError);

  // Subsequent ProcessHandshakeFrame after failure triggers BadMessage.
  mojo::test::BadMessageObserver observer;
  remote_->ProcessHandshakeFrame({1, 2, 3}, base::DoNothing());
  EXPECT_EQ(observer.WaitForBadMessage(),
            "ProcessHandshakeFrame called without an active TLS handshake");
}

TEST_F(OakTlsSessionServiceTest, HandshakeWithMultipleRootCertsInPemBundle) {
  auto server_id_provider_or = oak::session::tls::util::CreateSelfSigned();
  ASSERT_TRUE(server_id_provider_or.ok());
  auto server_id_or = (*server_id_provider_or)->GetIdentity();
  ASSERT_TRUE(server_id_or.ok());
  ASSERT_FALSE(server_id_or->cert_chain.empty());

  scoped_refptr<net::X509Certificate> server_cert =
      net::X509Certificate::CreateFromBytes(
          base::as_byte_span(server_id_or->cert_chain[0]));
  ASSERT_TRUE(server_cert);
  std::string server_root_pem;
  ASSERT_TRUE(net::X509Certificate::GetPEMEncoded(server_cert->cert_buffer(),
                                                  &server_root_pem));

  auto unrelated_ca = net::CertBuilder::CreateSimpleChain(1);
  // Put an unrelated cert first and the actual server root second (simulating
  // a CA key rotation bundle).
  std::string bundled_pem = unrelated_ca[0]->GetPEM() + "\n" + server_root_pem;

  oak::session::tls::ServerContextConfig server_config{
      .tls_identity_provider = std::move(*server_id_provider_or),
  };
  auto server_ctx_or =
      oak::session::tls::OakSessionTlsContext::Create(std::move(server_config));
  ASSERT_TRUE(server_ctx_or.ok());
  auto server_init_or = (*server_ctx_or)->NewSession();
  ASSERT_TRUE(server_init_or.ok());
  auto server_init = std::move(*server_init_or);

  base::test::TestFuture<
      base::expected<mojom::TlsHandshakePtr, mojom::TlsHandshakeError>>
      start_future;
  remote_->StartHandshake(std::string(oak::session::tls::kDefaultServerName),
                          bundled_pem, start_future.GetCallback());
  auto start_res = start_future.Take();
  ASSERT_TRUE(start_res.has_value());

  std::string_view client_hello(
      reinterpret_cast<const char*>(start_res.value()->outgoing_frame.data()),
      start_res.value()->outgoing_frame.size());
  ASSERT_TRUE(server_init->PutTLSFrame(client_hello).ok());
  auto server_flight_or = server_init->GetTLSFrame();
  ASSERT_TRUE(server_flight_or.ok());

  std::vector<uint8_t> server_flight(server_flight_or->begin(),
                                     server_flight_or->end());
  base::test::TestFuture<
      base::expected<mojom::TlsHandshakePtr, mojom::TlsHandshakeError>>
      proc_future;
  remote_->ProcessHandshakeFrame(server_flight, proc_future.GetCallback());
  auto proc_res = proc_future.Take();
  ASSERT_TRUE(proc_res.has_value());
  EXPECT_TRUE(proc_res.value()->is_ready);
}

}  // namespace
}  // namespace private_ai
