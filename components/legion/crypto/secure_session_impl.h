// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_CRYPTO_SECURE_SESSION_IMPL_H_
#define COMPONENTS_LEGION_CRYPTO_SECURE_SESSION_IMPL_H_

#include <memory>
#include <optional>
#include <vector>

#include "components/legion/crypto/crypter.h"
#include "components/legion/crypto/noise.h"
#include "third_party/boringssl/src/include/openssl/ec.h"

namespace legion {

struct HandshakeMessage {
  HandshakeMessage(std::vector<uint8_t> ephemeral_public_key,
                   std::vector<uint8_t> ciphertext);
  ~HandshakeMessage();

  HandshakeMessage(HandshakeMessage&&);
  HandshakeMessage& operator=(HandshakeMessage&&);

  HandshakeMessage(const HandshakeMessage&) = delete;
  HandshakeMessage& operator=(const HandshakeMessage&) = delete;

  std::vector<uint8_t> ephemeral_public_key;
  std::vector<uint8_t> ciphertext;
};

class SecureSessionImpl {
 public:
  SecureSessionImpl();
  ~SecureSessionImpl();

  HandshakeMessage GetHandshakeMessage();

  bool ProcessHandshakeResponse(const HandshakeMessage& response);

  std::optional<std::vector<uint8_t>> Encrypt(
      const std::vector<uint8_t>& input);

  std::optional<std::vector<uint8_t>> Decrypt(
      const std::vector<uint8_t>& input);

  void set_crypter_for_testing(std::unique_ptr<Crypter> crypter);

 private:
  std::optional<Noise> noise_;
  bssl::UniquePtr<EC_KEY> ephemeral_key_;
  std::unique_ptr<Crypter> crypter_;
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_CRYPTO_SECURE_SESSION_IMPL_H_
