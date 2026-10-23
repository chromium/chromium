// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/oak_session_service/oak_tls_session_service.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_view_util.h"
#include "base/types/expected.h"
#include "third_party/boringssl/src/include/openssl/x509_vfy.h"
#include "third_party/boringssl/src/pki/pem.h"
#include "third_party/oak/src/oak_session/tls/oak_session_tls.h"
#include "third_party/oak/src/oak_session/tls/util.h"

namespace private_ai {

namespace {

constexpr std::string_view kAllowedBlockTypes[] = {"CERTIFICATE"};

base::unexpected<mojom::TlsHandshakeError> ProtocolError() {
  return base::unexpected(mojom::TlsHandshakeError::kProtocolError);
}

}  // namespace

OakTlsSessionService::OakTlsSessionService(
    mojo::PendingReceiver<mojom::OakTlsSession> receiver)
    : receiver_(this, std::move(receiver)) {}

OakTlsSessionService::~OakTlsSessionService() = default;

void OakTlsSessionService::ResetInitializer() {
  tls_initializer_.reset();
  tls_context_.reset();
}

void OakTlsSessionService::StartHandshake(
    const std::string& expected_server_name,
    const std::string& tca_root_cert_pem,
    StartHandshakeCallback callback) {
  if (handshake_started_) {
    receiver_.ReportBadMessage("Unexpected StartHandshake call");
    return;
  }
  if (expected_server_name.empty() || tca_root_cert_pem.empty()) {
    receiver_.ReportBadMessage("Invalid StartHandshake parameters");
    return;
  }
  handshake_started_ = true;

  // Oak's TrustAnchorProvider takes DER. The TCA root travels as the PEM file
  // it was published as, so it is decoded here (in the sandboxed utility
  // process on Content platforms, or in-process on iOS).
  bssl::PEMTokenizer tokenizer(tca_root_cert_pem, kAllowedBlockTypes);
  std::vector<std::string> trust_anchors_der;
  while (tokenizer.GetNext()) {
    if (!tokenizer.data().empty()) {
      trust_anchors_der.push_back(tokenizer.data());
    }
  }
  if (trust_anchors_der.empty()) {
    DLOG(ERROR) << "TCA root is not a PEM-encoded certificate";
    std::move(callback).Run(ProtocolError());
    return;
  }

  oak::session::tls::ClientContextConfig config{
      .server_trust_anchor_provider =
          oak::session::tls::util::CreateStaticTrustAnchors(
              std::move(trust_anchors_der)),
      .expected_server_name = expected_server_name,
  };
  config.custom_cert_verifier =
      [this](const std::vector<std::string>& /*cert_chain*/,
             int error_code) -> absl::Status {
    if (error_code != X509_V_OK) {
      cert_verify_failed_ = true;
      return absl::UnauthenticatedError(
          "BoringSSL certificate verification failed");
    }
    return absl::OkStatus();
  };

  auto context_or =
      oak::session::tls::OakSessionTlsContext::Create(std::move(config));
  if (!context_or.ok()) {
    DLOG(ERROR) << "Failed to create OakSessionTlsContext: "
                << context_or.status();
    std::move(callback).Run(ProtocolError());
    return;
  }
  tls_context_ = std::move(*context_or);

  auto init_or = tls_context_->NewSession();
  if (!init_or.ok()) {
    DLOG(ERROR) << "Failed to create OakSessionTlsInitializer: "
                << init_or.status();
    ResetInitializer();
    std::move(callback).Run(ProtocolError());
    return;
  }
  tls_initializer_ = std::move(*init_or);

  auto client_hello_or = tls_initializer_->GetTLSFrame();
  if (!client_hello_or.ok() || client_hello_or->empty()) {
    DLOG(ERROR) << "Failed to get initial TLS ClientHello";
    ResetInitializer();
    std::move(callback).Run(ProtocolError());
    return;
  }

  std::vector<uint8_t> client_hello(client_hello_or->begin(),
                                    client_hello_or->end());
  std::move(callback).Run(
      mojom::TlsHandshake::New(std::move(client_hello), /*is_ready=*/false));
}

void OakTlsSessionService::ProcessHandshakeFrame(
    const std::vector<uint8_t>& incoming_frame,
    ProcessHandshakeFrameCallback callback) {
  if (!tls_initializer_ || tls_session_) {
    receiver_.ReportBadMessage(
        "ProcessHandshakeFrame called without an active TLS handshake");
    return;
  }
  if (incoming_frame.empty()) {
    DLOG(ERROR) << "ProcessHandshakeFrame called with empty frame";
    ResetInitializer();
    std::move(callback).Run(ProtocolError());
    return;
  }

  std::string_view frame_view = base::as_string_view(incoming_frame);
  absl::Status put_status = tls_initializer_->PutTLSFrame(frame_view);
  if (!put_status.ok()) {
    DLOG(ERROR) << "Failed to put TLS frame: " << put_status;
    ResetInitializer();
    if (cert_verify_failed_) {
      std::move(callback).Run(
          base::unexpected(mojom::TlsHandshakeError::kCertVerificationFailed));
      return;
    }
    std::move(callback).Run(ProtocolError());
    return;
  }

  auto out_or = tls_initializer_->GetTLSFrame();
  if (!out_or.ok()) {
    DLOG(ERROR) << "Failed to get outgoing TLS frame: " << out_or.status();
    ResetInitializer();
    std::move(callback).Run(ProtocolError());
    return;
  }
  std::vector<uint8_t> outgoing_frame(out_or->begin(), out_or->end());

  if (tls_initializer_->IsReady()) {
    auto open_session_or = tls_initializer_->GetOpenSession();
    if (!open_session_or.ok() || !open_session_or->initial_data.empty()) {
      DLOG(ERROR) << "Failed to get open TLS session or unexpected initial "
                     "data during client handshake";
      ResetInitializer();
      std::move(callback).Run(ProtocolError());
      return;
    }
    // Only reset tls_context_ on success, retaining tls_initializer_ so that
    // custom_verifier_ remains valid while tls_session_ is alive.
    tls_context_.reset();
    tls_session_ = std::move(open_session_or->session);
    std::move(callback).Run(
        mojom::TlsHandshake::New(std::move(outgoing_frame), /*is_ready=*/true));
    return;
  }

  std::move(callback).Run(
      mojom::TlsHandshake::New(std::move(outgoing_frame), /*is_ready=*/false));
}

void OakTlsSessionService::Encrypt(const std::vector<uint8_t>& input,
                                   EncryptCallback callback) {
  if (!tls_session_) {
    receiver_.ReportBadMessage("Encrypt called before handshake completion");
    return;
  }
  if (input.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::string_view plaintext = base::as_string_view(input);
  auto encrypted_or = tls_session_->Encrypt(plaintext);
  if (!encrypted_or.ok()) {
    DLOG(ERROR) << "TLS Encrypt failed: " << encrypted_or.status();
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(
      std::vector<uint8_t>(encrypted_or->begin(), encrypted_or->end()));
}

void OakTlsSessionService::Decrypt(const std::vector<uint8_t>& input,
                                   DecryptCallback callback) {
  if (!tls_session_) {
    receiver_.ReportBadMessage("Decrypt called before handshake completion");
    return;
  }
  if (input.empty()) {
    std::move(callback).Run(std::nullopt, /*peer_closed=*/false);
    return;
  }
  std::string_view ciphertext = base::as_string_view(input);
  auto decrypted_or = tls_session_->Decrypt(ciphertext);
  if (!decrypted_or.ok()) {
    DLOG(ERROR) << "TLS Decrypt failed: " << decrypted_or.status();
    std::move(callback).Run(std::nullopt, /*peer_closed=*/false);
    return;
  }
  // TODO(crbug.com/576736338): Expose SSL_RECEIVED_SHUTDOWN in upstream Oak
  // and pass true for peer_closed when close_notify is received.
  std::move(callback).Run(
      std::vector<uint8_t>(decrypted_or->begin(), decrypted_or->end()),
      /*peer_closed=*/false);
}

}  // namespace private_ai
