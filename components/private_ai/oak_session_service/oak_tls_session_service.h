// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_OAK_SESSION_SERVICE_OAK_TLS_SESSION_SERVICE_H_
#define COMPONENTS_PRIVATE_AI_OAK_SESSION_SERVICE_OAK_TLS_SESSION_SERVICE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "components/private_ai/mojom/oak_session.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace oak::session::tls {
class OakSessionTlsContext;
class OakSessionTlsInitializer;
class OakSessionTls;
}  // namespace oak::session::tls

namespace private_ai {

// Implements the mojom::OakTlsSession interface using BoringSSL and Oak TLS.
// On Content platforms (via PrivateAiOakSessionDriverContent), this runs in a
// sandboxed utility process. On iOS (via PrivateAiOakSessionDriverIOS), it runs
// in-process on the browser sequence.
class OakTlsSessionService : public mojom::OakTlsSession {
 public:
  explicit OakTlsSessionService(
      mojo::PendingReceiver<mojom::OakTlsSession> receiver);
  ~OakTlsSessionService() override;

  void set_disconnect_handler(base::OnceClosure handler) {
    receiver_.set_disconnect_handler(std::move(handler));
  }

  OakTlsSessionService(const OakTlsSessionService&) = delete;
  OakTlsSessionService& operator=(const OakTlsSessionService&) = delete;

  // mojom::OakTlsSession:
  void StartHandshake(const std::string& expected_server_name,
                      const std::string& tca_root_cert_pem,
                      StartHandshakeCallback callback) override;
  void ProcessHandshakeFrame(const std::vector<uint8_t>& incoming_frame,
                             ProcessHandshakeFrameCallback callback) override;
  void Encrypt(const std::vector<uint8_t>& input,
               EncryptCallback callback) override;
  void Decrypt(const std::vector<uint8_t>& input,
               DecryptCallback callback) override;

 private:
  void ResetInitializer();

  bool handshake_started_ = false;
  // Set by the custom cert verifier when BoringSSL rejects the server chain.
  bool cert_verify_failed_ = false;

  std::unique_ptr<oak::session::tls::OakSessionTlsContext> tls_context_;
  std::unique_ptr<oak::session::tls::OakSessionTlsInitializer> tls_initializer_;
  std::unique_ptr<oak::session::tls::OakSessionTls> tls_session_;

  mojo::Receiver<mojom::OakTlsSession> receiver_;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_OAK_SESSION_SERVICE_OAK_TLS_SESSION_SERVICE_H_
