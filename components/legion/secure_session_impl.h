// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_SECURE_SESSION_IMPL_H_
#define COMPONENTS_LEGION_SECURE_SESSION_IMPL_H_

#include "components/legion/secure_session.h"

#include <memory>
#include <optional>

#include "components/legion/crypter.h"
#include "components/legion/noise.h"
#include "third_party/boringssl/src/include/openssl/ec.h"

namespace legion {

class SecureSessionImpl : public SecureSession {
 public:
  SecureSessionImpl();
  ~SecureSessionImpl() override;

  // SecureSession:
  std::optional<oak::session::v1::HandshakeRequest> GetHandshakeMessage()
      override;
  bool ProcessHandshakeResponse(
      const oak::session::v1::HandshakeResponse& response) override;
  std::optional<oak::session::v1::EncryptedMessage> Encrypt(
      const Request& data) override;
  std::optional<Response> Decrypt(
      const oak::session::v1::EncryptedMessage& data) override;

  void set_crypter_for_testing(std::unique_ptr<Crypter> crypter) {
    crypter_ = std::move(crypter);
  }

 private:
  std::optional<Noise> noise_;
  bssl::UniquePtr<EC_KEY> ephemeral_key_;
  std::unique_ptr<Crypter> crypter_;
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_SECURE_SESSION_IMPL_H_
