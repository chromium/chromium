// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_CRYPTO_SECURE_SESSION_IMPL_H_
#define COMPONENTS_LEGION_CRYPTO_SECURE_SESSION_IMPL_H_

#include <memory>
#include <optional>

#include "components/legion/crypto/crypter.h"
#include "components/legion/crypto/noise.h"
#include "components/legion/secure_session.h"
#include "third_party/boringssl/src/include/openssl/ec.h"

namespace legion {

class SecureSessionImpl : public SecureSession {
 public:
  SecureSessionImpl();
  ~SecureSessionImpl() override;

  // SecureSession:
  void GetHandshakeMessage(
      SecureSession::GetHandshakeMessageOnceCallback callback) override;
  void ProcessHandshakeResponse(
      const oak::session::v1::HandshakeResponse& response,
      ProcessHandshakeResponseOnceCallback callback) override;
  void Encrypt(const Request& data, EncryptOnceCallback callback) override;
  void Decrypt(const oak::session::v1::EncryptedMessage& data,
               DecryptOnceCallback callback) override;

  void set_crypter_for_testing(std::unique_ptr<Crypter> crypter) {
    crypter_ = std::move(crypter);
  }

 private:
  oak::session::v1::HandshakeRequest GetHandshakeMessageSync();

  bool ProcessHandshakeResponseSync(
      const oak::session::v1::HandshakeResponse& response);

  std::optional<oak::session::v1::EncryptedMessage> EncryptSync(
      const Request& data);

  std::optional<Response> DecryptSync(
      const oak::session::v1::EncryptedMessage& data);

  std::optional<Noise> noise_;
  bssl::UniquePtr<EC_KEY> ephemeral_key_;
  std::unique_ptr<Crypter> crypter_;
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_CRYPTO_SECURE_SESSION_IMPL_H_
