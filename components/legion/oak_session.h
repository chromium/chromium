// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_OAK_SESSION_H_
#define COMPONENTS_LEGION_OAK_SESSION_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "components/legion/legion_common.h"

namespace oak::session::v1 {
class EncryptedMessage;
class HandshakeRequest;
class HandshakeResponse;
}  // namespace oak::session::v1

namespace legion {

// Interface for Oak session management.
// Handles cryptographic operations, including handshake, encryption, and decryption.
class OakSession {
 public:
  virtual ~OakSession() = default;

  // Generates the initial handshake message.
  // Returns std::nullopt on failure.
  virtual std::optional<oak::session::v1::HandshakeRequest>
  GetHandshakeMessage() = 0;

  // Processes the server's handshake response (e.g., keys).
  // This should be called after the initial handshake message has been sent
  // and a response has been received from the transport layer.
  // Returns true on success.
  virtual bool ProcessHandshakeResponse(
      const oak::session::v1::HandshakeResponse& response) = 0;

  // Encrypts the given data.
  // This should only be called after the handshake is complete.
  // Returns std::nullopt on failure.
  virtual std::optional<oak::session::v1::EncryptedMessage> Encrypt(
      const Request& data) = 0;

  // Decrypts the given data.
  // This should only be called after the handshake is complete.
  // Returns std::nullopt on failure.
  virtual std::optional<Response> Decrypt(
      const oak::session::v1::EncryptedMessage& data) = 0;
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_OAK_SESSION_H_
