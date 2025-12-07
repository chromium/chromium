// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_CRYPTO_HANDSHAKE_MESSAGE_H_
#define COMPONENTS_LEGION_CRYPTO_HANDSHAKE_MESSAGE_H_

#include <stdint.h>

#include <array>
#include <vector>

#include "components/legion/crypto/constants.h"

namespace legion {

struct HandshakeMessage {
  HandshakeMessage();
  HandshakeMessage(std::array<uint8_t, kP256X962Length> ephemeral_public_key,
                   std::vector<uint8_t> ciphertext);
  ~HandshakeMessage();

  HandshakeMessage(HandshakeMessage&&);
  HandshakeMessage& operator=(HandshakeMessage&&);

  HandshakeMessage(const HandshakeMessage&) = delete;
  HandshakeMessage& operator=(const HandshakeMessage&) = delete;

  std::array<uint8_t, kP256X962Length> ephemeral_public_key;
  std::vector<uint8_t> ciphertext;
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_CRYPTO_HANDSHAKE_MESSAGE_H_
